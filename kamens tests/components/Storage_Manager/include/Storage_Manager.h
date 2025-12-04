#pragma once

#include "esp_err.h"

/* ======================= STRING STORAGE HEADER ======================= */
/*
 * This header lets other files save/read/delete the short string in flash.
 */
void storage_manager_init(void);
const char *storage_manager_get_string(void);
void storage_manager_save_string(const char *value);
void storage_manager_delete_string(void);
