/**
 * @file mqtt_retry_manager.c
 * @brief MQTT retry state machine implementation
 */

#include "mqtt_retry_manager.h"
#include <string.h>

void mqtt_retry_init(mqtt_retry_state_t* state, bool auto_reconnect)
{
    if (state == NULL) return;
    
    state->should_reconnect = auto_reconnect;
    state->is_connected = false;
    state->disconnect_count = 0;
}

mqtt_retry_result_t mqtt_retry_on_disconnect(mqtt_retry_state_t* state)
{
    mqtt_retry_result_t result = {0};
    
    if (state == NULL) {
        return result;
    }
    
    state->is_connected = false;
    state->disconnect_count++;
    
    result.should_callback_disconnected = true;
    
    if (state->should_reconnect) {
        result.action = MQTT_RETRY_ACTION_RECONNECT;
    } else {
        result.action = MQTT_RETRY_ACTION_NONE;
    }
    
    return result;
}

mqtt_retry_result_t mqtt_retry_on_connected(mqtt_retry_state_t* state)
{
    mqtt_retry_result_t result = {0};
    
    if (state == NULL) {
        return result;
    }
    
    state->is_connected = true;
    result.action = MQTT_RETRY_ACTION_NONE;
    result.should_callback_connected = true;
    
    return result;
}

int mqtt_retry_get_disconnect_count(const mqtt_retry_state_t* state)
{
    return (state != NULL) ? state->disconnect_count : 0;
}

bool mqtt_retry_is_connected(const mqtt_retry_state_t* state)
{
    return (state != NULL) ? state->is_connected : false;
}
