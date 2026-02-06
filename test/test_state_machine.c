/**
 * @file test_state_machine.c
 * @brief Unit tests for garage door state machine using CTest
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "garage_state_machine.h"

/* Simple test framework macros */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("  FAIL: %s\n", message); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return 1; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) do { \
    tests_run++; \
    if ((expected) != (actual)) { \
        printf("  FAIL: %s\n", message); \
        printf("        Expected: %d, Got: %d\n", (int)(expected), (int)(actual)); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return 1; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("Running %s...\n", #test_func); \
    int result = test_func(); \
    if (result == 0) { \
        printf("  PASSED\n"); \
    } \
} while(0)

/* ========== Test Cases ========== */

/**
 * Test: Initial state is set correctly
 */
int test_init_sets_initial_state(void)
{
    garage_state_machine_t sm;
    
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    TEST_ASSERT_EQUAL(GARAGE_STATE_UNKNOWN, garage_sm_get_state(&sm), 
                      "Initial state should be UNKNOWN");
    
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, garage_sm_get_state(&sm),
                      "Initial state should be CLOSED");
    
    return 0;
}

/**
 * Test: NULL pointer handling
 */
int test_null_pointer_handling(void)
{
    garage_sm_init(NULL, GARAGE_STATE_CLOSED);  // Should not crash
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_UNKNOWN, garage_sm_get_state(NULL),
                      "get_state with NULL should return UNKNOWN");
    
    // process_event with NULL should return safe defaults
    garage_transition_result_t result = garage_sm_process_event(NULL, GARAGE_EVENT_COMMAND_OPEN);
    TEST_ASSERT_EQUAL(GARAGE_STATE_UNKNOWN, result.new_state,
                      "process_event with NULL should return UNKNOWN state");
    TEST_ASSERT(!result.state_changed, "No state change for NULL");
    
    return 0;
}

/**
 * Test: CLOSED -> OPENING via command
 */
int test_closed_to_opening_via_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state,
                      "Should transition to OPENING");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(result.actions.trigger_button_press, "Should trigger button press");
    TEST_ASSERT(result.actions.start_timeout_timer, "Should start timeout timer");
    TEST_ASSERT(result.actions.publish_state, "Should publish state");
    
    return 0;
}

/**
 * Test: CLOSED -> OPENING via sensor
 */
int test_closed_to_opening_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state,
                      "Should transition to OPENING when sensor shows open");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(!result.actions.trigger_button_press, "Should NOT trigger button (sensor triggered)");
    TEST_ASSERT(result.actions.start_timeout_timer, "Should start timeout timer");
    
    return 0;
}

/**
 * Test: CLOSED ignores close command
 */
int test_closed_ignores_close_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state,
                      "Should stay CLOSED");
    TEST_ASSERT(!result.state_changed, "State should NOT have changed");
    TEST_ASSERT(!result.actions.trigger_button_press, "Should NOT trigger button");
    
    return 0;
}

/**
 * Test: OPEN -> CLOSING via command
 */
int test_open_to_closing_via_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSING, result.new_state,
                      "Should transition to CLOSING");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(result.actions.trigger_button_press, "Should trigger button press");
    TEST_ASSERT(result.actions.start_timeout_timer, "Should start timeout timer");
    
    return 0;
}

/**
 * Test: OPEN -> CLOSED via sensor
 */
int test_open_to_closed_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state,
                      "Should transition to CLOSED");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(!result.actions.trigger_button_press, "Should NOT trigger button");
    
    return 0;
}

/**
 * Test: OPEN ignores open command
 */
int test_open_ignores_open_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state,
                      "Should stay OPEN");
    TEST_ASSERT(!result.state_changed, "State should NOT have changed");
    
    return 0;
}

/**
 * Test: CLOSING -> CLOSED via sensor
 */
int test_closing_to_closed_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state,
                      "Should transition to CLOSED");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: CLOSING -> UNKNOWN via timeout
 */
int test_closing_to_unknown_via_timeout(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_UNKNOWN, result.new_state,
                      "Should transition to UNKNOWN on timeout");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: OPENING -> OPEN via timeout
 */
int test_opening_to_open_via_timeout(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPENING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state,
                      "Should transition to OPEN on timeout");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: OPENING -> CLOSED via sensor (door reversed)
 */
int test_opening_to_closed_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPENING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state,
                      "Should transition to CLOSED (door reversed)");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: UNKNOWN -> CLOSED via sensor
 */
int test_unknown_to_closed_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state,
                      "Should transition to CLOSED");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: UNKNOWN -> OPEN via sensor
 */
int test_unknown_to_open_via_sensor(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state,
                      "Should transition to OPEN");
    TEST_ASSERT(result.state_changed, "State should have changed");
    
    return 0;
}

/**
 * Test: UNKNOWN -> OPENING via command
 */
