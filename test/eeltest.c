/*
---------------------------------------------------------------------------
	Minimal client that loads and runs a test script.
---------------------------------------------------------------------------
 * David Olofson 2002, 2004, 2005, 2007, 2014
 *
 * This code is in the public domain. NO WARRANTY!
 */

#include <stdio.h>
#include "EEL.h"
#include "eel_io.h"
#include "eel_system.h"
#include "eel_math.h"
#include "eel_dir.h"
#include "eel_dsp.h"
#include "eel_loader.h"

int main(int argc, const char *argv[])
{
	EEL_object *m;
	EEL_vm *vm = eel_open(argc, argv);
	if(!vm)
	{
		fprintf(stderr, "Could not initialize EEL!\n");
		return 1;
	}

	/* Install system module */
	if(eel_system_init(vm, argc, argv))
	{
		fprintf(stderr, "Could not initialize built-in system module!\n");
		return 1;
	}

	/* Install io module */
	if(eel_io_init(vm))
	{
		fprintf(stderr, "Could not initialize built-in io module!\n");
		return 1;
	}

	/* Install math module */
	if(eel_math_init(vm))
	{
		fprintf(stderr, "Could not initialize built-in math module!\n");
		return 1;
	}

	/* Install directory module */
	if(eel_dir_init(vm))
	{
		fprintf(stderr, "Could not initialize built-in directory module!\n");
		return 1;
	}

	/* Install DSP module */
	if(eel_dsp_init(vm))
	{
		fprintf(stderr, "Could not initialize built-in DSP module!\n");
		return 1;
	}
 
	/* Install loader module */
	if(eel_loader_init(vm))
	{
		fprintf(stderr, "Could not initialize built-in loader module!\n");
		return 1;
	}

	m = eel_load(vm, "test.eel", 0);
	if(!m)
	{
		fprintf(stderr, "Could not load script!\n");
		return 1;
	}
	eel_callnf(vm, m, "main", "*");
	eel_disown(m);
	eel_close(vm);
	return 0;
}
