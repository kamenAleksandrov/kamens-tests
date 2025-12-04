/**
 * @file station_example_main.c
 * @brief Main entry point - WiFi, Web Server, and BLE Central
 * 
 * Initializes all modules: NVS, LED, Storage, WiFi, Web Server, and BLE.
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "LED_Controler.h"
#include "Storage_Manager.h"
#include "WiFi.h"
#include "WEB_Server.h"
#include "BLE.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Station Starting ===");
    
    /* Initialize NVS - required for WiFi and BLE bonding storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    /* Initialize LED control */
    led_control_init();
    ESP_LOGI(TAG, "LED control initialized");

    /* Initialize storage manager */
    storage_manager_init();
    ESP_LOGI(TAG, "Storage manager initialized");

    /* Start WiFi (will start web server when connected) */
    wifi_manager_start();
    ESP_LOGI(TAG, "WiFi manager started");

    /* Initialize and start BLE Peripheral */
    int rc = ble_peripheral_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize BLE Peripheral");
    } else {
        ESP_LOGI(TAG, "BLE Peripheral started as 'ESP-SKYNET'. Waiting for connections...");
    }

    ESP_LOGI(TAG, "=== All modules initialized ===");
}

