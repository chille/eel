/*
---------------------------------------------------------------------------
	eel_loader.c - EEL module for dynamic loading of code
---------------------------------------------------------------------------
 * Copyright 2002-2014, 2019 David Olofson
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "eel_loader.h"
#include "ec_symtab.h"
#include "e_function.h"
#include "e_state.h"
#include "e_object.h"

/*
 * NOTE: This looks at the EEL_SF_NOSHARED flag, but all other flags are
 *       ignored! That is, if a module is found, there is no checking whether
 *       or not it was loaded with matching flags.
 */
static EEL_xno loader__get_loaded_module(EEL_vm *vm)
{
	EEL_object *m;
	EEL_value *args = vm->heap + vm->argv;
	const char *name = eel_v2s(args + 0);
	unsigned flags = (unsigned)eel_v2l(args + 1);
	if(!name)
		return EEL_XNEEDSTRING;
	if(!(flags & EEL_SF_ALLOWSHARED))
		return EEL_XWRONGINDEX;	/* No shared modules, please! */
	if(!(m = eel_get_loaded_module(vm, name)))
		return EEL_XWRONGINDEX;
	eel_o2v(vm->heap + vm->resv, m);
	return 0;
}


static EEL_xno loader__load_binary_module(EEL_vm *vm)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno loader_getmt(EEL_vm *vm)
{
	vm->heap[vm->resv].classid = EEL_COBJREF;
	vm->heap[vm->resv].objref.v = VMP->state->modules;
	eel_o_own(VMP->state->modules);
	return 0;
}


static EEL_xno loader_clean_modules(EEL_vm *vm)
{
	eel_clean_modules(vm);
	return 0;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

#ifdef	EEL_USE_EELBIL
static char builtin_eel2[] =
#include "builtin-loader.c"
;
#endif


EEL_xno eel_loader_init(EEL_vm *vm)
{
	EEL_object *m;
#ifdef	EEL_USE_EELBIL
	EEL_state *es = VMP->state;
#endif

	m = eel_create_module(vm, "loader_c", NULL, NULL);
	if(!m)
	{
		return EEL_XMODULEINIT;
	}

	/* Run-time EEL module management */
	eel_export_cfunction(m, 1, "__get_loaded_module", 2, 0, 0,
			loader__get_loaded_module);
	eel_export_cfunction(m, 1, "__load_binary_module", 2, 0, 0,
			loader__load_binary_module);
	eel_export_cfunction(m, 1, "__modules", 0, 0, 0, loader_getmt);
	eel_export_cfunction(m, 0, "__clean_modules", 0, 0, 0,
			loader_clean_modules);

#ifdef	EEL_USE_EELBIL
	/* Built-in EEL library */
	es->loader = eel_load_buffer(vm, builtin_eel2, strlen(builtin_eel2), 0);
	if(!es->loader)
	{
		eel_o_disown_nz(m);
		return EEL_XMODULEINIT;
	}
	SETNAME(es->loader, "EEL Built-in Library Module");

	eel_try(es)
		eel_s_get_exports(es->root_symtab, es->loader);
	eel_except
	{
		eel_o_disown(&es->loader);
		eel_o_disown_nz(m);
		return EEL_XMODULEINIT;
	}
#endif
	eel_o_disown_nz(m);
	return 0;
}
