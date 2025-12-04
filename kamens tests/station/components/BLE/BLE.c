/**
 * @file ble_central.c
 * @brief BLE Central (Client) implementation
 * 
 * Scans for BLE peripherals and connects to them, printing device info on success.
 */

#include "esp_mac.h"

#include "BLE.h"

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_bt.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "ble_central";

/* Connection state */
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_is_connected = false;
static uint8_t s_own_addr_type;

/* Forward declarations */
static void ble_central_on_sync(void);
static void ble_central_on_reset(int reason);
static int ble_central_gap_event(struct ble_gap_event *event, void *arg);
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
 * @brief Print discovered device information
 */
static void print_device_info(const struct ble_gap_disc_desc *disc)
{
    char addr_str[18];
    addr_to_string(disc->addr.val, addr_str, sizeof(addr_str));
    
    ESP_LOGI(TAG, "Discovered device:");
    ESP_LOGI(TAG, "  Address: %s (%s)", addr_str, addr_type_to_string(disc->addr.type));
    ESP_LOGI(TAG, "  RSSI: %d dBm", disc->rssi);
    
    /* Parse advertisement data for device name */
    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc == 0 && fields.name != NULL && fields.name_len > 0) {
        ESP_LOGI(TAG, "  Name: %.*s%s", 
                 fields.name_len, fields.name,
                 fields.name_is_complete ? "" : " (partial)");
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
    ESP_LOGI(TAG, "Successfully connected to BLE device!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Connection Handle: %u", desc->conn_handle);
    ESP_LOGI(TAG, "  Peer Address: %s (%s)", addr_str, addr_type_to_string(desc->peer_id_addr.type));
    ESP_LOGI(TAG, "  Connection Interval: %.2f ms", desc->conn_itvl * 1.25f);
    ESP_LOGI(TAG, "  Slave Latency: %u", desc->conn_latency);
    ESP_LOGI(TAG, "  Supervision Timeout: %u ms", desc->supervision_timeout * 10);
    ESP_LOGI(TAG, "  Role: %s", desc->role == BLE_GAP_ROLE_MASTER ? "Central" : "Peripheral");
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief Attempt to connect to a discovered device
 */
static int connect_to_device(const ble_addr_t *addr)
{
    char addr_str[18];
    addr_to_string(addr->val, addr_str, sizeof(addr_str));
    
    ESP_LOGI(TAG, "Attempting to connect to %s...", addr_str);
    
    /* Stop scanning before connecting */
    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "Failed to cancel discovery: %d", rc);
    }
    
    /* Connection parameters */
    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 0x0010,            /* 10ms */
        .scan_window = 0x0010,          /* 10ms */
        .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,  /* 30ms */
        .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,  /* 50ms */
        .latency = 0,
        .supervision_timeout = 0x0100,  /* 2.56s */
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    
    rc = ble_gap_connect(s_own_addr_type, addr, 10000, &conn_params,
                         ble_central_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initiate connection: %d", rc);
        return rc;
    }
    
    return 0;
}

/**
 * @brief GAP event handler
 */
static int ble_central_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        /* Device discovered during scan */
        const struct ble_gap_disc_desc *disc = &event->disc;
        
        print_device_info(disc);
        
        /* Check if device is connectable */
        if (disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
            disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
            
            /* Try to connect to this device */
            if (!s_is_connected) {
                connect_to_device(&disc->addr);
            }
        }
        return 0;
    }
    
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
            
            /* Restart scanning */
            ESP_LOGI(TAG, "Restarting scan...");
            ble_central_start_scan();
        }
        return 0;
    }
    
    case BLE_GAP_EVENT_DISCONNECT: {
        char addr_str[18];
        addr_to_string(event->disconnect.conn.peer_id_addr.val, addr_str, sizeof(addr_str));
        
        ESP_LOGI(TAG, "Disconnected from %s (reason=0x%02x)", 
                 addr_str, event->disconnect.reason);
        
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_is_connected = false;
        
        /* Restart scanning to find new devices */
        ESP_LOGI(TAG, "Restarting scan to find new devices...");
        ble_central_start_scan();
        return 0;
    }
    
    case BLE_GAP_EVENT_DISC_COMPLETE: {
        ESP_LOGI(TAG, "Scan complete (reason=%d)", event->disc_complete.reason);
        
        /* Restart scan if not connected */
        if (!s_is_connected) {
            ESP_LOGI(TAG, "No connection established, restarting scan...");
            ble_central_start_scan();
        }
        return 0;
    }
    
    case BLE_GAP_EVENT_CONN_UPDATE: {
        ESP_LOGI(TAG, "Connection parameters updated");
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
static void ble_central_on_sync(void)
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
        ESP_LOGI(TAG, "BLE Central initialized. Our address: %s", addr_str);
    }
    
    /* Start scanning for devices */
    ESP_LOGI(TAG, "Starting BLE scan for nearby devices...");
    ble_central_start_scan();
}

/**
 * @brief Called when BLE host resets
 */
static void ble_central_on_reset(int reason)
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

int ble_central_init(void)
{
    int rc;
    
    ESP_LOGI(TAG, "Initializing BLE Central...");
    
    /* Release memory used by classic BT (we only use BLE) */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    /* Initialize NimBLE port (handles HCI and controller in ESP-IDF v5.x) */
    nimble_port_init();
    
    /* Configure NimBLE host callbacks */
    ble_hs_cfg.reset_cb = ble_central_on_reset;
    ble_hs_cfg.sync_cb = ble_central_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    /* Start NimBLE host task */
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "BLE Central initialization complete");
    return 0;
}

int ble_central_start_scan(void)
{
    struct ble_gap_disc_params disc_params = {
        .filter_duplicates = 1,
        .passive = 0,               /* Active scan (request scan response) */
        .itvl = 0x0010,             /* 10ms interval */
        .window = 0x0010,           /* 10ms window */
        .filter_policy = 0,         /* No whitelist filtering */
        .limited = 0,               /* General discovery */
    };
    
    int rc = ble_gap_disc(s_own_addr_type, 30000, &disc_params,
                          ble_central_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scan: %d", rc);
        return rc;
    }
    
    ESP_LOGI(TAG, "Scanning for BLE devices (30 seconds)...");
    return 0;
}

int ble_central_stop_scan(void)
{
    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop scan: %d", rc);
        return rc;
    }
    
    ESP_LOGI(TAG, "Scan stopped");
    return 0;
}

bool ble_central_is_connected(void)
{
    return s_is_connected;
}

int ble_central_disconnect(void)
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
