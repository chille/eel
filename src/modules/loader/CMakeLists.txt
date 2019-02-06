include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/compiler)
include_directories(${EEL_SOURCE_DIR}/src/modules/loader)

# loader module
set(sources ${sources}
	eel_loader.c
)
add_library(libeelmoduleloader ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmoduleloader PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmoduleloader PROPERTIES OUTPUT_NAME "eelmoduleloader")

install(TARGETS libeelmoduleloader DESTINATION lib)
