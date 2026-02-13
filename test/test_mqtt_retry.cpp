/**
 * @file test_mqtt_retry.cpp
 * @brief Unit tests for MQTT retry manager using Google Test
 * 
 * Tests the pure C MQTT retry logic without any ESP SDK or hardware dependencies.
 */

#include <gtest/gtest.h>

extern "C" {
#include "mqtt_retry_manager.h"
}

// ========== Test Cases ==========

/**
 * Test: Initialize with auto-reconnect enabled
 */
TEST(MqttRetry, InitWithAutoReconnect)
{
    mqtt_retry_state_t state;
    
    mqtt_retry_init(&state, true);
    
    EXPECT_FALSE(mqtt_retry_is_connected(&state)) << "Should not be connected initially";
    EXPECT_EQ(0, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should be 0";
    EXPECT_TRUE(state.should_reconnect) << "Auto-reconnect should be enabled";
}

/**
 * Test: Initialize with auto-reconnect disabled
 */
TEST(MqttRetry, InitWithoutAutoReconnect)
{
    mqtt_retry_state_t state;
    
    mqtt_retry_init(&state, false);
    
    EXPECT_FALSE(mqtt_retry_is_connected(&state)) << "Should not be connected initially";
    EXPECT_EQ(0, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should be 0";
    EXPECT_FALSE(state.should_reconnect) << "Auto-reconnect should be disabled";
}

/**
 * Test: Disconnect with auto-reconnect triggers reconnect action
 */
TEST(MqttRetry, DisconnectWithAutoReconnect)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
    
    EXPECT_EQ(MQTT_RETRY_ACTION_RECONNECT, result.action) << "Should attempt to reconnect";
    EXPECT_TRUE(result.should_callback_disconnected) << "Should trigger disconnect callback";
    EXPECT_EQ(1, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should be 1";
    EXPECT_FALSE(mqtt_retry_is_connected(&state)) << "Should not be connected";
}

/**
 * Test: Disconnect without auto-reconnect does nothing
 */
TEST(MqttRetry, DisconnectWithoutAutoReconnect)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, false);
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
    
    EXPECT_EQ(MQTT_RETRY_ACTION_NONE, result.action) << "Should not attempt to reconnect";
    EXPECT_TRUE(result.should_callback_disconnected) << "Should still trigger disconnect callback";
    EXPECT_EQ(1, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should be 1";
}

/**
 * Test: Multiple disconnects increment counter
 */
TEST(MqttRetry, MultipleDisconnects)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    for (int i = 1; i <= 5; i++) {
        mqtt_retry_result_t result = mqtt_retry_on_disconnect(&state);
        
        EXPECT_EQ(MQTT_RETRY_ACTION_RECONNECT, result.action) << "Should keep trying to reconnect";
        EXPECT_EQ(i, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should increment";
    }
}

/**
 * Test: Connected updates state correctly
 */
TEST(MqttRetry, Connected)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    // Simulate some disconnects first
    mqtt_retry_on_disconnect(&state);
    mqtt_retry_on_disconnect(&state);
    EXPECT_EQ(2, mqtt_retry_get_disconnect_count(&state)) << "Should have 2 disconnects";
    
    // Connect
    mqtt_retry_result_t result = mqtt_retry_on_connected(&state);
    
    EXPECT_EQ(MQTT_RETRY_ACTION_NONE, result.action) << "No action needed";
    EXPECT_TRUE(result.should_callback_connected) << "Should trigger connected callback";
    EXPECT_TRUE(mqtt_retry_is_connected(&state)) << "Should be connected";
    // Note: disconnect count is NOT reset - it's cumulative
    EXPECT_EQ(2, mqtt_retry_get_disconnect_count(&state)) << "Disconnect count should persist";
}

/**
 * Test: NULL state pointer is handled safely
 */
TEST(MqttRetry, NullStateSafe)
{
    mqtt_retry_init(NULL, true);  // Should not crash
    
    mqtt_retry_result_t result = mqtt_retry_on_disconnect(NULL);
    EXPECT_EQ(MQTT_RETRY_ACTION_NONE, result.action) << "Should return NONE for NULL";
    
    result = mqtt_retry_on_connected(NULL);
    EXPECT_EQ(MQTT_RETRY_ACTION_NONE, result.action) << "Should return NONE for NULL";
    
    EXPECT_EQ(0, mqtt_retry_get_disconnect_count(NULL)) << "Should return 0 for NULL";
    EXPECT_FALSE(mqtt_retry_is_connected(NULL)) << "Should return false for NULL";
}

/**
 * Test: Full connection cycle
 */
TEST(MqttRetry, FullConnectionCycle)
{
    mqtt_retry_state_t state;
    mqtt_retry_init(&state, true);
    
    // Initial state - not connected
    EXPECT_FALSE(mqtt_retry_is_connected(&state)) << "Initially not connected";
    EXPECT_EQ(0, mqtt_retry_get_disconnect_count(&state)) << "No disconnects yet";
    
    // Connect
    mqtt_retry_result_t r1 = mqtt_retry_on_connected(&state);
    EXPECT_TRUE(mqtt_retry_is_connected(&state)) << "Should be connected";
    EXPECT_TRUE(r1.should_callback_connected) << "Trigger connected callback";
    
    // Disconnect - should auto-reconnect
    mqtt_retry_result_t r2 = mqtt_retry_on_disconnect(&state);
    EXPECT_EQ(MQTT_RETRY_ACTION_RECONNECT, r2.action) << "Should reconnect";
    EXPECT_FALSE(mqtt_retry_is_connected(&state)) << "Should be disconnected";
    EXPECT_EQ(1, mqtt_retry_get_disconnect_count(&state)) << "One disconnect";
    
    // Reconnect
    mqtt_retry_result_t r3 = mqtt_retry_on_connected(&state);
    EXPECT_TRUE(mqtt_retry_is_connected(&state)) << "Connected again";
    
    // Disconnect again
    mqtt_retry_result_t r4 = mqtt_retry_on_disconnect(&state);
    EXPECT_EQ(MQTT_RETRY_ACTION_RECONNECT, r4.action) << "Should reconnect again";
    EXPECT_EQ(2, mqtt_retry_get_disconnect_count(&state)) << "Two disconnects total";
}

/**
 * Test: Disconnect count tracks all disconnections
 */
TEST(MqttRetry, DisconnectCountTracking)
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
    
    EXPECT_EQ(3, mqtt_retry_get_disconnect_count(&state)) << "Should track all disconnects";
}

/**
 * Test: Auto-reconnect can be enabled/disabled
 */
TEST(MqttRetry, AutoReconnectToggle)
{
    mqtt_retry_state_t state;
    
    // Start with auto-reconnect enabled
    mqtt_retry_init(&state, true);
    mqtt_retry_result_t r1 = mqtt_retry_on_disconnect(&state);
    EXPECT_EQ(MQTT_RETRY_ACTION_RECONNECT, r1.action) << "Should reconnect";
    
    // Disable auto-reconnect (by reinitializing)
    mqtt_retry_init(&state, false);
    mqtt_retry_result_t r2 = mqtt_retry_on_disconnect(&state);
    EXPECT_EQ(MQTT_RETRY_ACTION_NONE, r2.action) << "Should not reconnect";
}
