/**
 * @file BLE.c
 * @brief BLE Peripheral (Server) implementation
 * 
 * Advertises as "ESP-SKYNET" and waits for devices to connect.
 * Prints connection info when a device connects.
 */

#include "BLE.h"

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_mac.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_peripheral";

/* Device name */
#define DEVICE_NAME "ESP-SKYNET"

/* Connection state */
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_is_connected = false;
static uint8_t s_own_addr_type;

/* Forward declarations */
static void ble_peripheral_on_sync(void);
static void ble_peripheral_on_reset(int reason);
static int ble_peripheral_gap_event(struct ble_gap_event *event, void *arg);
static void ble_host_task(void *param);

/**
 * @brief Convert BLE address to string
 */
static void addr_to_string(const uint8_t *addr, char *str, size_t str_len)
{
    snprintf(str, str_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

/**
 * @brief Get human-readable address type
 */
static const char *addr_type_to_string(uint8_t addr_type)
{
    switch (addr_type) {
        case BLE_ADDR_PUBLIC:       return "Public";
        case BLE_ADDR_RANDOM:       return "Random";
        case BLE_ADDR_PUBLIC_ID:    return "Public ID";
        case BLE_ADDR_RANDOM_ID:    return "Random ID";
        default:                    return "Unknown";
    }
}

/**
 * @brief Print connected device details
 */
static void print_connection_info(const struct ble_gap_conn_desc *desc)
{
    char addr_str[18];
    addr_to_string(desc->peer_id_addr.val, addr_str, sizeof(addr_str));
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "CONNECTION SUCCESSFUL!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Device Address: %s (%s)", addr_str, addr_type_to_string(desc->peer_id_addr.type));
    ESP_LOGI(TAG, "  Connection Handle: %u", desc->conn_handle);
    ESP_LOGI(TAG, "  Connection Interval: %.2f ms", desc->conn_itvl * 1.25f);
    ESP_LOGI(TAG, "  Slave Latency: %u", desc->conn_latency);
    ESP_LOGI(TAG, "  Supervision Timeout: %u ms", desc->supervision_timeout * 10);
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief Start advertising
 */
static int start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    /* Set advertising flags */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    /* Set TX power level (optional) */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data: %d", rc);
        return rc;
    }

    /* Configure advertising parameters */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  /* Undirected connectable */
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  /* General discoverable */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100); /* 100ms min interval */
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(150); /* 150ms max interval */

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_peripheral_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "Advertising started as '%s'", DEVICE_NAME);
    ESP_LOGI(TAG, "Waiting for a device to connect...");
    return 0;
}

/**
 * @brief GAP event handler
 */
static int ble_peripheral_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            /* Connection successful */
            s_conn_handle = event->connect.conn_handle;
            s_is_connected = true;
            
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                print_connection_info(&desc);
            }
        } else {
            /* Connection failed */
            ESP_LOGW(TAG, "Connection failed (status=%d)", event->connect.status);
            s_is_connected = false;
            
            /* Restart advertising */
            start_advertising();
        }
        return 0;
    }
    
    case BLE_GAP_EVENT_DISCONNECT: {
        char addr_str[18];
        addr_to_string(event->disconnect.conn.peer_id_addr.val, addr_str, sizeof(addr_str));
        
        ESP_LOGI(TAG, "Device disconnected: %s (reason=0x%02x)", 
                 addr_str, event->disconnect.reason);
        
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_is_connected = false;
        
        /* Restart advertising to accept new connections */
        ESP_LOGI(TAG, "Restarting advertising...");
        start_advertising();
        return 0;
    }
    
    case BLE_GAP_EVENT_CONN_UPDATE: {
        ESP_LOGI(TAG, "Connection parameters updated");
        return 0;
    }
    
    case BLE_GAP_EVENT_ADV_COMPLETE: {
        ESP_LOGI(TAG, "Advertising complete");
        /* Restart advertising if not connected */
        if (!s_is_connected) {
            start_advertising();
        }
        return 0;
    }
    
    default:
        ESP_LOGD(TAG, "Unhandled GAP event: %d", event->type);
        return 0;
    }
}

/**
 * @brief Called when BLE host and controller are in sync
 */
static void ble_peripheral_on_sync(void)
{
    int rc;
    
    /* Determine best address type to use */
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer address type: %d", rc);
        return;
    }
    
    /* Ensure we have a valid address */
    rc = ble_hs_util_ensure_addr(s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address: %d", rc);
        return;
    }
    
    /* Print our address */
    uint8_t addr[6];
    rc = ble_hs_id_copy_addr(s_own_addr_type, addr, NULL);
    if (rc == 0) {
        char addr_str[18];
        addr_to_string(addr, addr_str, sizeof(addr_str));
        ESP_LOGI(TAG, "BLE Peripheral initialized. Our address: %s", addr_str);
    }
    
    /* Start advertising */
    ESP_LOGI(TAG, "Starting BLE advertising as '%s'...", DEVICE_NAME);
    start_advertising();
}

/**
 * @brief Called when BLE host resets
 */
static void ble_peripheral_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset (reason=0x%02x)", reason);
    s_is_connected = false;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

/**
 * @brief NimBLE host task
 */
static void ble_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* Public API implementation */

int ble_peripheral_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE Peripheral...");
    
    /* Release memory used by classic BT (we only use BLE) */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    /* Initialize NimBLE port */
    nimble_port_init();
    
    /* Initialize GAP and GATT services */
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    /* Set the device name */
    ble_svc_gap_device_name_set(DEVICE_NAME);
    
    /* Configure NimBLE host callbacks */
    ble_hs_cfg.reset_cb = ble_peripheral_on_reset;
    ble_hs_cfg.sync_cb = ble_peripheral_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    /* Start NimBLE host task */
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "BLE Peripheral initialization complete");
    return 0;
}

int ble_peripheral_start_advertising(void)
{
    if (s_is_connected) {
        ESP_LOGW(TAG, "Already connected, cannot advertise");
        return -1;
    }
    return start_advertising();
}

int ble_peripheral_stop_advertising(void)
{
    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising: %d", rc);
        return rc;
    }
    
    ESP_LOGI(TAG, "Advertising stopped");
    return 0;
}

bool ble_peripheral_is_connected(void)
{
    return s_is_connected;
}

int ble_peripheral_disconnect(void)
{
    if (!s_is_connected || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "Not connected");
        return -1;
    }
    
    int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to disconnect: %d", rc);
        return rc;
    }
    
    return 0;
}
