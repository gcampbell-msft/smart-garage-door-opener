#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

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

#include "wifi_credentials.h"
#include "mqtt_credentials.h"
#include "mqtt_client.h"
#include "mqtt_interface.h"
#include "wifi_interface.h"
#include "garage_state_machine.h"

#define ON_BOARD_LED_PIN GPIO_Pin_2 // D4 pin
#define ON_BOARD_LED GPIO_NUM_2 // D4
#define REED_SWITCH_INPUT_PIN GPIO_Pin_4 // D2
#define REED_SWITCH_INPUT_GPIO GPIO_NUM_4 // D2
#define RELAY_CONTROL_OUTPUT_PIN GPIO_Pin_5 // D1
#define RELAY_CONTROL_OUTPUT_GPIO GPIO_NUM_5 // D1

#define ESP_MAXIMUM_WIFI_RETRY  10
#define WIFI_RETRY_INTERVAL_MS  (30 * 60 * 1000) // 30 minutes in milliseconds

/* Timer handle */
TimerHandle_t wifi_retry_timer_handle;
TimerHandle_t state_machine_timer_handle;

static const char* APP_TAG = "app";
static const char* REED_SWITCH_TAG = "reed_switch";
static const char* STATE_MACHINE_TAG = "state_machine";
static const char* TIMER_TAG = "timer";
static const char* COMMAND_OPEN = "OPEN";
static const char* COMMAND_CLOSE = "CLOSE";

#ifdef TEST_MODE
#define STATUS_TOPIC "garage_door/status_TEST"
#define AVAILABILITY_TOPIC "garage_door/availability_TEST"
#define COMMAND_TOPIC "garage_door/buttonpress_TEST"

static bool test_mode_wifi_ready = false;
static bool test_mode_mqtt_ready = false;
static const char* REED_SWITCH_OPEN_TAG = "OPEN";
static const char* REED_SWITCH_CLOSE_TAG = "CLOSED";
#else
#define STATUS_TOPIC "garage_door/status"
#define AVAILABILITY_TOPIC "garage_door/availability"
#define COMMAND_TOPIC "garage_door/buttonpress"
#endif

// State machine instance
static garage_state_machine_t state_machine;

// state machine event queue handle
static xQueueHandle state_machine_queue = NULL;

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
    garage_transition_result_t result = garage_sm_update_timer(&state_machine, 100);
    
    if (result.state_changed) {
        ESP_LOGI(TIMER_TAG, "Timer expired, transitioning to %s", garage_state_to_string(result.new_state));
        
        if (result.actions.publish_state) {
            mqtt_publish(STATUS_TOPIC, garage_state_to_string(result.new_state), 0, 1);
            ESP_LOGI(STATE_MACHINE_TAG, "Published state due to timer: %s", garage_state_to_string(result.new_state));
        }
    }
}

/// @brief Converts input string to garage event type
/// @param input The input string from the queue
/// @param sensor_level The GPIO level if input is REED_SWITCH_TAG, otherwise ignored
/// @return The corresponding garage event
static garage_event_t input_to_event(const char* input, int sensor_level)
{
    #ifdef TEST_MODE
    if (input == REED_SWITCH_OPEN_TAG || input == REED_SWITCH_CLOSE_TAG) {
    #else
    if (input == REED_SWITCH_TAG) {
    #endif
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
        mqtt_publish(STATUS_TOPIC, state_str, 0, 1);
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

            int sensor_level = 0;
            #ifdef TEST_MODE
            if (input == REED_SWITCH_OPEN_TAG) {
                sensor_level = 1;
            } else if (input == REED_SWITCH_CLOSE_TAG) {
                sensor_level = 0;
            }
            #else
            if (input == REED_SWITCH_TAG) {
                sensor_level = gpio_get_level(REED_SWITCH_INPUT_GPIO);
            }
            #endif

            // Convert input to event and process it
            garage_event_t event = input_to_event(input, sensor_level);
            if (event != GARAGE_EVENT_NONE) {
                garage_transition_result_t result = garage_sm_process_event(&state_machine, event);
                
                if (result.state_changed) {
                    ESP_LOGI(STATE_MACHINE_TAG, "State changed to: %s", 
                            garage_state_to_display_string(result.new_state));
                }
                
                execute_state_actions(&result.actions, result.new_state);
            }
        }
    }
}

