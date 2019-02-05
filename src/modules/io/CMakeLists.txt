include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/io)

# io module
set(sources ${sources}
	eel_io.c
)
add_library(libeelmoduleio ${sources})
