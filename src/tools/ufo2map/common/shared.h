/**
 * @file shared.h
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

#ifndef _SHARED_H
#define _SHARED_H

#include "../../../shared/ufotypes.h"
#include "../../../common/unzip.h"
#include "../../../shared/shared.h"
#include "../../../shared/mathlib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

#include "../../../shared/defines.h"
#include "../../../shared/typedefs.h"


#ifndef errno
extern int errno;
#endif

/*========================================================================
The .pak files are just a linear collapse of a directory tree
========================================================================*/

/** @brief verbosity levels for use in calls to Verb_Printf
 *  and on the command line -v <int> */
typedef enum {
	VERB_SILENT_EXCEPT_ERROR,
	VERB_CHECK, /**< print nothing, except check/fix messages */
	VERB_MAPNAME, /**< print mapname only. in check/fix, only do this if a problem is found */
	VERB_LESS,
	VERB_NORMAL,
	VERB_EXTRA, /**< the highest verbosity level */
	VERB_NUM /**< the number of verbosity levels */
} verbosityLevels_t;

typedef struct mapConfig_s {
	float subdivideSize;
	int nice;
	qboolean verbose;
	int verbosity;
	qboolean noprune;
	qboolean nodetail;
	qboolean fulldetail;
	qboolean onlyents;
	qboolean nomerge;
	qboolean nowater;
	qboolean nocsg;
	qboolean noweld;
	qboolean noshare;
	qboolean nosubdiv;
	qboolean notjunc;
	qboolean verboseentities;
	qboolean nobackclip;
	int noradiosity;
	int block_xl, block_xh, block_yl, block_yh;
	float microvolume; /**< -micro test after CSG */
	unsigned int numbounce;
	qboolean extrasamples;
	float subdiv;
	qboolean info;
	qboolean generateFootstepFile;
	qboolean generateMaterialFile;

	qboolean performMapCheck;
	qboolean chkEntities;
	qboolean chkContained;
	qboolean chkFillLevelFlags;
	qboolean chkIntersection;
	qboolean chkNodraws;
	qboolean chkBrushes;
	qboolean chkLevelFlags;
	qboolean chkTextures;
	qboolean chkMixedFaceContents;
	qboolean chkMMicro;
	float mapMicrovol; /**< -check mbr micro test before CSG */
	qboolean chkAll;
	qboolean fixMap;

	float day_ambient_red;
	float day_ambient_green;
	float day_ambient_blue;
	float day_sun_intensity;
	float day_sun_pitch;
	float day_sun_yaw;
	vec3_t day_sun_color;
	vec3_t day_sun_dir;

	float night_ambient_red;
	float night_ambient_green;
	float night_ambient_blue;
	float night_sun_intensity;
	float night_sun_pitch;
	float night_sun_yaw;
	vec3_t night_sun_color;
	vec3_t night_sun_dir;

	float maxlight;
	float lightscale;
	byte lightquant;
	float direct_scale;
	float entity_scale;

	int onlynewer;	/**< only compile (update) the bsp if the map file is newer */

	int compile_for_day;	/**< set this to 1 if you want to compile the day version of the lightmap */
} mapConfig_t;

extern mapConfig_t config;

typedef struct threadstate_s {
	int numthreads;		/**< spawned threads */
	int workindex;		/**< current work cycle */
	int workcount;		/**< total work cycles */
	int workfrac;		/**< last fraction of work completed (tenths) */
	int worktick;		/**< Number of iterations before the progress spinner is spun */
	qboolean progress;	/**< are we reporting progress */
} threadstate_t;

extern threadstate_t threadstate;

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(void (*func)(unsigned int), unsigned int workcount, qboolean progress, const char *id);
void RunSingleThreadOn(void (*func)(unsigned int), unsigned int workcount, qboolean progress, const char *id);

#include "../../../common/qfiles.h"

/** @todo when the Verb_Printf stuff works, the compile-time
 * verbosity setting should be removed */
#define SYS_VRB 0 /* verbose support (on/off) */
#define SYS_STD 1 /* standard print level */
#define SYS_WRN 2 /* warnings */
#define SYS_ERR 3 /* error */

void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Com_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void Sys_FPrintf(int flag, const char *text, ...) __attribute__((format(printf, 2, 3)));

#endif /* _SHARED_H */
