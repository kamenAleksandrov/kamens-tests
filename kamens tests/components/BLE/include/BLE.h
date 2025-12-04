/**
 * @file BLE.h
 * @brief BLE Peripheral (Server) functionality - waits for devices to connect
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BLE peripheral module
 * 
 * Sets up NimBLE stack and starts advertising as "ESP-SKYNET"
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_peripheral_init(void);

/**
 * @brief Start BLE advertising
 * 
 * Makes the device discoverable and connectable
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_peripheral_start_advertising(void);

/**
 * @brief Stop BLE advertising
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_peripheral_stop_advertising(void);

/**
 * @brief Check if a device is currently connected
 * 
 * @return true if connected, false otherwise
 */
bool ble_peripheral_is_connected(void);

/**
 * @brief Disconnect the currently connected device
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_peripheral_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_H */
