/* ======================= NVS STRING STORAGE FUNCTIONS ======================= */
/*
 * This file hides all the flash (NVS) handling details.
 * Other code can just call the helper functions without worrying about NVS.
 */

#include "Storage_Manager.h"

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_mac.h"

/* Tiny logging tag for this module. */
static const char *TAG = "storage";

/* Configuration for where the string lives inside NVS. */
#define STRING_NAMESPACE "storage"
#define STRING_KEY "my_string"
#define STRING_MAX_LEN 64

/* Buffer in RAM that always mirrors the flash value. */
static char s_stored_string[STRING_MAX_LEN] = {0};

void storage_manager_init(void)
{
    /* First time setup: make sure NVS itself is ready to use. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Once NVS is good, load the previously saved string (if any). */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STRING_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "No stored string yet, using empty");
        s_stored_string[0] = '\0';
        return;
    }

    size_t required_size = STRING_MAX_LEN;
    err = nvs_get_str(nvs_handle, STRING_KEY, s_stored_string, &required_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Loaded string from NVS: '%s'", s_stored_string);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "String key not found in NVS, using empty");
        s_stored_string[0] = '\0';
    }
    else
    {
        ESP_LOGW(TAG, "Error reading string from NVS: %s", esp_err_to_name(err));
        s_stored_string[0] = '\0';
    }

    nvs_close(nvs_handle);
}

const char *storage_manager_get_string(void)
{
    return s_stored_string;
}

void storage_manager_save_string(const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STRING_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs_handle, STRING_KEY, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set string in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }
    else
    {
        strncpy(s_stored_string, value, STRING_MAX_LEN - 1);
        s_stored_string[STRING_MAX_LEN - 1] = '\0';
        ESP_LOGI(TAG, "String saved to NVS: '%s'", s_stored_string);
    }

    nvs_close(nvs_handle);
}

void storage_manager_delete_string(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STRING_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for deleting: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_erase_key(nvs_handle, STRING_KEY);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "String key not found in NVS, nothing to delete");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase key from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }
    else
    {
        ESP_LOGI(TAG, "String deleted from NVS");
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS after erase: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    s_stored_string[0] = '\0';
}
