/*
  Based on zlib license - see http://www.gzip.org/zlib/zlib_license.html

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  "Philip D. Bober" <wildfire1138@mchsi.com>

  David Olofson <david@olofson.net> 2010:
	* 'compression' argument renamed as 'flags'
	* IMG_COMPRESS_DEFAULT constant is now 0xff instead of -1
	* Added IMG_UPSIDEDOWN for saving OpenGL screenshots and the like
	* Fixed mixed declarations and code in IMG_SavePNG_RW()

  David Olofson <david@olofson.net> 2011:
	* Included png.h for libPNG 1.5.x support.

  David Olofson <david@olofson.net> 2016:
	* Simplified colorkey handling code.

  David Olofson <david@olofson.net> 2017:
	* Renamed symbols to avoid conflicts with SDL2_image.
*/

/**
 * 4/17/04 - IMG_SavePNG & IMG_SavePNG_RW - Philip D. Bober
 * 11/08/2004 - Compr fix, levels -1,1-7 now work - Tyler Montbriand
 * 20091122 - Fixed temp_alpha "uninitialized warning" - David Olofson
 */
#include <stdlib.h>
#include "SDL.h"
#include <zlib.h>
#include <png.h>
#include "IMG_savepng.h"

int EEL_SavePNG(const char *file, SDL_Surface *surf, int flags)
{
	int ret;
	SDL_RWops *fp = SDL_RWFromFile(file, "wb");
	if(!fp)
		return -1;
	ret = EEL_SavePNG_RW(fp, surf, flags);
	SDL_RWclose(fp);
	return ret;
}

static void png_write_data(png_structp png_ptr,png_bytep data, png_size_t length)
{
	SDL_RWops *rp = (SDL_RWops*)png_get_io_ptr(png_ptr);
	SDL_RWwrite(rp, data, 1, length);
}

