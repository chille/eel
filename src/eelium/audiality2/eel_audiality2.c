/*
---------------------------------------------------------------------------
	eel_audiality2.h - EEL Audiality 2 binding
---------------------------------------------------------------------------
 * Copyright 2011-2012, 2014, 2016-2017, 2019 David Olofson
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

#include <string.h>
#include <stdlib.h>
#include "eel_audiality2.h"
#include "a2_vm.h"


typedef struct
{
	/* Class type IDs */
	int		iface_cid;

	EEL_object	*ifacefields;
} A2_moduledata;

/*FIXME: Can't get moduledata from within constructors... */
static A2_moduledata a2_md;


static int ea2_error_base;

static inline EEL_xno ea2_translate_error(A2_errors a2e)
{
	return (EEL_xno)a2e + ea2_error_base;
}


static inline EEL_xno get_raw_data_o(EEL_object *o, A2_sampleformats *fmt,
		void **data, unsigned *size, unsigned *itemsize)
{
	*size = eel_length(o);
	*data = eel_rawdata(o);
	if(!*data)
		return EEL_XWRONGTYPE;
	switch(o->classid)
	{
	  case EEL_CSTRING:
	  case EEL_CDSTRING:
	  case EEL_CVECTOR_S8:
		*fmt = A2_I8;
		if(itemsize)
			*itemsize = 1;
		return 0;
	  case EEL_CVECTOR_S16:
		*fmt = A2_I16;
		*size *= 2;
		if(itemsize)
			*itemsize = 2;
		return 0;
	  case EEL_CVECTOR_S32:
		*fmt = A2_I24;
		*size *= 4;
		if(itemsize)
			*itemsize = 4;
		return 0;
	  case EEL_CVECTOR_F:
		*fmt = A2_F32;
		*size *= 4;
		if(itemsize)
			*itemsize = 4;
		return 0;
	  default:
		return EEL_XWRONGTYPE;
	}
}

/* Check type, calculate size and get raw data pointer */
static EEL_xno get_raw_data(EEL_value *v, A2_sampleformats *fmt,
		void **data, unsigned *size, unsigned *itemsize)
{
	if(!EEL_IS_OBJREF(v->classid))
		return EEL_XNEEDOBJECT;
	return get_raw_data_o(v->objref.v, fmt, data, size, itemsize);
}


/*----------------------------------------------------------
	Audiality 2 interface class
----------------------------------------------------------*/

/*
 * Master interface:
 *	initv [sample_rate, buffer_size, channels, flags]
 *
 * Substate:
 *	initv [parent_state, sample_rate, buffer_size, channels, flags]
 */
static EEL_xno a2s_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	EA2_iface *ea2i;
	A2_interface *iface;
	EEL_value v;
	EEL_xno xno;
	EEL_object *eo;
	A2_config *cfg;
	EA2_iface *parentiface = NULL;
	if(initc && (EEL_CLASS(initv) == a2_md.iface_cid))
	{
		/* Grab the parent iface and skip that initializer element! */
		parentiface = o2EA2_iface(initv->objref.v);
		++initv;
		--initc;
	}
	cfg = a2_OpenConfig(
		initc >= 1 ? eel_v2l(initv) : -1,	/* sample rate */
		initc >= 2 ? eel_v2l(initv + 1) : -1,	/* buffering */
		initc >= 3 ? eel_v2l(initv + 2) : -1,	/* channels */
		initc >= 4 ? eel_v2l(initv + 3) : -1	/* flags */
	);
	if(!cfg)
		return EEL_XDEVICEOPEN;
	if(initc >= 5)
	{
		const char *adname = eel_v2s(initv + 4);
		if(adname)
		{
			A2_errors ae;
			A2_driver *ad = a2_NewDriver(A2_AUDIODRIVER, adname);
			if(!ad)
			{
				a2_CloseConfig(cfg);
				return EEL_XNOTFOUND;
			}
			if((ae = a2_AddDriver(cfg, ad)))
			{
				a2_CloseConfig(cfg);
				return ea2_translate_error(ae);
			}
		}
	}
	/* Hand it over to the iface, so we don't have to keep track of it! */
	cfg->flags |= A2_AUTOCLOSE;
	if(parentiface)
	{
		if(!(iface = a2_SubState(parentiface->iface, cfg)))
			return EEL_XDEVICEOPEN;
	}
	else
	{
		if(!(iface = a2_Open(cfg)))
			return EEL_XDEVICEOPEN;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(EA2_iface), cid)))
	{
		a2_Close(iface);
		return EEL_XMEMORY;
	}
	ea2i = o2EA2_iface(eo);
	memset(ea2i, 0, sizeof(EA2_iface));
	ea2i->iface = iface;
	if((xno = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v)))
	{
		eel_o_free(eo);
		a2_Close(iface);
		return xno;
	}
	ea2i->table = eel_v2o(&v);
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno a2s_destruct(EEL_object *eo)
{
	EA2_iface *ea2i = o2EA2_iface(eo);
	a2_Close(ea2i->iface);
	ea2i->iface = NULL;
	eel_disown(ea2i->table);
	ea2i->table = NULL;
	return 0;
}


