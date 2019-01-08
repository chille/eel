/*
 * 
 * Compile and run with:
 *   export LD_LIBRARY_PATH=/path/to/libeel.so
 *   gcc main2.c -I ../src/core -I ../src/core/eelc -I ../include/ ../src/core/libeel.so && ./a.out
*/

#include <stdio.h>
#include <assert.h>
#include <EEL.h>
#include "../../eel/src/core/e_state.h" // EEL_state
#include "../../eel/src/core/e_vm.h" // VMP
#include "../../eel/src/core/e_class.h" // EEL_classdef


typedef struct
{
	int counter;
} D_moduledata;


static EEL_xno test_construct(EEL_vm *vm, EEL_types type, EEL_value *initv, int initc, EEL_value *result)
{
	D_moduledata *md = (D_moduledata *)eel_get_current_moduledata(vm);
	printf("Ptr moduledata: %p\n", md);
	assert(md != NULL);
}


int main(void)
{
	EEL_vm *vm = eel_open(0, NULL);

	D_moduledata *md = (D_moduledata *)eel_malloc(vm, sizeof(D_moduledata));
	printf("Ptr moduledata: %p\n", md);
	md->counter = 123;
	EEL_object *module = eel_create_module(vm, "testmodule", NULL, md);
	eel_export_class(module, "TestClass", EEL_COBJECT, test_construct, NULL, NULL);

	EEL_object *eelscript = eel_load(vm, "test.eel", 0);
	eel_callnf(vm, eelscript, "main", "");

	return 0;
}