int EEL_SavePNG_RW(SDL_RWops *src, SDL_Surface *surf, int flags)
{
	png_structp png_ptr;
	png_infop info_ptr;
	SDL_PixelFormat *fmt = NULL;
	SDL_Surface *tempsurf = NULL;
	unsigned int i;
	Uint8 temp_alpha;
	png_colorp palette;
	int ret = -1;
	int funky_format = 0;
	int compression = flags & IMG_COMPRESS_MASK;
	int rp, drp;
	png_byte **row_pointers = NULL;
	png_ptr = NULL;
	info_ptr = NULL;
	palette = NULL;
	if(!src || !surf)
		goto savedone; /* Nothing to do. */

	row_pointers = (png_byte **)malloc(surf->h * sizeof(png_byte*));
	if(!row_pointers)
	{ 
		SDL_SetError("Couldn't allocate memory for rowpointers");
		goto savedone;
	}
	
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
	{
		SDL_SetError("Couldn't allocate memory for PNG file");
		goto savedone;
	}
	info_ptr= png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		SDL_SetError("Couldn't allocate image information for PNG file");
		goto savedone;
	}
	/* setup custom writer functions */
	png_set_write_fn(png_ptr, (voidp)src, png_write_data, NULL);

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		SDL_SetError("Unknown error writing PNG");
		goto savedone;
	}

	if(compression == Z_NO_COMPRESSION) // No compression
	{
		png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
		png_set_compression_level(png_ptr, Z_NO_COMPRESSION);
	}
        else if(compression == IMG_COMPRESS_DEFAULT) // Default compression
		png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
        else if(compression > Z_BEST_COMPRESSION)
		png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	else
		png_set_compression_level(png_ptr, compression);

	fmt = surf->format;
	if(fmt->BitsPerPixel == 8)
	{
		/* Paletted */
		Uint32 ck;
		png_set_IHDR(png_ptr, info_ptr,
			surf->w, surf->h, 8, PNG_COLOR_TYPE_PALETTE,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
		palette = (png_colorp)malloc(fmt->palette->ncolors *
				sizeof(png_color));
		if(!palette)
		{
			SDL_SetError("Couldn't create memory for palette");
			goto savedone;
		}
		for(i = 0; i < fmt->palette->ncolors; i++)
		{
			palette[i].red = fmt->palette->colors[i].r;
			palette[i].green = fmt->palette->colors[i].g;
			palette[i].blue = fmt->palette->colors[i].b;
		}
		png_set_PLTE(png_ptr, info_ptr, palette, fmt->palette->ncolors);
		if(SDL_GetColorKey(surf, &ck) == 0)
		{
			Uint8 palette_alpha[256];
			memset(palette_alpha, 255, sizeof(palette_alpha));
			palette_alpha[ck] = 0;
			png_set_tRNS(png_ptr, info_ptr, palette_alpha,
					sizeof(palette_alpha), NULL);
		}
	}
	else
	{
		/* Truecolor */
		if(fmt->Amask)
		{
			png_set_IHDR(png_ptr, info_ptr,
				surf->w, surf->h, 8, PNG_COLOR_TYPE_RGB_ALPHA,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				PNG_FILTER_TYPE_DEFAULT);
		}
		else
		{
			png_set_IHDR(png_ptr, info_ptr,
				surf->w, surf->h, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				PNG_FILTER_TYPE_DEFAULT);
		}
	}
	png_write_info(png_ptr, info_ptr);

	if(flags & IMG_UPSIDEDOWN)
	{
		rp = surf->h - 1;
		drp = -1;
	}
	else
	{
		rp = 0;
		drp = 1;
	}
	if(fmt->BitsPerPixel == 8)
	{
		/* Paletted */
		for(i = 0; i < surf->h; i++, rp += drp)
			row_pointers[rp] = ((png_byte*)surf->pixels) +
					i * surf->pitch;
		if(SDL_MUSTLOCK(surf))
			SDL_LockSurface(surf);
		png_write_image(png_ptr, row_pointers);
		if(SDL_MUSTLOCK(surf))
			SDL_UnlockSurface(surf);
	}
	else
	{
		/* Truecolor */
		if(fmt->BytesPerPixel == 3)
		{
			if(fmt->Amask) /* check for 24 bit with alpha */
				funky_format = 1;
			else
			{
				/* Check for RGB/BGR/GBR/RBG/etc surfaces.*/
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				if(fmt->Rmask != 0xFF0000 ||
						fmt->Gmask != 0x00FF00 ||
						fmt->Bmask != 0x0000FF)
#else
				if(fmt->Rmask != 0x0000FF ||
						fmt->Gmask != 0x00FF00 ||
						fmt->Bmask != 0xFF0000)
#endif
					funky_format = 1;
			}
		}
		else if(fmt->BytesPerPixel == 4)
		{
			if(!fmt->Amask) /* check for 32bit but no alpha */
				funky_format = 1;
			else
			{
				/* Check for ARGB/ABGR/GBAR/RABG/etc surfaces.*/
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				if(fmt->Rmask!=0xFF000000 ||
						fmt->Gmask!=0x00FF0000 ||
						fmt->Bmask!=0x0000FF00 ||
						fmt->Amask!=0x000000FF)
#else
				if(fmt->Rmask!=0x000000FF ||
						fmt->Gmask!=0x0000FF00 ||
						fmt->Bmask!=0x00FF0000 ||
						fmt->Amask!=0xFF000000)
#endif
					funky_format = 1;
			}
		}
		else /* 555 or 565 16 bit color */
			funky_format = 1;
		if(funky_format)
		{
			SDL_BlendMode bm;
			/* Allocate non-funky format, and copy pixeldata in*/
			if(fmt->Amask)
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				tempsurf = SDL_CreateRGBSurface(SDL_SWSURFACE,
						surf->w, surf->h, 24,
						0xff000000, 0x00ff0000,
						0x0000ff00, 0x000000ff);
#else
				tempsurf = SDL_CreateRGBSurface(SDL_SWSURFACE,
						surf->w, surf->h, 24,
						0x000000ff, 0x0000ff00,
						0x00ff0000, 0xff000000);
#endif
			else
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				tempsurf = SDL_CreateRGBSurface(SDL_SWSURFACE,
						surf->w, surf->h, 24,
						0xff0000, 0x00ff00,
						0x0000ff, 0x00000000);
#else
				tempsurf = SDL_CreateRGBSurface(SDL_SWSURFACE,
						surf->w, surf->h, 24,
						0x000000ff, 0x0000ff00,
						0x00ff0000, 0x00000000);
#endif
			if(!tempsurf)
			{
				SDL_SetError("Couldn't allocate temp surface");
				goto savedone;
			}
			SDL_GetSurfaceAlphaMod(surf, &temp_alpha);
			SDL_GetSurfaceBlendMode(surf, &bm);
			SDL_SetSurfaceAlphaMod(surf, 255);
			SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
			if(SDL_BlitSurface(surf, NULL, tempsurf, NULL) != 0)
			{
				SDL_SetError("Couldn't blit surface to temp surface");
				SDL_FreeSurface(tempsurf);
				goto savedone;
			}
			SDL_SetSurfaceAlphaMod(surf, temp_alpha);
			SDL_SetSurfaceBlendMode(surf, bm);
			for(i = 0; i < tempsurf->h; i++, rp += drp)
				row_pointers[rp] = ((png_byte*)tempsurf->pixels)
						+ i*tempsurf->pitch;
			if(SDL_MUSTLOCK(tempsurf))
				SDL_LockSurface(tempsurf);
			png_write_image(png_ptr, row_pointers);
			if(SDL_MUSTLOCK(tempsurf))
				SDL_UnlockSurface(tempsurf);
			SDL_FreeSurface(tempsurf);
		}
		else
		{
			for(i = 0; i < surf->h; i++, rp += drp)
				row_pointers[rp] = ((png_byte*)surf->pixels) +
						i * surf->pitch;
			if(SDL_MUSTLOCK(surf))
				SDL_LockSurface(surf);
			png_write_image(png_ptr, row_pointers);
			if(SDL_MUSTLOCK(surf))
				SDL_UnlockSurface(surf);
		}
	}

	png_write_end(png_ptr, NULL);
	ret = 0; /* got here, so nothing went wrong. YAY! */

savedone: /* clean up and return */
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if(palette)
		free(palette);
	if(row_pointers)
		free(row_pointers);
	return ret;
}