/// @brief Sets up GPIOs for on-board LED, reed switch input (with gpio ISR handler), and relay control output.
/// @param void.
void gpio_init(void)
{
    // Setup on-board LED
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    // This enables both the on-board LED and the relay control output pin.
    io_conf.pin_bit_mask = ON_BOARD_LED_PIN | RELAY_CONTROL_OUTPUT_PIN;
    io_conf.pull_down_en = GPIO_MODE_DISABLE;
    io_conf.pull_up_en = GPIO_MODE_DISABLE;
    gpio_config(&io_conf);

    // Setup reed switch input pin
    // interrupt on rising and falling edge.
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    // bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = REED_SWITCH_INPUT_PIN;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_MODE_DISABLE;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(0);
    // hook isr handler for relay control pin. This handles reacting to the garage door state.
    gpio_isr_handler_add(REED_SWITCH_INPUT_GPIO, gpio_isr_handler, (void *) REED_SWITCH_TAG);
}

void on_wifi_connected_callback(void) {
    gpio_set_level(ON_BOARD_LED, 1); // Turn off LED to indicate successful connection
    mqtt_start();
#ifdef TEST_MODE
    test_mode_wifi_ready = true;
    ESP_LOGI(APP_TAG, "[TEST MODE] WiFi connected");
#endif
}

void on_wifi_disconnected_callback(const int retry_count) {
    gpio_set_level(ON_BOARD_LED, 0); // Turn on LED to indicate failure to connect
}

void on_wifi_got_ip_callback(const char* ip_addr) {
    gpio_set_level(ON_BOARD_LED, 1); // Turn off LED to indicate successful connection
    ESP_LOGI(APP_TAG, "Got IP: %s", ip_addr);
}

static const wifi_event_callbacks_t wifi_callbacks = {
    .on_sta_start = NULL,
    .on_connected = on_wifi_connected_callback,
    .on_disconnected = on_wifi_disconnected_callback,
    .on_got_ip = on_wifi_got_ip_callback,
    .on_failed = NULL
};

void mqtt_data_callback(const char* topic, int topic_len, const char* command, int command_len) {
    if (topic_len == strlen(COMMAND_TOPIC) && strncmp(topic, COMMAND_TOPIC, topic_len) == 0) {
        if (command_len == strlen(COMMAND_OPEN) && strncmp(command, COMMAND_OPEN, command_len) == 0) {
            ESP_LOGI(APP_TAG, "Received OPEN command");
            xQueueSend(state_machine_queue, &COMMAND_OPEN, 0);
        } else if (command_len == strlen(COMMAND_CLOSE) && strncmp(command, COMMAND_CLOSE, command_len) == 0) {
            ESP_LOGI(APP_TAG, "Received CLOSE command");
            xQueueSend(state_machine_queue, &COMMAND_CLOSE, 0);
        }
    } else if (topic_len == strlen(STATUS_TOPIC) && strncmp(topic, STATUS_TOPIC, topic_len) == 0) {
        ESP_LOGI(APP_TAG, "Received status update");
        ESP_LOGI(APP_TAG, "Status: %.*s\r\n", command_len, command);
    } else {
        ESP_LOGI(APP_TAG, "Received message on unknown topic");
    }
}

#ifdef TEST_MODE
static int ASSERT(bool condition, const char* testInfo, const char* errorMessage) {
    if (condition) {
        ESP_LOGI(APP_TAG, "[PASSED] %s", testInfo);
        return 0;
    } else {
        ESP_LOGE(APP_TAG, "[FAILED] %s - %s", testInfo, errorMessage);
        return 1;
    }
}

