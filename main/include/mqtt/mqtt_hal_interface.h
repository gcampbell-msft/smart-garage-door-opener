/**
 * @file mqtt_hal_interface.h
 * @brief Hardware Abstraction Layer for ESP8266 MQTT client functions
 * 
 * This interface abstracts all ESP MQTT client SDK calls used by mqtt_impl.c,
 * allowing for testing with mocks without requiring actual hardware or MQTT broker.
 */

#ifndef MQTT_HAL_INTERFACE_H
#define MQTT_HAL_INTERFACE_H

#include "esp_err.h"
#include "esp_event.h"
#include "mqtt_client.h"

/* ============================================================================
 * MQTT HAL Functions
 * ============================================================================ */

/**
 * @brief Initialize MQTT client
 * @param config MQTT client configuration
 * @return MQTT client handle or NULL on failure
 */
esp_mqtt_client_handle_t mqtt_hal_client_init(const esp_mqtt_client_config_t *config);

/**
 * @brief Start MQTT client
 * @param client MQTT client handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_hal_client_start(esp_mqtt_client_handle_t client);

/**
 * @brief Publish MQTT message
 * @param client MQTT client handle
 * @param topic Topic to publish to
 * @param data Data to publish
 * @param len Length of data (0 for null-terminated string)
 * @param qos Quality of Service level
 * @param retain Retain flag
 * @return Message ID on success, -1 on failure
 */
int mqtt_hal_client_publish(esp_mqtt_client_handle_t client,
                             const char *topic,
                             const char *data,
                             int len,
                             int qos,
                             int retain);

/**
 * @brief Subscribe to MQTT topic
 * @param client MQTT client handle
 * @param topic Topic to subscribe to
 * @param qos Quality of Service level
 * @return Message ID on success, -1 on failure
 */
int mqtt_hal_client_subscribe(esp_mqtt_client_handle_t client,
                               const char *topic,
                               int qos);

/**
 * @brief Register MQTT event callback
 * @param client MQTT client handle
 * @param event Event type (ESP_EVENT_ANY_ID for all events)
 * @param event_handler Event handler function
 * @param event_handler_arg Argument to pass to handler
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_hal_client_register_event(esp_mqtt_client_handle_t client,
                                          esp_mqtt_event_id_t event,
                                          esp_event_handler_t event_handler,
                                          void* event_handler_arg);

/* ============================================================================
 * Logging HAL Functions
 * ============================================================================ */

/**
 * @brief Log at INFO level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void mqtt_hal_log_info(const char* tag, const char* format, ...);

/**
 * @brief Log at ERROR level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void mqtt_hal_log_error(const char* tag, const char* format, ...);

/**
 * @brief Log at DEBUG level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void mqtt_hal_log_debug(const char* tag, const char* format, ...);

#endif // MQTT_HAL_INTERFACE_H
