# For stand-alone build, linking to the system EEL library
#find_package(EEL REQUIRED)
#include_directories(${EEL_INCLUDE_DIR})

# For local build, not relying on an installed library
include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/modules/dir)
include_directories(${EEL_SOURCE_DIR}/src/modules/dsp)
include_directories(${EEL_SOURCE_DIR}/src/modules/io)
include_directories(${EEL_SOURCE_DIR}/src/modules/loader)
include_directories(${EEL_SOURCE_DIR}/src/modules/math)
include_directories(${EEL_SOURCE_DIR}/src/modules/system)
link_directories(${EEL_BINARY_DIR})

set(EEL_LIBRARY libeelcore)

add_executable(eeltest WIN32 MACOSX_BUNDLE eeltest.c)
target_link_libraries(eeltest ${EEL_LIBRARY})
target_link_libraries(eeltest libeelcompiler)
target_link_libraries(eeltest libeelmoduledir)
target_link_libraries(eeltest libeelmoduledsp)
target_link_libraries(eeltest libeelmoduleio)
target_link_libraries(eeltest libeelmoduleloader)
target_link_libraries(eeltest libeelmodulemath)
target_link_libraries(eeltest libeelmodulesystem)
if(WIN32)
	target_link_libraries(eeltest -lwinmm -ldxguid)
	# FIXME: Why do we have to pull all these in manually now...?
	target_link_libraries(eeltest -lws2_32 -liphlpapi -ljpeg -lz)
endif(WIN32)
