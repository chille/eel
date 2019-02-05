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