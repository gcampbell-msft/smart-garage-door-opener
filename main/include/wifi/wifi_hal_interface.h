/**
 * @file wifi_hal_interface.h
 * @brief Hardware Abstraction Layer for ESP8266 WiFi and FreeRTOS functions
 * 
 * This interface abstracts all ESP8266 WiFi SDK calls and FreeRTOS primitives
 * used by wifi_impl.c, allowing for testing with mocks without requiring actual hardware.
 */

#ifndef WIFI_HAL_INTERFACE_H
#define WIFI_HAL_INTERFACE_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

/* ============================================================================
 * Network Initialization HAL Functions
 * ============================================================================ */

/**
 * @brief Initialize the TCP/IP adapter
 */
void wifi_hal_tcpip_adapter_init(void);

/**
 * @brief Create the default event loop
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_event_loop_create_default(void);

/* ============================================================================
 * WiFi HAL Functions
 * ============================================================================ */

/**
 * @brief Get default WiFi initialization configuration
 * @return Default WiFi initialization configuration
 */
wifi_init_config_t wifi_hal_get_default_wifi_init_config(void);

/**
 * @brief Initialize WiFi with the given configuration
 * @param config WiFi initialization configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_wifi_init(const wifi_init_config_t *config);

/**
 * @brief Register an event handler
 * @param event_base Event base (e.g., WIFI_EVENT, IP_EVENT)
 * @param event_id Event ID (e.g., ESP_EVENT_ANY_ID)
 * @param event_handler Handler function pointer
 * @param event_handler_arg Argument to pass to handler
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_event_handler_register(esp_event_base_t event_base,
                                           int32_t event_id,
                                           esp_event_handler_t event_handler,
                                           void* event_handler_arg);

/**
 * @brief Set WiFi operating mode
 * @param mode WiFi mode (e.g., WIFI_MODE_STA)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_wifi_set_mode(wifi_mode_t mode);

/**
 * @brief Set WiFi configuration
 * @param interface WiFi interface (e.g., ESP_IF_WIFI_STA)
 * @param conf WiFi configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);

/**
 * @brief Start WiFi
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_wifi_start(void);

/**
 * @brief Connect to WiFi AP
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_hal_wifi_connect(void);

/**
 * @brief Extract IP address string from IP_EVENT_STA_GOT_IP event data
 * @param event_data Pointer to event data (ip_event_got_ip_t*)
 * @return IP address as string (e.g., "192.168.1.100")
 */
const char* wifi_hal_get_ip_string_from_event(void* event_data);

/* ============================================================================
 * FreeRTOS Event Group HAL Functions
 * ============================================================================ */

/**
 * @brief Create an event group
 * @return Event group handle
 */
EventGroupHandle_t wifi_hal_event_group_create(void);

/**
 * @brief Wait for bits in an event group
 * @param xEventGroup Event group handle
 * @param uxBitsToWaitFor Bits to wait for
 * @param xClearOnExit Clear bits on exit
 * @param xWaitForAllBits Wait for all bits or any bit
 * @param xTicksToWait Ticks to wait
 * @return Event bits value
 */
EventBits_t wifi_hal_event_group_wait_bits(EventGroupHandle_t xEventGroup,
                                            const EventBits_t uxBitsToWaitFor,
                                            const BaseType_t xClearOnExit,
                                            const BaseType_t xWaitForAllBits,
                                            TickType_t xTicksToWait);

/**
 * @brief Set bits in an event group
 * @param xEventGroup Event group handle
 * @param uxBitsToSet Bits to set
 * @return Event bits value
 */
EventBits_t wifi_hal_event_group_set_bits(EventGroupHandle_t xEventGroup,
                                           const EventBits_t uxBitsToSet);

/**
 * @brief Delete an event group
 * @param xEventGroup Event group handle
 */
void wifi_hal_event_group_delete(EventGroupHandle_t xEventGroup);

/* ============================================================================
 * FreeRTOS Timer HAL Functions
 * ============================================================================ */

/**
 * @brief Create a timer
 * @param pcTimerName Timer name
 * @param xTimerPeriodInTicks Timer period in ticks
 * @param uxAutoReload Auto-reload flag
 * @param pvTimerID Timer ID
 * @param pxCallbackFunction Callback function
 * @return Timer handle
 */
TimerHandle_t wifi_hal_timer_create(const char * const pcTimerName,
                                     const TickType_t xTimerPeriodInTicks,
                                     const UBaseType_t uxAutoReload,
                                     void * const pvTimerID,
                                     TimerCallbackFunction_t pxCallbackFunction);

/**
 * @brief Start a timer
 * @param xTimer Timer handle
 * @param xTicksToWait Ticks to wait
 * @return pdPASS if successful
 */
BaseType_t wifi_hal_timer_start(TimerHandle_t xTimer, TickType_t xTicksToWait);

/**
 * @brief Stop a timer
 * @param xTimer Timer handle
 * @param xTicksToWait Ticks to wait
 * @return pdPASS if successful
 */
BaseType_t wifi_hal_timer_stop(TimerHandle_t xTimer, TickType_t xTicksToWait);

/**
 * @brief Reset a timer
 * @param xTimer Timer handle
 * @param xTicksToWait Ticks to wait
 * @return pdPASS if successful
 */
BaseType_t wifi_hal_timer_reset(TimerHandle_t xTimer, TickType_t xTicksToWait);

/* ============================================================================
 * Logging HAL Functions
 * ============================================================================ */

/**
 * @brief Log at INFO level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void wifi_hal_log_info(const char* tag, const char* format, ...);

/**
 * @brief Log at ERROR level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void wifi_hal_log_error(const char* tag, const char* format, ...);

/**
 * @brief Log at DEBUG level
 * @param tag Log tag
 * @param format Format string
 * @param ... Variable arguments
 */
void wifi_hal_log_debug(const char* tag, const char* format, ...);

/**
 * @brief Check error code and abort if not ESP_OK
 * @param x Expression that returns esp_err_t
 */
#define WIFI_HAL_ERROR_CHECK(x) do { \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) { \
        wifi_hal_log_error("WIFI_HAL", "Error 0x%x at %s:%d", __err_rc, __FILE__, __LINE__); \
    } \
} while(0)

#endif // WIFI_HAL_INTERFACE_H
