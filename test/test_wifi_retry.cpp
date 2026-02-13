/**
 * @file test_wifi_retry.cpp
 * @brief Unit tests for WiFi retry manager using Google Test
 * 
 * Tests the pure C WiFi retry logic without any ESP SDK or hardware dependencies.
 */

#include <gtest/gtest.h>

extern "C" {
#include "wifi_retry_manager.h"
}

// ========== Test Cases ==========

/**
 * Test: Initialize sets initial state correctly
 */
TEST(WifiRetry, InitState)
{
    wifi_retry_state_t state;
    
    wifi_retry_init(&state, 10, 60000);
    
    EXPECT_EQ(10, state.max_retries) << "Max retries should be set";
    EXPECT_EQ(60000, state.retry_interval_ms) << "Retry interval should be set";
    EXPECT_EQ(0, wifi_retry_get_count(&state)) << "Initial retry count should be 0";
    EXPECT_FALSE(wifi_retry_is_connected(&state)) << "Should not be connected initially";
    EXPECT_FALSE(wifi_retry_should_timer_run(&state)) << "Timer should not run initially";
}

/**
 * Test: First disconnect triggers immediate retry
 */
TEST(WifiRetry, FirstDisconnectRetries)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, result.action) << "Should attempt to connect";
    EXPECT_TRUE(result.should_callback_disconnected) << "Should trigger disconnect callback";
    EXPECT_EQ(1, result.callback_retry_count) << "Retry count should be 1";
    EXPECT_EQ(1, wifi_retry_get_count(&state)) << "State retry count should be 1";
    EXPECT_FALSE(wifi_retry_is_connected(&state)) << "Should not be connected";
}

/**
 * Test: Multiple disconnects increment retry count
 */
TEST(WifiRetry, MultipleDisconnects)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    for (int i = 1; i <= 3; i++) {
        wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
        
        EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, result.action) << "Should keep retrying";
        EXPECT_EQ(i, wifi_retry_get_count(&state)) << "Retry count should increment";
        EXPECT_EQ(i, result.callback_retry_count) << "Callback retry count should match";
    }
}

/**
 * Test: Max retries exceeded starts timer
 */
TEST(WifiRetry, MaxRetriesExceeded)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 3, 30000);
    
    // Use up all immediate retries
    wifi_retry_on_disconnect(&state);  // retry 1
    wifi_retry_on_disconnect(&state);  // retry 2
    wifi_retry_on_disconnect(&state);  // retry 3
    
    // Next disconnect should start timer
    wifi_retry_result_t result = wifi_retry_on_disconnect(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_FAIL, result.action) << "Should start timer after max retries";
    EXPECT_TRUE(wifi_retry_should_timer_run(&state)) << "Timer should be marked as running";
    EXPECT_TRUE(result.should_callback_disconnected) << "Should still trigger disconnect callback";
}

/**
 * Test: Connected resets retry count
 */
TEST(WifiRetry, ConnectedResetsCount)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 5, 30000);
    
    // Simulate some retries
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    EXPECT_EQ(2, wifi_retry_get_count(&state)) << "Should have 2 retries";
    
    // Connect
    wifi_retry_result_t result = wifi_retry_on_connected(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_NONE, result.action) << "No action needed";
    EXPECT_TRUE(result.should_callback_connected) << "Should trigger connected callback";
    EXPECT_EQ(0, wifi_retry_get_count(&state)) << "Retry count should reset to 0";
    EXPECT_TRUE(wifi_retry_is_connected(&state)) << "Should be connected";
}

/**
 * Test: Connected stops timer if running
 */
TEST(WifiRetry, ConnectedStopsTimer)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // Exceed max retries to start timer
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    wifi_retry_result_t timer_result = wifi_retry_on_disconnect(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_FAIL, timer_result.action) << "Timer should start";
    EXPECT_TRUE(wifi_retry_should_timer_run(&state)) << "Timer should be running";
    
    // Now connect
    wifi_retry_result_t result = wifi_retry_on_connected(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_STOP_TIMER, result.action) << "Should stop timer";
    EXPECT_FALSE(wifi_retry_should_timer_run(&state)) << "Timer should not be running";
    EXPECT_TRUE(wifi_retry_is_connected(&state)) << "Should be connected";
}

