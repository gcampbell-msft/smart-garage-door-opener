/**
 * @file test_state_machine.cpp
 * @brief Unit tests for garage door state machine using Google Test
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "garage_state_machine.h"
}

// ========== State Transition Tests ==========

/**
 * Test: Initial state is set correctly
 */
TEST(StateMachine, InitSetsInitialState)
{
    garage_state_machine_t sm;
    
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    EXPECT_EQ(GARAGE_STATE_UNKNOWN, garage_sm_get_state(&sm)) << "Initial state should be UNKNOWN";
    
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    EXPECT_EQ(GARAGE_STATE_CLOSED, garage_sm_get_state(&sm)) << "Initial state should be CLOSED";
}

/**
 * Test: NULL pointer handling
 */
TEST(StateMachine, NullPointerHandling)
{
    garage_sm_init(NULL, GARAGE_STATE_CLOSED);  // Should not crash
    
    EXPECT_EQ(GARAGE_STATE_UNKNOWN, garage_sm_get_state(NULL)) << "get_state with NULL should return UNKNOWN";
    
    // process_event with NULL should return safe defaults
    garage_transition_result_t result = garage_sm_process_event(NULL, GARAGE_EVENT_COMMAND_OPEN);
    EXPECT_EQ(GARAGE_STATE_UNKNOWN, result.new_state) << "process_event with NULL should return UNKNOWN state";
    EXPECT_FALSE(result.state_changed) << "No state change for NULL";
}

/**
 * Test: CLOSED -> OPENING via command
 */
TEST(StateMachine, ClosedToOpeningViaCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Should transition to OPENING";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_TRUE(result.actions.trigger_button_press) << "Should trigger button press";
    EXPECT_TRUE(result.actions.start_timeout_timer) << "Should start timeout timer";
    EXPECT_TRUE(result.actions.publish_state) << "Should publish state";
}

/**
 * Test: CLOSED -> OPENING via sensor
 */
TEST(StateMachine, ClosedToOpeningViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Should transition to OPENING when sensor shows open";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_FALSE(result.actions.trigger_button_press) << "Should NOT trigger button (sensor triggered)";
    EXPECT_TRUE(result.actions.start_timeout_timer) << "Should start timeout timer";
}

/**
 * Test: CLOSED ignores close command
 */
TEST(StateMachine, ClosedIgnoresCloseCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should stay CLOSED";
    EXPECT_FALSE(result.state_changed) << "State should NOT have changed";
    EXPECT_FALSE(result.actions.trigger_button_press) << "Should NOT trigger button";
}

/**
 * Test: OPEN -> CLOSING via command
 */
TEST(StateMachine, OpenToClosingViaCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    EXPECT_EQ(GARAGE_STATE_CLOSING, result.new_state) << "Should transition to CLOSING";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_TRUE(result.actions.trigger_button_press) << "Should trigger button press";
    EXPECT_TRUE(result.actions.start_timeout_timer) << "Should start timeout timer";
}

/**
 * Test: OPEN -> CLOSED via sensor
 */
TEST(StateMachine, OpenToClosedViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should transition to CLOSED";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_FALSE(result.actions.trigger_button_press) << "Should NOT trigger button";
}

/**
 * Test: OPEN ignores open command
 */
TEST(StateMachine, OpenIgnoresOpenCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Should stay OPEN";
    EXPECT_FALSE(result.state_changed) << "State should NOT have changed";
}

/**
 * Test: CLOSING -> CLOSED via sensor
 */
TEST(StateMachine, ClosingToClosedViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should transition to CLOSED";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: CLOSING -> UNKNOWN via timeout
 */
TEST(StateMachine, ClosingToUnknownViaTimeout)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    EXPECT_EQ(GARAGE_STATE_UNKNOWN, result.new_state) << "Should transition to UNKNOWN on timeout";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: OPENING -> OPEN via timeout
 */
TEST(StateMachine, OpeningToOpenViaTimeout)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPENING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Should transition to OPEN on timeout";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: OPENING -> CLOSED via sensor (door reversed)
 */
TEST(StateMachine, OpeningToClosedViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPENING);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should transition to CLOSED (door reversed)";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: UNKNOWN -> CLOSED via sensor
 */
TEST(StateMachine, UnknownToClosedViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should transition to CLOSED";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: UNKNOWN -> OPEN via sensor
 */
TEST(StateMachine, UnknownToOpenViaSensor)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Should transition to OPEN";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
}

/**
 * Test: UNKNOWN -> OPENING via command
 */
TEST(StateMachine, UnknownToOpeningViaCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Should transition to OPENING";
    EXPECT_TRUE(result.actions.trigger_button_press) << "Should trigger button press";
}

/**
 * Test: UNKNOWN -> CLOSING via command
 */
TEST(StateMachine, UnknownToClosingViaCommand)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    EXPECT_EQ(GARAGE_STATE_CLOSING, result.new_state) << "Should transition to CLOSING";
    EXPECT_TRUE(result.actions.trigger_button_press) << "Should trigger button press";
}

