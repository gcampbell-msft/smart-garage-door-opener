#include "wifi_interface.h"
#include "wifi_hal_interface.h"
#include "wifi_retry_manager.h"
#include "wifi_credentials.h"  
#include "esp_wifi.h"
#include "string.h"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define ESP_MAXIMUM_WIFI_RETRY  10
#define WIFI_RETRY_INTERVAL_MS  (30 * 60 * 1000) // 30 minutes in milliseconds

static const char* WIFI_TAG = "wifi_station";

static wifi_event_callbacks_t s_event_callbacks = {0};

static wifi_retry_state_t s_retry_state = {0};

static EventGroupHandle_t s_wifi_event_group;
static TimerHandle_t s_wifi_retry_timer_handle = NULL;
typedef esp_err_t (*wifi_func)(void);

// Forward declarations
static void start_wifi_retry_timer(void);
static void stop_wifi_retry_timer(void);

/// @brief Method that waits for WiFi connection to be established or fail.
/// @param func Function that could either be esp_wifi_start or esp_wifi_connect.
void wifi_wait_connected(wifi_func func) {
    s_wifi_event_group = wifi_hal_event_group_create();
    WIFI_HAL_ERROR_CHECK(func());

    wifi_hal_log_info(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = wifi_hal_event_group_wait_bits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        wifi_hal_log_info(WIFI_TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
        stop_wifi_retry_timer();
        if (s_event_callbacks.on_connected != NULL) {
            s_event_callbacks.on_connected();
        }
    } else if (bits & WIFI_FAIL_BIT) {
        wifi_hal_log_info(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
        start_wifi_retry_timer();
        if (s_event_callbacks.on_failed != NULL) {
            s_event_callbacks.on_failed();
        }
    } else {
        wifi_hal_log_error(WIFI_TAG, "UNEXPECTED EVENT");


        if (s_event_callbacks.on_failed != NULL) {
            s_event_callbacks.on_failed();
        }
    }

    wifi_hal_event_group_delete(s_wifi_event_group);
}

/// @brief Timer callback that attempts to reconnect to WiFi.
static void wifi_retry_timer_callback(TimerHandle_t xTimer)
{
    wifi_hal_log_info(WIFI_TAG, "WiFi retry timer triggered, attempting to reconnect...");
    
    wifi_retry_result_t result = wifi_retry_on_timer_expired(&s_retry_state);
    
    if (result.action == WIFI_RETRY_ACTION_CONNECT) {
        wifi_wait_connected(wifi_hal_wifi_connect);
    }
}

/// @brief Starts a timer that will attempt WiFi reconnection every 30 minutes.
static void start_wifi_retry_timer(void)
{
    if (s_wifi_retry_timer_handle != NULL) {
        if (wifi_hal_timer_reset(s_wifi_retry_timer_handle, 0) != pdPASS) {
            wifi_hal_log_error(WIFI_TAG, "Failed to reset WiFi retry timer");
        }
        return;
    }

    s_wifi_retry_timer_handle = wifi_hal_timer_create(
        "wifi_retry_timer",
        pdMS_TO_TICKS(s_retry_state.retry_interval_ms),
        pdTRUE,  // Auto-reload: will repeat
        (void *)0,
        wifi_retry_timer_callback
    );

    if (s_wifi_retry_timer_handle == NULL) {
        wifi_hal_log_error(WIFI_TAG, "Failed to create WiFi retry timer");
        return;
    }

    if (wifi_hal_timer_start(s_wifi_retry_timer_handle, 0) != pdPASS) {
        wifi_hal_log_error(WIFI_TAG, "Failed to start WiFi retry timer");
    } else {
        wifi_hal_log_info(WIFI_TAG, "Started WiFi retry timer (%d minute interval)", s_retry_state.retry_interval_ms / 60000);
    }
}

/// @brief Stops the WiFi retry timer if it's running.
static void stop_wifi_retry_timer(void)
{
    if (s_wifi_retry_timer_handle != NULL) {
        if (wifi_hal_timer_stop(s_wifi_retry_timer_handle, 0) == pdPASS) {
            wifi_hal_log_info(WIFI_TAG, "Stopped WiFi retry timer");
        }
    }
}

/// @brief Event handler for WiFi and IP events.
/// @param arg Unused. Only needed for event handler signature.
/// @param event_base Indicates the event base (WIFI_EVENT or IP_EVENT).
/// @param event_id ID of the event.
/// @param event_data Data associated with the event.
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_hal_wifi_connect();

        if (s_event_callbacks.on_sta_start != NULL) {
            s_event_callbacks.on_sta_start();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_hal_log_info(WIFI_TAG, "Disconnected from AP");
        
        wifi_retry_result_t result = wifi_retry_on_disconnect(&s_retry_state);
        
        if (result.action == WIFI_RETRY_ACTION_CONNECT) {
            wifi_hal_log_info(WIFI_TAG, "Retry %d: attempting to reconnect", wifi_retry_get_count(&s_retry_state));
            wifi_hal_wifi_connect();
        } else if (result.action == WIFI_RETRY_ACTION_FAIL) {
            wifi_hal_log_info(WIFI_TAG, "Max immediate retries exceeded, starting long interval timer");
            wifi_hal_event_group_set_bits(s_wifi_event_group, WIFI_FAIL_BIT);
            start_wifi_retry_timer();
        }
        
        if (result.should_callback_disconnected && s_event_callbacks.on_disconnected != NULL) {
            s_event_callbacks.on_disconnected(result.callback_retry_count);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const char* ip_str = wifi_hal_get_ip_string_from_event(event_data);
        wifi_hal_log_info(WIFI_TAG, "got ip:%s", ip_str);
        
        wifi_retry_result_t result = wifi_retry_on_connected(&s_retry_state);
        
        if (result.action == WIFI_RETRY_ACTION_STOP_TIMER) {
            stop_wifi_retry_timer();
        }
        
        wifi_hal_event_group_set_bits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (s_event_callbacks.on_got_ip != NULL) {
            s_event_callbacks.on_got_ip(ip_str);
        }
    }
}

/// @brief Initializes all the wifi components and connects to the AP.
/// @param max_retries Maximum number of retry attempts for WiFi connection
/// @param retry_interval_ms Interval between retry attempts in milliseconds
void wifi_init_sta(const int max_retries, const int retry_interval_ms)
{
    wifi_retry_init(&s_retry_state, max_retries, retry_interval_ms);

    wifi_hal_tcpip_adapter_init();

    WIFI_HAL_ERROR_CHECK(wifi_hal_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    WIFI_HAL_ERROR_CHECK(wifi_hal_wifi_init(&cfg));

    WIFI_HAL_ERROR_CHECK(wifi_hal_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    WIFI_HAL_ERROR_CHECK(wifi_hal_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        },
    };

    /* Setting a password implies station will connect to all security modes including WEP/WPA.
        * However these modes are deprecated and not advisable to be used. Incase your Access point
        * doesn't support WPA2, these mode can be enabled by commenting below line */

    if (strlen((char *)wifi_config.sta.password)) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    WIFI_HAL_ERROR_CHECK(wifi_hal_wifi_set_mode(WIFI_MODE_STA) );
    WIFI_HAL_ERROR_CHECK(wifi_hal_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

    wifi_wait_connected(wifi_hal_wifi_start);
    wifi_hal_log_info(WIFI_TAG, "wifi_init_sta finished.");
}

void wifi_register_event_callbacks(const wifi_event_callbacks_t* callbacks)
{
    if (callbacks != NULL) {
        s_event_callbacks = *callbacks;
        wifi_hal_log_info(WIFI_TAG, "WiFi event callbacks registered");
    }
}