/**
 * Test: Timer expiry resets retry count and attempts connect
 */
TEST(WifiRetry, TimerExpiry)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // Exceed max retries
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    wifi_retry_on_disconnect(&state);
    
    EXPECT_EQ(2, wifi_retry_get_count(&state)) << "Should have 2 retries, we can't retry past max";
    
    // Timer expires
    wifi_retry_result_t result = wifi_retry_on_timer_expired(&state);
    
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, result.action) << "Should attempt to connect";
    EXPECT_EQ(0, wifi_retry_get_count(&state)) << "Retry count should reset to 0";
}

/**
 * Test: NULL state pointer is handled safely
 */
TEST(WifiRetry, NullStateSafe)
{
    wifi_retry_init(NULL, 5, 30000);  // Should not crash
    
    wifi_retry_result_t result = wifi_retry_on_disconnect(NULL);
    EXPECT_EQ(WIFI_RETRY_ACTION_NONE, result.action) << "Should return NONE for NULL";
    
    result = wifi_retry_on_connected(NULL);
    EXPECT_EQ(WIFI_RETRY_ACTION_NONE, result.action) << "Should return NONE for NULL";
    
    result = wifi_retry_on_timer_expired(NULL);
    EXPECT_EQ(WIFI_RETRY_ACTION_NONE, result.action) << "Should return NONE for NULL";
    
    EXPECT_EQ(0, wifi_retry_get_count(NULL)) << "Should return 0 for NULL";
    EXPECT_FALSE(wifi_retry_is_connected(NULL)) << "Should return false for NULL";
    EXPECT_FALSE(wifi_retry_should_timer_run(NULL)) << "Should return false for NULL";
}

/**
 * Test: Full retry cycle - disconnect, retry, fail, timer, reconnect
 */
TEST(WifiRetry, FullRetryCycle)
{
    wifi_retry_state_t state;
    wifi_retry_init(&state, 2, 30000);
    
    // First disconnect - immediate retry
    wifi_retry_result_t r1 = wifi_retry_on_disconnect(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, r1.action) << "First retry";
    EXPECT_EQ(1, wifi_retry_get_count(&state)) << "Count = 1";
    
    // Second disconnect - immediate retry
    wifi_retry_result_t r2 = wifi_retry_on_disconnect(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, r2.action) << "Second retry";
    EXPECT_EQ(2, wifi_retry_get_count(&state)) << "Count = 2";
    
    // Third disconnect - max exceeded, start timer
    wifi_retry_result_t r3 = wifi_retry_on_disconnect(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_FAIL, r3.action) << "Start timer";
    EXPECT_TRUE(wifi_retry_should_timer_run(&state)) << "Timer running";
    
    // Timer expires - reset and retry
    wifi_retry_result_t r4 = wifi_retry_on_timer_expired(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, r4.action) << "Retry after timer";
    EXPECT_EQ(0, wifi_retry_get_count(&state)) << "Count reset";
    
    // Successfully connect
    wifi_retry_result_t r5 = wifi_retry_on_connected(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_STOP_TIMER, r5.action) << "Stop timer";
    EXPECT_TRUE(wifi_retry_is_connected(&state)) << "Connected";
    EXPECT_FALSE(wifi_retry_should_timer_run(&state)) << "Timer stopped";
}

/**
 * Test: Custom retry configuration
 */
TEST(WifiRetry, CustomConfiguration)
{
    wifi_retry_state_t state;
    
    // Test with 1 retry, 1 minute interval
    wifi_retry_init(&state, 1, 60000);
    
    EXPECT_EQ(1, state.max_retries) << "Max retries = 1";
    EXPECT_EQ(60000, state.retry_interval_ms) << "Interval = 60000ms";
    
    // First disconnect - retry
    wifi_retry_result_t r1 = wifi_retry_on_disconnect(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_CONNECT, r1.action) << "First retry";
    
    // Second disconnect - already at max, start timer
    wifi_retry_result_t r2 = wifi_retry_on_disconnect(&state);
    EXPECT_EQ(WIFI_RETRY_ACTION_FAIL, r2.action) << "Exceeded after 1 retry";
}
