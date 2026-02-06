/**
 * @file garage_state_machine.c
 * @brief Garage door state machine implementation - pure logic, no hardware dependencies.
 */

#include "garage_state_machine.h"
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_TIMEOUT_MS 15000  // 15 seconds

void garage_sm_init(garage_state_machine_t* sm, garage_state_t initial_state)
{
    if (sm != NULL) {
        sm->current_state = initial_state;
        sm->timeout_ms = DEFAULT_TIMEOUT_MS;
        sm->timer_elapsed_ms = 0;
        sm->timer_active = false;
    }
}

void garage_sm_init_with_config(garage_state_machine_t* sm, garage_state_t initial_state,
                                 const garage_sm_config_t* config)
{
    if (sm != NULL) {
        sm->current_state = initial_state;
        sm->timeout_ms = (config != NULL && config->timeout_ms > 0) ? 
                         config->timeout_ms : DEFAULT_TIMEOUT_MS;
        sm->timer_elapsed_ms = 0;
        sm->timer_active = false;
    }
}

garage_state_t garage_sm_get_state(const garage_state_machine_t* sm)
{
    if (sm == NULL) {
        return GARAGE_STATE_UNKNOWN;
    }
    return sm->current_state;
}

bool garage_sm_is_timer_active(const garage_state_machine_t* sm)
{
    if (sm == NULL) {
        return false;
    }
    return sm->timer_active;
}

int garage_sm_get_timer_elapsed(const garage_state_machine_t* sm)
{
    if (sm == NULL) {
        return 0;
    }
    return sm->timer_elapsed_ms;
}

const char* garage_state_to_string(garage_state_t state)
{
    switch (state) {
        case GARAGE_STATE_CLOSED:  return "CLOSED";
        case GARAGE_STATE_OPEN:    return "OPEN";
        case GARAGE_STATE_CLOSING: return "CLOSING";
        case GARAGE_STATE_OPENING: return "OPENING";
        case GARAGE_STATE_UNKNOWN:
        default:                   return "UNKNOWN";
    }
}

const char* garage_state_to_display_string(garage_state_t state)
{
    switch (state) {
        case GARAGE_STATE_CLOSED:  return "CLOSED";
        case GARAGE_STATE_OPEN:    return "OPEN";
        case GARAGE_STATE_CLOSING: return "CLOSING";
        case GARAGE_STATE_OPENING: return "OPENING";
        case GARAGE_STATE_UNKNOWN:
        default:                   return "UNKNOWN";
    }
}

/**
 * @brief Create a transition result with common defaults
 */
static garage_transition_result_t make_result(garage_state_t current, garage_state_t new_state,
                                               bool button_press, bool start_timer)
{
    garage_transition_result_t result = {
        .new_state = new_state,
        .state_changed = (current != new_state),
        .actions = {
            .publish_state = (current != new_state),
            .trigger_button_press = button_press,
            .start_timeout_timer = start_timer
        }
    };
    return result;
}

/**
 * @brief Handle events when in CLOSED state
 */
static garage_transition_result_t handle_closed_state(garage_state_t current, garage_event_t event)
{
    switch (event) {
        case GARAGE_EVENT_SENSOR_OPEN:
            // Door sensor says not closed -> door is opening
            return make_result(current, GARAGE_STATE_OPENING, false, true);

        case GARAGE_EVENT_COMMAND_OPEN:
            // Command to open -> press button, transition to opening
            return make_result(current, GARAGE_STATE_OPENING, true, true);

        default:
            // No state change
            return make_result(current, current, false, false);
    }
}

/**
 * @brief Handle events when in OPEN state
 */
static garage_transition_result_t handle_open_state(garage_state_t current, garage_event_t event)
{
    switch (event) {
        case GARAGE_EVENT_SENSOR_CLOSED:
            // Sensor says closed -> door is now closed
            return make_result(current, GARAGE_STATE_CLOSED, false, false);

        case GARAGE_EVENT_COMMAND_CLOSE:
            // Command to close -> press button, transition to closing
            return make_result(current, GARAGE_STATE_CLOSING, true, true);

        default:
            // No state change
            return make_result(current, current, false, false);
    }
}

/**
 * @brief Handle events when in CLOSING state
 */
