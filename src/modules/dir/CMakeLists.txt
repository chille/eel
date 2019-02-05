include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/modules/dir)

# dir module
set(sources ${sources}
	eel_dir.c
)

add_library(libeelmoduledir ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmoduledir PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmoduledir PROPERTIES OUTPUT_NAME "eelmoduledir")

install(TARGETS libeelmoduledir DESTINATION lib)
