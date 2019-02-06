/*
 * 
 * Compile and run with:
 *   export LD_LIBRARY_PATH=../../../build/src/core/
 *   gcc test1.c -I ../../../src/core -I ../../../src/core/eelc -I ../../../include/ ../../../build/src/core/libeel.so && ./a.out
*/

#include <stdio.h>
#include <assert.h>
#include <EEL.h>
#include <stdlib.h> // exit()

typedef struct
{
	int counter;
} ClassData;

static EEL_xno test_constructor(EEL_vm *vm, EEL_classes type, EEL_value *initv, int initc, EEL_value *result)
{
/*
	// This works, but use a global pointer to the classdef object
//	ClassData *cd = (ClassData*)o2EEL_classdef(c)->classdata;

	// This works, but is not using eel_get_classdata()
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
*/
	printf("test_constructor()\n");

	ClassData *classdata = (ClassData*)eel_get_classdata(vm, type);
	printf("  Pointer to classdata: %p\n", classdata);
	printf("  Classid: %i\n", type);
	printf("  Counter: %i\n", classdata->counter);
	return EEL_XOK;
}

EEL_xno test_destructor(EEL_object *eo)
{
	printf("test_destructor()\n");
	return EEL_XOK;
}

int main(void)
{
	EEL_vm *vm = eel_open(0, NULL);

	// Create a new module
	EEL_object *module = eel_create_module(vm, "testmodule", NULL, NULL);

	// Create a new class
	EEL_object *c = eel_export_class(module, "TestClass", EEL_COBJECT, test_constructor, test_destructor, NULL);

	// Attach class data to our new class
	ClassData *cd = (ClassData*)eel_malloc(vm, sizeof(ClassData));
	eel_set_classdata(vm, eel_class_cid(c), cd);
	cd->counter = 123;
	printf("Ptr classdata: %p\n", cd);
	printf("Classid: %i\n", c->classid);
	printf("Classid: %i\n", eel_class_cid(c));

	EEL_object *eelscript = eel_load(vm, "test1.eel", 0);
	EEL_xno result = eel_callnf(vm, eelscript, "main", "");
	if(result != EEL_XOK)
	{
		eel_perror(vm, 1);
		exit(0);
	}

	return 0;
}