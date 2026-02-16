/**
 * @file garage_state_machine.h
 * @brief Garage door state machine - pure logic, no hardware dependencies.
 * 
 * This module contains the garage door state machine logic extracted
 * for testability. It takes events as input and returns actions to perform.
 */

#ifndef GARAGE_STATE_MACHINE_H
#define GARAGE_STATE_MACHINE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Garage door states
 */
typedef enum {
    GARAGE_STATE_CLOSED = 0,
    GARAGE_STATE_OPEN,
    GARAGE_STATE_CLOSING,
    GARAGE_STATE_OPENING,
    GARAGE_STATE_UNKNOWN
} garage_state_t;

/**
 * @brief Input events to the state machine
 */
typedef enum {
    GARAGE_EVENT_NONE = 0,
    GARAGE_EVENT_SENSOR_CLOSED,     // Reed switch indicates door closed
    GARAGE_EVENT_SENSOR_OPEN,       // Reed switch indicates door not closed
    GARAGE_EVENT_COMMAND_OPEN,      // MQTT command to open
    GARAGE_EVENT_COMMAND_CLOSE,     // MQTT command to close
    GARAGE_EVENT_TIMER_EXPIRED      // Timeout timer expired
} garage_event_t;

/**
 * @brief Actions that should be performed after state transition
 */
typedef struct {
    bool publish_state;          // Should publish new state to MQTT
    bool trigger_button_press;   // Should trigger the relay/button press
    bool start_timeout_timer;    // Should start the timeout timer
} garage_actions_t;

/**
 * @brief Result of processing an event
 */
typedef struct {
    garage_state_t new_state;    // The new state after transition
    bool state_changed;          // Whether the state actually changed
    garage_actions_t actions;    // Actions to perform
} garage_transition_result_t;

/**
 * @brief State machine configuration
 */
typedef struct {
    int timeout_ms;  // Timeout for OPENING/CLOSING states in milliseconds
} garage_sm_config_t;

/**
 * @brief State machine context
 */
typedef struct {
    garage_state_t current_state;
    int timeout_ms;           // Configured timeout duration
    int timer_elapsed_ms;     // Current timer elapsed time
    bool timer_active;             // Whether timer is currently running
} garage_state_machine_t;

/**
 * @brief Initialize the state machine with default config (15 second timeout)
 * @param sm Pointer to state machine context
 * @param initial_state Initial state (typically GARAGE_STATE_UNKNOWN)
 */
void garage_sm_init(garage_state_machine_t* sm, garage_state_t initial_state);

/**
 * @brief Initialize the state machine with custom configuration
 * @param sm Pointer to state machine context
 * @param initial_state Initial state (typically GARAGE_STATE_UNKNOWN)
 * @param config Configuration (timeout duration, etc.)
 */
void garage_sm_init_with_config(garage_state_machine_t* sm, garage_state_t initial_state, 
                                 const garage_sm_config_t* config);

/**
 * @brief Process an event and get the resulting state transition
 * 
 * This is a pure function - it computes the new state and required actions
 * without performing any side effects.
 * 
 * @param sm Pointer to state machine context
 * @param event The event to process
 * @return Result containing new state and actions to perform
 */
garage_transition_result_t garage_sm_process_event(garage_state_machine_t* sm, garage_event_t event);

/**
 * @brief Get current state
 * @param sm Pointer to state machine context
 * @return Current state
 */
garage_state_t garage_sm_get_state(const garage_state_machine_t* sm);

/**
 * @brief Update the internal timer (call periodically or for testing)
 * 
 * This function advances the internal timer and will automatically transition
 * to UNKNOWN or OPEN if the timeout expires while in CLOSING/OPENING states.
 * 
 * @param sm Pointer to state machine context
 * @param delta_ms Time elapsed since last update in milliseconds
 * @return Result if timeout caused a state transition, otherwise state_changed will be false
 */
garage_transition_result_t garage_sm_update_timer(garage_state_machine_t* sm, int delta_ms);

/**
 * @brief Check if timer is currently active
 * @param sm Pointer to state machine context
 * @return true if timer is running, false otherwise
 */
bool garage_sm_is_timer_active(const garage_state_machine_t* sm);

/**
 * @brief Get elapsed time on the current timer
 * @param sm Pointer to state machine context
 * @return Elapsed time in milliseconds
 */
int garage_sm_get_timer_elapsed(const garage_state_machine_t* sm);

/**
 * @brief Convert state to string for publishing
 * @param state The state to convert
 * @return String representation (lowercase for MQTT)
 */
const char* garage_state_to_string(garage_state_t state);

/**
 * @brief Convert state to display string
 * @param state The state to convert
 * @return String representation (uppercase for display)
 */
const char* garage_state_to_display_string(garage_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* GARAGE_STATE_MACHINE_H */