static EEL_xno a2s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EA2_iface *ea2i = o2EA2_iface(eo);
	/* Passive content only for now, so we just read the table directly! */
	if(eel_table_get(a2_md.ifacefields, op1, op2) != EEL_XOK)
	{
		/* No hit! Fall through to extension table. */
		EEL_xno x = eel_table_get(ea2i->table, op1, op2);
		if(x)
			return x;
		eel_v_own(op2);
		return 0;
	}
	eel_v_own(op2);
	return 0;
}


static EEL_xno a2s_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EA2_iface *ea2i = o2EA2_iface(eo);
	if(eel_table_get(a2_md.ifacefields, op1, &v) == EEL_XOK)
		return EEL_XCANTWRITE;
	return eel_o_metamethod(ea2i->table, EEL_MM_SETINDEX, op1, op2);
}


/*---------------------------------------------------------
	Error handling
---------------------------------------------------------*/

static EEL_xno ea2_LastError(EEL_vm *vm)
{
	if(vm->argc >= 1)
	{
		EEL_value *args = vm->heap + vm->argv;
		EA2_iface *ea2i;
		if(EEL_CLASS(vm->heap + vm->argv) != a2_md.iface_cid)
			return EEL_XWRONGTYPE;
		ea2i = o2EA2_iface(args->objref.v);
		eel_l2v(vm->heap + vm->resv, a2_LastRTError(ea2i->iface));
	}
	else
		eel_l2v(vm->heap + vm->resv, a2_LastError());
	return 0;
}


