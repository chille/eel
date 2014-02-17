/*
---------------------------------------------------------------------------
	e_operate.c - Operations on values and objects
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009-2011 David Olofson
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

#include "e_operate.h"
#include "e_object.h"


/* Tries to index 'key' out of 'io' and returns 0, or an exception code. */
static inline EEL_xno test_index(EEL_object *io, EEL_value *key)
{
	EEL_value v;
	EEL_xno x = eel_o__metamethod(io, EEL_MM_GETINDEX, key, &v);
	if(!x)
		eel_v_disown(&v);
	return x;
}


EEL_xno eel_object_op(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result, int inplace)
{
	EEL_xno x;
	EEL_value v;
	EEL_object *o = left->objref.v;
	switch(binop)
	{
	  case EEL_OP_POWER:
		return eel_o__metamethod(o, EEL_MM_POWER + inplace, right, result);
	  case EEL_OP_MOD:
		return eel_o__metamethod(o, EEL_MM_MOD + inplace, right, result);
	  case EEL_OP_DIV:
		return eel_o__metamethod(o, EEL_MM_DIV + inplace, right, result);
	  case EEL_OP_SUB:
		return eel_o__metamethod(o, EEL_MM_SUB + inplace, right, result);
	  case EEL_OP_MUL:
		return eel_o__metamethod(o, EEL_MM_MUL + inplace, right, result);
	  case EEL_OP_ADD:
		return eel_o__metamethod(o, EEL_MM_ADD + inplace, right, result);

	  case EEL_OP_VPOWER:
		return eel_o__metamethod(o, EEL_MM_VPOWER + inplace, right, result);
	  case EEL_OP_VMOD:
		return eel_o__metamethod(o, EEL_MM_VMOD + inplace, right, result);
	  case EEL_OP_VDIV:
		return eel_o__metamethod(o, EEL_MM_VDIV + inplace, right, result);
	  case EEL_OP_VSUB:
		return eel_o__metamethod(o, EEL_MM_VSUB + inplace, right, result);
	  case EEL_OP_VMUL:
		return eel_o__metamethod(o, EEL_MM_VMUL + inplace, right, result);
	  case EEL_OP_VADD:
		return eel_o__metamethod(o, EEL_MM_VADD + inplace, right, result);

	  /* Boolean (any object == true) */
	  case EEL_OP_AND:
		result->type = EEL_TBOOLEAN;
		switch(right->type)
		{
		  case EEL_TNIL:
			result->integer.v = 0;
			break;
		  case EEL_TREAL:
			result->integer.v = (right->real.v != 0.0f);
			break;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
			result->integer.v = (right->integer.v != 0);
			break;
		  case EEL_TTYPEID:
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			result->integer.v = 1;
			break;
		}
		return 0;
	  case EEL_OP_OR:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_OP_XOR:
		result->type = EEL_TBOOLEAN;
		switch(right->type)
		{
		  case EEL_TNIL:
			result->integer.v = 1;
			break;
		  case EEL_TREAL:
			result->integer.v = (right->real.v == 0.0f);
			break;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
			result->integer.v = (right->integer.v == 0);
			break;
		  case EEL_TTYPEID:
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			result->integer.v = 0;
			break;
		}
		return 0;

	  case EEL_OP_EQ:
		if(inplace)
			return EEL_XCANTINPLACE;
		return eel_o__metamethod(o, EEL_MM_EQ, right, result);
	  case EEL_OP_NE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_EQ, right, result);
		result->integer.v = !result->integer.v;
		return x;
	  case EEL_OP_GT:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, result);
		result->type = EEL_TBOOLEAN;
		result->integer.v = result->integer.v > 0;
		return x;
	  case EEL_OP_GE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, result);
		result->type = EEL_TBOOLEAN;
		result->integer.v = result->integer.v >= 0;
		return x;
	  case EEL_OP_LT:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, result);
		result->type = EEL_TBOOLEAN;
		result->integer.v = result->integer.v < 0;
		return x;
	  case EEL_OP_LE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, result);
		result->type = EEL_TBOOLEAN;
		result->integer.v = result->integer.v <= 0;
		return x;
	  case EEL_OP_IN:
		if(inplace)
			return EEL_XCANTINPLACE;
		if(!EEL_IS_OBJREF(right->type))
		{
			/* Not an indexable object, so "no"! */
			result->type = EEL_TBOOLEAN;
			result->integer.v = 0;
			return 0;
		}
		x = test_index(right->objref.v, left);
		result->type = EEL_TBOOLEAN;
		result->integer.v = (x == 0);
		return 0;
	  case EEL_OP_MIN:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		if(v.integer.v <= 0)
			eel_v_copy(result, left);
		else
			eel_v_copy(result, right);
		return x;
	  case EEL_OP_MAX:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		if(v.integer.v >= 0)
			eel_v_copy(result, left);
		else
			eel_v_copy(result, right);
		return x;
	  default:
		return EEL_XNOTIMPLEMENTED;
	}
}


