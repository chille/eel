include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/math)

# math module
set(sources ${sources}
	eel_math.c
)

add_library(libeelmodulemath ${sources})

if(UNIX)
	target_link_libraries(libeelmodulemath m)
endif(UNIX)

if(WIN32)
	target_link_libraries(libeelmodulemath winmm)
endif(WIN32)

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmodulemath PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmodulemath PROPERTIES OUTPUT_NAME "eelmodulemath")

install(TARGETS libeelmodulemath DESTINATION lib)
