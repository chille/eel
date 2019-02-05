include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/system)

# system module
set(sources ${sources}
	eel_system.c
)

add_library(libeelmodulesystem ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmodulesystem PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmodulesystem PROPERTIES OUTPUT_NAME "eelmodulesystem")

install(TARGETS libeelmodulesystem DESTINATION lib)
