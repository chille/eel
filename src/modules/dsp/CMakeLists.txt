include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/compiler)
include_directories(${EEL_SOURCE_DIR}/src/modules/dsp)

# dsp module
set(sources ${sources}
	eel_dsp.c
	kfc.c
	kiss_fft.c
	kiss_fftr.c
)

add_library(libeelmoduledsp ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelmoduledsp PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelmoduledsp PROPERTIES OUTPUT_NAME "eelmoduledsp")

install(TARGETS libeelmoduledsp DESTINATION lib)
