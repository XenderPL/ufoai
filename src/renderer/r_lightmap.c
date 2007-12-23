/**
 * @file r_lightmap.c
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "r_error.h"
#include "r_lightmap.h"

#define	MAX_LIGHTMAPS	256
#define LIGHTMAP_BYTES	4
#define RGB_PIXELSIZE 3

typedef struct {
	GLuint lightmap_texture_count;

	int allocated[LIGHTMAP_BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in */
	/* main memory so texsubimage can update properly */
	byte lightmap_buffer[LIGHTMAP_BYTES * LIGHTMAP_BLOCK_WIDTH * LIGHTMAP_BLOCK_HEIGHT];
} lightmapstate_t;

static lightmapstate_t r_lightmapstate;
static float s_blocklights[LIGHTMAP_BLOCK_WIDTH * LIGHTMAP_BLOCK_HEIGHT * LIGHTMAP_BYTES];

/**
 * @brief Combine and scale multiple lightmaps into the floating format in blocklights
 */
void R_BuildLightMap (mBspSurface_t * surf, byte * dest, int stride)
{
	unsigned int smax, tmax;
	int r, g, b, a, max;
	unsigned int i, j, size;
	byte *lightmap, *lm;
	int nummaps;
	float *bl;
	int maps;

	/* no rad processing - for map testing */
	if (!surf->lquant) {
		Com_Printf("R_BuildLightMap - no lightmap\n");
		return;
	}

	if ((surf->texinfo->flags & SURF_WARP))
		Com_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->stmaxs[0] >> surf->lquant) + 1;
	tmax = (surf->stmaxs[1] >> surf->lquant) + 1;
	size = smax * tmax;
	if (size > (sizeof(s_blocklights) >> surf->lquant)) {
		Com_Error(ERR_DROP, "Bad s_blocklights size (%i) - should be "UFO_SIZE_T" (lquant: %i) (smax: %i, stmaxs[0]: %i, tmax: %i, stmaxs[1]: %i)\n",
			size, (sizeof(s_blocklights) >> surf->lquant), surf->lquant, smax, surf->stmaxs[0], tmax, surf->stmaxs[1]);
	}

	lightmap = surf->samples;

	/* only add one lightmap */
	if (nummaps != 1)
		memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * RGB_PIXELSIZE);

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
		bl = s_blocklights;

		for (i = 0; i < size; i++, bl += RGB_PIXELSIZE) {
			if (maps > 0) {
				bl[0] += lightmap[i * RGB_PIXELSIZE + 0] * r_modulate->value;
				bl[1] += lightmap[i * RGB_PIXELSIZE + 1] * r_modulate->value;
				bl[2] += lightmap[i * RGB_PIXELSIZE + 2] * r_modulate->value;
			} else {
				bl[0] = lightmap[i * RGB_PIXELSIZE + 0] * r_modulate->value;
				bl[1] = lightmap[i * RGB_PIXELSIZE + 1] * r_modulate->value;
				bl[2] = lightmap[i * RGB_PIXELSIZE + 2] * r_modulate->value;
			}
		}
		lightmap += size * RGB_PIXELSIZE;	/* skip to next lightmap */
	}

	/* put into texture format */
	stride -= (smax << 2);
	bl = s_blocklights;

	/* first into an rgba linear block for softening */
	lightmap = (byte *)VID_TagAlloc(vid_lightPool, size * 4, 0);
	lm = lightmap;

	for (i = 0; i < tmax; i++) {
		for (j = 0; j < smax; j++) {
			r = Q_ftol(bl[0]);
			g = Q_ftol(bl[1]);
			b = Q_ftol(bl[2]);

			/* catch negative lights */
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			/* determine the brightest of the three color components */
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/* alpha is ONLY used for the mono lightmap case.  For this reason
			 * we set it to the brightest of the color components so that
			 * things don't get too dim. */
			a = max;

			/* rescale all the color components if the intensity of the greatest
			 * channel exceeds 1.0 */
			if (max > 255) {
				float t = 255.0F / max;

				r = r * t;
				g = g * t;
				b = b * t;
				a = a * t;
			}

			lm[0] = r;
			lm[1] = g;
			lm[2] = b;
			lm[3] = a;

			bl += RGB_PIXELSIZE;
			lm += 4;
		}
	}

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 1024)
		for (i = 0; i < 4; i++)
			R_SoftenTexture(lightmap, smax, tmax, 4);

	/* then copy into the final strided lightmap block */
	lm = lightmap;
	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			dest[0] = lm[0];
			dest[1] = lm[1];
			dest[2] = lm[2];
			dest[3] = lm[3];

			lm += 4;
			dest += 4;
		}
	}

	VID_MemFree(lightmap);
}

