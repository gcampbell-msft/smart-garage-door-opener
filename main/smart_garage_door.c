#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "wifi_credentials.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_credentials.h"
#include "mqtt_client.h"
#include "garage_state_machine.h"

#define ESP_MAXIMUM_WIFI_RETRY  10
#define ON_BOARD_LED_PIN GPIO_Pin_2 // D4 pin
#define ON_BOARD_LED GPIO_NUM_2 // D4
#define REED_SWITCH_INPUT_PIN GPIO_Pin_4 // D2
#define REED_SWITCH_INPUT_GPIO GPIO_NUM_4 // D2
#define RELAY_CONTROL_OUTPUT_PIN GPIO_Pin_5 // D1
#define RELAY_CONTROL_OUTPUT_GPIO GPIO_NUM_5 // D1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static esp_mqtt_client_handle_t mqtt_handle;

/* Timer handle */
TimerHandle_t wifi_retry_timer_handle;
TimerHandle_t state_machine_timer_handle;

#define WIFI_RETRY_INTERVAL_MS (30 * 60 * 1000) // 30 minutes in milliseconds


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char* REED_SWITCH_TAG = "reed_switch";
static const char* WIFI_TAG = "wifi_station";
static const char* STATE_MACHINE_TAG = "state_machine";
static const char* MQTT_TAG = "mqtt_client";
static const char* TIMER_TAG = "timer";
static const char* COMMAND_OPEN = "OPEN";
static const char* COMMAND_CLOSE = "CLOSE";

//#define TESTING
#ifdef TESTING
static const char* STATUS_TOPIC = "garage_door/status_TEST";
static const char* AVAILABILITY_TOPIC = "garage_door/availability_TEST";
static const char* COMMAND_TOPIC = "garage_door/buttonpress_TEST";
#else
static const char* STATUS_TOPIC = "garage_door/status";
static const char* AVAILABILITY_TOPIC = "garage_door/availability";
static const char* COMMAND_TOPIC = "garage_door/buttonpress";
#endif

// State machine instance
static garage_state_machine_t state_machine;

static int s_retry_num = 0;

// state machine event queue handle
static xQueueHandle state_machine_queue = NULL;

// Forward declarations for WiFi retry timer functions
static void start_wifi_retry_timer(void);
static void stop_wifi_retry_timer(void);

/// @brief GPIO interrupt handler for the reed switch input pin.
/// @param arg Will only be REED_SWITCH_TAG to indicate the source of the interrupt. 
static void gpio_isr_handler(void *arg)
{
    char* input = (char*) arg;
    xQueueSendFromISR(state_machine_queue, &input, NULL);
}