static EEL_xno ea2_ErrorString(EEL_vm *vm)
{
	eel_s2v(vm, vm->heap + vm->resv, a2_ErrorString(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


static EEL_xno ea2_ErrorName(EEL_vm *vm)
{
	eel_s2v(vm, vm->heap + vm->resv, a2_ErrorName(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


static EEL_xno ea2_ErrorDescription(EEL_vm *vm)
{
	eel_s2v(vm, vm->heap + vm->resv, a2_ErrorDescription(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


/*---------------------------------------------------------
	Handle management
---------------------------------------------------------*/

/* function RootVoice(iface) */
static EEL_xno ea2_RootVoice(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_RootVoice(ea2i->iface));
	return 0;
}

/* function TypeOf(iface, handle) */
static EEL_xno ea2_TypeOf(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_TypeOf(ea2i->iface, eel_v2l(args + 1)));
	return 0;
}

/* function TypeName(iface, typecode) */
static EEL_xno ea2_TypeName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *name;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((name = a2_TypeName(ea2i->iface, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* function String(iface, handle) */
static EEL_xno ea2_String(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *s;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((s = a2_String(ea2i->iface, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, s);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* function Name(iface, handle) */
static EEL_xno ea2_Name(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *name;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((name = a2_Name(ea2i->iface, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* function Size(iface, handle) */
static EEL_xno ea2_Size(EEL_vm *vm)
{
	int res;
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	res = a2_Size(o2EA2_iface(args->objref.v)->iface, eel_v2l(args + 1));
	if(res < 0)
		return ea2_translate_error(-res);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}

/* procedure Retain(iface, handle) */
static EEL_xno ea2_Retain(EEL_vm *vm)
{
	A2_errors res;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_Retain(ea2i->iface, eel_v2l(args + 1))))
		return ea2_translate_error(res);
	return 0;
}

/* procedure Release(iface, handle) */
static EEL_xno ea2_Release(EEL_vm *vm)
{
	A2_errors res;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	res = a2_Release(ea2i->iface, eel_v2l(args + 1));
	if(res && (res != A2_REFUSE))
		return ea2_translate_error(res);
	return 0;
}

/* procedure Assign(iface, owner, handle) */
static EEL_xno ea2_Assign(EEL_vm *vm)
{
	A2_errors res;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_Assign(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2))))
		return ea2_translate_error(res);
	return 0;
}

/* procedure Export(iface, owner, handle)[name] */
static EEL_xno ea2_Export(EEL_vm *vm)
{
	A2_errors res;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *name;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(vm->argc >= 4)
		name = eel_v2s(args + 3);
	else
		name = NULL;
	if((res = a2_Export(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), name)))
		return ea2_translate_error(res);
	return 0;
}


/*---------------------------------------------------------
	Object loading/creation
---------------------------------------------------------*/

/* function NewBank(iface, name)[flags] */
static EEL_xno ea2_NewBank(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *n;
	int flags = 0;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(!(n = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		flags = eel_v2l(args + 2);
	eel_l2v(vm->heap + vm->resv, a2_NewBank(ea2i->iface, n, flags));
	return 0;
}

/* function LoadString(iface, code, name) */
static EEL_xno ea2_LoadString(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *code;
	const char *name = NULL;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(!(code = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		if(!(name = eel_v2s(args + 2)))
			return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_LoadString(ea2i->iface, code, name));
	return 0;
}

/* function Load(iface, filename) */
static EEL_xno ea2_Load(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	const char *fn;
	unsigned flags = 0;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	if(!(fn = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		flags = eel_v2l(args + 2);
	eel_l2v(vm->heap + vm->resv,
			a2_Load(o2EA2_iface(args->objref.v)->iface, fn,
			flags));
	return 0;
}

/* function NewString(iface, string) */
static EEL_xno ea2_NewString(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *s;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(!(s = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_NewString(ea2i->iface, s));
	return 0;
}

/* function UnloadAll(iface) */
static EEL_xno ea2_UnloadAll(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_UnloadAll(ea2i->iface));
	return 0;
}


/*---------------------------------------------------------
	Objects and exports
---------------------------------------------------------*/

/* function Get(iface, node, path) */
static EEL_xno ea2_Get(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *path;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(!(path = eel_v2s(args + 2)))
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_Get(ea2i->iface, eel_v2l(args + 1),
			path));
	return 0;
}

/* function GetExport(iface, node, index) */
static EEL_xno ea2_GetExport(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_GetExport(ea2i->iface,
			eel_v2l(args + 1), eel_v2l(args + 2)));
	return 0;
}

/* function GetExportName(iface, node, index) */
static EEL_xno ea2_GetExportName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	const char *s;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	s = a2_GetExportName(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2));
	if(s)
		eel_s2v(vm, vm->heap + vm->resv, s);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}


/*---------------------------------------------------------
	Timestamping
---------------------------------------------------------*/

/* function TSDiff(a, b) */
static EEL_xno ea2_TSDiff(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv,
			a2_TSDiff(eel_v2l(args), eel_v2l(args + 1)));
	return 0;
}


/* function TimestampNow(iface) */
static EEL_xno ea2_TimestampNow(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampNow(
			o2EA2_iface(args->objref.v)->iface));
	return 0;
}


/* function TimestampGet(iface) */
static EEL_xno ea2_TimestampGet(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampGet(
			o2EA2_iface(args->objref.v)->iface));
	return 0;
}


/* function TimestampSet(iface, ts) */
static EEL_xno ea2_TimestampSet(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampSet(
			o2EA2_iface(args->objref.v)->iface,
			eel_v2l(args + 1)));
	return 0;
}


/* function ms2Timestamp(iface, t) */
static EEL_xno ea2_ms2Timestamp(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_ms2Timestamp(
			o2EA2_iface(args->objref.v)->iface,
			eel_v2d(args + 1)));
	return 0;
}


/* function Timestamp2ms(iface, ts) */
static EEL_xno ea2_Timestamp2ms(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_d2v(vm->heap + vm->resv, a2_Timestamp2ms(
			o2EA2_iface(args->objref.v)->iface,
			eel_v2l(args + 1)));
	return 0;
}


/* function TimestampReset(iface) */
static EEL_xno ea2_TimestampReset(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampReset(
			o2EA2_iface(args->objref.v)->iface));
	return 0;
}


/* function TimestampBump(iface, dt) */
static EEL_xno ea2_TimestampBump(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampBump(
			o2EA2_iface(args->objref.v)->iface,
			eel_v2l(args + 1)));
	return 0;
}


/* function TimestampNudge(iface, offset, amount) */
static EEL_xno ea2_TimestampNudge(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_TimestampNudge(
			o2EA2_iface(args->objref.v)->iface,
			eel_v2l(args + 1), eel_v2d(args + 2)));
	return 0;
}


/*---------------------------------------------------------
	Playing and controlling
---------------------------------------------------------*/

/* procedure PumpMessages(iface) */
static EEL_xno ea2_PumpMessages(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	a2_PumpMessages(o2EA2_iface(args->objref.v)->iface);
	return 0;
}


/* function NewGroup(iface, parent) */
static EEL_xno ea2_NewGroup(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_NewGroup(ea2i->iface,
			eel_v2l(args + 1)));
	return 0;
}

/* function Start(iface, parent, program)<args> */
static EEL_xno ea2_Start(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int i, a[A2_MAXARGS];
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(vm->argc - 3 > A2_MAXARGS)
		return EEL_XMANYARGS;
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Starta(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* function Play(iface, parent, program)<args> */
static EEL_xno ea2_Play(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int i, a[A2_MAXARGS];
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(vm->argc - 3 > A2_MAXARGS)
		return EEL_XMANYARGS;
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Playa(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* function Send(iface, voice, entrypoint)<args> */
static EEL_xno ea2_Send(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int i, a[A2_MAXARGS];
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(vm->argc - 3 > A2_MAXARGS)
		return EEL_XMANYARGS;
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Senda(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* function SendSub(iface, voice, entrypoint)<args> */
static EEL_xno ea2_SendSub(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int i, a[A2_MAXARGS];
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(vm->argc - 3 > A2_MAXARGS)
		return EEL_XMANYARGS;
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_SendSuba(ea2i->iface,
			eel_v2l(args + 1), eel_v2l(args + 2),
			vm->argc - 3, a));
	return 0;
}

/* function Kill(iface, voice) */
static EEL_xno ea2_Kill(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_Kill(ea2i->iface, eel_v2l(args + 1)));
	return 0;
}

/* function KillSub(iface, voice) */
static EEL_xno ea2_KillSub(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_KillSub(ea2i->iface,
			eel_v2l(args + 1)));
	return 0;
}


/*---------------------------------------------------------
	Streams
---------------------------------------------------------*/

/* function OpenStream(iface, handle)[channel, size, flags] */
static EEL_xno ea2_OpenStream(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int channel = 0;
	int size = 0;
	unsigned flags = 0;
	A2_handle h;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	switch(vm->argc)
	{
	  case 5:
		flags = eel_v2l(args + 4);
	  case 4:
		size = eel_v2l(args + 3);
	  case 3:
		channel = eel_v2l(args + 2);
	}
	h = a2_OpenStream(ea2i->iface, eel_v2l(args + 1),
			channel, size, flags);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}

/* function OpenSink(iface, voice, channel, size)[flags] */
static EEL_xno ea2_OpenSink(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_handle h;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	h = a2_OpenSink(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3),
			vm->argc >= 5 ? eel_v2l(args + 4) : 0);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}

/* function OpenSource(iface, voice, channel, size)[flags] */
static EEL_xno ea2_OpenSource(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_handle h;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	h = a2_OpenSource(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3),
			vm->argc >= 5 ? eel_v2l(args + 4) : 0);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}

/* procedure SetPosition(iface, handle, offset) */
static EEL_xno ea2_SetPosition(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_errors res;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_SetPosition(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2))))
		return ea2_translate_error(res);
	return 0;
}

/* function GetPosition(iface, handle) */
static EEL_xno ea2_GetPosition(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int res;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_GetPosition(ea2i->iface, eel_v2l(args + 1))) < 0)
		return ea2_translate_error(-res);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}

