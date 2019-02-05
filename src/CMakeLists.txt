
add_subdirectory(compiler)
add_subdirectory(core)
add_subdirectory(modules)
if(BUILD_EELIUM)
	add_subdirectory(eelium)
endif(BUILD_EELIUM)
add_subdirectory(executive)
