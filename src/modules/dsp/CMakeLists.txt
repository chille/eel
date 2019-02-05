include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/core/eelc)
include_directories(${EEL_SOURCE_DIR}/src/modules/dsp)

# dsp module
set(sources ${sources}
	eel_dsp.c
	kfc.c
	kiss_fft.c
	kiss_fftr.c
)

add_library(libeelmoduledsp ${sources})