/**
 * Test: State to string conversion
 */
TEST(StateMachine, StateToString)
{
    EXPECT_STREQ("closed", garage_state_to_string(GARAGE_STATE_CLOSED)) << "CLOSED should convert to 'closed'";
    EXPECT_STREQ("open", garage_state_to_string(GARAGE_STATE_OPEN)) << "OPEN should convert to 'open'";
    EXPECT_STREQ("closing", garage_state_to_string(GARAGE_STATE_CLOSING)) << "CLOSING should convert to 'closing'";
    EXPECT_STREQ("opening", garage_state_to_string(GARAGE_STATE_OPENING)) << "OPENING should convert to 'opening'";
    EXPECT_STREQ("unknown", garage_state_to_string(GARAGE_STATE_UNKNOWN)) << "UNKNOWN should convert to 'unknown'";
}

/**
 * Test: Full sequence - Open and Close cycle
 */
TEST(StateMachine, FullOpenCloseCycle)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Open command
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Step 1: CLOSED -> OPENING";
    
    // Timer expires (door opened)
    result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Step 2: OPENING -> OPEN";
    
    // Close command
    result = garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    EXPECT_EQ(GARAGE_STATE_CLOSING, result.new_state) << "Step 3: OPEN -> CLOSING";
    
    // Sensor detects closed
    result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Step 4: CLOSING -> CLOSED";
}

/**
 * Test: Sequence with physical button press (no command, sensor only)
 */
TEST(StateMachine, PhysicalButtonSequence)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Someone pressed physical button, sensor shows door not closed
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_OPEN);
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Sensor open -> OPENING";
    EXPECT_FALSE(result.actions.trigger_button_press) << "No button press (physical)";
    
    // Timer expires
    result = garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Timer -> OPEN";
}

// ========== Timer Tests ==========

/**
 * Test: Timer starts when transitioning to OPENING
 */
TEST(StateMachineTimer, TimerStartsOnOpening)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    EXPECT_FALSE(garage_sm_is_timer_active(&sm)) << "Timer should not be active initially";
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    EXPECT_TRUE(garage_sm_is_timer_active(&sm)) << "Timer should be active in OPENING state";
    EXPECT_EQ(0, garage_sm_get_timer_elapsed(&sm)) << "Timer should start at 0";
}

/**
 * Test: Timer starts when transitioning to CLOSING
 */
TEST(StateMachineTimer, TimerStartsOnClosing)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    
    EXPECT_TRUE(garage_sm_is_timer_active(&sm)) << "Timer should be active in CLOSING state";
    EXPECT_EQ(0, garage_sm_get_timer_elapsed(&sm)) << "Timer should start at 0";
}

/**
 * Test: Timer stops when reaching CLOSED state
 */
TEST(StateMachineTimer, TimerStopsOnClosed)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    // Manually activate timer
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    EXPECT_TRUE(garage_sm_is_timer_active(&sm)) << "Timer should be active";

    // Transition to CLOSED
    garage_transition_result_t result = garage_sm_process_event(&sm, GARAGE_EVENT_SENSOR_CLOSED);
    EXPECT_EQ(GARAGE_STATE_CLOSED, result.new_state) << "Should be in CLOSED state";
    
    EXPECT_FALSE(garage_sm_is_timer_active(&sm)) << "Timer should stop in CLOSED state";
    EXPECT_EQ(0, garage_sm_get_timer_elapsed(&sm)) << "Timer should reset";
}

/**
 * Test: Timer stops when reaching OPEN state
 */
