/**
 * @file test_mqtt_retry.c
 * @brief Unit tests for MQTT retry manager
 * 
 * Tests the pure C MQTT retry logic without any ESP SDK or hardware dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mqtt_retry_manager.h"

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
 * Test: Initialize with auto-reconnect enabled
 */
int test_init_with_auto_reconnect(void)
{
    mqtt_retry_state_t state;
    
    mqtt_retry_init(&state, true);
    
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(&state), "Should not be connected initially");
    TEST_ASSERT_EQUAL(0, mqtt_retry_get_disconnect_count(&state), "Disconnect count should be 0");
    TEST_ASSERT_EQUAL(true, state.should_reconnect, "Auto-reconnect should be enabled");
    
    return 0;
}

/**
 * Test: Initialize with auto-reconnect disabled
 */
int test_init_without_auto_reconnect(void)
{
    mqtt_retry_state_t state;
    
    mqtt_retry_init(&state, false);
    
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(&state), "Should not be connected initially");
    TEST_ASSERT_EQUAL(0, mqtt_retry_get_disconnect_count(&state), "Disconnect count should be 0");
    TEST_ASSERT_EQUAL(false, state.should_reconnect, "Auto-reconnect should be disabled");
    
    return 0;
}

/**
 * Test: Disconnect with auto-reconnect triggers reconnect action
 */
int test_disconnect_with_auto_reconnect(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_RECONNECT, result.action, 
                      "Should attempt to reconnect");
    TEST_ASSERT_EQUAL(true, result.should_callback_disconnected, 
                      "Should trigger disconnect callback");
    TEST_ASSERT_EQUAL(1, mqtt_retry_get_disconnect_count(&state), 
                      "Disconnect count should be 1");
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(&state), 
                      "Should not be connected");
    
    return 0;
}

/**
 * Test: Disconnect without auto-reconnect does nothing
 */
int test_disconnect_without_auto_reconnect(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, false);
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_NONE, result.action, 
                      "Should not attempt to reconnect");
    TEST_ASSERT_EQUAL(true, result.should_callback_disconnected, 
                      "Should still trigger disconnect callback");
    TEST_ASSERT_EQUAL(1, mqtt_retry_get_disconnect_count(&state), 
                      "Disconnect count should be 1");
    
    return 0;
}

/**
 * Test: Multiple disconnects increment counter
 */
int test_multiple_disconnects(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    for (int i = 1; i <= 5; i++) {
        mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
        
        TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_RECONNECT, result.action, 
                          "Should keep trying to reconnect");
        TEST_ASSERT_EQUAL(i, mqtt_retry_get_disconnect_count(&state), 
                          "Disconnect count should increment");
    }
    
    return 0;
}

/**
 * Test: Connected updates state correctly
 */
int test_connected(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    // Simulate some disconnects first
    mqtt_retry_on_disconnect(&state);
    mqtt_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(2, mqtt_retry_get_disconnect_count(&state), "Should have 2 disconnects");
    
    // Connect
    mqtt_retry_result_t result = mqtt_retry_on_connected(&state);
    
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_NONE, result.action, "No action needed");
    TEST_ASSERT_EQUAL(true, result.should_callback_connected, 
                      "Should trigger connected callback");
    TEST_ASSERT_EQUAL(true, mqtt_retry_is_connected(&state), "Should be connected");
    // Note: disconnect count is NOT reset - it's cumulative
    TEST_ASSERT_EQUAL(2, mqtt_retry_get_disconnect_count(&state), 
                      "Disconnect count should persist");
    
    return 0;
}

/**
 * Test: NULL state pointer is handled safely
 */
int test_null_state_safe(void)
{
    mqtt_retry_init(NULL, true);  // Should not crash
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(NULL);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_NONE, result.action, "Should return NONE for NULL");
    
    result = mqtt_retry_on_connected(NULL);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_NONE, result.action, "Should return NONE for NULL");
    
    TEST_ASSERT_EQUAL(0, mqtt_retry_get_disconnect_count(NULL), "Should return 0 for NULL");
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(NULL), "Should return false for NULL");
    
    return 0;
}

