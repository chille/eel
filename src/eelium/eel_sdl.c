/*
---------------------------------------------------------------------------
	eel_sdl.c - EEL SDL Binding
---------------------------------------------------------------------------
 * Copyright 2005-2007, 2009-2011, 2013-2014, 2016-2017 David Olofson
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

#ifndef _WIN32
#include <sched.h>
#endif

#include <string.h>
#include "eel_sdl.h"
#include "eel_net.h"
#include "fastevents.h"
#include "net2.h"
#include "eel_opengl.h"

/*
 * Since SDL is not fully thread safe, only one instance of
 * the EEL SDL library at a time is allowed in a process.
 * This has the advantage that the other Eelium modules can
 * get at the EEL SDL type IDs by simply looking at the
 * static fake "moduledata" struct.
 */
static int loaded = 0;

ESDL_moduledata esdl_md;


/*----------------------------------------------------------
	EEL utilities
----------------------------------------------------------*/

/* Set integer field 'n' of table 'io' to 'val' */
static inline void esdl_seti(EEL_object *io, const char *n, long val)
{
	EEL_value v;
	eel_l2v(&v, val);
	eel_setsindex(io, n, &v);
}


/* Set boolean field 'n' of table 'io' to 'val' */
static inline void esdl_setb(EEL_object *io, const char *n, int val)
{
	EEL_value v;
	eel_b2v(&v, val);
	eel_setsindex(io, n, &v);
}


/*----------------------------------------------------------
	Low level graphics utilities
----------------------------------------------------------*/

static inline void clip_rect(SDL_Rect *r, SDL_Rect *to)
{
	int dx1 = r->x;
	int dy1 = r->y;
	int dx2 = dx1 + r->w;
	int dy2 = dy1 + r->h;
	if(dx1 < to->x)
		dx1 = to->x;
	if(dy1 < to->y)
		dy1 = to->y;
	if(dx2 > to->x + to->w)
		dx2 = to->x + to->w;
	if(dy2 > to->y + to->h)
		dy2 = to->y + to->h;
	if(dx2 < dx1 || dy2 < dy1)
	{
		r->x = r->y = 0;
		r->w = r->h = 0;
	}
	else
	{
		r->x = dx1;
		r->y = dy1;
		r->w = dx2 - dx1;
		r->h = dy2 - dy1;
	}
}


/*
 * Get and lock (if necessary) an SDL surface from an argument, which can be a
 * Surface or a SurfaceLock. Returns 0 upon success, or an EEL exception code.
 */
static inline EEL_xno get_surface(EEL_value *arg, int *locked, SDL_Surface **s)
{
	*locked = 0;
	if(EEL_TYPE(arg) == esdl_md.surfacelock_cid)
	{
		EEL_object *so = o2ESDL_surfacelock(arg->objref.v)->surface;
		if(!so)
			return EEL_XARGUMENTS;	/* No surface! */
		*s = o2ESDL_surface(so)->surface;
		/* This one's locked by definition, so we're done! */
		return 0;
	}
	else if(EEL_TYPE(arg) == esdl_md.surface_cid)
	{
		*s = o2ESDL_surface(arg->objref.v)->surface;
		if(SDL_MUSTLOCK((*s)))
		{
			SDL_LockSurface(*s);
			*locked = 1;
		}
		return 0;
	}
	else
		return EEL_XWRONGTYPE;
}


/* Get pixel from 24 bpp surface, no clipping. */
static inline Uint32 getpixel24(SDL_Surface *s, int x, int y)
{
	Uint8 *p = (Uint8 *)s->pixels;
	Uint8 *p24 = p + y * s->pitch + x * 3;
#if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
	return p24[0] | ((Uint32)p24[1] << 8) | ((Uint32)p24[2] << 16);
#else
	return p24[2] | ((Uint32)p24[1] << 8) | ((Uint32)p24[0] << 16);
#endif
}


/* Get pixel from 32 bpp surface, no clipping. */
static inline Uint32 getpixel32(SDL_Surface *s, int x, int y)
{
	Uint32 *p = (Uint32 *)s->pixels;
	int ppitch = s->pitch / 4;
	return p[y * ppitch + x];
}


/* Get pixel from 24 bpp surface, with clipping. */
static inline Uint32 getpixel24c(SDL_Surface *s, int x, int y, Uint32 cc)
{
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
		return cc;
	else
		return getpixel24(s, x, y);
}


/* Get pixel from 32 bpp surface; with clipping. */
static inline Uint32 getpixel32c(SDL_Surface *s, int x, int y, Uint32 cc)
{
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
		return cc;
	else
		return getpixel32(s, x, y);
}


/*
 * Multiply the R/G/B channels from color value 'c' with weight 'w', and add
 * them to the respective accumulators. 'w' and the accumulators are 16:16
 * fixed point.
 */
static inline void maccrgb(Uint32 c, int w, int *r, int *g, int *b)
{
	*b += (c & 0xff) * w >> 8;
	*g += ((c >> 8) & 0xff) * w >> 8;
	*r += ((c >> 16) & 0xff) * w >> 8;
}


/*
 * Multiply the R/G/B/A channels from color value 'c' with weight 'w', and add
 * them to the respective accumulators. 'w' and the accumulators are 16:16
 * fixed point.
 */
static inline void maccrgba(Uint32 c, int w, int *r, int *g, int *b, int *a)
{
	*b += (c & 0xff) * w >> 8;
	*g += ((c >> 8) & 0xff) * w >> 8;
	*r += ((c >> 16) & 0xff) * w >> 8;
	*a += ((c >> 24) & 0xff) * w >> 8;
}


/* Assemble channels to ARGB color value. (No clamping or masking!) */
static inline Uint32 rgba2c(int r, int g, int b, int a)
{
	return a << 24 | r << 16 | g << 8 | b;
}


/*----------------------------------------------------------
	Rect class
----------------------------------------------------------*/
static EEL_xno r_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	SDL_Rect *r;
	EEL_object *eo = eel_o_alloc(vm, sizeof(SDL_Rect), type);
	if(!eo)
		return EEL_XMEMORY;
	r = o2SDL_Rect(eo);
	if(!initc)
	{
		r->x = r->y = 0;
		r->w = r->h = 0;
	}
	else if(initc == 1)
	{
		SDL_Rect *sr;
		if(initv->objref.v->type != esdl_md.rect_cid)
		{
			eel_o_free(eo);
			return EEL_XWRONGTYPE;
		}
		sr = o2SDL_Rect(initv->objref.v);
		memcpy(r, sr, sizeof(SDL_Rect));
	}
	else
	{
		int v;
		if(initc != 4)
		{
			eel_o_free(eo);
			return EEL_XARGUMENTS;
		}
		r->x = eel_v2l(initv);
		r->y = eel_v2l(initv + 1);
		v = eel_v2l(initv + 2);
		r->w = v >= 0 ? v : 0;
		v = eel_v2l(initv + 3);
		r->h = v >= 0 ? v : 0;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno r_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *co = eel_o_alloc(vm, sizeof(SDL_Rect), t);
	if(!co)
		return EEL_XMEMORY;
	memcpy(o2SDL_Rect(co), o2SDL_Rect(src->objref.v), sizeof(SDL_Rect));
	eel_o2v(dst, co);
	return 0;
}


static EEL_xno r_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Rect *r = o2SDL_Rect(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		op2->integer.v = r->x;
		break;
	  case 'y':
		op2->integer.v = r->y;
		break;
	  case 'w':
		op2->integer.v = r->w;
		break;
	  case 'h':
		op2->integer.v = r->h;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	op2->type = EEL_TINTEGER;
	return 0;
}


static EEL_xno r_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Rect *r = o2SDL_Rect(eo);
	const char *is = eel_v2s(op1);
	int iv = eel_v2l(op2);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		r->x = iv;
		break;
	  case 'y':
		r->y = iv;
		break;
	  case 'w':
		if(iv >= 0)
			r->w = iv;
		else
			r->w = 0;
		break;
	  case 'h':
		if(iv >= 0)
			r->h = iv;
		else
			r->h = 0;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	return 0;
}