/*
 * NOTE:
 *	The 'inplace' feature of this one is not used currently,
 *	as there's no way to express inplace reverse operations
 *	in EEL source or bytecode as of now.
 */
EEL_xno eel_object_rop(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result, int inplace)
{
	EEL_xno x;
	EEL_value v;
	EEL_object *o = left->objref.v;
	switch(binop)
	{
	  case EEL_OP_POWER:
		return eel_o__metamethod(o, EEL_MM_RPOWER + inplace, right, result);
	  case EEL_OP_MOD:
		return eel_o__metamethod(o, EEL_MM_RMOD + inplace, right, result);
	  case EEL_OP_DIV:
		return eel_o__metamethod(o, EEL_MM_RDIV + inplace, right, result);
	  case EEL_OP_SUB:
		return eel_o__metamethod(o, EEL_MM_RSUB + inplace, right, result);
	  case EEL_OP_MUL:
		return eel_o__metamethod(o, EEL_MM_RMUL + inplace, right, result);
	  case EEL_OP_ADD:
		return eel_o__metamethod(o, EEL_MM_RADD + inplace, right, result);

	  case EEL_OP_VPOWER:
		return eel_o__metamethod(o, EEL_MM_VRPOWER + inplace, right, result);
	  case EEL_OP_VMOD:
		return eel_o__metamethod(o, EEL_MM_VRMOD + inplace, right, result);
	  case EEL_OP_VDIV:
		return eel_o__metamethod(o, EEL_MM_VRDIV + inplace, right, result);
	  case EEL_OP_VSUB:
		return eel_o__metamethod(o, EEL_MM_VRSUB + inplace, right, result);
	  case EEL_OP_VMUL:
		return eel_o__metamethod(o, EEL_MM_VRMUL + inplace, right, result);
	  case EEL_OP_VADD:
		return eel_o__metamethod(o, EEL_MM_VRADD + inplace, right, result);

	  /* Boolean (any object == true) */
	  case EEL_OP_AND:
		result->type = EEL_TBOOLEAN;
		switch(right->type)
		{
		  case EEL_TNIL:
			result->integer.v = 0;
			break;
		  case EEL_TREAL:
			result->integer.v = (right->real.v != 0.0f);
			break;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
			result->integer.v = (right->integer.v != 0);
			break;
		  case EEL_TTYPEID:
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			result->integer.v = 1;
			break;
		}
		return 0;
	  case EEL_OP_OR:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_OP_XOR:
		result->type = EEL_TBOOLEAN;
		switch(right->type)
		{
		  case EEL_TNIL:
			result->integer.v = 1;
			break;
		  case EEL_TREAL:
			result->integer.v = (right->real.v == 0.0f);
			break;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
			result->integer.v = (right->integer.v == 0);
			break;
		  case EEL_TTYPEID:
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			result->integer.v = 0;
			break;
		}
		return 0;

	  /* Commutative */
	  case EEL_OP_EQ:
		if(inplace)
			return EEL_XCANTINPLACE;
		return eel_o__metamethod(o, EEL_MM_EQ, right, result);
	  case EEL_OP_NE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_EQ, right, result);
		result->integer.v = !result->integer.v;
		return x;
	  case EEL_OP_MIN:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		if(v.integer.v <= 0)
			eel_v_copy(result, left);
		else
			eel_v_copy(result, right);
		return x;
	  case EEL_OP_MAX:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		if(v.integer.v >= 0)
			eel_v_copy(result, left);
		else
			eel_v_copy(result, right);
		return x;

	  /* "Swappable" */
	  case EEL_OP_GT:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		result->type = EEL_TBOOLEAN;
		result->integer.v = v.integer.v < 0;
		break;
	  case EEL_OP_GE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		result->type = EEL_TBOOLEAN;
		result->integer.v = v.integer.v <= 0;
		break;
	  case EEL_OP_LT:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		result->type = EEL_TBOOLEAN;
		result->integer.v = v.integer.v > 0;
		break;
	  case EEL_OP_LE:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = eel_o__metamethod(o, EEL_MM_COMPARE, right, &v);
		result->type = EEL_TBOOLEAN;
		result->integer.v = v.integer.v >= 0;
		break;

	  /* <non-object> in <object> */
	  case EEL_OP_IN:
		if(inplace)
			return EEL_XCANTINPLACE;
		x = test_index(o, right);
		result->type = EEL_TBOOLEAN;
		result->integer.v = (x == 0);
		return 0;

	  default:
		return EEL_XNOTIMPLEMENTED;
	}
	return x;
}
