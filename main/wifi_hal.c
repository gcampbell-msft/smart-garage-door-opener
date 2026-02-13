/**
 * @file wifi_hal.c
 * @brief Real ESP8266 WiFi HAL implementation - passes through to ESP SDK
 * 
 * This is the production implementation that calls the actual ESP8266 SDK functions.
 * For testing, use wifi_hal_mock.c instead.
 */

#include "wifi_hal_interface.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include <stdarg.h>

/* ============================================================================
 * Network Initialization HAL Implementation
 * ============================================================================ */

void wifi_hal_tcpip_adapter_init(void)
{
    tcpip_adapter_init();
}

esp_err_t wifi_hal_event_loop_create_default(void)
{
    return esp_event_loop_create_default();
}

/* ============================================================================
 * WiFi HAL Implementation
 * ============================================================================ */

wifi_init_config_t wifi_hal_get_default_wifi_init_config(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    return cfg;
}

esp_err_t wifi_hal_wifi_init(const wifi_init_config_t *config)
{
    return esp_wifi_init(config);
}

esp_err_t wifi_hal_event_handler_register(esp_event_base_t event_base,
                                           int32_t event_id,
                                           esp_event_handler_t event_handler,
                                           void* event_handler_arg)
{
    return esp_event_handler_register(event_base, event_id, event_handler, event_handler_arg);
}

esp_err_t wifi_hal_wifi_set_mode(wifi_mode_t mode)
{
    return esp_wifi_set_mode(mode);
}

esp_err_t wifi_hal_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_wifi_set_config(interface, conf);
}

esp_err_t wifi_hal_wifi_start(void)
{
    return esp_wifi_start();
}

esp_err_t wifi_hal_wifi_connect(void)
{
    return esp_wifi_connect();
}

const char* wifi_hal_get_ip_string_from_event(void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    return ip4addr_ntoa(&event->ip_info.ip);
}

/* ============================================================================
 * FreeRTOS Event Group HAL Implementation
 * ============================================================================ */

EventGroupHandle_t wifi_hal_event_group_create(void)
{
    return xEventGroupCreate();
}

EventBits_t wifi_hal_event_group_wait_bits(EventGroupHandle_t xEventGroup,
                                            const EventBits_t uxBitsToWaitFor,
                                            const BaseType_t xClearOnExit,
                                            const BaseType_t xWaitForAllBits,
                                            TickType_t xTicksToWait)
{
    return xEventGroupWaitBits(xEventGroup, uxBitsToWaitFor, xClearOnExit, xWaitForAllBits, xTicksToWait);
}

EventBits_t wifi_hal_event_group_set_bits(EventGroupHandle_t xEventGroup,
                                           const EventBits_t uxBitsToSet)
{
    return xEventGroupSetBits(xEventGroup, uxBitsToSet);
}

void wifi_hal_event_group_delete(EventGroupHandle_t xEventGroup)
{
    vEventGroupDelete(xEventGroup);
}

/* ============================================================================
 * FreeRTOS Timer HAL Implementation
 * ============================================================================ */

TimerHandle_t wifi_hal_timer_create(const char * const pcTimerName,
                                     const TickType_t xTimerPeriodInTicks,
                                     const UBaseType_t uxAutoReload,
                                     void * const pvTimerID,
                                     TimerCallbackFunction_t pxCallbackFunction)
{
    return xTimerCreate(pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction);
}

BaseType_t wifi_hal_timer_start(TimerHandle_t xTimer, TickType_t xTicksToWait)
{
    return xTimerStart(xTimer, xTicksToWait);
}

BaseType_t wifi_hal_timer_stop(TimerHandle_t xTimer, TickType_t xTicksToWait)
{
    return xTimerStop(xTimer, xTicksToWait);
}

BaseType_t wifi_hal_timer_reset(TimerHandle_t xTimer, TickType_t xTicksToWait)
{
    return xTimerReset(xTimer, xTicksToWait);
}

/* ============================================================================
 * Logging HAL Implementation
 * ============================================================================ */

void wifi_hal_log_info(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_write(ESP_LOG_INFO, tag, format, args);
    va_end(args);
}

void wifi_hal_log_error(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_write(ESP_LOG_ERROR, tag, format, args);
    va_end(args);
}

void wifi_hal_log_debug(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_write(ESP_LOG_DEBUG, tag, format, args);
    va_end(args);
}
