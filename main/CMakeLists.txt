set(MAIN_SRCS
    "smart_garage_door.c"
    "garage_state_machine.c"
)

idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS ".")