/*
=============================================================================
LIGHTMAP ALLOCATION
=============================================================================
*/

static inline void LM_InitBlock (void)
{
	memset(r_lightmapstate.allocated, 0, sizeof(r_lightmapstate.allocated));
}

static void LM_UploadBlock (void)
{
	R_Bind(r_state.lightmap_texnum + r_lightmapstate.lightmap_texture_count);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LIGHTMAP_BLOCK_WIDTH, LIGHTMAP_BLOCK_HEIGHT,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmapstate.lightmap_buffer);
	R_CheckError();

	if (++r_lightmapstate.lightmap_texture_count == MAX_LIGHTMAPS) {
		Com_Printf("LM_UploadBlock: MAX_LIGHTMAPS exceeded\n");
		r_lightmapstate.lightmap_texture_count--;
		return;
	}
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean LM_AllocBlock (int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;

	best = LIGHTMAP_BLOCK_HEIGHT;

	for (i = 0; i < LIGHTMAP_BLOCK_WIDTH - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmapstate.allocated[i + j] >= best)
				break;
			if (r_lightmapstate.allocated[i + j] > best2)
				best2 = r_lightmapstate.allocated[i + j];
		}
		/* this is a valid spot */
		if (j == w) {
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_BLOCK_HEIGHT)
		return qfalse;

	for (i = 0; i < w; i++)
		r_lightmapstate.allocated[*x + i] = best + h;

	return qtrue;
}

/**
 * @sa R_ModLoadFaces
 */
void R_CreateSurfaceLightmap (mBspSurface_t * surf)
{
	int smax, tmax;
	byte *base;

	if (surf->texinfo->flags & SURF_WARP)
		return;

	smax = (surf->stmaxs[0] >> surf->lquant) + 1;
	tmax = (surf->stmaxs[1] >> surf->lquant) + 1;

	if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		LM_UploadBlock();
		LM_InitBlock();
		if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "Consecutive calls to LM_AllocBlock(%d,%d) failed (lquant: %i)\n", smax, tmax, surf->lquant);
	}

	surf->lightmaptexturenum = r_lightmapstate.lightmap_texture_count;

	base = r_lightmapstate.lightmap_buffer;
	base += (surf->light_t * LIGHTMAP_BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	R_BuildLightMap(surf, base, LIGHTMAP_BLOCK_WIDTH * LIGHTMAP_BYTES);
}

/**
 * @sa R_ModBeginLoading
 * @sa R_EndBuildingLightmaps
 */
void R_BeginBuildingLightmaps (void)
{
	memset(r_lightmapstate.allocated, 0, sizeof(r_lightmapstate.allocated));

	R_EnableMultitexture(qtrue);
	R_SelectTexture(&r_state.lightmap_texunit);

	r_state.lightmap_texnum = TEXNUM_LIGHTMAPS;

	r_lightmapstate.lightmap_texture_count = 1;
}

/**
 * @sa R_BeginBuildingLightmaps
 * @sa R_ModBeginLoading
 */
void R_EndBuildingLightmaps (void)
{
	LM_UploadBlock();
	R_EnableMultitexture(qfalse);
	Com_DPrintf(DEBUG_RENDERER, "lightmaps: %i\n", r_lightmapstate.lightmap_texture_count);
}
