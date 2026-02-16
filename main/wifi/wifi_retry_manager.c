/**
 * @file wifi_retry_manager.c
 * @brief WiFi retry state machine implementation
 */

#include "wifi_retry_manager.h"
#include <string.h>

void wifi_retry_init(wifi_retry_state_t* state, int max_retries, int retry_interval_ms)
{
    if (state == NULL) return;
    
    memset(state, 0, sizeof(wifi_retry_state_t));
    state->max_retries = max_retries;
    state->retry_interval_ms = retry_interval_ms;
    state->is_connected = false;
    state->timer_should_be_running = false;
}

wifi_retry_result_t wifi_retry_on_disconnect(wifi_retry_state_t* state)
{
    wifi_retry_result_t result = {0};
    
    if (state == NULL) {
        return result;
    }
    
    state->is_connected = false;
    
    if (state->retry_count < state->max_retries) {
        // Still have immediate retries left
        state->retry_count++;
        result.action = WIFI_RETRY_ACTION_CONNECT;
        result.should_callback_disconnected = true;
        result.callback_retry_count = state->retry_count;
    } else {
        // Max immediate retries exceeded - start long interval timer
        result.action = WIFI_RETRY_ACTION_FAIL;
        result.should_callback_disconnected = true;
        result.callback_retry_count = state->retry_count;
        state->timer_should_be_running = true;
    }
    
    return result;
}

wifi_retry_result_t wifi_retry_on_connected(wifi_retry_state_t* state)
{
    wifi_retry_result_t result = {0};
    
    if (state == NULL) {
        return result;
    }
    
    state->is_connected = true;
    state->retry_count = 0;
    
    if (state->timer_should_be_running) {
        result.action = WIFI_RETRY_ACTION_STOP_TIMER;
        state->timer_should_be_running = false;
    } else {
        result.action = WIFI_RETRY_ACTION_NONE;
    }
    
    result.should_callback_connected = true;
    
    return result;
}

wifi_retry_result_t wifi_retry_on_timer_expired(wifi_retry_state_t* state)
{
    wifi_retry_result_t result = {0};
    
    if (state == NULL) {
        return result;
    }
    
    // Timer expired - reset retry count and try again
    state->retry_count = 0;
    result.action = WIFI_RETRY_ACTION_CONNECT;
    
    return result;
}

int wifi_retry_get_count(const wifi_retry_state_t* state)
{
    return (state != NULL) ? state->retry_count : 0;
}

bool wifi_retry_is_connected(const wifi_retry_state_t* state)
{
    return (state != NULL) ? state->is_connected : false;
}

bool wifi_retry_should_timer_run(const wifi_retry_state_t* state)
{
    return (state != NULL) ? state->timer_should_be_running : false;
}
