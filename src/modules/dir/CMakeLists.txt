include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/modules/dir)

# dir module
set(sources ${sources}
	eel_dir.c
)

add_library(libeelmoduledir ${sources})