/**
 * Test: Full connection cycle
 */
int test_full_connection_cycle(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    // Initial state - not connected
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(&state), "Initially not connected");
    TEST_ASSERT_EQUAL(0, mqtt_retry_get_disconnect_count(&state), "No disconnects yet");
    
    // Connect
    mqtt_retry_result_t r1 = mqtt_retry_on_connected(&state);
    TEST_ASSERT_EQUAL(true, mqtt_retry_is_connected(&state), "Should be connected");
    TEST_ASSERT_EQUAL(true, r1.should_callback_connected, "Trigger connected callback");
    
    // Disconnect - should auto-reconnect
    mqtt_retry_result_t r2 = mqtt_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_RECONNECT, r2.action, "Should reconnect");
    TEST_ASSERT_EQUAL(false, mqtt_retry_is_connected(&state), "Should be disconnected");
    TEST_ASSERT_EQUAL(1, mqtt_retry_get_disconnect_count(&state), "One disconnect");
    
    // Reconnect
    mqtt_retry_result_t r3 = mqtt_retry_on_connected(&state);
    TEST_ASSERT_EQUAL(true, mqtt_retry_is_connected(&state), "Connected again");
    
    // Disconnect again
    mqtt_retry_result_t r4 = mqtt_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_RECONNECT, r4.action, "Should reconnect again");
    TEST_ASSERT_EQUAL(2, mqtt_retry_get_disconnect_count(&state), "Two disconnects total");
    
    return 0;
}

/**
 * Test: Disconnect count tracks all disconnections
 */
int test_disconnect_count_tracking(void)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    // Connect and disconnect multiple times
    mqtt_retry_on_connected(&state);
    mqtt_retry_on_disconnect(&state);  // 1
    
    mqtt_retry_on_connected(&state);
    mqtt_retry_on_disconnect(&state);  // 2
    
    mqtt_retry_on_connected(&state);
    mqtt_retry_on_disconnect(&state);  // 3
    
    TEST_ASSERT_EQUAL(3, mqtt_retry_get_disconnect_count(&state), 
                      "Should track all disconnects");
    
    return 0;
}

/**
 * Test: Auto-reconnect can be enabled/disabled
 */
int test_auto_reconnect_toggle(void)
{
    mqtt_retry_state_t state;
    
    // Start with auto-reconnect enabled
    mqtt_retry_init(&state, true);
    mqtt_retry_result_t r1 = mqtt_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_RECONNECT, r1.action, "Should reconnect");
    
    // Disable auto-reconnect (by reinitializing)
    mqtt_retry_init(&state, false);
    mqtt_retry_result_t r2 = mqtt_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(MQTT_RETRY_ACTION_NONE, r2.action, "Should not reconnect");
    
    return 0;
}

/* ========== Main Test Runner ========== */

int main(void)
{
    printf("\n");
    printf("======================================\n");
    printf("  MQTT Retry Manager Tests\n");
    printf("======================================\n\n");
    
    RUN_TEST(test_init_with_auto_reconnect);
    RUN_TEST(test_init_without_auto_reconnect);
    RUN_TEST(test_disconnect_with_auto_reconnect);
    RUN_TEST(test_disconnect_without_auto_reconnect);
    RUN_TEST(test_multiple_disconnects);
    RUN_TEST(test_connected);
    RUN_TEST(test_null_state_safe);
    RUN_TEST(test_full_connection_cycle);
    RUN_TEST(test_disconnect_count_tracking);
    RUN_TEST(test_auto_reconnect_toggle);
    
    printf("\n");
    printf("======================================\n");
    printf("  Test Results\n");
    printf("======================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d\n", tests_passed);
    printf("Failed:       %d\n", tests_failed);
    printf("======================================\n\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
