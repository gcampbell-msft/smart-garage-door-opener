#ifndef MQTT_IMPL_H
#define MQTT_IMPL_H

#include "esp_err.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief MQTT configuration structure
 */
typedef struct {
    const char* broker_address;       /**< MQTT broker hostname or IP */
    int port;                         /**< MQTT broker port (default: 1883) */
    const char* username;             /**< MQTT username (NULL if not required) */
    const char* password;             /**< MQTT password (NULL if not required) */
    const char* lwt_topic;            /**< Last Will and Testament topic */
    const char* lwt_message;          /**< Last Will and Testament message */
} mqtt_config_t;

/**
 * @brief Callback function type for MQTT command received
 * Called when a command is received on the command topic
 * @param command Command string received (e.g., "OPEN", "CLOSE")
 */
typedef void (*mqtt_command_cb_t)(const char* topic, int topic_len, const char* command, int command_len);

/**
 * @brief Callback function type for MQTT connected event
 * Called when MQTT client successfully connects to broker
 */
typedef void (*mqtt_connected_cb_t)(void);

/**
 * @brief Callback function type for MQTT disconnected event
 * Called when MQTT client disconnects from broker
 */
typedef void (*mqtt_disconnected_cb_t)(void);

/**
 * @brief Structure containing MQTT event callbacks
 */
typedef struct {
    mqtt_connected_cb_t on_connected;       /**< Called when connected to broker */
    mqtt_disconnected_cb_t on_disconnected; /**< Called when disconnected from broker */
    mqtt_command_cb_t on_data;           /**< Called when command is received */
} mqtt_event_callbacks_t;

/**
 * @brief Initialize MQTT client with configuration
 * @param config Pointer to MQTT configuration structure
 * @param callbacks Pointer to callback structure (can be NULL)
 */
void mqtt_init(const mqtt_config_t* config, const mqtt_event_callbacks_t* callbacks);

/**
 * @brief Start the MQTT client
 * Should be called after mqtt_init() when ready to connect
 */
void mqtt_start(void);

/**
 * @brief Publish a message to an MQTT topic
 * @param topic Topic to publish to
 * @param data Data to publish
 * @param qos Quality of Service (0, 1, or 2)
 * @param retain Whether to retain the message
 * @return Message ID, or -1 on error
 */
int mqtt_publish(const char* topic, const char* data, int qos, bool retain);

/**
 * @brief Subscribe to an MQTT topic
 * @param topic Topic to subscribe to
 * @param qos Quality of Service (0, 1, or 2)
 * @return Message ID, or -1 on error
 */
int mqtt_subscribe(const char* topic, int qos);

/**
 * @brief Get the MQTT client handle
 * @return MQTT client handle, or NULL if not initialized
 */
esp_mqtt_client_handle_t mqtt_get_handle(void);

#endif // MQTT_IMPL_H