/// @brief Test mode task that simulates various garage door events and commands
/// @param arg Unused
static void test_simulation_task(void *arg)
{
    ESP_LOGI(APP_TAG, "*** TEST MODE ACTIVE - Starting simulation ***");
    int countFailed = 0;

    // Wait 2 seconds for system to stabilize
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Simulate door closed initially
    ESP_LOGI(APP_TAG, "[TEST] Simulating reed switch: DOOR CLOSED");
    xQueueSend(state_machine_queue, &REED_SWITCH_CLOSE_TAG, 0);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_CLOSED, "Initial door state", "Door state mismatch - expected CLOSED");
    
    // Simulate OPEN command
    ESP_LOGI(APP_TAG, "[TEST] Simulating OPEN command");
    mqtt_publish(COMMAND_TOPIC, COMMAND_OPEN, 0, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_OPENING, "State after OPEN command", "Door state mismatch - expected OPENING");

    vTaskDelay(16000 / portTICK_PERIOD_MS);
    ESP_LOGI(APP_TAG, "[TEST] After timeout, door should be OPEN");
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_OPEN, "State after opening timeout", "Door state mismatch - expected OPEN");

    ESP_LOGI(APP_TAG, "[TEST] Simulating CLOSE command");
    mqtt_publish(COMMAND_TOPIC, COMMAND_CLOSE, 0, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_CLOSING, "State after CLOSE command", "Door state mismatch - expected CLOSING");

    vTaskDelay(8000 / portTICK_PERIOD_MS);
    xQueueSend(state_machine_queue, &REED_SWITCH_CLOSE_TAG, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_CLOSED, "State after door closed", "Door state mismatch - expected CLOSED");

    xQueueSend(state_machine_queue, &REED_SWITCH_OPEN_TAG, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_OPENING, "State after door opened from sensor", "Door state mismatch - expected OPENING");

    vTaskDelay(16000 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_OPEN, "State after opening timeout from sensor", "Door state mismatch - expected OPEN");

    mqtt_publish(COMMAND_TOPIC, COMMAND_CLOSE, 0, 1);
    vTaskDelay(16000 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_UNKNOWN, "State after CLOSE command + 15 s", "Door state mismatch - expected UNKNOWN due to timeout");
    
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    xQueueSend(state_machine_queue, &REED_SWITCH_CLOSE_TAG, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    countFailed += ASSERT(state_machine.current_state == GARAGE_STATE_CLOSED, "State after door closed from UNKNOWN", "Door state mismatch - expected CLOSED");

    ESP_LOGI(APP_TAG, "*** TEST MODE - Simulation complete ***");
    if (countFailed == 0) {
        ESP_LOGI(APP_TAG, "ALL TESTS PASSED");
    } else {
        ESP_LOGE(APP_TAG, "%d TESTS FAILED", countFailed);
    }
    vTaskDelete(NULL);
}

/// @brief Starts test simulation task when both WiFi and MQTT are ready
static void check_and_start_test_mode()
{
    if (test_mode_wifi_ready && test_mode_mqtt_ready) {
        ESP_LOGI(APP_TAG, "Both WiFi and MQTT ready - starting test simulation");
        xTaskCreate(test_simulation_task, "test_simulation", 4096, NULL, 5, NULL);
    }
}
#endif

void mqtt_connected_callback(void) {
    mqtt_publish(AVAILABILITY_TOPIC, "available", 0, 1);
    mqtt_subscribe(COMMAND_TOPIC, 0);
    mqtt_subscribe(STATUS_TOPIC, 0);
    
#ifdef TEST_MODE
    test_mode_mqtt_ready = true;
    ESP_LOGI(APP_TAG, "[TEST MODE] MQTT connected");
    check_and_start_test_mode();
#else
    xQueueSend(state_machine_queue, &REED_SWITCH_TAG, 0);
#endif
}

const mqtt_config_t mqtt_cfg = {
    .broker_address = MQTT_BROKER_ADDRESS,
    .port = 1883,
    .username = MQTT_USER_NAME,
    .password = MQTT_USER_PASSWORD,
    .lwt_topic = AVAILABILITY_TOPIC,
    .lwt_message = "unavailable",
};
const mqtt_event_callbacks_t mqtt_callbacks = {
    .on_data = mqtt_data_callback,
    .on_connected = mqtt_connected_callback,
    .on_disconnected = NULL
};

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

    ESP_LOGI(APP_TAG, "[APP] Startup..");
    ESP_LOGI(APP_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(APP_TAG, "[APP] IDF version: %s", esp_get_idf_version());
    
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
    
    mqtt_init(&mqtt_cfg, &mqtt_callbacks);

    // Sets up the wifi
    wifi_register_event_callbacks(&wifi_callbacks);
    wifi_init_sta(ESP_MAXIMUM_WIFI_RETRY, WIFI_RETRY_INTERVAL_MS);
}
