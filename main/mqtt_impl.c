#include "mqtt_interface.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>

static const char* MQTT_TAG = "mqtt_client";

static esp_mqtt_client_handle_t s_mqtt_handle = NULL;
static mqtt_config_t s_mqtt_config = {0};
static mqtt_event_callbacks_t s_mqtt_callbacks = {0};

void mqtt_start(void) {
    if (s_mqtt_handle != NULL) {
        esp_mqtt_client_start(s_mqtt_handle);
        ESP_LOGI(MQTT_TAG, "MQTT client started");
    } else {
        ESP_LOGE(MQTT_TAG, "MQTT client not initialized");
    }
}

int mqtt_publish(const char* topic, const char* data, int qos, bool retain) {
    if (s_mqtt_handle == NULL) {
        ESP_LOGE(MQTT_TAG, "MQTT client not initialized");
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_handle, topic, data, 0, qos, retain);
}

int mqtt_subscribe(const char* topic, int qos) {
    if (s_mqtt_handle == NULL) {
        ESP_LOGE(MQTT_TAG, "MQTT client not initialized");
        return -1;
    }
    return esp_mqtt_client_subscribe(s_mqtt_handle, topic, qos);
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
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            // Invoke connected callback if registered
            if (s_mqtt_callbacks.on_connected != NULL) {
                s_mqtt_callbacks.on_connected();
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            // Invoke disconnected callback if registered
            if (s_mqtt_callbacks.on_disconnected != NULL) {
                s_mqtt_callbacks.on_disconnected();
            }
            esp_mqtt_client_start(s_mqtt_handle);
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
            ESP_LOGI(MQTT_TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(MQTT_TAG, "DATA=%.*s", event->data_len, event->data);

            if (s_mqtt_callbacks.on_data != NULL) {
                s_mqtt_callbacks.on_data(event->topic, event->topic_len, event->data, event->data_len);
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

/// @brief A wrapper for the MQTT event handler to match the esp_event_handler_t signature.
/// @param handler_args Arguments passed to the handler.
/// @param base Event base.
/// @param event_id Event ID.
/// @param event_data Event data.
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

/// @brief Initializes the MQTT client with configuration and event handlers.
/// @param config Pointer to MQTT configuration structure.
/// @param callbacks Pointer to callback structure (can be NULL).
void mqtt_init(const mqtt_config_t* config, const mqtt_event_callbacks_t* callbacks)
{
    if (config == NULL) {
        ESP_LOGE(MQTT_TAG, "MQTT config is NULL");
        return;
    }

    // Store configuration
    s_mqtt_config = *config;

    // Store callbacks if provided
    if (callbacks != NULL) {
        s_mqtt_callbacks = *callbacks;
    }

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

    s_mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_handle == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, s_mqtt_handle);
    ESP_LOGI(MQTT_TAG, "MQTT client initialized");
}