/// @brief Simulates a garage door button press by toggling the relay control pin.
/// @param arg Unused, only required for task signature.
static void button_press_task(void *arg)
{
    // Simulate button press by setting relay control pin low for 500ms
    gpio_set_level(RELAY_CONTROL_OUTPUT_GPIO, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(RELAY_CONTROL_OUTPUT_GPIO, 0);
    vTaskDelete(NULL);
}

/// @brief Starts the button press task to simulate a garage door button press.
static void start_button_press_task()
{
    xTaskCreate(button_press_task, "button_press_task", 2048, NULL, 15, NULL);
}

/// @brief Timer callback that updates the state machine timer periodically.
static void state_machine_timer_callback(TimerHandle_t xTimer)
{
    // Update timer by 100ms (this timer fires every 100ms)
    garage_transition_result_t result = garage_sm_update_timer(&state_machine, 100);
    
    // If timer caused a state transition, execute the actions
    if (result.state_changed) {
        ESP_LOGI(TIMER_TAG, "Timer expired, transitioning to %s", garage_state_to_string(result.new_state));
        
        // Publish new state to MQTT
        if (result.actions.publish_state) {
            esp_mqtt_client_publish(mqtt_handle, STATUS_TOPIC, 
                                   garage_state_to_string(result.new_state), 0, 0, 1);
            ESP_LOGI(WIFI_TAG, "Published state due to timer: %s", garage_state_to_string(result.new_state));
        }
    }
}

/// @brief Converts input string to garage event type
/// @param input The input string from the queue
/// @param sensor_level The GPIO level if input is REED_SWITCH_TAG, otherwise ignored
/// @return The corresponding garage event
static garage_event_t input_to_event(const char* input, int sensor_level)
{
    if (input == REED_SWITCH_TAG) {
        return sensor_level == 0 ? GARAGE_EVENT_SENSOR_CLOSED : GARAGE_EVENT_SENSOR_OPEN;
    } else if (input == COMMAND_OPEN) {
        return GARAGE_EVENT_COMMAND_OPEN;
    } else if (input == COMMAND_CLOSE) {
        return GARAGE_EVENT_COMMAND_CLOSE;
    }
    return GARAGE_EVENT_NONE;
}

/// @brief Executes actions returned by state machine
/// @param actions The actions to execute
/// @param new_state The new state after transition
static void execute_state_actions(const garage_actions_t* actions, garage_state_t new_state)
{
    if (actions->trigger_button_press) {
        ESP_LOGI(STATE_MACHINE_TAG, "Triggering button press");
        start_button_press_task();
    }
    
    if (actions->publish_state) {
        const char* state_str = garage_state_to_string(new_state);
        ESP_LOGI(STATE_MACHINE_TAG, "Publishing state: %s", state_str);
        esp_mqtt_client_publish(mqtt_handle, STATUS_TOPIC, state_str, 0, 0, 1);
    }
}

/// @brief State machine handler task that processes events from the state machine queue.
/// This will read from the state_machine_queue and update the garage door state accordingly.
/// @param arg Unused
static void state_machine_handler(void *arg)
{
    char* input;

    for (;;) {
        // Wait for events from the queue
        if (xQueueReceive(state_machine_queue, &input, portMAX_DELAY)) {
            ESP_LOGI(STATE_MACHINE_TAG, "State machine received input: %s", input);

            // Get sensor level if needed
            int sensor_level = 0;
            if (input == REED_SWITCH_TAG) {
                sensor_level = gpio_get_level(REED_SWITCH_INPUT_GPIO);
            }

            // Convert input to event and process it
            garage_event_t event = input_to_event(input, sensor_level);
            if (event != GARAGE_EVENT_NONE) {
                garage_transition_result_t result = garage_sm_process_event(&state_machine, event);
                
                if (result.state_changed) {
                    ESP_LOGI(STATE_MACHINE_TAG, "State changed to: %s", 
                            garage_state_to_display_string(result.new_state));
                }
                
                // Execute the actions
                execute_state_actions(&result.actions, result.new_state);
            }
        }
    }
}

typedef esp_err_t (*wifi_func)(void);

static void mqtt_start(void) {
    esp_mqtt_client_start(mqtt_handle);
}

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
        gpio_set_level(ON_BOARD_LED, 1); // Turn off LED to indicate successful connection
        stop_wifi_retry_timer(); // Stop retry timer on successful connection
        mqtt_start(); // Start MQTT client
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
        gpio_set_level(ON_BOARD_LED, 0); // Turn on LED to indicate failure to connect
        start_wifi_retry_timer(); // Start 30-minute retry timer
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
        gpio_set_level(ON_BOARD_LED, 0); // Turn on LED to indicate failure to connect
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
    if (wifi_retry_timer_handle != NULL) {
        // Timer already exists, just restart it
        if (xTimerReset(wifi_retry_timer_handle, 0) != pdPASS) {
            ESP_LOGE(WIFI_TAG, "Failed to reset WiFi retry timer");
        }
        return;
    }

    wifi_retry_timer_handle = xTimerCreate(
        "wifi_retry_timer",
        pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS),
        pdTRUE,  // Auto-reload: will repeat every 30 minutes
        (void *)0,
        wifi_retry_timer_callback
    );

    if (wifi_retry_timer_handle == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to create WiFi retry timer");
        return;
    }

    if (xTimerStart(wifi_retry_timer_handle, 0) != pdPASS) {
        ESP_LOGE(WIFI_TAG, "Failed to start WiFi retry timer");
    } else {
        ESP_LOGI(WIFI_TAG, "Started WiFi retry timer (30 minute interval)");
    }
}

