/**
 * @file ble_central.h
 * @brief BLE Central (Client) functionality for scanning and connecting to devices
 */

#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BLE central module
 * 
 * Sets up NimBLE stack and prepares for scanning/connecting
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_central_init(void);

/**
 * @brief Start scanning for BLE devices
 * 
 * Scans for nearby BLE peripherals and attempts to connect
 * to the first connectable device found
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_central_start_scan(void);

/**
 * @brief Stop scanning for BLE devices
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_central_stop_scan(void);

/**
 * @brief Check if currently connected to a device
 * 
 * @return true if connected, false otherwise
 */
bool ble_central_is_connected(void);

/**
 * @brief Disconnect from the currently connected device
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_central_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CENTRAL_H */
