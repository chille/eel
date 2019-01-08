/*
 * 
 * Compile and run with:
 *   export LD_LIBRARY_PATH=/path/to/libeel.so
 *   gcc main.c -I ../src/core -I ../src/core/eelc -I ../include/ ../src/core/libeel.so && ./a.out
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
} ClassData;


EEL_object *c;
int type;


static EEL_xno test_construct(EEL_vm *vm, EEL_types type, EEL_value *initv, int initc, EEL_value *result)
{
	// This works, but use a global pointer to the classdef object
//	ClassData *cd = (ClassData*)o2EEL_classdef(c)->classdata;

	// This works, but require that I know the typeid for the class
	EEL_state *es = VMP->state;
	EEL_classdef *cd1 = o2EEL_classdef(es->classes[type]);
	ClassData *cd = (ClassData*)cd1->classdata;

	// Gives a pointer to the wrong address
//	EEL_object *eo = eel_current_function(vm);
//	ClassData *cd = (ClassData*)eel_get_classdata(eo);

	// Gives a null pointer
//	ClassData *cd = (ClassData*)eel_get_classdata(c);

	printf("Ptr classdata: %p\n", cd);
	printf("classdata->counter: %i\n", cd->counter);
}


int main(void)
{
	EEL_vm *vm = eel_open(0, NULL);

	EEL_object *module = eel_create_module(vm, "testmodule", NULL, NULL);
	c = eel_export_class(module, "TestClass", EEL_COBJECT, test_construct, NULL, NULL);

	ClassData *cd = (ClassData*)eel_malloc(vm, sizeof(ClassData));
	eel_set_classdata(c, cd);
	cd->counter = 123;
	printf("Ptr classdata: %p\n", cd);

//	type = c->type; // Gives me 9
	type = eel_class_typeid(c); // Gives me 28, which seems to be correct
	printf("Type: %i\n", type);

	EEL_object *eelscript = eel_load(vm, "test.eel", 0);
	eel_callnf(vm, eelscript, "main", "");

	return 0;
}