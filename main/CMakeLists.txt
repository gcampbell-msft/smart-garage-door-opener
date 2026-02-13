set(MAIN_SRCS
    "smart_garage_door.c"
    "garage_state_machine.c"
    "wifi_impl.c"
    "mqtt_impl.c"
)

set(INCLUDE_DIRS
    "."
    "interfaces")

idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS ${INCLUDE_DIRS})