/* function Available(iface, handle) */
static EEL_xno ea2_Available(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int res;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_Available(ea2i->iface, eel_v2l(args + 1))) < 0)
		return ea2_translate_error(-res);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}

/* function Space(iface, handle) */
static EEL_xno ea2_Space(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	int res;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_Space(ea2i->iface, eel_v2l(args + 1))) < 0)
		return ea2_translate_error(-res);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}

/*
 * function Read(iface, handle, frames)[target, offset]
 *
 *	Reads the specified number of audio sample frames from stream 'handle'
 *	into an indexable container that is either provided by the caller, or
 *	created by this call.
 *
 *	If 'target' is a vector type ID, the sample frames read are returned
 *	as a vector of that type, or, if 'target' is not specified, the
 *	closest matching the native type of the data.
 *
 *	If 'target' is a supported indexable object, the samples are converted
 *	and written into that object. 'target' is grown as needed, but will not
 *	be truncated if its size is greater than 'frames'.
 *
 *	If 'offset' is specified, it specifies the index of 'target' where the
 *	first sample will be written.
 *
 *	Returns 'target', or if 'target' was not specified, or a type ID, a new
 *	object containing the data read.
 *
 *	This call is non-blocking, and will throw an exception if the 'target'
 *	stream cannot instantly receive the amount of data to write.
 */
static EEL_xno ea2_Read(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	EEL_object *o;
	A2_errors res;
	A2_sampleformats fmt;
	void *data;
	unsigned size, isize;
	unsigned offset = vm->argc >= 5 ? eel_v2l(args + 4) : 0;
	unsigned frames = eel_v2l(args + 2);
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);

	/* Create or get target object */
	if(vm->argc < 4)
	{
		/*
		 * Create object of type matching native format
		 * FIXME: Hardwired to vector_f for now...
		 */
		if(!(o = eel_new_indexable(vm, EEL_CVECTOR_F,
				offset + frames)))
			return EEL_XCONSTRUCTOR;
	}
	else if(args[3].classid == EEL_CCLASSID)
	{
		/* Create result object of specified type */
		if(!(o = eel_new_indexable(vm, args[3].integer.v,
				offset + frames)))
			return EEL_XCONSTRUCTOR;
	}
	else
	{
		/* Write to existing object! */
		if(!EEL_IS_OBJREF(args[3].classid))
			return EEL_XNEEDOBJECT;
		o = args[3].objref.v;
		eel_own(o);	/* We're going to return this as result! */

		/* Make sure the target object is large enough! */
		if(eel_length(o) < offset + frames)
		{
			EEL_xno x;
			EEL_value v;
			eel_l2v(&v, 0);
			if((x = eel_setlindex(o, offset + frames - 1, &v)))
				return x;
		}
	}

	if((res = get_raw_data_o(o, &fmt, &data, &size, &isize)))
		return res;
	data = (char *)data + isize * offset;
	size = isize * frames;

	/* Read! */
	if((res = a2_Read(ea2i->iface, eel_v2l(args + 1), fmt, data, size)))
		return ea2_translate_error(res);

	eel_o2v(vm->heap + vm->resv, o);
	return 0;
}

/*
 * procedure Write(iface, handle, frames, source)[offset]
 *
 *	Writes the specified number of audio sample frames from the indexable
 *	container 'source' to stream 'handle'. 'source' needs to be of a signed
 *	integer or floating point vector type.
 *
 *	If 'offset' is specified, it specifies the index of 'source' where the
 *	first sample will be read from.
 *
 *	This call is non-blocking, and will throw an exception if the 'source'
 *	stream cannot instantly deliver the amount of data requested.
 */
static EEL_xno ea2_Write(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	EEL_object *o;
	A2_errors res;
	A2_sampleformats fmt;
	void *data;
	unsigned size, isize;
	unsigned offset = vm->argc >= 5 ? eel_v2l(args + 4) : 0;
	unsigned frames = eel_v2l(args + 2);

	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);

	/* Get and check source object */
	if(!EEL_IS_OBJREF(args[3].classid))
		return EEL_XNEEDOBJECT;
	o = args[3].objref.v;
	if(eel_length(o) < offset + frames)
		return EEL_XHIGHINDEX;
	if((res = get_raw_data_o(o, &fmt, &data, &size, &isize)))
		return res;
	data = (char *)data + isize * offset;
	size = isize * frames;

	/* Write! */
	if((res = a2_Write(ea2i->iface, eel_v2l(args + 1), fmt, data, size)))
		return ea2_translate_error(res);
	return 0;
}