/*----------------------------------------------------------
	Surface class
----------------------------------------------------------*/
static EEL_xno s_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_surface *s;
	EEL_object *eo;
	int w, h, bpp;
	Uint32	flags, rmask, gmask, bmask, amask;
	/* Get the specified parameters */
	amask = 0;
	switch(initc)
	{
	  case 8:	/* RGBA */
		amask = eel_v2l(initv + 7);
		/* Fall through! */
	  case 7:	/* RGB - no alpha */
		flags = eel_v2l(initv);
		w = eel_v2l(initv + 1);
		h = eel_v2l(initv + 2);
		bpp = eel_v2l(initv + 3);
		rmask = eel_v2l(initv + 4);
		gmask = eel_v2l(initv + 5);
		bmask = eel_v2l(initv + 6);
		break;
	  case 4:	/* Use "sensible" default masks for the bpp */
		flags = eel_v2l(initv);
		w = eel_v2l(initv + 1);
		h = eel_v2l(initv + 2);
		bpp = eel_v2l(initv + 3);
		switch(bpp)
		{
		  case 8:
			rmask = 0xe0;
			gmask = 0x1c;
			bmask = 0x03;
			amask = 0;
			break;
		  case 15:
			rmask = 0x7c00;
			gmask = 0x03e0;
			bmask = 0x001f;
			amask = 0;
			break;
		  case 16:
			rmask = 0xf800;
			gmask = 0x07e0;
			bmask = 0x001f;
			amask = 0;
			break;
		  case 24:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			rmask = 0x00ff0000;
			gmask = 0x0000ff00;
			bmask = 0x000000ff;
#else
			rmask = 0x000000ff;
			gmask = 0x0000ff00;
			bmask = 0x00ff0000;
#endif
			amask = 0;
			break;
		  case 32:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			rmask = 0xff000000;
			gmask = 0x00ff0000;
			bmask = 0x0000ff00;
			amask = 0x000000ff;
#else
			rmask = 0x000000ff;
			gmask = 0x0000ff00;
			bmask = 0x00ff0000;
			amask = 0xff000000;
#endif
			break;
		  default:
			return EEL_XARGUMENTS;
		}
		break;
	  case 0:
		// Create empty Surface object
		eo = eel_o_alloc(vm, sizeof(ESDL_surface), type);
		if(!eo)
			return EEL_XMEMORY;
		s = o2ESDL_surface(eo);
		s->surface = NULL;
		eel_o2v(result, eo);
		return 0;
	  default:
		return EEL_XARGUMENTS;
	}
	eo = eel_o_alloc(vm, sizeof(ESDL_surface), type);
	if(!eo)
		return EEL_XMEMORY;
	s = o2ESDL_surface(eo);
	s->surface = SDL_CreateRGBSurface(flags, w, h, bpp,
			rmask, gmask, bmask, amask);
	if(!s->surface)
	{
		eel_o_free(eo);
		return EEL_XDEVICEERROR;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno s_destruct(EEL_object *eo)
{
	ESDL_surface *s = o2ESDL_surface(eo);
	if(s->surface && !s->is_window_surface)
		SDL_FreeSurface(s->surface);
	return 0;
}


static EEL_xno s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ESDL_surface *s = o2ESDL_surface(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) == 1)
		switch(is[0])
		{
		  case 'w':
			op2->integer.v = s->surface->w;
			break;
		  case 'h':
			op2->integer.v = s->surface->h;
			break;
		  default:
			return EEL_XWRONGINDEX;
		}
	else if(!strcmp(is, "flags"))
		op2->integer.v = s->surface->flags;
	else if(!strcmp(is, "alpha"))
	{
		Uint8 am;
		SDL_GetSurfaceAlphaMod(s->surface, &am);
		op2->integer.v = am;
	}
	else if(!strcmp(is, "colorkey"))
	{
		Uint32 ck;
		if(SDL_GetColorKey(s->surface, &ck))
			return EEL_XWRONGINDEX;
		op2->integer.v = ck;
	}
	else if(!strcmp(is, "palette"))
	{
		op2->integer.v = s->surface->format->palette ? 1 : 0;
		op2->type = EEL_TBOOLEAN;
		return 0;
	}
	else if(!strcmp(is, "BitsPerPixel"))
		op2->integer.v = s->surface->format->BitsPerPixel;
	else if(!strcmp(is, "BytesPerPixel"))
		op2->integer.v = s->surface->format->BytesPerPixel;
	else if(!strcmp(is, "Rmask"))
		op2->integer.v = s->surface->format->Rmask;
	else if(!strcmp(is, "Gmask"))
		op2->integer.v = s->surface->format->Gmask;
	else if(!strcmp(is, "Bmask"))
		op2->integer.v = s->surface->format->Bmask;
	else if(!strcmp(is, "Amask"))
		op2->integer.v = s->surface->format->Amask;
	else
		return EEL_XWRONGINDEX;
	op2->type = EEL_TINTEGER;
	return 0;
}


/*----------------------------------------------------------
	SurfaceLock class
----------------------------------------------------------*/
static EEL_xno sl_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_surfacelock *sl;
	ESDL_surface *s;
	EEL_object *eo;
	if(initc != 1)
		return EEL_XARGUMENTS;
	if(EEL_TYPE(initv) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	eo = eel_o_alloc(vm, sizeof(ESDL_surfacelock), type);
	if(!eo)
		return EEL_XMEMORY;
	sl = o2ESDL_surfacelock(eo);
	sl->surface = initv->objref.v;
	eel_own(sl->surface);
	s = o2ESDL_surface(sl->surface);
	SDL_LockSurface(s->surface);
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno sl_destruct(EEL_object *eo)
{
	ESDL_surface *s;
	ESDL_surfacelock *sl = o2ESDL_surfacelock(eo);
	if(!sl->surface)
		return 0;
	s = o2ESDL_surface(sl->surface);
	SDL_UnlockSurface(s->surface);
	eel_disown(sl->surface);
	return 0;
}


static EEL_xno sl_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ESDL_surfacelock *sl = o2ESDL_surfacelock(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 's':
		if(sl->surface)
		{
			eel_o2v(op2, sl->surface);
			eel_own(sl->surface);
		}
		else
			eel_nil2v(op2);
		return 0;
	  default:
		return EEL_XWRONGINDEX;
	}
}


