/**
 * @file cl_view.h
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_VIEW_H
#define CLIENT_CL_VIEW_H

void V_RenderView(void);
void V_UpdateRefDef(void);
void V_CenterView(const pos3_t pos);
void V_CalcFovX(void);
void V_LoadMedia(void);

extern cvar_t *cl_isometric;
extern cvar_t* cl_map_debug;
extern cvar_t* cl_le_debug;

#endif
