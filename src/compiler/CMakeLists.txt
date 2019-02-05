include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/core)
include_directories(${EEL_SOURCE_DIR}/src/compiler)

# Compiler files
set(sources ${sources}
	ec_bio.c
	ec_coder.c
	ec_context.c
	ec_event.c
	ec_lexer.c
	ec_manip.c
	ec_mlist.c
	ec_parser.c
	ec_symtab.c
	ec_optimizer.c
)

add_library(libeelcompiler ${sources})

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelcompiler PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelcompiler PROPERTIES OUTPUT_NAME "eelcompiler")

install(TARGETS libeelcompiler DESTINATION lib)
