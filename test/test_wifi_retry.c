/**
 * @file test_wifi_retry.c
 * @brief Unit tests for WiFi retry manager
 * 
 * Tests the pure C WiFi retry logic without any ESP SDK or hardware dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifi_retry_manager.h"

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
 * Test: Initialize sets initial state correctly
 */
int test_init_state(void)
{
    wifi_retry_state_t state;
    
    wifi_retry_init(&state, 10, 60000);
    
    TEST_ASSERT_EQUAL(10, state.max_retries, "Max retries should be set");
    TEST_ASSERT_EQUAL(60000, state.retry_interval_ms, "Retry interval should be set");
    TEST_ASSERT_EQUAL(0, wifi_retry_get_count(&state), "Initial retry count should be 0");
    TEST_ASSERT_EQUAL(false, wifi_retry_is_connected(&state), "Should not be connected initially");
    TEST_ASSERT_EQUAL(false, wifi_retry_should_timer_run(&state), "Timer should not run initially");
    
    return 0;
}

/**
 * Test: First disconnect triggers immediate retry
 */
int test_first_disconnect_retries(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, result.action, "Should attempt to connect");
    TEST_ASSERT_EQUAL(true, result.should_callback_disconnected, "Should trigger disconnect callback");
    TEST_ASSERT_EQUAL(1, result.callback_retry_count, "Retry count should be 1");
    TEST_ASSERT_EQUAL(1, wifi_retry_get_count(&state), "State retry count should be 1");
    TEST_ASSERT_EQUAL(false, wifi_retry_is_connected(&state), "Should not be connected");
    
    return 0;
}

/**
 * Test: Multiple disconnects increment retry count
 */
int test_multiple_disconnects(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    for (int i = 1; i <= 3; i++) {
        wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
        
        TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, result.action, "Should keep retrying");
        TEST_ASSERT_EQUAL(i, wifi_retry_get_count(&state), "Retry count should increment");
        TEST_ASSERT_EQUAL(i, result.callback_retry_count, "Callback retry count should match");
    }
    
    return 0;
}

/**
 * Test: Max retries exceeded starts timer
 */
int test_max_retries_exceeded(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 3, 30000);
    
    // Use up all immediate retries
    wifi_retry_on_disconnect(&state);  // retry 1
    wifi_retry_on_disconnect(&state);  // retry 2
    wifi_retry_on_disconnect(&state);  // retry 3
    
    // Next disconnect should start timer
    wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_FAIL, result.action, 
                      "Should start timer after max retries");
    TEST_ASSERT_EQUAL(true, wifi_retry_should_timer_run(&state), 
                      "Timer should be marked as running");
    TEST_ASSERT_EQUAL(true, result.should_callback_disconnected, 
                      "Should still trigger disconnect callback");
    
    return 0;
}

/**
 * Test: Connected resets retry count
 */
int test_connected_resets_count(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    // Simulate some retries
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(2, wifi_retry_get_count(&state), "Should have 2 retries");
    
    // Connect
    wifi_retry_result_t result = wifi_retry_on_connected(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_NONE, result.action, "No action needed");
    TEST_ASSERT_EQUAL(true, result.should_callback_connected, "Should trigger connected callback");
    TEST_ASSERT_EQUAL(0, wifi_retry_get_count(&state), "Retry count should reset to 0");
    TEST_ASSERT_EQUAL(true, wifi_retry_is_connected(&state), "Should be connected");
    
    return 0;
}

/**
 * Test: Connected stops timer if running
 */
int test_connected_stops_timer(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // Exceed max retries to start timer
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    wifi_retry_result_t timer_result = wifi_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_FAIL, timer_result.action, "Timer should start");
    TEST_ASSERT_EQUAL(true, wifi_retry_should_timer_run(&state), "Timer should be running");
    
    // Now connect
    wifi_retry_result_t result = wifi_retry_on_connected(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_STOP_TIMER, result.action, "Should stop timer");
    TEST_ASSERT_EQUAL(false, wifi_retry_should_timer_run(&state), "Timer should not be running");
    TEST_ASSERT_EQUAL(true, wifi_retry_is_connected(&state), "Should be connected");
    
    return 0;
}

/**
 * Test: Timer expiry resets retry count and attempts connect
 */
