#ifndef WIFI_IMPL_H
#define WIFI_IMPL_H

#include "esp_err.h"
#include "esp_event.h"

/**
 * @brief Callback function type for WiFi connected event
 * Called when WiFi successfully connects
 */
typedef void (*wifi_connected_cb_t)(void);

/**
 * @brief Callback function type for WiFi disconnected event
 * Called when WiFi disconnects
 * @param retry_count Number of reconnection attempts made
 */
typedef void (*wifi_disconnected_cb_t)(int retry_count);

/**
 * @brief Callback function type for WiFi connection failed event
 * Called when WiFi fails to connect after maximum retry attempts
 */
typedef void (*wifi_failed_cb_t)(void);

/**
 * @brief Callback function type for WiFi station started event
 * Called when WiFi station mode is initialized and started
 */
typedef void (*wifi_sta_start_cb_t)(void);

/**
 * @brief Callback function type for IP obtained event
 * Called when an IP address is obtained
 * @param ip_addr IP address as string (e.g., "192.168.1.100")
 */
typedef void (*wifi_got_ip_cb_t)(const char* ip_addr);

/**
 * @brief Structure containing all WiFi event callbacks
 */
typedef struct {
    wifi_connected_cb_t on_connected;           /**< Called on successful connection */
    wifi_disconnected_cb_t on_disconnected;     /**< Called on disconnection */
    wifi_failed_cb_t on_failed;                 /**< Called when connection fails after max retries */
    wifi_sta_start_cb_t on_sta_start;           /**< Called when WiFi station starts */
    wifi_got_ip_cb_t on_got_ip;                 /**< Called when IP address is obtained */
} wifi_event_callbacks_t;

/**
 * @brief Register WiFi event callbacks
 * @param callbacks Pointer to structure containing callback functions
 *                  NULL callbacks will be ignored
 */
void wifi_register_event_callbacks(const wifi_event_callbacks_t* callbacks);

/**
 * @brief Initialize WiFi in station mode and connect to AP
 * @param max_retries Maximum number of retry attempts for WiFi connection
 * @param retry_interval_ms Interval between retry attempts in milliseconds
 */
void wifi_init_sta(const int max_retries, const int retry_interval_ms);

#endif // WIFI_IMPL_H
