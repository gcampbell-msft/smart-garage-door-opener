#include "wifi_interface.h"
#include "wifi_credentials.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "lwip/ip4_addr.h"
#include "string.h"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* WiFi retry configuration */
#define ESP_MAXIMUM_WIFI_RETRY  10
#define WIFI_RETRY_INTERVAL_MS  (30 * 60 * 1000) // 30 minutes in milliseconds

static const char* WIFI_TAG = "wifi_station";

// WiFi event callbacks
static wifi_event_callbacks_t s_event_callbacks = {0};
static int maximum_wifi_retries = ESP_MAXIMUM_WIFI_RETRY;
static int wifi_retry_interval = WIFI_RETRY_INTERVAL_MS;

static EventGroupHandle_t s_wifi_event_group;
static TimerHandle_t s_wifi_retry_timer_handle = NULL;
typedef esp_err_t (*wifi_func)(void);
static int s_retry_num = 0;

// Forward declarations
static void start_wifi_retry_timer(void);
static void stop_wifi_retry_timer(void);

/// @brief Method that waits for WiFi connection to be established or fail.
/// @param func Function that could either be esp_wifi_start or esp_wifi_connect.
void wifi_wait_connected(wifi_func func) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(func());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
        stop_wifi_retry_timer(); // Stop retry timer on successful connection
        // Invoke connected callback if registered
        if (s_event_callbacks.on_connected != NULL) {
            s_event_callbacks.on_connected();
        }
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
        start_wifi_retry_timer(); // Start 30-minute retry timer
        // Invoke failed callback if registered
        if (s_event_callbacks.on_failed != NULL) {
            s_event_callbacks.on_failed();
        }
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
        // Invoke failed callback if registered
        if (s_event_callbacks.on_failed != NULL) {
            s_event_callbacks.on_failed();
        }
    }

    vEventGroupDelete(s_wifi_event_group);
}

/// @brief Timer callback that attempts to reconnect to WiFi.
static void wifi_retry_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(WIFI_TAG, "WiFi retry timer triggered, attempting to reconnect...");
    s_retry_num = 0; // Reset retry counter
    wifi_wait_connected(esp_wifi_connect);
}

/// @brief Starts a timer that will attempt WiFi reconnection every 30 minutes.
static void start_wifi_retry_timer(void)
{
    if (s_wifi_retry_timer_handle != NULL) {
        // Timer already exists, just restart it
        if (xTimerReset(s_wifi_retry_timer_handle, 0) != pdPASS) {
            ESP_LOGE(WIFI_TAG, "Failed to reset WiFi retry timer");
        }
        return;
    }

    s_wifi_retry_timer_handle = xTimerCreate(
        "wifi_retry_timer",
        pdMS_TO_TICKS(wifi_retry_interval),
        pdTRUE,  // Auto-reload: will repeat every 30 minutes
        (void *)0,
        wifi_retry_timer_callback
    );

    if (s_wifi_retry_timer_handle == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to create WiFi retry timer");
        return;
    }

    if (xTimerStart(s_wifi_retry_timer_handle, 0) != pdPASS) {
        ESP_LOGE(WIFI_TAG, "Failed to start WiFi retry timer");
    } else {
        ESP_LOGI(WIFI_TAG, "Started WiFi retry timer (30 minute interval)");
    }
}

/// @brief Stops the WiFi retry timer if it's running.
static void stop_wifi_retry_timer(void)
{
    if (s_wifi_retry_timer_handle != NULL) {
        if (xTimerStop(s_wifi_retry_timer_handle, 0) == pdPASS) {
            ESP_LOGI(WIFI_TAG, "Stopped WiFi retry timer");
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
        esp_wifi_connect();
        // Invoke sta_start callback if registered
        if (s_event_callbacks.on_sta_start != NULL) {
            s_event_callbacks.on_sta_start();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < maximum_wifi_retries) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
        // Invoke disconnected callback if registered
        if (s_event_callbacks.on_disconnected != NULL) {
            s_event_callbacks.on_disconnected(s_retry_num);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        // Invoke got_ip callback if registered
        if (s_event_callbacks.on_got_ip != NULL) {
            s_event_callbacks.on_got_ip(ip4addr_ntoa(&event->ip_info.ip));
        }
    }
}

/// @brief Initializes all the wifi components and connects to the AP.
/// @param max_retries Maximum number of retry attempts for WiFi connection
/// @param retry_interval_ms Interval between retry attempts in milliseconds
void wifi_init_sta(const int max_retries, const int retry_interval_ms)
{
    maximum_wifi_retries = max_retries;
    wifi_retry_interval = retry_interval_ms;

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

    wifi_wait_connected(esp_wifi_start);
    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}

void wifi_register_event_callbacks(const wifi_event_callbacks_t* callbacks)
{
    if (callbacks != NULL) {
        s_event_callbacks = *callbacks;
        ESP_LOGI(WIFI_TAG, "WiFi event callbacks registered");
    }
}