int test_timer_expiry(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // Exceed max retries
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    
    TEST_ASSERT_EQUAL(2, wifi_retry_get_count(&state), "Should have 2 retries, we can't retry past max");
    
    // Timer expires
    wifi_retry_result_t result = wifi_retry_on_timer_expired(&state);
    
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, result.action, "Should attempt to connect");
    TEST_ASSERT_EQUAL(0, wifi_retry_get_count(&state), "Retry count should reset to 0");
    
    return 0;
}

/**
 * Test: NULL state pointer is handled safely
 */
int test_null_state_safe(void)
{
    wifi_retry_init(NULL, 5, 30000);  // Should not crash
    
    wifi_retry_result_t result = wifi_retry_on_disconnect(NULL);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_NONE, result.action, "Should return NONE for NULL");
    
    result = wifi_retry_on_connected(NULL);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_NONE, result.action, "Should return NONE for NULL");
    
    result = wifi_retry_on_timer_expired(NULL);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_NONE, result.action, "Should return NONE for NULL");
    
    TEST_ASSERT_EQUAL(0, wifi_retry_get_count(NULL), "Should return 0 for NULL");
    TEST_ASSERT_EQUAL(false, wifi_retry_is_connected(NULL), "Should return false for NULL");
    TEST_ASSERT_EQUAL(false, wifi_retry_should_timer_run(NULL), "Should return false for NULL");
    
    return 0;
}

/**
 * Test: Full retry cycle - disconnect, retry, fail, timer, reconnect
 */
int test_full_retry_cycle(void)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // First disconnect - immediate retry
    wifi_retry_result_t r1 = wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, r1.action, "First retry");
    TEST_ASSERT_EQUAL(1, wifi_retry_get_count(&state), "Count = 1");
    
    // Second disconnect - immediate retry
    wifi_retry_result_t r2 = wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, r2.action, "Second retry");
    TEST_ASSERT_EQUAL(2, wifi_retry_get_count(&state), "Count = 2");
    
    // Third disconnect - max exceeded, start timer
    wifi_retry_result_t r3 = wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_FAIL, r3.action, "Start timer");
    TEST_ASSERT_EQUAL(true, wifi_retry_should_timer_run(&state), "Timer running");
    
    // Timer expires - reset and retry
    wifi_retry_result_t r4 = wifi_retry_on_timer_expired(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, r4.action, "Retry after timer");
    TEST_ASSERT_EQUAL(0, wifi_retry_get_count(&state), "Count reset");
    
    // Successfully connect
    wifi_retry_result_t r5 = wifi_retry_on_connected(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_STOP_TIMER, r5.action, "Stop timer");
    TEST_ASSERT_EQUAL(true, wifi_retry_is_connected(&state), "Connected");
    TEST_ASSERT_EQUAL(false, wifi_retry_should_timer_run(&state), "Timer stopped");
    
    return 0;
}

/**
 * Test: Custom retry configuration
 */
int test_custom_configuration(void)
{
    wifi_retry_state_t state;
    
    // Test with 1 retry, 1 minute interval
    wifi_retry_init(&state, 1, 60000);
    
    TEST_ASSERT_EQUAL(1, state.max_retries, "Max retries = 1");
    TEST_ASSERT_EQUAL(60000, state.retry_interval_ms, "Interval = 60000ms");
    
    // First disconnect - retry
    wifi_retry_result_t r1 = wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_CONNECT, r1.action, "First retry");
    
    // Second disconnect - already at max, start timer
    wifi_retry_result_t r2 = wifi_retry_on_disconnect(&state);
    TEST_ASSERT_EQUAL(WIFI_RETRY_ACTION_FAIL, r2.action, "Exceeded after 1 retry");
    
    return 0;
}

/* ========== Main Test Runner ========== */

int main(void)
{
    printf("\n");
    printf("======================================\n");
    printf("  WiFi Retry Manager Tests\n");
    printf("======================================\n\n");
    
    RUN_TEST(test_init_state);
    RUN_TEST(test_first_disconnect_retries);
    RUN_TEST(test_multiple_disconnects);
    RUN_TEST(test_max_retries_exceeded);
    RUN_TEST(test_connected_resets_count);
    RUN_TEST(test_connected_stops_timer);
    RUN_TEST(test_timer_expiry);
    RUN_TEST(test_null_state_safe);
    RUN_TEST(test_full_retry_cycle);
    RUN_TEST(test_custom_configuration);
    
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
