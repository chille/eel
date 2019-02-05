include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/system)

# system module
set(sources ${sources}
	eel_system.c
)

add_library(libeelmodulesystem ${sources})