int test_unknown_to_opening_via_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state,
                      "Should transition to OPENING");
    TEST_ASSERT(result.actions.trigger_button_press, "Should trigger button press");
    
    return 0;
}

/**
 * Test: UNKNOWN -> CLOSING via command
 */
int test_unknown_to_closing_via_command(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSING, result.new_state,
                      "Should transition to CLOSING");
    TEST_ASSERT(result.actions.trigger_button_press, "Should trigger button press");
    
    return 0;
}

/**
 * Test: State to string conversion
 */
int test_state_to_string(void)
{
    TEST_ASSERT(strcmp(garage_state_to_string(GARAGE_STATE_CLOSED), "closed") == 0,
                "CLOSED should convert to 'closed'");
    TEST_ASSERT(strcmp(garage_state_to_string(GARAGE_STATE_OPEN), "open") == 0,
                "OPEN should convert to 'open'");
    TEST_ASSERT(strcmp(garage_state_to_string(GARAGE_STATE_CLOSING), "closing") == 0,
                "CLOSING should convert to 'closing'");
    TEST_ASSERT(strcmp(garage_state_to_string(GARAGE_STATE_OPENING), "opening") == 0,
                "OPENING should convert to 'opening'");
    TEST_ASSERT(strcmp(garage_state_to_string(GARAGE_STATE_UNKNOWN), "unknown") == 0,
                "UNKNOWN should convert to 'unknown'");
    
    return 0;
}

/**
 * Test: Full sequence - Open and Close cycle
 */
int test_full_open_close_cycle(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Open command
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state, "Step 1: CLOSED -> OPENING");
    
    // Timer expires (door opened)
    result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state, "Step 2: OPENING -> OPEN");
    
    // Close command
    result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSING, result.new_state, "Step 3: OPEN -> CLOSING");
    
    // Sensor detects closed
    result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state, "Step 4: CLOSING -> CLOSED");
    
    return 0;
}

/**
 * Test: Sequence with physical button press (no command, sensor only)
 */
int test_physical_button_sequence(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Someone pressed physical button, sensor shows door not closed
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state, "Sensor open -> OPENING");
    TEST_ASSERT(!result.actions.trigger_button_press, "No button press (physical)");
    
    // Timer expires
    result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state, "Timer -> OPEN");
    
    return 0;
}

/**
 * Test: Timer starts when transitioning to OPENING
 */
int test_timer_starts_on_opening(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    TEST_ASSERT(!garage_sm_is_timer_active(&sm), "Timer should not be active initially");
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    TEST_ASSERT(garage_sm_is_timer_active(&sm), "Timer should be active in OPENING state");
    TEST_ASSERT_EQUAL(0, garage_sm_get_timer_elapsed(&sm), "Timer should start at 0");
    
    return 0;
}

/**
 * Test: Timer starts when transitioning to CLOSING
 */
int test_timer_starts_on_closing(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    TEST_ASSERT(garage_sm_is_timer_active(&sm), "Timer should be active in CLOSING state");
    TEST_ASSERT_EQUAL(0, garage_sm_get_timer_elapsed(&sm), "Timer should start at 0");
    
    return 0;
}

/**
 * Test: Timer stops when reaching CLOSED state
 */
int test_timer_stops_on_closed(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    // Manually activate timer
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    TEST_ASSERT(garage_sm_is_timer_active(&sm), "Timer should be active");

    // Transition to CLOSED
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSED, result.new_state, "Should be in CLOSED state");
    
    TEST_ASSERT(!garage_sm_is_timer_active(&sm), "Timer should stop in CLOSED state");
    TEST_ASSERT_EQUAL(0, garage_sm_get_timer_elapsed(&sm), "Timer should reset");
    
    return 0;
}

/**
 * Test: Timer stops when reaching OPEN state
 */
int test_timer_stops_on_open(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    TEST_ASSERT(garage_sm_is_timer_active(&sm), "Timer should be active");
    
    // Manually trigger timeout to reach OPEN
    garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    TEST_ASSERT(!garage_sm_is_timer_active(&sm), "Timer should stop in OPEN state");
    
    return 0;
}

/**
 * Test: Timer update increments elapsed time
 */
int test_timer_update_increments(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    // Update timer
    garage_transition_result_t result = garage_sm_update_timer(&sm, 1000);
    TEST_ASSERT_EQUAL(1000, garage_sm_get_timer_elapsed(&sm), "Timer should be at 1000ms");
    TEST_ASSERT(!result.state_changed, "No state change yet");
    
    result = garage_sm_update_timer(&sm, 2000);
    TEST_ASSERT_EQUAL(3000, garage_sm_get_timer_elapsed(&sm), "Timer should be at 3000ms");
    TEST_ASSERT(!result.state_changed, "No state change yet");
    
    return 0;
}

/**
 * Test: Timer timeout causes OPENING -> OPEN transition
 */
