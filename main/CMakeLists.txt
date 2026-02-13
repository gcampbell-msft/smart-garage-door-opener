set(MAIN_SRCS
    "smart_garage_door.c"
    "garage_state_machine.c"
    "wifi_impl.c"
    "wifi_hal.c"
    "wifi_retry_manager.c"
    "mqtt_impl.c"
    "mqtt_hal.c"
    "mqtt_retry_manager.c"
)

set(INCLUDE_DIRS
    "."
    "interfaces"
    "credentials")

idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS ${INCLUDE_DIRS})