TEST(StateMachineTimer, TimerStopsOnOpen)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    EXPECT_TRUE(garage_sm_is_timer_active(&sm)) << "Timer should be active";
    
    // Manually trigger timeout to reach OPEN
    garage_sm_process_event(&sm, GARAGE_EVENT_TIMER_EXPIRED);
    
    EXPECT_FALSE(garage_sm_is_timer_active(&sm)) << "Timer should stop in OPEN state";
}

/**
 * Test: Timer update increments elapsed time
 */
TEST(StateMachineTimer, TimerUpdateIncrements)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    // Update timer
    garage_transition_result_t result = garage_sm_update_timer(&sm, 1000);
    EXPECT_EQ(1000, garage_sm_get_timer_elapsed(&sm)) << "Timer should be at 1000ms";
    EXPECT_FALSE(result.state_changed) << "No state change yet";
    
    result = garage_sm_update_timer(&sm, 2000);
    EXPECT_EQ(3000, garage_sm_get_timer_elapsed(&sm)) << "Timer should be at 3000ms";
    EXPECT_FALSE(result.state_changed) << "No state change yet";
}

/**
 * Test: Timer timeout causes OPENING -> OPEN transition
 */
TEST(StateMachineTimer, TimerTimeoutOpeningToOpen)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    EXPECT_EQ(GARAGE_STATE_OPENING, garage_sm_get_state(&sm)) << "Should be OPENING";
    
    // Update timer to just before timeout (default 15000ms)
    garage_transition_result_t result = garage_sm_update_timer(&sm, 14999);
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Should still be OPENING";
    EXPECT_FALSE(result.state_changed) << "Should not have changed yet";
    
    // Now exceed timeout
    result = garage_sm_update_timer(&sm, 1);
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Should transition to OPEN";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_FALSE(garage_sm_is_timer_active(&sm)) << "Timer should be stopped";
}

/**
 * Test: Timer timeout causes CLOSING -> UNKNOWN transition
 */
TEST(StateMachineTimer, TimerTimeoutClosingToUnknown)
{
    garage_state_machine_t sm;
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    
    // Start closing
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_CLOSE);
    EXPECT_EQ(GARAGE_STATE_CLOSING, garage_sm_get_state(&sm)) << "Should be CLOSING";
    
    // Exceed timeout
    garage_transition_result_t result = garage_sm_update_timer(&sm, 15000);
    EXPECT_EQ(GARAGE_STATE_UNKNOWN, result.new_state) << "Should transition to UNKNOWN";
    EXPECT_TRUE(result.state_changed) << "State should have changed";
    EXPECT_FALSE(garage_sm_is_timer_active(&sm)) << "Timer should be stopped";
}

/**
 * Test: Custom timeout configuration
 */
TEST(StateMachineTimer, CustomTimeoutConfig)
{
    garage_state_machine_t sm;
    garage_sm_config_t config = { .timeout_ms = 5000 };  // 5 second timeout
    
    garage_sm_init_with_config(&sm, GARAGE_STATE_CLOSED, &config);
    
    // Start opening
    garage_sm_process_event(&sm, GARAGE_EVENT_COMMAND_OPEN);
    
    // After 4999ms, should still be opening
    garage_transition_result_t result = garage_sm_update_timer(&sm, 4999);
    EXPECT_EQ(GARAGE_STATE_OPENING, result.new_state) << "Should still be OPENING";
    
    // After 5000ms, should transition to OPEN
    result = garage_sm_update_timer(&sm, 1);
    EXPECT_EQ(GARAGE_STATE_OPEN, result.new_state) << "Should transition to OPEN";
}

/**
 * Test: Timer doesn't run when not in transitional state
 */
TEST(StateMachineTimer, TimerInactiveInStableStates)
{
    garage_state_machine_t sm;
    
    // Test CLOSED state
    garage_sm_init(&sm, GARAGE_STATE_CLOSED);
    garage_transition_result_t result = garage_sm_update_timer(&sm, 10000);
    EXPECT_FALSE(result.state_changed) << "Timer update should have no effect in CLOSED";
    
    // Test OPEN state
    garage_sm_init(&sm, GARAGE_STATE_OPEN);
    result = garage_sm_update_timer(&sm, 10000);
    EXPECT_FALSE(result.state_changed) << "Timer update should have no effect in OPEN";
    
    // Test UNKNOWN state
    garage_sm_init(&sm, GARAGE_STATE_UNKNOWN);
    result = garage_sm_update_timer(&sm, 10000);
    EXPECT_FALSE(result.state_changed) << "Timer update should have no effect in UNKNOWN";
}