/* procedure Flush(iface, handle) */
static EEL_xno ea2_Flush(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_errors res;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = a2_Flush(ea2i->iface, eel_v2l(args + 1))))
		return ea2_translate_error(res);
	return 0;
}


/*---------------------------------------------------------
	Waveform management
---------------------------------------------------------*/

/* function NewWave(iface, wavetype, period)[flags] */
static EEL_xno ea2_NewWave(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_handle h;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	h = a2_NewWave(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2),
			vm->argc >= 4 ? eel_v2l(args + 3) : 0);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}


/* function UploadWave(iface, wavetype, period, flags, data) */
static EEL_xno ea2_UploadWave(EEL_vm *vm)
{
	EEL_xno res;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_sampleformats fmt;
	void *data;
	unsigned size;
	A2_handle h;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if((res = get_raw_data(args + 4, &fmt, &data, &size, NULL)))
		return res;
	h = a2_UploadWave(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3), fmt, data, size);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}


static EEL_xno ea2_parse_properties(EEL_vm *vm, EEL_object *a, A2_property **p)
{
	EEL_xno res;
	int i, len;
	if(!(len = eel_length(a)))
	{
		*p = NULL;
		return 0;
	}
	if(len & 1)
		return EEL_XNEEDEVEN;
	len /= 2;
	if(!(*p = (A2_property *)malloc(sizeof(A2_property) * len)))
		return EEL_XMEMORY;
	for(i = 0; i < len; ++i)
	{
		EEL_value v;
		if((res = eel_getlindex(a, i * 2, &v)))
			break;
		if(EEL_CLASS(&v) != EEL_CINTEGER)
		{
			eel_v_disown(&v);
			break;
		}
		(*p)[i].property = eel_v2l(&v);
		if((res = eel_getlindex(a, i * 2 + 1, &v)))
			break;
		if((EEL_CLASS(&v) != EEL_CINTEGER) &&
				(EEL_CLASS(&v) != EEL_CREAL))
		{
			eel_v_disown(&v);
			break;
		}
		(*p)[i].property = eel_v2l(&v);
	}
	if(res)
	{
		free(*p);
		return res;
	}
	return 0;
}


/*
 * function RenderWave(iface, wavetype, period, flags, samplerate, length)
 *		<args>
 *
 * <args> is an optional array of properties (<property, value> pairs),
 * followed by a program handle and zero or more arguments that will be passed
 * to the program.
 */
static EEL_xno ea2_RenderWave(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_handle h, progh;
	A2_property *props = NULL;
	int i, a[A2_MAXARGS], fargs;

	/* State */
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);

	/* Properties and/or program handle */
	switch(EEL_CLASS(args + 6))
	{
	  case EEL_CINTEGER:
		progh = eel_v2l(args + 6);
		fargs = 7;
		break;
	  case EEL_CARRAY:
	  {
		EEL_xno res;
		if(vm->argc < 8)
			return EEL_XFEWARGS;
		if((res = ea2_parse_properties(vm, args[6].objref.v, &props)))
			return res;
		fargs = 8;
		progh = eel_v2l(args + 7);
		break;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}

	/* Program arguments */
	if(vm->argc - fargs > A2_MAXARGS)
	{
		free(props);
		return EEL_XMANYARGS;
	}
	for(i = 0; i < vm->argc - fargs; ++i)
		a[i] = eel_v2d(args + fargs + i) * 65536.0f;

	h = a2_RenderWave(ea2i->iface,
			eel_v2l(args + 1), eel_v2l(args + 2), /* wt, period */
			eel_v2l(args + 3), eel_v2l(args + 4), /* flags, fs */
			eel_v2l(args + 5), props, /* length, properties */
			progh, vm->argc - fargs, a); /* program, arguments */
	free(props);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}


/*---------------------------------------------------------
	Off-line rendering
---------------------------------------------------------*/

/*
 * function Run(iface, frames)
 *
 * Run a state (or substate) that's using a driver without a thread or similar
 * context of its own, that is, one that implements the Run() method. Typically
 * the "buffer" driver is used for this, and this is the default driver for
 * states created with a2_SubState().
 *
 * Returns the number of sample frames (not bytes!) actually rendered, or
 * throws an exception.
 */
static EEL_xno ea2_Run(EEL_vm *vm)
{
	int res;
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	res = a2_Run(o2EA2_iface(args->objref.v)->iface, eel_v2l(args + 1));
	if(res < 0)
		return ea2_translate_error(-res);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}


/*
 * function Render(iface, stream, samplerate, length)<args>
 *
 * Runs a program off-line with the specified arguments, rendering at
 * 'samplerate', writing the output to 'stream'. <args> is an optional array of
 * properties (<property, value> pairs), followed by a program handle and zero
 * or more arguments that will be passed to the program.
 *
 * Rendering will stop after 'length' sample frames have been rendered, or if
 * 'length' is 0, when the output is silent.
 *
 * Returns the number of sample frames rendered, or throws an exception.
 */
