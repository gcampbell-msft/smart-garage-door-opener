/**
 * @file mqtt_hal.c
 * @brief Real ESP8266 MQTT HAL implementation - passes through to ESP MQTT client
 * 
 * This is the production implementation that calls the actual ESP MQTT client functions.
 * For testing, use mqtt_hal_mock.c instead.
 */

#include "mqtt_hal_interface.h"
#include "esp_log.h"
#include <stdarg.h>

/* ============================================================================
 * MQTT HAL Implementation
 * ============================================================================ */

esp_mqtt_client_handle_t mqtt_hal_client_init(const esp_mqtt_client_config_t *config)
{
    return esp_mqtt_client_init(config);
}

esp_err_t mqtt_hal_client_start(esp_mqtt_client_handle_t client)
{
    return esp_mqtt_client_start(client);
}

int mqtt_hal_client_publish(esp_mqtt_client_handle_t client,
                             const char *topic,
                             const char *data,
                             int len,
                             int qos,
                             int retain)
{
    return esp_mqtt_client_publish(client, topic, data, len, qos, retain);
}

int mqtt_hal_client_subscribe(esp_mqtt_client_handle_t client,
                               const char *topic,
                               int qos)
{
    return esp_mqtt_client_subscribe(client, topic, qos);
}

esp_err_t mqtt_hal_client_register_event(esp_mqtt_client_handle_t client,
                                          esp_mqtt_event_id_t event,
                                          esp_event_handler_t event_handler,
                                          void* event_handler_arg)
{
    return esp_mqtt_client_register_event(client, event, event_handler, event_handler_arg);
}

/* ============================================================================
 * Logging HAL Implementation
 * ============================================================================ */

void mqtt_hal_log_info(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_INFO, tag, format, args);
    va_end(args);
}

void mqtt_hal_log_error(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_ERROR, tag, format, args);
    va_end(args);
}

void mqtt_hal_log_debug(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_DEBUG, tag, format, args);
    va_end(args);
}
