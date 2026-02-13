/**
 * @file wifi_retry_manager.h
 * @brief Pure C WiFi retry state machine - no ESP dependencies
 * 
 * This module manages WiFi connection retry logic and can be tested
 * independently without any hardware or ESP SDK dependencies.
 */

#ifndef WIFI_RETRY_MANAGER_H
#define WIFI_RETRY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief WiFi retry state
 */
typedef struct {
    int retry_count;              /**< Current retry attempt count */
    int max_retries;              /**< Maximum immediate retries before long interval */
    int retry_interval_ms;        /**< Long retry interval in milliseconds */
    bool is_connected;            /**< Current connection state */
    bool timer_should_be_running; /**< Whether retry timer should be active */
} wifi_retry_state_t;

/**
 * @brief Actions that should be taken based on retry logic
 */
typedef enum {
    WIFI_RETRY_ACTION_NONE,           /**< No action needed */
    WIFI_RETRY_ACTION_CONNECT,        /**< Attempt to connect */
    WIFI_RETRY_ACTION_STOP_TIMER,     /**< Stop retry timer */
    WIFI_RETRY_ACTION_FAIL,           /**< Max retries exceeded, mark fail and start timer */
} wifi_retry_action_t;

/**
 * @brief Result of processing an event
 */
typedef struct {
    wifi_retry_action_t action;       /**< Primary action to take */
    bool should_callback_connected;   /**< Trigger connected callback */
    bool should_callback_disconnected;/**< Trigger disconnected callback */
    bool should_callback_failed;      /**< Trigger failed callback */
    int callback_retry_count;         /**< Retry count to pass to callback */
} wifi_retry_result_t;

/**
 * @brief Initialize retry state
 * @param state Pointer to retry state structure
 * @param max_retries Maximum number of immediate retry attempts
 * @param retry_interval_ms Interval for long-term retries in milliseconds
 */
void wifi_retry_init(wifi_retry_state_t* state, int max_retries, int retry_interval_ms);

/**
 * @brief Process WiFi disconnection event
 * @param state Pointer to retry state structure
 * @return Result indicating what action to take
 */
wifi_retry_result_t wifi_retry_on_disconnect(wifi_retry_state_t* state);

/**
 * @brief Process WiFi connected event (got IP)
 * @param state Pointer to retry state structure
 * @return Result indicating what action to take
 */
wifi_retry_result_t wifi_retry_on_connected(wifi_retry_state_t* state);

/**
 * @brief Process retry timer expiration
 * @param state Pointer to retry state structure
 * @return Result indicating what action to take
 */
wifi_retry_result_t wifi_retry_on_timer_expired(wifi_retry_state_t* state);

/**
 * @brief Get current retry count
 * @param state Pointer to retry state structure
 * @return Current retry count
 */
int wifi_retry_get_count(const wifi_retry_state_t* state);

/**
 * @brief Check if currently connected
 * @param state Pointer to retry state structure
 * @return true if connected, false otherwise
 */
bool wifi_retry_is_connected(const wifi_retry_state_t* state);

/**
 * @brief Check if retry timer should be running
 * @param state Pointer to retry state structure
 * @return true if timer should be running, false otherwise
 */
bool wifi_retry_should_timer_run(const wifi_retry_state_t* state);

#endif // WIFI_RETRY_MANAGER_H