static EEL_xno ea2_Render(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	A2_handle h, progh;
	A2_property *props = NULL;
	int i, a[A2_MAXARGS], fargs;

	/* State */
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);

	/* Properties and/or program handle */
	switch(EEL_CLASS(args + 4))
	{
	  case EEL_CINTEGER:
		progh = eel_v2l(args + 4);
		fargs = 5;
		break;
	  case EEL_CARRAY:
	  {
		EEL_xno res;
		if(vm->argc < 6)
			return EEL_XFEWARGS;
		if((res = ea2_parse_properties(vm, args[4].objref.v, &props)))
			return res;
		fargs = 6;
		progh = eel_v2l(args + 5);
		break;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}

	/* Program arguments */
	if(vm->argc - fargs > A2_MAXARGS)
	{
		free(props);
		return EEL_XMANYARGS;
	}
	for(i = 0; i < vm->argc - fargs; ++i)
		a[i] = eel_v2d(args + fargs + i) * 65536.0f;

	/* Render! */
	h = a2_Render(ea2i->iface, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3), props, progh, vm->argc - fargs, a);
	free(props);
	if(h < 0)
		return ea2_translate_error(-h);
	eel_l2v(vm->heap + vm->resv, h);
	return 0;
}


/*---------------------------------------------------------
	Utilities
---------------------------------------------------------*/

static EEL_xno ea2_F2P(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	float ref = vm->argc >= 2 ? eel_v2d(args + 1) : A2_MIDDLEC;
	eel_d2v(vm->heap + vm->resv, a2_F2Pf(eel_v2d(args), ref));
	return 0;
}


static EEL_xno ea2_P2I(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, a2_P2I(eel_v2d(args) * 65536.0f) *
			(1.0f / 16777216.0f));
	return 0;
}


static EEL_xno ea2_Rand(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	eel_d2v(vm->heap + vm->resv, a2_Rand(ea2i->iface, eel_v2d(args + 1)));
	return 0;
}


/*---------------------------------------------------------
	Object property interface
---------------------------------------------------------*/

/* function GetProperty(iface, handle, property) */
static EEL_xno ea2_GetProperty(EEL_vm *vm)
{
	A2_errors ae;
	int v;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(EEL_CLASS(args + 1) != EEL_CINTEGER)
		return EEL_XWRONGTYPE;
	if(EEL_CLASS(args + 2) != EEL_CINTEGER)
		return EEL_XWRONGTYPE;
	if((ae = a2_GetProperty(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), &v)))
		return ea2_translate_error(ae);
	eel_l2v(vm->heap + vm->resv, v);
	return 0;
}

/* procedure SetProperty(iface, handle, property, value) */
static EEL_xno ea2_SetProperty(EEL_vm *vm)
{
	A2_errors ae;
	EEL_value *args = vm->heap + vm->argv;
	EA2_iface *ea2i;
	if(EEL_CLASS(args) != a2_md.iface_cid)
		return EEL_XWRONGTYPE;
	ea2i = o2EA2_iface(args->objref.v);
	if(EEL_CLASS(args + 1) != EEL_CINTEGER)
		return EEL_XWRONGTYPE;
	if(EEL_CLASS(args + 2) != EEL_CINTEGER)
		return EEL_XWRONGTYPE;
	if((ae = a2_SetProperty(ea2i->iface, eel_v2l(args + 1),
			eel_v2l(args + 2), eel_v2l(args + 3))))
		return ea2_translate_error(ae);
	return 0;
}


/*----------------------------------------------------------
	Versioning
----------------------------------------------------------*/

static EEL_xno ea2_HeaderVersion(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	unsigned ver = a2_HeaderVersion();
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, A2_MAJOR(ver));
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, A2_MINOR(ver));
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, A2_MICRO(ver));
	eel_setlindex(a, 2, &v);
	if(A2_BUILD(ver))
	{
		eel_l2v(&v, A2_BUILD(ver));
		eel_setlindex(a, 3, &v);
	}
	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}