int test_timer_timeout_opening_to_open(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, garage_sm_get_state(&sm), "Should be OPENING");
    
    // Update timer to just before timeout (default 15000ms)
    garage_transition_result_t result = garage_sm_update_timer(&sm, 14999);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state, "Should still be OPENING");
    TEST_ASSERT(!result.state_changed, "Should not have changed yet");
    
    // Now exceed timeout
    result = garage_sm_update_timer(&sm, 1);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state, "Should transition to OPEN");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(!garage_sm_is_timer_active(&sm), "Timer should be stopped");
    
    return 0;
}

/**
 * Test: Timer timeout causes CLOSING -> UNKNOWN transition
 */
int test_timer_timeout_closing_to_unknown(void)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    // Start closing
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    TEST_ASSERT_EQUAL(GARAGE_STATE_CLOSING, garage_sm_get_state(&sm), "Should be CLOSING");
    
    // Exceed timeout
    garage_transition_result_t result = garage_sm_update_timer(&sm, 15000);
    TEST_ASSERT_EQUAL(GARAGE_STATE_UNKNOWN, result.new_state, "Should transition to UNKNOWN");
    TEST_ASSERT(result.state_changed, "State should have changed");
    TEST_ASSERT(!garage_sm_is_timer_active(&sm), "Timer should be stopped");
    
    return 0;
}

/**
 * Test: Custom timeout configuration
 */
int test_custom_timeout_config(void)
{
    garage_state_machine_t sm;
    garage_sm_config_t config = { .timeout_ms = 5000 };  // 5 second timeout
    
    garage_sm_init_with_config(&sm, GARAGE_STATE_CLOSED, &config);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    // After 4999ms, should still be opening
    garage_transition_result_t result = garage_sm_update_timer(&sm, 4999);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPENING, result.new_state, "Should still be OPENING");
    
    // After 5000ms, should transition to OPEN
    result = garage_sm_update_timer(&sm, 1);
    TEST_ASSERT_EQUAL(GARAGE_STATE_OPEN, result.new_state, "Should transition to OPEN");
    
    return 0;
}

/**
 * Test: Timer doesn't run when not in transitional state
 */
int test_timer_inactive_in_stable_states(void)
{
    garage_state_machine_t sm;
    
    // Test CLOSED state
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    garage_transition_result_t result = garage_sm_update_timer(&sm, 10000);
    TEST_ASSERT(!result.state_changed, "Timer update should have no effect in CLOSED");
    
    // Test OPEN state
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    result = garage_sm_update_timer(&sm, 10000);
    TEST_ASSERT(!result.state_changed, "Timer update should have no effect in OPEN");
    
    // Test UNKNOWN state
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    result = garage_sm_update_timer(&sm, 10000);
    TEST_ASSERT(!result.state_changed, "Timer update should have no effect in UNKNOWN");
    
    return 0;
}

/* ========== Main ========== */

int main(void)
{
    printf("========================================\n");
    printf("Garage State Machine Tests\n");
    printf("========================================\n\n");
    
    RUN_TEST(test_init_sets_initial_state);
    RUN_TEST(test_null_pointer_handling);
    RUN_TEST(test_closed_to_opening_via_command);
    RUN_TEST(test_closed_to_opening_via_sensor);
    RUN_TEST(test_closed_ignores_close_command);
    RUN_TEST(test_open_to_closing_via_command);
    RUN_TEST(test_open_to_closed_via_sensor);
    RUN_TEST(test_open_ignores_open_command);
    RUN_TEST(test_closing_to_closed_via_sensor);
    RUN_TEST(test_closing_to_unknown_via_timeout);
    RUN_TEST(test_opening_to_open_via_timeout);
    RUN_TEST(test_opening_to_closed_via_sensor);
    RUN_TEST(test_unknown_to_closed_via_sensor);
    RUN_TEST(test_unknown_to_open_via_sensor);
    RUN_TEST(test_unknown_to_opening_via_command);
    RUN_TEST(test_unknown_to_closing_via_command);
    RUN_TEST(test_state_to_string);
    RUN_TEST(test_full_open_close_cycle);
    RUN_TEST(test_physical_button_sequence);
    
    // Timer tests
    RUN_TEST(test_timer_starts_on_opening);
    RUN_TEST(test_timer_starts_on_closing);
    RUN_TEST(test_timer_stops_on_closed);
    RUN_TEST(test_timer_stops_on_open);
    RUN_TEST(test_timer_update_increments);
    RUN_TEST(test_timer_timeout_opening_to_open);
    RUN_TEST(test_timer_timeout_closing_to_unknown);
    RUN_TEST(test_custom_timeout_config);
    RUN_TEST(test_timer_inactive_in_stable_states);
    
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed, %d total\n", 
           tests_passed, tests_failed, tests_run);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
