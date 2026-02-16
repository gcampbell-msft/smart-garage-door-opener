#include "mqtt_interface.h"
#include "mqtt_hal_interface.h"
#include "mqtt_retry_manager.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>

static const char* MQTT_TAG = "mqtt_client";

static esp_mqtt_client_handle_t s_mqtt_handle = NULL;
static mqtt_config_t s_mqtt_config = {0};
static mqtt_event_callbacks_t s_mqtt_callbacks = {0};
static mqtt_retry_state_t s_retry_state = {0};

void mqtt_start(void) {
    if (s_mqtt_handle != NULL) {
        mqtt_hal_client_start(s_mqtt_handle);
        mqtt_hal_log_info(MQTT_TAG, "MQTT client started");
    } else {
        mqtt_hal_log_error(MQTT_TAG, "MQTT client not initialized");
    }
}

int mqtt_publish(const char* topic, const char* data, int qos, bool retain) {
    if (s_mqtt_handle == NULL) {
        mqtt_hal_log_error(MQTT_TAG, "MQTT client not initialized");
        return -1;
    }
    return mqtt_hal_client_publish(s_mqtt_handle, topic, data, 0, qos, retain);
}

int mqtt_subscribe(const char* topic, int qos) {
    if (s_mqtt_handle == NULL) {
        mqtt_hal_log_error(MQTT_TAG, "MQTT client not initialized");
        return -1;
    }
    return mqtt_hal_client_subscribe(s_mqtt_handle, topic, qos);
}

esp_mqtt_client_handle_t mqtt_get_handle(void) {
    return s_mqtt_handle;
}

/// @brief Callback function for MQTT events.
/// @param event The MQTT event handle. Will contain ID and data
/// @return ESP_OK on success, or an error code on failure.
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            
            mqtt_retry_result_t result_connect = mqtt_retry_on_connected(&s_retry_state);
            
            if (result_connect.should_callback_connected && s_mqtt_callbacks.on_connected != NULL) {
                s_mqtt_callbacks.on_connected();
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            
            mqtt_retry_result_t result_disconnect = mqtt_retry_on_disconnect(&s_retry_state);
            
            if (result_disconnect.should_callback_disconnected && s_mqtt_callbacks.on_disconnected != NULL) {
                s_mqtt_callbacks.on_disconnected();
            }
            
            if (result_disconnect.action == MQTT_RETRY_ACTION_RECONNECT) {
                mqtt_hal_log_info(MQTT_TAG, "Auto-reconnecting... (disconnect #%d)", 
                                  mqtt_retry_get_disconnect_count(&s_retry_state));
                mqtt_hal_client_start(s_mqtt_handle);
            }
            break;
        case MQTT_EVENT_SUBSCRIBED:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_DATA");
            if (event->topic != NULL && event->topic_len > 0) {
                mqtt_hal_log_info(MQTT_TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            }
            if (event->data != NULL && event->data_len > 0) {
                mqtt_hal_log_info(MQTT_TAG, "DATA=%.*s", event->data_len, event->data);
            }

            if (s_mqtt_callbacks.on_data != NULL) {
                s_mqtt_callbacks.on_data(event->topic, event->topic_len, event->data, event->data_len);
            }

            break;
        case MQTT_EVENT_ERROR:
            mqtt_hal_log_info(MQTT_TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            mqtt_hal_log_info(MQTT_TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

/// @brief A wrapper for the MQTT event handler to match the esp_event_handler_t signature.
/// @param handler_args Arguments passed to the handler.
/// @param base Event base.
/// @param event_id Event ID.
/// @param event_data Event data.
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_hal_log_debug(MQTT_TAG, "Event dispatched from event loop event_id=%d", event_id);
    mqtt_event_handler_cb(event_data);
}

/// @brief Initializes the MQTT client with configuration and event handlers.
/// @param config Pointer to MQTT configuration structure.
/// @param callbacks Pointer to callback structure (can be NULL).
void mqtt_init(const mqtt_config_t* config, const mqtt_event_callbacks_t* callbacks)
{
    if (config == NULL) {
        mqtt_hal_log_error(MQTT_TAG, "MQTT config is NULL");
        return;
    }

    s_mqtt_config = *config;

    if (callbacks != NULL) {
        s_mqtt_callbacks = *callbacks;
    }
    
    mqtt_retry_init(&s_retry_state, true);

    esp_mqtt_client_config_t mqtt_cfg = {
        .host = config->broker_address,
        .port = config->port,
        .username = config->username,
        .password = config->password,
        .lwt_topic = config->lwt_topic,
        .lwt_qos = 0,
        .lwt_msg = config->lwt_message != NULL ? config->lwt_message : "unavailable",
        .lwt_retain = true,
    };

    s_mqtt_handle = mqtt_hal_client_init(&mqtt_cfg);
    if (s_mqtt_handle == NULL) {
        mqtt_hal_log_error(MQTT_TAG, "Failed to initialize MQTT client");
        return;
    }

    mqtt_hal_client_register_event(s_mqtt_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, s_mqtt_handle);
}