static EEL_xno ea2_LinkedVersion(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	unsigned ver = a2_LinkedVersion();
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, A2_MAJOR(ver));
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, A2_MINOR(ver));
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, A2_MICRO(ver));
	eel_setlindex(a, 2, &v);
	if(A2_BUILD(ver))
	{
		eel_l2v(&v, A2_BUILD(ver));
		eel_setlindex(a, 3, &v);
	}
	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno eel_a2_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close the EEL state */
	if(closing)
	{
		if(a2_md.ifacefields)
			eel_disown(a2_md.ifacefields);
		memset(&a2_md, 0, sizeof(a2_md));
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp eel_a2_constants[] =
{
	/* Engine and API limits and information */
	{"MIDDLEC",		A2_MIDDLEC	},
	{"REGISTERS",		A2_REGISTERS	},
	{"MAXARGS",		A2_MAXARGS	},
	{"MAXEPS",		A2_MAXEPS	},
	{"MIPLEVELS",		A2_MIPLEVELS	},
	{"MAXFRAG",		A2_MAXFRAG	},
	{"MAXCHANNELS",		A2_MAXCHANNELS	},
	{"WAVEPERIOD",		A2_WAVEPERIOD	},

	/* Object types */
	{"TBANK",		A2_TBANK	},
	{"TWAVE",		A2_TWAVE	},
	{"TPROGRAM",		A2_TPROGRAM	},
	{"TUNIT",		A2_TUNIT	},
	{"TSTRING",		A2_TSTRING	},
	{"TSTREAM",		A2_TSTREAM	},
	{"TXICLIENT",		A2_TXICLIENT	},
	{"TDETACHED",		A2_TDETACHED	},
	{"TNEWVOICE",		A2_TNEWVOICE	},
	{"TVOICE",		A2_TVOICE	},

	/* Flags for a2_Open() */
	{"TIMESTAMP",		A2_TIMESTAMP	},
	{"NOAUTOCNX",		A2_NOAUTOCNX	},
	{"REALTIME",		A2_REALTIME	},
	{"SILENT",		A2_SILENT	},
	{"RTSILENT",		A2_RTSILENT	},
	{"NOSHARED",		A2_NOSHARED	},

	/* Wave types */
	{"WOFF",		A2_WOFF		},
	{"WNOISE",		A2_WNOISE	},
	{"WWAVE",		A2_WWAVE	},
	{"WMIPWAVE",		A2_WMIPWAVE	},

	/* Flags for waveform uploading and rendering */
	{"LOOPED",		A2_LOOPED	},
	{"NORMALIZE",		A2_NORMALIZE	},
	{"XFADE",		A2_XFADE	},
	{"REVMIX",		A2_REVMIX	},
	{"CLEAR",		A2_CLEAR	},
	{"UNPREPARED",		A2_UNPREPARED	},

	/* Sample formats for wave uploading, stream I/O etc */
	{"I8",			A2_I8		},
	{"I16",			A2_I16		},
	{"I24",			A2_I24		},
	{"I32",			A2_I32		},
	{"F32",			A2_F32		},

	/* Properties: General */
	{"PCHANNELS",		A2_PCHANNELS		},
	{"PFLAGS",		A2_PFLAGS		},
	{"PREFCOUNT",		A2_PREFCOUNT		},

	{"PSIZE",		A2_PSIZE		},
	{"PPOSITION",		A2_PPOSITION		},
	{"PAVAILABLE",		A2_PAVAILABLE		},
	{"PSPACE",		A2_PSPACE		},

	/* Properties: Global settings */
	{"PSAMPLERATE",		A2_PSAMPLERATE		},
	{"PBUFFER",		A2_PBUFFER		},
	{"PTABSIZE",		A2_PTABSIZE		},
	{"POFFLINEBUFFER",	A2_POFFLINEBUFFER	},
	{"PSILENCELEVEL",	A2_PSILENCELEVEL	},
	{"PSILENCEWINDOW",	A2_PSILENCEWINDOW	},
	{"PSILENCEGRACE",	A2_PSILENCEGRACE	},
	{"PRANDSEED",		A2_PRANDSEED		},
	{"PNOISESEED",		A2_PNOISESEED		},

	/* Properties: Profiling information */
	{"PACTIVEVOICES",	A2_PACTIVEVOICES	},
	{"PFREEVOICES",		A2_PFREEVOICES		},
	{"PTOTALVOICES",	A2_PTOTALVOICES		},
	{"PCPULOADAVG",		A2_PCPULOADAVG		},
	{"PCPULOADMAX",		A2_PCPULOADMAX		},
	{"PCPUTIMEAVG",		A2_PCPUTIMEAVG		},
	{"PCPUTIMEMAX",		A2_PCPUTIMEMAX		},
	{"PINSTRUCTIONS",	A2_PINSTRUCTIONS	},

	{NULL, 0}
};


/*
 * FIXME: Nice and simple, but we're duplicating all the name and description
 * strings already compiled into libaudiality2... :-/
 */
#define	A2_DEFERR(x, y)	{ A2_##x, #x, y },
static const EEL_xdef eel_a2_exceptions[] =
{
	A2_ALLERRORS
	{ 0, NULL,  NULL }
};
#undef	A2_DEFERR


static void addfunc(EEL_object *m, EEL_object *t,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_value v;
	v.classid = EEL_COBJREF;
	v.objref.v = eel_export_cfunction(m, results, name,
			reqargs, optargs, tupargs, func);
	if(!v.objref.v)
		return;
	eel_table_sets(t, name, &v);
}


EEL_xno eel_audiality2_init(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value v;
	EEL_object *t;
	EEL_object *c;
	EEL_object *m = eel_create_module(vm, "Audiality2", eel_a2_unload, &a2_md);
	memset(&a2_md, 0, sizeof(a2_md));
	if(!m)
		return EEL_XMODULEINIT;

	/* Exception code translation */
	if((ea2_error_base = eel_x_register(vm, eel_a2_exceptions)) < 0)
		return -ea2_error_base;

	/* Register class 'a2state' */
	c = eel_export_class(m, "a2state", -1, a2s_construct, a2s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, a2s_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, a2s_setindex);
	a2_md.iface_cid = eel_class_cid(c);

	x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return EEL_XMODULEINIT;
	t = a2_md.ifacefields = eel_v2o(&v);

	/* Error handling */
	eel_export_cfunction(m, 1, "LastError", 0, 1, 0, ea2_LastError);
	eel_export_cfunction(m, 1, "ErrorString", 1, 0, 0, ea2_ErrorString);
	eel_export_cfunction(m, 1, "ErrorName", 1, 0, 0, ea2_ErrorName);
	eel_export_cfunction(m, 1, "ErrorDescription", 1, 0, 0,
			ea2_ErrorDescription);

	/* Handle management */
	addfunc(m, t, 1, "RootVoice", 1, 0, 0, ea2_RootVoice);
	addfunc(m, t, 1, "TypeOf", 2, 0, 0, ea2_TypeOf);
	addfunc(m, t, 1, "TypeName", 2, 0, 0, ea2_TypeName);
	addfunc(m, t, 1, "String", 2, 0, 0, ea2_String);
	addfunc(m, t, 1, "Name", 2, 0, 0, ea2_Name);
	addfunc(m, t, 1, "Size", 2, 0, 0, ea2_Size);
	addfunc(m, t, 0, "Retain", 2, 0, 0, ea2_Retain);
	addfunc(m, t, 0, "Release", 2, 0, 0, ea2_Release);
	addfunc(m, t, 0, "Assign", 3, 0, 0, ea2_Assign);
	addfunc(m, t, 0, "Export", 3, 1, 0, ea2_Export);

	/* Object loading/creation */
	addfunc(m, t, 1, "NewBank", 2, 1, 0, ea2_NewBank);
	addfunc(m, t, 1, "LoadString", 2, 1, 0, ea2_LoadString);
	addfunc(m, t, 1, "Load", 2, 1, 0, ea2_Load);
	addfunc(m, t, 1, "NewString", 2, 0, 0, ea2_NewString);
	addfunc(m, t, 1, "UnloadAll", 2, 0, 0, ea2_UnloadAll);

	/* Objects and exports */
	addfunc(m, t, 1, "Get", 3, 0, 0, ea2_Get);
	addfunc(m, t, 1, "GetExport", 3, 0, 0, ea2_GetExport);
	addfunc(m, t, 1, "GetExportName", 3, 0, 0, ea2_GetExportName);

	/* Timestamping */
	addfunc(m, t, 1, "TSDiff", 2, 0, 0, ea2_TSDiff);
	addfunc(m, t, 1, "TimestampNow", 1, 0, 0, ea2_TimestampNow);
	addfunc(m, t, 1, "TimestampGet", 1, 0, 0, ea2_TimestampGet);
	addfunc(m, t, 1, "TimestampSet", 2, 0, 0, ea2_TimestampSet);
	addfunc(m, t, 1, "ms2Timestamp", 2, 0, 0, ea2_ms2Timestamp);
	addfunc(m, t, 1, "Timestamp2ms", 2, 0, 0, ea2_Timestamp2ms);
	addfunc(m, t, 1, "TimestampReset", 1, 0, 0, ea2_TimestampReset);
	addfunc(m, t, 1, "TimestampBump", 2, 0, 0, ea2_TimestampBump);
	addfunc(m, t, 1, "TimestampNudge", 3, 0, 0, ea2_TimestampNudge);

	/* Playing and controlling */
	addfunc(m, t, 0, "PumpMessages", 1, 0, 0, ea2_PumpMessages);
	addfunc(m, t, 1, "NewGroup", 2, 0, 0, ea2_NewGroup);
	addfunc(m, t, 1, "Start", 3, 0, 1, ea2_Start);
	addfunc(m, t, 1, "Play", 3, 0, 1, ea2_Play);
	addfunc(m, t, 1, "Send", 3, 0, 1, ea2_Send);
	addfunc(m, t, 1, "SendSub", 3, 0, 1, ea2_SendSub);
	addfunc(m, t, 1, "Kill", 2, 0, 0, ea2_Kill);
	addfunc(m, t, 1, "KillSub", 2, 0, 0, ea2_KillSub);

	/* Streams */
	addfunc(m, t, 1, "OpenStream", 2, 3, 0, ea2_OpenStream);
	addfunc(m, t, 1, "OpenSink", 4, 1, 0, ea2_OpenSink);
	addfunc(m, t, 1, "OpenSource", 4, 1, 0, ea2_OpenSource);
	addfunc(m, t, 0, "SetPosition", 3, 0, 0, ea2_SetPosition);
	addfunc(m, t, 1, "GetPosition", 2, 0, 0, ea2_GetPosition);
	addfunc(m, t, 1, "Available", 2, 0, 0, ea2_Available);
	addfunc(m, t, 1, "Space", 2, 0, 0, ea2_Space);
	addfunc(m, t, 1, "Read", 3, 2, 0, ea2_Read);
	addfunc(m, t, 0, "Write", 3, 2, 0, ea2_Write);
	addfunc(m, t, 0, "Flush", 2, 0, 0, ea2_Flush);

	/* Waveform management (waves.h) */
	addfunc(m, t, 1, "NewWave", 3, 1, 0, ea2_NewWave);
	addfunc(m, t, 1, "UploadWave", 5, 0, 0, ea2_UploadWave);
	addfunc(m, t, 1, "RenderWave", 6, 0, 1, ea2_RenderWave);

	/* Off-line rendering */
	addfunc(m, t, 1, "Run", 2, 0, 0, ea2_Run);
	addfunc(m, t, 1, "Render", 4, 0, 1, ea2_Render);

	/* Utilities */
	eel_export_cfunction(m, 1, "F2P", 1, 1, 0, ea2_F2P);
	eel_export_cfunction(m, 1, "P2I", 1, 1, 0, ea2_P2I);
	addfunc(m, t, 1, "Rand", 2, 0, 0, ea2_Rand);

	/* Object property interface */
	addfunc(m, t, 1, "GetProperty", 3, 0, 0, ea2_GetProperty);
	addfunc(m, t, 0, "SetProperty", 4, 0, 0, ea2_SetProperty);

	/* Versioning */
	eel_export_cfunction(m, 1, "HeaderVersion", 0, 0, 0, ea2_HeaderVersion);
	eel_export_cfunction(m, 1, "LinkedVersion", 0, 0, 0, ea2_LinkedVersion);

	/* Constants and enums */
	eel_export_lconstants(m, eel_a2_constants);

	eel_disown(m);
	return 0;
}