/// @brief Stops the WiFi retry timer if it's running.
static void stop_wifi_retry_timer(void)
{
    if (wifi_retry_timer_handle != NULL) {
        if (xTimerStop(wifi_retry_timer_handle, 0) == pdPASS) {
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
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_WIFI_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
        gpio_set_level(ON_BOARD_LED, 0); // Turn on LED to indicate failure to connect
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        gpio_set_level(ON_BOARD_LED, 1); // Turn off LED to indicate successful connection
    }
}

/// @brief Initializes all the wifi components and connects to the AP.
/// @param void.
void wifi_init_sta(void)
{
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

/// @brief Sets up GPIOs for on-board LED, reed switch input (with gpio ISR handler), and relay control output.
/// @param void.
void gpio_init(void)
{
    // Setup on-board LED
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    // This enables both the on-board LED and the relay control output pin.
    io_conf.pin_bit_mask = ON_BOARD_LED_PIN | RELAY_CONTROL_OUTPUT_PIN;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_MODE_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_MODE_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    // Setup reed switch input pin
    // interrupt on rising and falling edge.
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = REED_SWITCH_INPUT_PIN;
    // set pull up
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    // disable pull down
    io_conf.pull_down_en = GPIO_MODE_DISABLE;
    gpio_config(&io_conf);

    // Setup relay control output pin

    // install gpio isr service
    gpio_install_isr_service(0);
    // hook isr handler for relay control pin. This handles reacting to the garage door state.
    gpio_isr_handler_add(REED_SWITCH_INPUT_GPIO, gpio_isr_handler, (void *) REED_SWITCH_TAG);
}

/// @brief Callback function for MQTT events.
/// @param event The MQTT event handle. Will contain ID and data
/// @return ESP_OK on success, or an error code on failure.
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_publish(mqtt_handle, AVAILABILITY_TOPIC, "available", 0, 0, 1);
            // We only need to subscribe to the command topic.
            esp_mqtt_client_subscribe(mqtt_handle, COMMAND_TOPIC, 0);
            esp_mqtt_client_subscribe(mqtt_handle, STATUS_TOPIC, 0);
            // Initialize initial state by sending message that reed switch state changed.
            xQueueSend(state_machine_queue, &REED_SWITCH_TAG, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            esp_mqtt_client_start(mqtt_handle);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            if (event->topic_len == strlen(COMMAND_TOPIC) && strncmp(event->topic, COMMAND_TOPIC, event->topic_len) == 0) {
                if (event->data_len == strlen(COMMAND_OPEN) && strncmp(event->data, COMMAND_OPEN, event->data_len) == 0) {
                    ESP_LOGI(WIFI_TAG, "Received OPEN command");
                    xQueueSend(state_machine_queue, &COMMAND_OPEN, 0);
                } else if (event->data_len == strlen(COMMAND_CLOSE) && strncmp(event->data, COMMAND_CLOSE, event->data_len) == 0) {
                    ESP_LOGI(WIFI_TAG, "Received CLOSE command");
                    xQueueSend(state_machine_queue, &COMMAND_CLOSE, 0);
                }
            } else if (event->topic_len == strlen(STATUS_TOPIC) && strncmp(event->topic, STATUS_TOPIC, event->topic_len) == 0) {
                ESP_LOGI(MQTT_TAG, "Received status update");
                ESP_LOGI(MQTT_TAG, "Status: %.*s\r\n", event->data_len, event->data);
            } else {
                ESP_LOGI(MQTT_TAG, "Received message on unknown topic");
            }

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

/// @brief A wrapper for the MQTT event handler to match the esp_event_handler_t signature. Primarily used for a generic logger.
/// @param handler_args Arguments passed to the handler.
/// @param base Event base.
/// @param event_id Event ID.
/// @param event_data Event data.
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(WIFI_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

/// @brief Initializes and starts the MQTT client, sets up event handlers, and subscribes to necessary topics.
/// @param void.
static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = MQTT_BROKER_ADDRESS,
        .port = 1883,
        .username = MQTT_USER_NAME,
        .password = MQTT_USER_PASSWORD,
        .lwt_topic = AVAILABILITY_TOPIC,
        .lwt_qos = 0,
        .lwt_msg = "unavailable",
        .lwt_retain = true,
    };

    mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_handle);
}

void app_main()
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP8266 chip with %d CPU cores, WiFi, ",
            chip_info.cores);

    printf("silicon revision %d, ", chip_info.revision);

    // This is what necessitates the spi include.
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
            
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_LOGI(MQTT_TAG, "[APP] Startup..");
    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(MQTT_TAG, "[APP] IDF version: %s", esp_get_idf_version());
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize state machine
    garage_sm_init(&state_machine, GARAGE_STATE_UNKNOWN);

    // Setup interrupt handlers and event queue
    state_machine_queue = xQueueCreate(5, sizeof(uint32_t));
    xTaskCreate(state_machine_handler, "state_machine_handler", 2048, NULL, 10, NULL);
    
    // Create periodic timer for state machine updates (100ms interval)
    state_machine_timer_handle = xTimerCreate(
        "sm_timer",
        pdMS_TO_TICKS(100),  // 100ms period
        pdTRUE,              // Auto-reload
        (void *)0,
        state_machine_timer_callback
    );
    if (state_machine_timer_handle != NULL) {
        xTimerStart(state_machine_timer_handle, 0);
    }

    // Sets up error indicator LED, GPIOs for reed switch and relay control.
    gpio_init();

    // Sets up MQTT handles.
    mqtt_init();

    // Sets up the wifi
    wifi_init_sta();
}
