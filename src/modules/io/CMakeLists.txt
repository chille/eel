include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/io)

# io module
set(sources ${sources}
	eel_io.c
)
add_library(libeelmoduleio ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmoduleio PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmoduleio PROPERTIES OUTPUT_NAME "eelmoduleio")

install(TARGETS libeelmoduleio DESTINATION lib)
