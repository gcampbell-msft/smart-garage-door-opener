set(MAIN_SRCS
    "smart_garage_door.c"
    "garage_state_machine.c"
    "wifi/wifi_impl.c"
    "wifi/wifi_hal.c"
    "wifi/wifi_retry_manager.c"
    "mqtt/mqtt_impl.c"
    "mqtt/mqtt_hal.c"
    "mqtt/mqtt_retry_manager.c"
)

set(INCLUDE_DIRS
    "."
    "include"
    "include/mqtt"
    "include/wifi"
    "include/credentials")

if (TEST_MODE)
    add_compile_definitions(TEST_MODE=1)
endif()

idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS ${INCLUDE_DIRS})