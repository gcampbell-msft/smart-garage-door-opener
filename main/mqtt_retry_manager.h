/**
 * @file mqtt_retry_manager.h
 * @brief Pure C MQTT retry state machine - no ESP dependencies
 * 
 * This module manages MQTT reconnection logic and can be tested
 * independently without any hardware or ESP SDK dependencies.
 */

#ifndef MQTT_RETRY_MANAGER_H
#define MQTT_RETRY_MANAGER_H

#include <stdbool.h>

/**
 * @brief MQTT retry state
 */
typedef struct {
    bool is_connected;           /**< Current connection state */
    int disconnect_count;        /**< Number of disconnections */
    bool should_reconnect;       /**< Whether auto-reconnect is enabled */
} mqtt_retry_state_t;

/**
 * @brief Actions that should be taken based on MQTT retry logic
 */
typedef enum {
    MQTT_RETRY_ACTION_NONE,         /**< No action needed */
    MQTT_RETRY_ACTION_RECONNECT,    /**< Attempt to reconnect */
} mqtt_retry_action_t;

/**
 * @brief Result of processing an event
 */
typedef struct {
    mqtt_retry_action_t action;         /**< Action to take */
    bool should_callback_connected;     /**< Trigger connected callback */
    bool should_callback_disconnected;  /**< Trigger disconnected callback */
} mqtt_retry_result_t;

/**
 * @brief Initialize MQTT retry state
 * @param state Pointer to retry state structure
 * @param auto_reconnect Enable automatic reconnection on disconnect
 */
void mqtt_retry_init(mqtt_retry_state_t* state, bool auto_reconnect);

/**
 * @brief Process MQTT disconnection event
 * @param state Pointer to retry state structure
 * @return Result indicating what action to take
 */
mqtt_retry_result_t mqtt_retry_on_disconnect(mqtt_retry_state_t* state);

/**
 * @brief Process MQTT connected event
 * @param state Pointer to retry state structure
 * @return Result indicating what action to take
 */
mqtt_retry_result_t mqtt_retry_on_connected(mqtt_retry_state_t* state);

/**
 * @brief Get disconnect count
 * @param state Pointer to retry state structure
 * @return Number of disconnections
 */
int mqtt_retry_get_disconnect_count(const mqtt_retry_state_t* state);

/**
 * @brief Check if currently connected
 * @param state Pointer to retry state structure
 * @return true if connected, false otherwise
 */
bool mqtt_retry_is_connected(const mqtt_retry_state_t* state);

#endif // MQTT_RETRY_MANAGER_H
