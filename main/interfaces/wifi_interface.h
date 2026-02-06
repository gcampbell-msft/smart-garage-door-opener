/**
 * @file wifi_interface.h
 * @brief WiFi interface abstraction for testability.
 * 
 * This interface allows swapping between real ESP8266 WiFi implementation
 * and mock implementations for testing.
 */

#ifndef WIFI_INTERFACE_H
#define WIFI_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi connection status
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED
} wifi_status_t;

/**
 * @brief Callback type for WiFi status changes
 */
typedef void (*wifi_status_callback_t)(wifi_status_t status);

/**
 * @brief WiFi interface function pointers
 */
typedef struct {
    /**
     * @brief Initialize the WiFi subsystem
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void);

    /**
     * @brief Connect to WiFi network
     * @param ssid Network SSID
     * @param password Network password
     * @return 0 on success, negative error code on failure
     */
    int (*connect)(const char* ssid, const char* password);

    /**
     * @brief Disconnect from WiFi network
     * @return 0 on success, negative error code on failure
     */
    int (*disconnect)(void);

    /**
     * @brief Get current WiFi status
     * @return Current WiFi status
     */
    wifi_status_t (*get_status)(void);

    /**
     * @brief Register callback for status changes
     * @param callback Function to call when status changes
     */
    void (*register_status_callback)(wifi_status_callback_t callback);

    /**
     * @brief Deinitialize the WiFi subsystem
     */
    void (*deinit)(void);
} wifi_interface_t;

/**
 * @brief Get the WiFi interface implementation
 * 
 * Returns either the real ESP8266 implementation or a mock
 * based on compile-time configuration.
 * 
 * @return Pointer to the WiFi interface
 */
const wifi_interface_t* wifi_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_INTERFACE_H */
