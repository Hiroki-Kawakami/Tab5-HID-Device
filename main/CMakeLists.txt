file(GLOB_RECURSE MAIN_SRCS "*.c")
idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS "." "hid_device")
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE ON)
