/**
 * @file m_node_special.c
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

#include "../../client.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_actions.h"
#include "m_node_window.h"
#include "m_node_special.h"
#include "m_node_abstractnode.h"

/**
 * @brief Call after the script initialized the node
 * @todo special cases should be managed as a common property event of the parent node
 */
static void MN_FuncNodeLoaded (menuNode_t *node)
{
	/** @todo move this code into the parser (it should not create a node) */
	const value_t *prop = MN_GetPropertyFromBehaviour(node->parent->behaviour, node->name);
	if (prop && prop->type == V_UI_ACTION) {
		void **value = (void**) ((uintptr_t)node->parent + prop->ofs);
		if (*value == NULL)
			*value = (void*) node->onClick;
		else
			Com_Printf("MN_FuncNodeLoaded: '%s' already defined. Second function ignored (\"%s\")\n", prop->string, MN_GetPath(node));
	}
}

void MN_RegisterSpecialNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "special";
	behaviour->isVirtual = qtrue;
}

void MN_RegisterFuncNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "func";
	behaviour->extends = "special";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->loaded = MN_FuncNodeLoaded;
}

void MN_RegisterNullNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "";
	behaviour->extends = "special";
	behaviour->isVirtual = qtrue;
}

/**
 * @brief Callback to execute a confunc
 */
static void MN_ConfuncCommand_f (void)
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	assert(node);
	assert(MN_NodeInstanceOf(node, "confunc"));
	MN_ExecuteConFuncActions(node, node->onClick);
}

/**
 * @brief Checks whether the given node is a virtual confunc that can be overridden from inheriting nodes.
 * @param node The node to check (must be a confunc node).
 * @return @c true if the given node is a dummy node, @c false otherwise.
 */
static qboolean MN_ConFuncIsVirtual (const menuNode_t *const node)
{
	/* magic way to know if it is a dummy node (used for inherited confunc) */
	const menuNode_t *dummy = (const menuNode_t*) Cmd_GetUserdata(node->name);
	assert(node);
	assert(MN_NodeInstanceOf(node, "confunc"));
	return (dummy != NULL && dummy->parent == NULL);
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_ConFuncNodeLoaded (menuNode_t *node)
{
	/* register confunc non inherited */
	if (node->super == NULL) {
		/* don't add a callback twice */
		if (!Cmd_Exists(node->name)) {
			Cmd_AddCommand(node->name, MN_ConfuncCommand_f, "Confunc callback");
			Cmd_AddUserdata(node->name, node);
		} else {
			Com_Printf("MN_ParseNodeBody: Command name for confunc '%s' already registered\n", MN_GetPath(node));
		}
	} else {
		menuNode_t *dummy;

		/* convert a confunc to an "inherited" confunc if it is possible */
		if (Cmd_Exists(node->name)) {
			if (MN_ConFuncIsVirtual(node))
				return;
		}

		dummy = MN_AllocStaticNode(node->name, "confunc");
		Cmd_AddCommand(node->name, MN_ConfuncCommand_f, "Inherited confunc callback");
		Cmd_AddUserdata(dummy->name, dummy);
	}
}

static void MN_ConFuncNodeClone (const menuNode_t *source, menuNode_t *clone)
{
	MN_ConFuncNodeLoaded(clone);
}

/**
 * @brief Callback every time the parent window is opened (pushed into the active window stack)
 */
static void MN_ConFuncNodeInit (menuNode_t *node)
{
	if (MN_ConFuncIsVirtual(node)) {
		const value_t *property = MN_GetPropertyFromBehaviour(node->behaviour, "onClick");
		menuNode_t *userData = (menuNode_t*) Cmd_GetUserdata(node->name);
		MN_AddListener(userData, property, node);
	}
}

/**
 * @brief Callback every time the parent window is closed (pop from the active window stack)
 */
static void MN_ConFuncNodeClose (menuNode_t *node)
{
	if (MN_ConFuncIsVirtual(node)) {
		const value_t *property = MN_GetPropertyFromBehaviour(node->behaviour, "onClick");
		menuNode_t *userData = (menuNode_t*) Cmd_GetUserdata(node->name);
		MN_RemoveListener(userData, property, node);
	}
}

void MN_RegisterConFuncNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "confunc";
	behaviour->extends = "special";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->loaded = MN_ConFuncNodeLoaded;
	behaviour->init = MN_ConFuncNodeInit;
	behaviour->close = MN_ConFuncNodeClose;
	behaviour->clone = MN_ConFuncNodeClone;
}

void MN_RegisterCvarFuncNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "cvarfunc";
	behaviour->extends = "special";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
}
