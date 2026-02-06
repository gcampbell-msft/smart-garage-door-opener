/**
 * @file mqtt_interface.h
 * @brief MQTT interface abstraction for testability.
 * 
 * This interface allows swapping between real ESP8266 MQTT implementation
 * and mock implementations for testing.
 */

#ifndef MQTT_INTERFACE_H
#define MQTT_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT connection status
 */
typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_ERROR
} mqtt_status_t;

/**
 * @brief MQTT message structure
 */
typedef struct {
    const char* topic;
    size_t topic_len;
    const char* data;
    size_t data_len;
} mqtt_message_t;

/**
 * @brief Callback type for MQTT connection status changes
 */
typedef void (*mqtt_status_callback_t)(mqtt_status_t status);

/**
 * @brief Callback type for incoming MQTT messages
 */
typedef void (*mqtt_message_callback_t)(const mqtt_message_t* message);

/**
 * @brief MQTT configuration
 */
typedef struct {
    const char* host;
    uint16_t port;
    const char* username;
    const char* password;
    const char* lwt_topic;
    const char* lwt_message;
    bool lwt_retain;
} mqtt_config_t;

/**
 * @brief MQTT interface function pointers
 */
typedef struct {
    /**
     * @brief Initialize the MQTT client
     * @param config MQTT configuration
     * @return 0 on success, negative error code on failure
     */
    int (*init)(const mqtt_config_t* config);

    /**
     * @brief Start the MQTT client (connect to broker)
     * @return 0 on success, negative error code on failure
     */
    int (*start)(void);

    /**
     * @brief Stop the MQTT client
     * @return 0 on success, negative error code on failure
     */
    int (*stop)(void);

    /**
     * @brief Publish a message
     * @param topic Topic to publish to
     * @param data Data to publish
     * @param len Length of data
     * @param qos Quality of service
     * @param retain Retain flag
     * @return Message ID on success, negative error code on failure
     */
    int (*publish)(const char* topic, const char* data, size_t len, int qos, bool retain);

    /**
     * @brief Subscribe to a topic
     * @param topic Topic to subscribe to
     * @param qos Quality of service
     * @return Message ID on success, negative error code on failure
     */
    int (*subscribe)(const char* topic, int qos);

    /**
     * @brief Unsubscribe from a topic
     * @param topic Topic to unsubscribe from
     * @return Message ID on success, negative error code on failure
     */
    int (*unsubscribe)(const char* topic);

    /**
     * @brief Get current MQTT status
     * @return Current MQTT status
     */
    mqtt_status_t (*get_status)(void);

    /**
     * @brief Register callback for status changes
     * @param callback Function to call when status changes
     */
    void (*register_status_callback)(mqtt_status_callback_t callback);

    /**
     * @brief Register callback for incoming messages
     * @param callback Function to call when message received
     */
    void (*register_message_callback)(mqtt_message_callback_t callback);

    /**
     * @brief Deinitialize the MQTT client
     */
    void (*deinit)(void);
} mqtt_interface_t;

/**
 * @brief Get the MQTT interface implementation
 * 
 * Returns either the real ESP8266 implementation or a mock
 * based on compile-time configuration.
 * 
 * @return Pointer to the MQTT interface
 */
const mqtt_interface_t* mqtt_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_INTERFACE_H */