static garage_transition_result_t handle_closing_state(garage_state_t current, garage_event_t event)
{
    switch (event) {
        case GARAGE_EVENT_SENSOR_CLOSED:
            // Sensor says closed -> door finished closing
            return make_result(current, GARAGE_STATE_CLOSED, false, false);

        case GARAGE_EVENT_TIMER_EXPIRED:
            // Timeout without reaching closed -> unknown state
            return make_result(current, GARAGE_STATE_UNKNOWN, false, false);

        default:
            // No state change
            return make_result(current, current, false, false);
    }
}

/**
 * @brief Handle events when in OPENING state
 */
static garage_transition_result_t handle_opening_state(garage_state_t current, garage_event_t event)
{
    switch (event) {
        case GARAGE_EVENT_SENSOR_CLOSED:
            // Sensor says closed -> door closed (maybe reversed?)
            return make_result(current, GARAGE_STATE_CLOSED, false, false);

        case GARAGE_EVENT_TIMER_EXPIRED:
            // Timeout -> assume door is now open
            return make_result(current, GARAGE_STATE_OPEN, false, false);

        default:
            // No state change
            return make_result(current, current, false, false);
    }
}

/**
 * @brief Handle events when in UNKNOWN state
 */
static garage_transition_result_t handle_unknown_state(garage_state_t current, garage_event_t event)
{
    switch (event) {
        case GARAGE_EVENT_SENSOR_CLOSED:
            // Sensor says closed -> door is closed
            return make_result(current, GARAGE_STATE_CLOSED, false, false);

        case GARAGE_EVENT_SENSOR_OPEN:
            // Sensor says not closed -> door is open
            return make_result(current, GARAGE_STATE_OPEN, false, false);

        case GARAGE_EVENT_COMMAND_OPEN:
            // Command to open -> press button, assume opening
            return make_result(current, GARAGE_STATE_OPENING, true, true);

        case GARAGE_EVENT_COMMAND_CLOSE:
            // Command to close -> press button, assume closing
            return make_result(current, GARAGE_STATE_CLOSING, true, true);

        default:
            // No state change
            return make_result(current, current, false, false);
    }
}

garage_transition_result_t garage_sm_process_event(garage_state_machine_t* sm, garage_event_t event)
{
    if (sm == NULL) {
        garage_transition_result_t error_result = {
            .new_state = GARAGE_STATE_UNKNOWN,
            .state_changed = false,
            .actions = { .publish_state = false, .trigger_button_press = false, .start_timeout_timer = false }
        };
        return error_result;
    }

    garage_state_t current = sm->current_state;
    garage_transition_result_t result;

    switch (current) {
        case GARAGE_STATE_CLOSED:
            result = handle_closed_state(current, event);
            break;

        case GARAGE_STATE_OPEN:
            result = handle_open_state(current, event);
            break;

        case GARAGE_STATE_CLOSING:
            result = handle_closing_state(current, event);
            break;

        case GARAGE_STATE_OPENING:
            result = handle_opening_state(current, event);
            break;

        case GARAGE_STATE_UNKNOWN:
        default:
            result = handle_unknown_state(current, event);
            break;
    }

    // Update state machine's current state
    sm->current_state = result.new_state;

    // Manage timer based on actions
    if (result.actions.start_timeout_timer) {
        sm->timer_active = true;
        sm->timer_elapsed_ms = 0;
    }

    // Stop timer if we reached a stable state (CLOSED or OPEN)
    if (result.new_state == GARAGE_STATE_CLOSED || result.new_state == GARAGE_STATE_OPEN) {
        sm->timer_active = false;
        sm->timer_elapsed_ms = 0;
    }

    return result;
}

garage_transition_result_t garage_sm_update_timer(garage_state_machine_t* sm, int delta_ms)
{
    garage_transition_result_t no_change = {
        .new_state = sm != NULL ? sm->current_state : GARAGE_STATE_UNKNOWN,
        .state_changed = false,
        .actions = { .publish_state = false, .trigger_button_press = false, .start_timeout_timer = false }
    };

    if (sm == NULL || !sm->timer_active) {
        return no_change;
    }

    sm->timer_elapsed_ms += delta_ms;

    // Check if timeout exceeded
    if (sm->timer_elapsed_ms >= sm->timeout_ms) {
        // Timer expired - send timer expired event
        sm->timer_active = false;
        sm->timer_elapsed_ms = 0;
        return garage_sm_process_event(sm, GARAGE_EVENT_TIMER_EXPIRED);
    }

    return no_change;
}