static EEL_xno esdl_LockSurface(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_TYPE(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	return eel_o_construct(vm, esdl_md.surfacelock_cid, arg, 1,
			vm->heap + vm->resv);
}


static EEL_xno esdl_UnlockSurface(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	ESDL_surfacelock *sl;
	ESDL_surface *s;
	if(EEL_TYPE(arg) != esdl_md.surfacelock_cid)
		return EEL_XWRONGTYPE;
	sl = o2ESDL_surfacelock(arg->objref.v);
	if(!sl->surface)
		return 0;
	s = o2ESDL_surface(sl->surface);
	SDL_UnlockSurface(s->surface);
	eel_disown(sl->surface);
	sl->surface = NULL;
	return 0;
}


/*----------------------------------------------------------
	Joystick input
----------------------------------------------------------*/

static void esdl_detach_joysticks(void)
{
	ESDL_joystick *j = esdl_md.joysticks;
	while(j)
	{
		j->joystick = NULL;
		j = j->next;
	}
}

static EEL_xno esdl_DetectJoysticks(EEL_vm *vm)
{
	esdl_detach_joysticks();
	if(SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	eel_l2v(vm->heap + vm->resv, SDL_NumJoysticks());
	return 0;
}


static EEL_xno esdl_NumJoysticks(EEL_vm *vm)
{
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	eel_l2v(vm->heap + vm->resv, SDL_NumJoysticks());
	return 0;
}


static EEL_xno esdl_JoystickName(EEL_vm *vm)
{
	const char *s;
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	s = SDL_JoystickNameForIndex(eel_v2l(vm->heap + vm->argv));
	if(!s)
		return EEL_XWRONGINDEX;
	eel_s2v(vm, vm->heap + vm->resv, s);
	return 0;
}


static EEL_xno j_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_joystick *j;
	EEL_object *eo;
	int ind;
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	switch(initc)
	{
	  case 0:
		ind = 0;
		break;
	  case 1:
		ind = eel_v2l(initv);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
#if 0
	if(SDL_JoystickOpened(ind))
		return EEL_XDEVICEOPENED;
#endif
	eo = eel_o_alloc(vm, sizeof(ESDL_joystick), type);
	if(!eo)
		return EEL_XMEMORY;
	j = o2ESDL_joystick(eo);
	j->index = ind;
	j->joystick = SDL_JoystickOpen(j->index);
	j->next = esdl_md.joysticks;
	esdl_md.joysticks = j;
	if(!j->joystick)
	{
		eel_o_free(eo);
		return EEL_XDEVICEERROR;
	}
	j->name = eel_ps_new(vm, SDL_JoystickName(j->joystick));
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno j_destruct(EEL_object *eo)
{
	ESDL_joystick *j = o2ESDL_joystick(eo);
	ESDL_joystick *js = esdl_md.joysticks;
	while(js)
	{
		ESDL_joystick *jp;
		if(js == j)
		{
			if(js == esdl_md.joysticks)
				esdl_md.joysticks = js->next;
			else
				jp->next = js->next;
			break;
		}
		jp = js;
		js = js->next;
	}
	if(j->joystick && SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_JoystickClose(j->joystick);
	eel_disown(j->name);
	return 0;
}


static EEL_xno j_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ESDL_joystick *j = o2ESDL_joystick(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(!strcmp(is, "name"))
		eel_o2v(op2, j->name);
	else if(!strcmp(is, "axes"))
		eel_l2v(op2, SDL_JoystickNumAxes(j->joystick));
	else if(!strcmp(is, "buttons"))
		eel_l2v(op2, SDL_JoystickNumButtons(j->joystick));
	else if(!strcmp(is, "balls"))
		eel_l2v(op2, SDL_JoystickNumBalls(j->joystick));
	else
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	Window class
----------------------------------------------------------*/

static EEL_xno w_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_window *win;
	EEL_object *eo;
	const char *title;
	int x, y, w, h;
	Uint32 flags = 0;
	switch(initc)
	{
	  case 6:
		flags = eel_v2l(initv + 5);
		/* Fall-trough */
	  case 5:
		title = eel_v2s(initv);
		x = eel_v2l(initv + 1);
		y = eel_v2l(initv + 2);
		w = eel_v2l(initv + 3);
		h = eel_v2l(initv + 4);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(ESDL_window), type)))
		return EEL_XMEMORY;
	win = o2ESDL_window(eo);
	if(!(win->window = SDL_CreateWindow(title, x, y, w, h, flags)))
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno w_destruct(EEL_object *eo)
{
	SDL_DestroyWindow(o2ESDL_window(eo)->window);
	return 0;
}


/*----------------------------------------------------------
	Renderer class
----------------------------------------------------------*/

static EEL_xno rn_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_renderer *r;
	EEL_object *eo;
	ESDL_window *win;
	int drv = -1;
	Uint32 flags = 0;
	switch(initc)
	{
	  case 3:
		flags = eel_v2l(initv + 2);
		/* Fall-trough */
	  case 2:
		drv = eel_v2l(initv + 1);
		/* Fall-trough */
	  case 1:
		if(EEL_TYPE(initv) != esdl_md.window_cid)
			return EEL_XWRONGTYPE;
		win = o2ESDL_window(initv->objref.v);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(ESDL_renderer), type)))
		return EEL_XMEMORY;
	r = o2ESDL_renderer(eo);
	if(!(r->renderer = SDL_CreateRenderer(win->window, drv, flags)))
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno rn_destruct(EEL_object *eo)
{
	SDL_DestroyRenderer(o2ESDL_renderer(eo)->renderer);
	return 0;
}


/*----------------------------------------------------------
	SDL Calls
----------------------------------------------------------*/

static EEL_xno esdl_SetWindowTitle(EEL_vm *vm)
{
	SDL_Window *win;
	const char *title;
	ESDL_ARG_WINDOW(0, win)
	ESDL_ARG_STRING(1, title)
	SDL_SetWindowTitle(win, title);
	return 0;
}


static EEL_xno esdl_ShowCursor(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv, SDL_ShowCursor(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


static EEL_xno esdl_WarpMouse(EEL_vm *vm)
{
	int x, y;
	SDL_Window *win;
	ESDL_ARG_INTEGER(0, x)
	ESDL_ARG_INTEGER(1, y)
	ESDL_OPTARG_WINDOW(2, win, NULL);
	if(win)
		SDL_WarpMouseInWindow(win, x, y);
	else
		SDL_WarpMouseGlobal(x, y);
	return 0;
}


static EEL_xno esdl_SetWindowGrab(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ESDL_window *win;
	if(EEL_TYPE(args) != esdl_md.window_cid)
		return EEL_XWRONGTYPE;
	win = o2ESDL_window(args[0].objref.v);
	SDL_SetWindowGrab(win->window, eel_v2l(args + 1));
	return 0;
}


static EEL_xno esdl_GetTicks(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv, SDL_GetTicks());
	return 0;
}


static EEL_xno esdl_Delay(EEL_vm *vm)
{
	Uint32 d = eel_v2l(vm->heap + vm->argv);
#ifdef _WIN32
	SDL_Delay(d);
#else
	if(!d)
		sched_yield();
	else
		SDL_Delay(d);
#endif
	return 0;
}


static EEL_xno esdl_RenderPresent(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ESDL_renderer *rn;
	if(EEL_TYPE(args) != esdl_md.renderer_cid)
		return EEL_XWRONGTYPE;
	rn = o2ESDL_renderer(args[0].objref.v);
	SDL_RenderPresent(rn->renderer);
	return 0;
}


static EEL_xno esdl_SetClipRect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Surface *s;
	SDL_Rect *r = NULL;
	if(EEL_TYPE(args) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(args->objref.v)->surface;
	if((vm->argc >= 2) && (EEL_TYPE(args + 1) != EEL_TNIL))
	{
		if(EEL_TYPE(args + 1) != esdl_md.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2SDL_Rect(args[1].objref.v);
	}
	SDL_SetClipRect(s, r);
	return 0;
}


static EEL_xno esdl_UpdateWindowSurface(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ESDL_window *win;
	if(EEL_TYPE(args) != esdl_md.window_cid)
		return EEL_XWRONGTYPE;
	win = o2ESDL_window(args[0].objref.v);
	if(SDL_UpdateWindowSurface(win->window) < 0)
		return EEL_XDEVICECONTROL;
	return 0;
}


static EEL_xno esdl_UpdateWindowSurfaceRects(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value *args = vm->heap + vm->argv;
	ESDL_window *win;
	if(EEL_TYPE(args) != esdl_md.window_cid)
		return EEL_XWRONGTYPE;
	win = o2ESDL_window(args[0].objref.v);
#if 0
	SDL_Rect cr;
	cr.x = 0;
	cr.y = 0;
	cr.w = s->w;
	cr.h = s->h;
#endif
	if(EEL_TYPE(args + 1) == esdl_md.rect_cid)
	{
		SDL_Rect r = *o2SDL_Rect(args[1].objref.v);
#if 0
		clip_rect(&r, &cr);
#endif
		SDL_UpdateWindowSurfaceRects(win->window, &r, 1);
	}
	else if((EEL_classes)EEL_TYPE(args + 1) == EEL_CARRAY)
	{
		int i, len;
		EEL_value v;
		EEL_object *a = args[1].objref.v;
		SDL_Rect *ra;
		len = eel_length(a);
		if(len < 0)
			return EEL_XCANTINDEX;
		ra = eel_malloc(vm, len * sizeof(SDL_Rect));
		if(!ra)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
		{
			x = eel_getlindex(a, i, &v);
			if(x)
			{
				eel_free(vm, ra);
				return x;
			}
			if(EEL_TYPE(&v) != esdl_md.rect_cid)
			{
				eel_v_disown(&v);
				continue;	/* Ignore... */
			}
			ra[i] = *o2SDL_Rect(v.objref.v);
#if 0
			clip_rect(&ra[i], &cr);
#endif
			eel_disown(v.objref.v);
		}
		SDL_UpdateWindowSurfaceRects(win->window, ra, len);
		eel_free(vm, ra);
	}
	else
		return EEL_XWRONGTYPE;
	return 0;
}


static EEL_xno esdl_BlitSurface(EEL_vm *vm)
{
	SDL_Surface *from, *to;
	SDL_Rect *fromr, *tor;
	ESDL_ARG_SURFACE(0, from)
	ESDL_ARG_RECT(1, fromr)
	ESDL_ARG_SURFACE(2, to)
	ESDL_OPTARG_RECT(3, tor, NULL)
	switch(SDL_BlitSurface(from, fromr, to, tor))
	{
	  case -1:
		return EEL_XDEVICEWRITE;
	  case -2:
		return EEL_XEOF;	/* Lost surface! (DirectX) */
	}
	return 0;
}


static EEL_xno esdl_FillRect(EEL_vm *vm)
{
	SDL_Surface *to;
	SDL_Rect *tor;
	int color;
	ESDL_ARG_SURFACE(0, to)
	ESDL_OPTARGNIL_RECT(1, tor, NULL)
	ESDL_OPTARG_INTEGER(2, color, 0)
	if(SDL_FillRect(to, tor, color) < 0)
		return EEL_XDEVICEWRITE;
	return 0;
}


static EEL_xno esdl_SetSurfaceAlphaMod(EEL_vm *vm)
{
	SDL_Surface *s;
	Uint8 a;
	ESDL_ARG_SURFACE(0, s)
	ESDL_OPTARG_INTEGER(1, a, 255)
	if(SDL_SetSurfaceAlphaMod(s, a) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static EEL_xno esdl_SetSurfaceColorMod(EEL_vm *vm)
{
	SDL_Surface *s;
	Uint8 r, g, b;
	ESDL_ARG_SURFACE(0, s)
	ESDL_OPTARG_INTEGER(1, r, 255)
	ESDL_OPTARG_INTEGER(2, g, 255)
	ESDL_OPTARG_INTEGER(3, b, 255)
	if(SDL_SetSurfaceColorMod(s, r, g, b) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static EEL_xno esdl_SetColorKey(EEL_vm *vm)
{
	SDL_Surface *s;
	int flag;
	Uint32 key;
	ESDL_ARG_SURFACE(0, s)
	ESDL_ARG_INTEGER(1, flag)
	ESDL_ARG_INTEGER(2, key)
	if(SDL_SetColorKey(s, flag, key) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static inline void esdl_raw2color(Uint32 r, SDL_Color *c)
{
	c->a = (r >> 24) & 0xff;
	c->r = (r >> 16) & 0xff;
	c->g = (r >> 8) & 0xff;
	c->b = r & 0xff;
}


/* function SetColors(surface, colors)[firstcolor] */
static EEL_xno esdl_SetColors(EEL_vm *vm)
{
	SDL_Surface *s;
	SDL_Palette *p;
	Uint32 first;
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	if(EEL_TYPE(args) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(args->objref.v)->surface;
	p = s->format->palette;
	if(!p)
		return EEL_XDEVICEWRITE;
	if(vm->argc >= 3)
		first = eel_v2l(args + 2);
	else
		first = 0;
	switch((EEL_classes)EEL_TYPE(args + 1))
	{
	  case EEL_TINTEGER:
	  {
		/* Single color value */
		SDL_Color c;
		esdl_raw2color(args[1].integer.v, &c);
		eel_b2v(res, SDL_SetPaletteColors(p, &c, first, 1));
		return 0;
	  }
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  {
		/* Vector of color values */
		int i;
		SDL_Color *c;
		EEL_object *o = eel_v2o(args + 1);
		Uint32 *rd;
		int len = eel_length(o);
		if(len < 1)
			return EEL_XFEWITEMS;
		rd = eel_rawdata(o);
		if(!rd)
			return EEL_XCANTREAD;
		c = (SDL_Color *)eel_malloc(vm, sizeof(SDL_Color) * len);
		if(!c)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
			esdl_raw2color(rd[i], c + i);
		eel_b2v(res, SDL_SetPaletteColors(p, c, first, len));
		eel_free(vm, c);
		return 0;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno esdl_MapColor(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	SDL_Surface *s;
	Uint32 color;
	int r, g, b;
	int a = -1;
	if(EEL_TYPE(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(arg->objref.v)->surface;
	if(vm->argc == 2)
	{
		// Packed "hex" ARGB color
		int c = eel_v2l(arg + 1);
		a = (c >> 24) & 0xff;
		r = (c >> 16) & 0xff;
		g = (c >> 8) & 0xff;
		b = c & 0xff;
	}
	else if(vm->argc >= 4)
	{
		// Individual R, G, B, and (optionally) A arguments
		r = eel_v2l(arg + 1);
		if(r < 0)
			r = 0;
		else if(r > 255)
			r = 255;
		g = eel_v2l(arg + 2);
		if(g < 0)
			g = 0;
		else if(g > 255)
			g = 255;
		b = eel_v2l(arg + 3);
		if(b < 0)
			b = 0;
		else if(b > 255)
			b = 255;
		if(vm->argc >= 5)
		{
			// A, if specified
			a = eel_v2l(arg + 4);
			if(a < 0)
				a = 0;
			else if(a > 255)
				a = 255;
		}
	}
	else
		return EEL_XARGUMENTS;
	if(a >= 0)
		color = SDL_MapRGBA(s->format, r, g, b, a);
	else
		color = SDL_MapRGB(s->format, r, g, b);
	vm->heap[vm->resv].type = EEL_TINTEGER;
	vm->heap[vm->resv].integer.v = color;
	return 0;
}


static EEL_xno esdl_GetColor(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	SDL_Surface *s;
	Uint8 r, g, b, a;
	if(EEL_TYPE(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(arg->objref.v)->surface;
	SDL_GetRGBA(eel_v2l(arg + 1), s->format, &r, &g, &b, &a);
	vm->heap[vm->resv].type = EEL_TINTEGER;
	vm->heap[vm->resv].integer.v = rgba2c(r, g, b, a);
	return 0;
}


static EEL_xno esdl_Plot(EEL_vm *vm)
{
	EEL_xno res;
	int i, xmin, ymin, xmax, ymax, locked;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	Uint32 color = eel_v2l(arg + 1);
	if((res = get_surface(arg, &locked, &s)))
		return res;
	xmin = s->clip_rect.x;
	ymin = s->clip_rect.y;
	xmax = xmin + s->clip_rect.w;
	ymax = ymin + s->clip_rect.h;
	switch(s->format->BytesPerPixel)
	{
	  case 1:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		int ppitch = s->pitch;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	  case 2:
	  {
		Uint16 *p = (Uint16 *)s->pixels;
		int ppitch = s->pitch / 2;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	  case 3:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		for(i = 2; i < vm->argc; i += 2)
		{
			Uint8 *p24;
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p24 = p + y * s->pitch + x * 3;
#if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
			p24[0] = color;
			p24[1] = color >> 8;
			p24[2] = color >> 16;
#else
			p24[0] = color >> 16;
			p24[1] = color >> 8;
			p24[2] = color;
#endif
		}
		break;
	  }
	  case 4:
	  {
		Uint32 *p = (Uint32 *)s->pixels;
		int ppitch = s->pitch / 4;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	}
	if(locked)
		SDL_UnlockSurface(s);
	return 0;
}


/* function GetPixel(surface, x, y)[clipreturn] */
static EEL_xno esdl_GetPixel(EEL_vm *vm)
{
	EEL_xno res;
	int x, y, locked;
	Uint32 color = 0;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	if((res = get_surface(arg, &locked, &s)))
		return res;
	x = eel_v2l(arg + 1);
	y = eel_v2l(arg + 2);
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
	{
		/* Clip! Fail, or return 'clipreturn'. */
		if(locked)
			SDL_UnlockSurface(s);
		if(vm->argc >= 4)
		{
			vm->heap[vm->resv] = arg[3];
			return 0;
		}
		else
			return EEL_XWRONGINDEX;
	}

	switch(s->format->BytesPerPixel)
	{
	  case 1:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		int ppitch = s->pitch;
		color = p[y * ppitch + x];
		break;
	  }
	  case 2:
	  {
		Uint16 *p = (Uint16 *)s->pixels;
		int ppitch = s->pitch / 2;
		color = p[y * ppitch + x];
		break;
	  }
	  case 3:
		color = getpixel24(s, x, y);
		break;
	  case 4:
		color = getpixel32(s, x, y);
		break;
	}
	if(locked)
		SDL_UnlockSurface(s);
	eel_l2v(vm->heap + vm->resv, color);
	return 0;
}


/*
 * function InterPixel(surface, x, y)[clipcolor]
 *
 *	'surface' needs to be a 24 (8:8:8) or 32 (8:8:8:8) bpp Surface[Lock].
 *
 *	'x' and 'y' can be integer or real coordinates.
 *
 *	'clipcolor' is the color used for off-surface pixels; 0 (black,
 *	transparent) if not specified.
 */
static EEL_xno esdl_InterPixel(EEL_vm *vm)
{
	EEL_xno res = 0;
	float x, y;
	int ix, iy, locked, w[4], r, g, b, a;
	Uint32 fx, fy, clipcolor;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	if((res = get_surface(arg, &locked, &s)))
		return res;
	x = eel_v2d(arg + 1);
	y = eel_v2d(arg + 2);
	if(vm->argc >= 4)
		clipcolor = eel_v2l(arg + 3);
	else
		clipcolor = 0;
	ix = floor(x);
	iy = floor(y);

	if((ix < -1) || (iy < -1) || (ix >= s->w - 1) || (iy >= s->h - 1))
	{
		/* Full clip! */
		if(locked)
			SDL_UnlockSurface(s);
		eel_l2v(vm->heap + vm->resv, clipcolor);
		return 0;
	}

	fx = (x - ix) * 32767.9f;
	fy = (y - iy) * 32767.9f;
	r = g = b = a = 0;
	w[0] = (32768 - fx) * (32768 - fy) >> 14;
	w[1] = fx * (32768 - fy) >> 14;
	w[2] = (32768 - fx) * fy >> 14;
	w[3] = fx * fy >> 14;

	if((ix >= 0) && (iy >= 0) && (ix < s->w - 1) && (iy < s->h - 1))
	{
		/* No clip! */
		switch(s->format->BytesPerPixel)
		{
		  case 3:
			maccrgb(getpixel24(s, x, y),
					w[0], &r, &g, &b);
			maccrgb(getpixel24(s, x + 1, y),
					w[1], &r, &g, &b);
			maccrgb(getpixel24(s, x, y + 1),
					w[2], &r, &g, &b);
			maccrgb(getpixel24(s, x + 1, y + 1),
					w[3], &r, &g, &b);
			break;
		  case 4:
			maccrgba(getpixel32(s, x, y),
					w[0], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x + 1, y),
					w[1], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x, y + 1),
					w[2], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x + 1, y + 1),
					w[3], &r, &g, &b, &a);
			break;
		  default:
			if(locked)
				SDL_UnlockSurface(s);
			return EEL_XWRONGFORMAT;
		}
	}
	else
	{
		/* Partial clip! */
		switch(s->format->BytesPerPixel)
		{
		  case 3:
			maccrgb(getpixel24c(s, ix, iy, clipcolor),
					w[0], &r, &g, &b);
			maccrgb(getpixel24c(s, ix + 1, iy, clipcolor),
					w[1], &r, &g, &b);
			maccrgb(getpixel24c(s, ix, iy + 1, clipcolor),
					w[2], &r, &g, &b);
			maccrgb(getpixel24c(s, ix + 1, iy + 1, clipcolor),
					w[3], &r, &g, &b);
			break;
		  case 4:
			maccrgba(getpixel32c(s, ix, iy, clipcolor),
					w[0], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix + 1, iy, clipcolor),
					w[1], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix, iy + 1, clipcolor),
					w[2], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix + 1, iy + 1, clipcolor),
					w[3], &r, &g, &b, &a);
			break;
		  default:
			if(locked)
				SDL_UnlockSurface(s);
			return EEL_XWRONGFORMAT;
		}
	}

	if(locked)
		SDL_UnlockSurface(s);

	eel_l2v(vm->heap + vm->resv, rgba2c(r >> 8, g >> 8, b >> 8, a >> 8));
	return 0;
}


/*
 * function FilterPixel(surface, x, y, filter)[clipcolor]
 *
 *	'surface' needs to be a 24 (8:8:8) or 32 (8:8:8:8) bpp Surface[Lock].
 *
 *	'x' and 'y' can be integer or real coordinates.
TODO: Actually implement real coordinates with interpolation?
 *
 *	'filter' is a vector_s32, where [0, 1] hold the width and height of the
 *	filter core, followed by the filter core itself, expressed as 16:16
 *	fixed point weights.
 *
 *	'clipcolor' is the color used for off-surface pixels; 0 (black,
 *	transparent) if not specified.
 */
static EEL_xno esdl_FilterPixel(EEL_vm *vm)
{
	EEL_xno res = 0;
	int x0, y0, x, y, locked, r, g, b, a;
	Uint32 clipcolor;
	Uint32 *filter;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	x0 = eel_v2l(arg + 1);
	y0 = eel_v2l(arg + 2);
	if(EEL_TYPE(arg + 3) == (EEL_types)EEL_CVECTOR_S32)
	{
		EEL_object *o = eel_v2o(arg + 3);
		int len = eel_length(o);
		if(len < 2)
			return EEL_XWRONGFORMAT;	/* Expected w, h! */
		filter = eel_rawdata(o);
		if(!filter)
			return EEL_XCANTREAD;	/* (Can this even happen?) */
		if(filter[0] * filter[1] > len + 2)
			return EEL_XFEWITEMS;	/* Incomplete filter core! */
	}
	else
		return EEL_XWRONGTYPE;
	if(vm->argc >= 5)
		clipcolor = eel_v2l(arg + 4);
	else
		clipcolor = 0;
	if((res = get_surface(arg, &locked, &s)))
		return res;

	/* No optimized cases here. Just brute-force per-pixel clipping. */
	x0 -= filter[0] >> 1;
	y0 -= filter[1] >> 1;
	r = g = b = a = 0;
	switch(s->format->BytesPerPixel)
	{
	  case 3:
		for(y = 0; y < filter[1]; ++y)
		{
			Uint32 *frow = filter + 2 + y * filter[0];
			for(x = 0; x < filter[0]; ++x)
				maccrgb(getpixel24c(s, x0 + x, y0 + y,
						clipcolor),
						frow[x], &r, &g, &b);
		}
		break;
	  case 4:
		for(y = 0; y < filter[1]; ++y)
		{
			Uint32 *frow = filter + 2 + y * filter[0];
			for(x = 0; x < filter[0]; ++x)
				maccrgba(getpixel32c(s, x0 + x, y0 + y,
						clipcolor),
						frow[x], &r, &g, &b, &a);
		}
		break;
	  default:
		if(locked)
			SDL_UnlockSurface(s);
		return EEL_XWRONGFORMAT;
	}

	if(locked)
		SDL_UnlockSurface(s);

	eel_l2v(vm->heap + vm->resv, rgba2c(r >> 8, g >> 8, b >> 8, a >> 8));
	return 0;
}


/*----------------------------------------------------------
	Event handling
----------------------------------------------------------*/

static EEL_xno esdl_PumpEvents(EEL_vm *vm)
{
	FE_PumpEvents();
	return 0;
}


static EEL_xno esdl_CheckEvent(EEL_vm *vm)
{
	eel_b2v(vm->heap + vm->resv, FE_PollEvent(NULL));
	return 0;
}


static inline void esdl_setsocket(EEL_object *io, SDL_Event *ev)
{
	EEL_value v;
	EEL_object *s = eel_net_get_socket(io->vm, NET2_GetSocket(ev));
	if(s)
		eel_o2v(&v, s);
	else
		eel_nil2v(&v);
	eel_setsindex(io, "socket", &v);
}

static EEL_xno esdl_decode_event(EEL_vm *vm, SDL_Event *ev)
{
	EEL_value v;
	EEL_object *t;
	EEL_xno x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return x;
	t = eel_v2o(&v);
	esdl_seti(t, "type", ev->type);
	esdl_seti(t, "timestamp", ev->common.timestamp);
	switch(ev->type)
	{
	  case SDL_WINDOWEVENT:
		esdl_seti(t, "windowID", ev->window.windowID);
		esdl_seti(t, "event", ev->window.event);
		esdl_seti(t, "data1", ev->window.data1);
		esdl_seti(t, "data2", ev->window.data2);
		break;
	  case SDL_KEYDOWN:
	  case SDL_KEYUP:
		esdl_seti(t, "windowID", ev->key.windowID);
		esdl_seti(t, "state", ev->key.state);
		esdl_seti(t, "scancode", ev->key.keysym.scancode);
		esdl_seti(t, "sym", ev->key.keysym.sym);
		esdl_seti(t, "mod", ev->key.keysym.mod);
		break;
	  case SDL_MOUSEMOTION:
		esdl_seti(t, "which", ev->motion.which);
		esdl_seti(t, "state", ev->motion.state);
		esdl_seti(t, "x", ev->motion.x);
		esdl_seti(t, "y", ev->motion.y);
		esdl_seti(t, "xrel", ev->motion.xrel);
		esdl_seti(t, "yrel", ev->motion.yrel);
		break;
	  case SDL_MOUSEBUTTONDOWN:
	  case SDL_MOUSEBUTTONUP:
		esdl_seti(t, "which", ev->button.which);
		esdl_seti(t, "button", ev->button.button);
		esdl_seti(t, "state", ev->button.state);
		esdl_seti(t, "x", ev->button.x);
		esdl_seti(t, "y", ev->button.y);
		break;
	  case SDL_JOYAXISMOTION:
		esdl_seti(t, "which", ev->jaxis.which);
		esdl_seti(t, "axis", ev->jaxis.axis);
		esdl_seti(t, "value", ev->jaxis.value);
		break;
	  case SDL_JOYBALLMOTION:
		esdl_seti(t, "which", ev->jball.which);
		esdl_seti(t, "ball", ev->jball.ball);
		esdl_seti(t, "xrel", ev->jball.xrel);
		esdl_seti(t, "yrel", ev->jball.yrel);
		break;
	  case SDL_JOYHATMOTION:
		esdl_seti(t, "which", ev->jhat.which);
		esdl_seti(t, "hat", ev->jhat.hat);
		esdl_seti(t, "value", ev->jhat.value);
		break;
	  case SDL_JOYBUTTONDOWN:
	  case SDL_JOYBUTTONUP:
		esdl_seti(t, "which", ev->jbutton.which);
		esdl_seti(t, "button", ev->jbutton.button);
		esdl_seti(t, "state", ev->jbutton.state);
		break;
	  case SDL_USEREVENT:
		switch(NET2_GetEventType(ev))
		{
		  case NET2_ERROREVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			eel_s2v(vm, &v, NET2_GetEventError(ev));
			eel_setsindex(t, "error", &v);
			break;
		  case NET2_TCPACCEPTEVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			esdl_seti(t, "port", NET2_GetEventData(ev));
			break;
		  case NET2_TCPRECEIVEEVENT:
		  case NET2_TCPCLOSEEVENT:
		  case NET2_UDPRECEIVEEVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			break;
		  default:
			esdl_seti(t, "code", ev->user.code);
			break;
		}
		break;
	  case SDL_QUIT:
	  default:
		break;
	}
	eel_o2v(vm->heap + vm->resv, t);
	return 0;
}


static EEL_xno esdl_PollEvent(EEL_vm *vm)
{
	SDL_Event ev;
	if(!FE_PollEvent(&ev))
	{
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	return esdl_decode_event(vm, &ev);
}


static EEL_xno esdl_WaitEvent(EEL_vm *vm)
{
	SDL_Event ev;
	if(!FE_WaitEvent(&ev))
		return EEL_XDEVICEERROR;
	return esdl_decode_event(vm, &ev);
}


static EEL_xno esdl_EventState(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	Uint8 etype = eel_v2l(args);
	Uint8 res;
	if(vm->argc >= 2)
		res = SDL_EventState(etype, eel_v2l(args + 1));
	else
		res = SDL_EventState(etype, SDL_QUERY);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}


static EEL_xno esdl_StartTextInput(EEL_vm *vm)
{
	SDL_StartTextInput();
	return 0;
}


static EEL_xno esdl_StopTextInput(EEL_vm *vm)
{
	SDL_StopTextInput();
	return 0;
}


/*----------------------------------------------------------
	Simple audio interface
----------------------------------------------------------*/

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
	int samples_out = len / 4;
	int samples_in = sfifo_used(&esdl_md.audiofifo) / 4;
	if(samples_in > samples_out)
		samples_in = samples_out;
	sfifo_read(&esdl_md.audiofifo, stream, samples_in * 4);
	if(samples_in < samples_out)
		memset(stream + samples_in * 4, 0, (samples_out - samples_in) * 4);
	esdl_md.audio_pos += samples_out;
}


/* procedure OpenAudio(samplerate, buffersize, fifosize); */
static EEL_xno esdl_OpenAudio(EEL_vm *vm)
{
	SDL_AudioSpec aspec;
	EEL_value *args = vm->heap + vm->argv;
	if(esdl_md.audio_open)
		return EEL_XDEVICEOPENED;
	if(sfifo_init(&esdl_md.audiofifo, eel_v2l(args + 2) * 4) < 0)
		return EEL_XDEVICEOPEN;
	aspec.freq = eel_v2l(args);
	aspec.format = AUDIO_S16SYS;
	aspec.channels = 2;
	aspec.samples = eel_v2l(args + 1);
	aspec.callback = audio_callback;
	if(SDL_OpenAudio(&aspec, NULL) < 0)
	{
		sfifo_close(&esdl_md.audiofifo);
		return EEL_XDEVICEOPEN;
	}
	esdl_md.audio_pos = 0;
	esdl_md.audio_open = 1;
	SDL_PauseAudio(0);
	return 0;
}


static void audio_close(void)
{
	if(!esdl_md.audio_open)
		return;
	SDL_CloseAudio();
	sfifo_close(&esdl_md.audiofifo);
	esdl_md.audio_open = 0;
}


static EEL_xno esdl_CloseAudio(EEL_vm *vm)
{
	audio_close();
	return 0;
}


static inline Sint16 get_sample(EEL_value *v)
{
	if(v->type == EEL_TINTEGER)
		return v->integer.v >> 16;
	else
	{
		double s = eel_v2d(v) * 32768.0f;
		if(s < -32768.0f)
			return -32768;
		else if(s > 32767.0f)
			return 32767;
		else
			return (Sint16)s;
	}
}

static EEL_xno esdl_PlayAudio(EEL_vm *vm)
{
	Sint16 s[2];
	EEL_value *args = vm->heap + vm->argv;
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	if(sfifo_space(&esdl_md.audiofifo) < sizeof(s))
		return EEL_XDEVICEWRITE;
	if(EEL_IS_OBJREF(args[0].type))
	{
		EEL_value iv;
		EEL_object *o0 = args[0].objref.v;
		long len = eel_length(o0);
		EEL_object *o1 = NULL;
		eel_l2v(&iv, 0);
		if(vm->argc >= 2)
		{
			if(!EEL_IS_OBJREF(args[1].type))
				return EEL_XNEEDOBJECT;
			o1 = args[1].objref.v;
			if(eel_length(o1) < 0)
				return EEL_XWRONGTYPE;
		}
		for(iv.integer.v = 0; iv.integer.v < len; ++iv.integer.v)
		{
			EEL_value v;
			EEL_xno x = eel_o_metamethod(o0, EEL_MM_GETINDEX,
					&iv, &v);
			if(x)
				return x;
			s[0] = get_sample(&v);
			eel_v_disown(&v);
			if(o1)
			{
				x = eel_o_metamethod(o1, EEL_MM_GETINDEX,
						&iv, &v);
				if(x)
					return x;
				s[1] = get_sample(&v);
				eel_v_disown(&v);
			}
			else
				s[1] = s[0];
			sfifo_write(&esdl_md.audiofifo, &s, sizeof(s));
		}
	}
	else
	{
		s[0] = get_sample(args);
		if(vm->argc >= 2)
			s[1] = get_sample(args + 1);
		else
			s[1] = s[0];
		sfifo_write(&esdl_md.audiofifo, &s, sizeof(s));
	}
	return 0;
}


static EEL_xno esdl_AudioPosition(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, esdl_md.audio_pos);
	return 0;
}


static EEL_xno esdl_AudioBuffer(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, sfifo_used(&esdl_md.audiofifo) / 4);
	return 0;
}


static EEL_xno esdl_AudioSpace(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, sfifo_space(&esdl_md.audiofifo) / 4);
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno esdl_sdl_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close the EEL state */
	if(closing)
	{
		esdl_detach_joysticks();
		if(esdl_md.audio_open)
			audio_close();
		eel_gl_dummy_calls();
		memset(&esdl_md, 0, sizeof(esdl_md));
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp esdl_constants[] =
{
	/* Endian handling */
	{"BYTEORDER",	SDL_BYTEORDER},
	{"BIG_ENDIAN",	SDL_BIG_ENDIAN},
	{"LIL_ENDIAN",	SDL_LIL_ENDIAN},

	/* Flags for Surface */
	{"SWSURFACE",	SDL_SWSURFACE},

	/* SDL event types */
	{"WINDOWEVENT",		SDL_WINDOWEVENT},
	{"KEYDOWN",		SDL_KEYDOWN},
	{"KEYUP",		SDL_KEYUP},
	{"MOUSEMOTION",		SDL_MOUSEMOTION},
	{"MOUSEBUTTONDOWN",	SDL_MOUSEBUTTONDOWN},
	{"MOUSEBUTTONUP",	SDL_MOUSEBUTTONUP},
	{"JOYAXISMOTION",	SDL_JOYAXISMOTION},
	{"JOYBALLMOTION",	SDL_JOYBALLMOTION},
	{"JOYHATMOTION",	SDL_JOYHATMOTION},
	{"JOYBUTTONDOWN",	SDL_JOYBUTTONDOWN},
	{"JOYBUTTONUP",		SDL_JOYBUTTONUP},
	{"QUIT",		SDL_QUIT},
	{"USEREVENT",		SDL_USEREVENT},

	/* Symbolic key codes */
#define	ESDL_SDLK(x)	{"K"#x,		SDLK_##x},
	ESDL_SDLK(UNKNOWN)
	ESDL_SDLK(RETURN)
	ESDL_SDLK(ESCAPE)
	ESDL_SDLK(BACKSPACE)
	ESDL_SDLK(TAB)
	ESDL_SDLK(SPACE)
	ESDL_SDLK(EXCLAIM)
	ESDL_SDLK(QUOTEDBL)
	ESDL_SDLK(HASH)
	ESDL_SDLK(PERCENT)
	ESDL_SDLK(DOLLAR)
	ESDL_SDLK(AMPERSAND)
	ESDL_SDLK(QUOTE)
	ESDL_SDLK(LEFTPAREN)
	ESDL_SDLK(RIGHTPAREN)
	ESDL_SDLK(ASTERISK)
	ESDL_SDLK(PLUS)
	ESDL_SDLK(COMMA)
	ESDL_SDLK(MINUS)
	ESDL_SDLK(PERIOD)
	ESDL_SDLK(SLASH)
	ESDL_SDLK(0)
	ESDL_SDLK(1)
	ESDL_SDLK(2)
	ESDL_SDLK(3)
	ESDL_SDLK(4)
	ESDL_SDLK(5)
	ESDL_SDLK(6)
	ESDL_SDLK(7)
	ESDL_SDLK(8)
	ESDL_SDLK(9)
	ESDL_SDLK(COLON)
	ESDL_SDLK(SEMICOLON)
	ESDL_SDLK(LESS)
	ESDL_SDLK(EQUALS)
	ESDL_SDLK(GREATER)
	ESDL_SDLK(QUESTION)
	ESDL_SDLK(AT)
	ESDL_SDLK(LEFTBRACKET)
	ESDL_SDLK(BACKSLASH)
	ESDL_SDLK(RIGHTBRACKET)
	ESDL_SDLK(CARET)
	ESDL_SDLK(UNDERSCORE)
	ESDL_SDLK(BACKQUOTE)
	ESDL_SDLK(a)
	ESDL_SDLK(b)
	ESDL_SDLK(c)
	ESDL_SDLK(d)
	ESDL_SDLK(e)
	ESDL_SDLK(f)
	ESDL_SDLK(g)
	ESDL_SDLK(h)
	ESDL_SDLK(i)
	ESDL_SDLK(j)
	ESDL_SDLK(k)
	ESDL_SDLK(l)
	ESDL_SDLK(m)
	ESDL_SDLK(n)
	ESDL_SDLK(o)
	ESDL_SDLK(p)
	ESDL_SDLK(q)
	ESDL_SDLK(r)
	ESDL_SDLK(s)
	ESDL_SDLK(t)
	ESDL_SDLK(u)
	ESDL_SDLK(v)
	ESDL_SDLK(w)
	ESDL_SDLK(x)
	ESDL_SDLK(y)
	ESDL_SDLK(z)
	ESDL_SDLK(CAPSLOCK)
	ESDL_SDLK(F1)
	ESDL_SDLK(F2)
	ESDL_SDLK(F3)
	ESDL_SDLK(F4)
	ESDL_SDLK(F5)
	ESDL_SDLK(F6)
	ESDL_SDLK(F7)
	ESDL_SDLK(F8)
	ESDL_SDLK(F9)
	ESDL_SDLK(F10)
	ESDL_SDLK(F11)
	ESDL_SDLK(F12)
	ESDL_SDLK(PRINTSCREEN)
	ESDL_SDLK(SCROLLLOCK)
	ESDL_SDLK(PAUSE)
	ESDL_SDLK(INSERT)
	ESDL_SDLK(HOME)
	ESDL_SDLK(PAGEUP)
	ESDL_SDLK(DELETE)
	ESDL_SDLK(END)
	ESDL_SDLK(PAGEDOWN)
	ESDL_SDLK(RIGHT)
	ESDL_SDLK(LEFT)
	ESDL_SDLK(DOWN)
	ESDL_SDLK(UP)
	ESDL_SDLK(NUMLOCKCLEAR)
	ESDL_SDLK(KP_DIVIDE)
	ESDL_SDLK(KP_MULTIPLY)
	ESDL_SDLK(KP_MINUS)
	ESDL_SDLK(KP_PLUS)
	ESDL_SDLK(KP_ENTER)
	ESDL_SDLK(KP_1)
	ESDL_SDLK(KP_2)
	ESDL_SDLK(KP_3)
	ESDL_SDLK(KP_4)
	ESDL_SDLK(KP_5)
	ESDL_SDLK(KP_6)
	ESDL_SDLK(KP_7)
	ESDL_SDLK(KP_8)
	ESDL_SDLK(KP_9)
	ESDL_SDLK(KP_0)
	ESDL_SDLK(KP_PERIOD)
	ESDL_SDLK(APPLICATION)
	ESDL_SDLK(POWER)
	ESDL_SDLK(KP_EQUALS)
	ESDL_SDLK(F13)
	ESDL_SDLK(F14)
	ESDL_SDLK(F15)
	ESDL_SDLK(F16)
	ESDL_SDLK(F17)
	ESDL_SDLK(F18)
	ESDL_SDLK(F19)
	ESDL_SDLK(F20)
	ESDL_SDLK(F21)
	ESDL_SDLK(F22)
	ESDL_SDLK(F23)
	ESDL_SDLK(F24)
	ESDL_SDLK(EXECUTE)
	ESDL_SDLK(HELP)
	ESDL_SDLK(MENU)
	ESDL_SDLK(SELECT)
	ESDL_SDLK(STOP)
	ESDL_SDLK(AGAIN)
	ESDL_SDLK(UNDO)
	ESDL_SDLK(CUT)
	ESDL_SDLK(COPY)
	ESDL_SDLK(PASTE)
	ESDL_SDLK(FIND)
	ESDL_SDLK(MUTE)
	ESDL_SDLK(VOLUMEUP)
	ESDL_SDLK(VOLUMEDOWN)
	ESDL_SDLK(KP_COMMA)
	ESDL_SDLK(KP_EQUALSAS400)
	ESDL_SDLK(ALTERASE)
	ESDL_SDLK(SYSREQ)
	ESDL_SDLK(CANCEL)
	ESDL_SDLK(CLEAR)
	ESDL_SDLK(PRIOR)
	ESDL_SDLK(RETURN2)
	ESDL_SDLK(SEPARATOR)
	ESDL_SDLK(OUT)
	ESDL_SDLK(OPER)
	ESDL_SDLK(CLEARAGAIN)
	ESDL_SDLK(CRSEL)
	ESDL_SDLK(EXSEL)
	ESDL_SDLK(KP_00)
	ESDL_SDLK(KP_000)
	ESDL_SDLK(THOUSANDSSEPARATOR)
	ESDL_SDLK(DECIMALSEPARATOR)
	ESDL_SDLK(CURRENCYUNIT)
	ESDL_SDLK(CURRENCYSUBUNIT)
	ESDL_SDLK(KP_LEFTPAREN)
	ESDL_SDLK(KP_RIGHTPAREN)
	ESDL_SDLK(KP_LEFTBRACE)
	ESDL_SDLK(KP_RIGHTBRACE)
	ESDL_SDLK(KP_TAB)
	ESDL_SDLK(KP_BACKSPACE)
	ESDL_SDLK(KP_A)
	ESDL_SDLK(KP_B)
	ESDL_SDLK(KP_C)
	ESDL_SDLK(KP_D)
	ESDL_SDLK(KP_E)
	ESDL_SDLK(KP_F)
	ESDL_SDLK(KP_XOR)
	ESDL_SDLK(KP_POWER)
	ESDL_SDLK(KP_PERCENT)
	ESDL_SDLK(KP_LESS)
	ESDL_SDLK(KP_GREATER)
	ESDL_SDLK(KP_AMPERSAND)
	ESDL_SDLK(KP_DBLAMPERSAND)
	ESDL_SDLK(KP_VERTICALBAR)
	ESDL_SDLK(KP_DBLVERTICALBAR)
	ESDL_SDLK(KP_COLON)
	ESDL_SDLK(KP_HASH)
	ESDL_SDLK(KP_SPACE)
	ESDL_SDLK(KP_AT)
	ESDL_SDLK(KP_EXCLAM)
	ESDL_SDLK(KP_MEMSTORE)
	ESDL_SDLK(KP_MEMRECALL)
	ESDL_SDLK(KP_MEMCLEAR)
	ESDL_SDLK(KP_MEMADD)
	ESDL_SDLK(KP_MEMSUBTRACT)
	ESDL_SDLK(KP_MEMMULTIPLY)
	ESDL_SDLK(KP_MEMDIVIDE)
	ESDL_SDLK(KP_PLUSMINUS)
	ESDL_SDLK(KP_CLEAR)
	ESDL_SDLK(KP_CLEARENTRY)
	ESDL_SDLK(KP_BINARY)
	ESDL_SDLK(KP_OCTAL)
	ESDL_SDLK(KP_DECIMAL)
	ESDL_SDLK(KP_HEXADECIMAL)
	ESDL_SDLK(LCTRL)
	ESDL_SDLK(LSHIFT)
	ESDL_SDLK(LALT)
	ESDL_SDLK(LGUI)
	ESDL_SDLK(RCTRL)
	ESDL_SDLK(RSHIFT)
	ESDL_SDLK(RALT)
	ESDL_SDLK(RGUI)
	ESDL_SDLK(MODE)
	ESDL_SDLK(AUDIONEXT)
	ESDL_SDLK(AUDIOPREV)
	ESDL_SDLK(AUDIOSTOP)
	ESDL_SDLK(AUDIOPLAY)
	ESDL_SDLK(AUDIOMUTE)
	ESDL_SDLK(MEDIASELECT)
	ESDL_SDLK(WWW)
	ESDL_SDLK(MAIL)
	ESDL_SDLK(CALCULATOR)
	ESDL_SDLK(COMPUTER)
	ESDL_SDLK(AC_SEARCH)
	ESDL_SDLK(AC_HOME)
	ESDL_SDLK(AC_BACK)
	ESDL_SDLK(AC_FORWARD)
	ESDL_SDLK(AC_STOP)
	ESDL_SDLK(AC_REFRESH)
	ESDL_SDLK(AC_BOOKMARKS)
	ESDL_SDLK(BRIGHTNESSDOWN)
	ESDL_SDLK(BRIGHTNESSUP)
	ESDL_SDLK(DISPLAYSWITCH)
	ESDL_SDLK(KBDILLUMTOGGLE)
	ESDL_SDLK(KBDILLUMDOWN)
	ESDL_SDLK(KBDILLUMUP)
	ESDL_SDLK(EJECT)
	ESDL_SDLK(SLEEP)
#undef	ESDL_SDLK

	/* Keyboard modifiers (masks for or:ing) */
	{"KMOD_NONE",		KMOD_NONE},
	{"KMOD_SHIFT",		KMOD_SHIFT},
	{"KMOD_LSHIFT",		KMOD_LSHIFT},
	{"KMOD_RSHIFT",		KMOD_RSHIFT},
	{"KMOD_CTRL",		KMOD_CTRL},
	{"KMOD_LCTRL",		KMOD_LCTRL},
	{"KMOD_RCTRL",		KMOD_RCTRL},
	{"KMOD_ALT",		KMOD_ALT},
	{"KMOD_LALT",		KMOD_LALT},
	{"KMOD_RALT",		KMOD_RALT},
	{"KMOD_GUI",		KMOD_GUI},
	{"KMOD_LGUI",		KMOD_LGUI},
	{"KMOD_RGUI",		KMOD_RGUI},
	{"KMOD_NUM",		KMOD_NUM},
	{"KMOD_CAPS",		KMOD_CAPS},
	{"KMOD_MODE",		KMOD_MODE},
	{"KMOD_RESERVED",	KMOD_RESERVED},

	/* Button states */
	{"PRESSED",		SDL_PRESSED},
	{"RELEASED",		SDL_RELEASED},

	/* Mouse buttons */
	{"BUTTON_LEFT",		SDL_BUTTON_LEFT},
	{"BUTTON_MIDDLE",	SDL_BUTTON_MIDDLE},
	{"BUTTON_RIGHT",	SDL_BUTTON_RIGHT},

	/* Joystick hat positions */
	{"HAT_LEFTUP",		SDL_HAT_LEFTUP},
	{"HAT_UP",		SDL_HAT_UP},
	{"HAT_RIGHTUP",		SDL_HAT_RIGHTUP},
	{"HAT_LEFT",		SDL_HAT_LEFT},
	{"HAT_CENTERED",	SDL_HAT_CENTERED},
	{"HAT_RIGHT",		SDL_HAT_RIGHT},
	{"HAT_LEFTDOWN",	SDL_HAT_LEFTDOWN},
	{"HAT_DOWN",		SDL_HAT_DOWN},
	{"HAT_RIGHTDOWN",	SDL_HAT_RIGHTDOWN},

	/* SDL_EventState */
	{"IGNORE",	SDL_IGNORE},
	{"DISABLE",	SDL_DISABLE},
	{"ENABLE",	SDL_ENABLE},
	{"QUERY",	SDL_QUERY},

	/* Alpha constants */
	{"ALPHA_TRANSPARENT",	SDL_ALPHA_TRANSPARENT},
	{"ALPHA_OPAQUE",	SDL_ALPHA_OPAQUE},

	{NULL, 0}
};


EEL_xno eel_sdl_init(EEL_vm *vm)
{
	EEL_object *c;
	EEL_object *m;

	if(loaded)
		return EEL_XDEVICEOPENED;

	m = eel_create_module(vm, "SDL", esdl_sdl_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	memset(&esdl_md, 0, sizeof(esdl_md));

	/* Types */
	c = eel_export_class(m, "Rect", EEL_COBJECT, r_construct, NULL, NULL);
	esdl_md.rect_cid = eel_class_typeid(c);
	eel_set_metamethod(c, EEL_MM_GETINDEX, r_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, r_setindex);
	eel_set_casts(vm, esdl_md.rect_cid, esdl_md.rect_cid, r_clone);

	c = eel_export_class(m, "Window", EEL_COBJECT,
			w_construct, w_destruct, NULL);
	esdl_md.window_cid = eel_class_typeid(c);

	c = eel_export_class(m, "Renderer", EEL_COBJECT,
			rn_construct, rn_destruct, NULL);
	esdl_md.renderer_cid = eel_class_typeid(c);

	c = eel_export_class(m, "Surface", EEL_COBJECT,
			s_construct, s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, s_getindex);
	esdl_md.surface_cid = eel_class_typeid(c);

	c = eel_export_class(m, "SurfaceLock", esdl_md.surface_cid,
			sl_construct, sl_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, sl_getindex);
	esdl_md.surfacelock_cid = eel_class_typeid(c);

	c = eel_export_class(m, "Joystick", EEL_COBJECT,
			j_construct, j_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, j_getindex);
	esdl_md.joystick_cid = eel_class_typeid(c);

	/* Windows and renderers */
	eel_export_cfunction(m, 0, "RenderPresent", 1, 0, 0,
			esdl_RenderPresent);
	eel_export_cfunction(m, 0, "SetWindowTitle", 2, 0, 0,
			esdl_SetWindowTitle);
	eel_export_cfunction(m, 0, "SetWindowGrab", 2, 0, 0,
			esdl_SetWindowGrab);

	/* Surfaces */
	eel_export_cfunction(m, 0, "SetClipRect", 0, 2, 0, esdl_SetClipRect);
	eel_export_cfunction(m, 0, "UpdateWindowSurface", 1, 0, 0,
			esdl_UpdateWindowSurface);
	eel_export_cfunction(m, 0, "UpdateWindowSurfaceRects", 2, 0, 0,
			esdl_UpdateWindowSurfaceRects);
	eel_export_cfunction(m, 0, "BlitSurface", 3, 1, 0, esdl_BlitSurface);
	eel_export_cfunction(m, 0, "FillRect", 1, 2, 0, esdl_FillRect);
	eel_export_cfunction(m, 1, "LockSurface", 1, 0, 0, esdl_LockSurface);
	eel_export_cfunction(m, 0, "UnlockSurface", 1, 0, 0,
			esdl_UnlockSurface);
	eel_export_cfunction(m, 0, "SetSurfaceAlphaMod", 1, 1, 0,
			esdl_SetSurfaceAlphaMod);
	eel_export_cfunction(m, 0, "SetSurfaceColorMod", 1, 3, 0,
			esdl_SetSurfaceColorMod);
	eel_export_cfunction(m, 0, "SetColorKey", 3, 0, 0, esdl_SetColorKey);
	eel_export_cfunction(m, 1, "SetColors", 2, 1, 0, esdl_SetColors);

	/* Timing */
	eel_export_cfunction(m, 1, "GetTicks", 0, 0, 0, esdl_GetTicks);
	eel_export_cfunction(m, 0, "Delay", 1, 0, 0, esdl_Delay);

	/* Mouse control */
	eel_export_cfunction(m, 1, "ShowCursor", 1, 0, 0, esdl_ShowCursor);
	eel_export_cfunction(m, 0, "WarpMouse", 2, 1, 0, esdl_WarpMouse);

	/* Joystick input */
	eel_export_cfunction(m, 0, "DetectJoysticks", 0, 0, 0, esdl_DetectJoysticks);
	eel_export_cfunction(m, 1, "NumJoysticks", 0, 0, 0, esdl_NumJoysticks);
	eel_export_cfunction(m, 1, "JoystickName", 1, 0, 0, esdl_JoystickName);

	/* EEL SDL extensions */
	eel_export_cfunction(m, 1, "MapColor", 2, 3, 0, esdl_MapColor);
	eel_export_cfunction(m, 1, "GetColor", 2, 0, 0, esdl_GetColor);
	eel_export_cfunction(m, 0, "Plot", 2, 0, 2, esdl_Plot);
	eel_export_cfunction(m, 1, "GetPixel", 3, 1, 0, esdl_GetPixel);
	eel_export_cfunction(m, 1, "InterPixel", 3, 1, 0, esdl_InterPixel);
	eel_export_cfunction(m, 1, "FilterPixel", 4, 1, 0, esdl_FilterPixel);

	/* Event handling */
	eel_export_cfunction(m, 0, "PumpEvents", 0, 0, 0, esdl_PumpEvents);
	eel_export_cfunction(m, 1, "CheckEvent", 0, 0, 0, esdl_CheckEvent);
	eel_export_cfunction(m, 1, "PollEvent", 0, 0, 0, esdl_PollEvent);
	eel_export_cfunction(m, 1, "WaitEvent", 0, 0, 0, esdl_WaitEvent);
	eel_export_cfunction(m, 1, "EventState", 1, 1, 0, esdl_EventState);
	eel_export_cfunction(m, 0, "StartTextInput", 0, 0, 0,
			esdl_StartTextInput);
	eel_export_cfunction(m, 0, "StopTextInput", 0, 0, 0,
			esdl_StopTextInput);

	/* Simple audio interface */
	eel_export_cfunction(m, 0, "OpenAudio", 3, 0, 0, esdl_OpenAudio);
	eel_export_cfunction(m, 0, "CloseAudio", 0, 0, 0, esdl_CloseAudio);
	eel_export_cfunction(m, 0, "PlayAudio", 1, 1, 0, esdl_PlayAudio);
	eel_export_cfunction(m, 1, "AudioPosition", 0, 0, 0, esdl_AudioPosition);
	eel_export_cfunction(m, 1, "AudioBuffer", 0, 0, 0, esdl_AudioBuffer);
	eel_export_cfunction(m, 1, "AudioSpace", 0, 0, 0, esdl_AudioSpace);

	/* Constants and enums */
	eel_export_lconstants(m, esdl_constants);

	loaded = 1;
	eel_disown(m);
	return 0;
}
