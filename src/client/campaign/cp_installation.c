/**
 * @file cp_installation.c
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: INS_*
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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

#include "../client.h"
#include "../mxml/mxml_ufoai.h"
#include "../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_map.h"
#include "cp_ufo.h"
#include "cp_installation.h"
#include "cp_installation_callbacks.h"

/**
 * @brief Get the type of an installation
 * @param[in] installation Pointer to the isntallation
 * @return type of the installation
 * @SA installationType_t
 */
installationType_t INS_GetType (const installation_t *installation)
{
	if (installation->installationTemplate->maxBatteries > 0)
		return INSTALLATION_DEFENCE;
	else if (installation->installationTemplate->maxUFOsStored > 0)
		return INSTALLATION_UFOYARD;

	/* default is radar */
	return INSTALLATION_RADAR;
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx.
 */
installation_t* INS_GetInstallationByIDX (int instIdx)
{
	if (instIdx < 0 || instIdx >= MAX_INSTALLATIONS)
		return NULL;

	return &ccs.installations[instIdx];
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx if installation is founded, NULL else.
 */
installation_t* INS_GetFoundedInstallationByIDX (int instIdx)
{
	installation_t *installation = INS_GetInstallationByIDX(instIdx);

	if (installation && installation->founded)
		return installation;

	return NULL;
}

/**
 * @brief Returns the installation Template for a given installation ID.
 * @param[in] id ID of the installation template to find.
 * @return corresponding installation Template, @c NULL if not found.
 */
installationTemplate_t* INS_GetInstallationTemplateFromInstallationID (const char *id)
{
	int idx;

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++)
		if (!strcmp(ccs.installationTemplates[idx].id, id))
			return &ccs.installationTemplates[idx];

	return NULL;
}

/**
 * @brief Setup new installation
 * @sa INS_NewInstallation
 */
void INS_SetUpInstallation (installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos)
{
	const int newInstallationAlienInterest = 1.0f;

	Vector2Copy(pos, installation->pos);

	installation->idx = INS_GetInstallationIDX(installation);
	installation->founded = qtrue;
	installation->installationStatus = INSTALLATION_UNDER_CONSTRUCTION;
	installation->installationTemplate = installationTemplate;
	installation->buildStart = ccs.date.day;

	/* a new installation is not discovered (yet) */
	installation->alienInterest = newInstallationAlienInterest;

	/* intialise hit points */
	installation->installationDamage = installation->installationTemplate->maxDamage;

	/* Reset UFO Yard capacities. */
	installation->ufoCapacity.max = 0;
	installation->ufoCapacity.cur = 0;
	/* Reset Batteries */
	installation->numBatteries = 0;
	/* Reset Radar */
	RADAR_Initialise(&(installation->radar), 0.0f, 0.0f, 0.0f, qfalse);

	Com_DPrintf(DEBUG_CLIENT, "INS_SetUpInstallation: Installation %s (idx: %i), type: %s has been set up. Will be finished in %i day(s).\n", installation->name, installation->idx, installation->installationTemplate->id, installation->installationTemplate->buildTime);
}

/**
 * @brief Get first not yet founded installation.
 * @returns installation Pointer to the first unfounded installation
 * @note it returns NULL if installation limit has reached
 */
installation_t *INS_GetFirstUnfoundedInstallation (void)
{
	const int maxInstallations = B_GetInstallationLimit();

	if (ccs.numInstallations >= maxInstallations)
		return NULL;

	return INS_GetInstallationByIDX(ccs.numInstallations);
}

/**
 * @brief Destroys an installation
 * @param[in] pointer to the installation to be destroyed
 */
void INS_DestroyInstallation (installation_t *installation)
{
	if (!installation)
		return;
	if (!installation->founded)
		return;

	RADAR_UpdateInstallationRadarCoverage(installation, 0, 0);
	CP_MissionNotifyInstallationDestroyed(installation);

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Installation %s was destroyed."), installation->name);
	MSO_CheckAddNewMessage(NT_INSTALLATION_DESTROY, _("Installation destroyed"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

	REMOVE_ELEM_ADJUST_IDX(ccs.installations, installation->idx, ccs.numInstallations);
	Cvar_Set("mn_installation_count", va("%i", ccs.numInstallations));
}

installation_t *INS_GetCurrentSelectedInstallation (void)
{
	int i;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		installation_t *ins = INS_GetInstallationByIDX(i);
		if (ins->selected)
			return ins;
	}

	return NULL;
}

/**
 * @sa INS_SelectInstallation
 * @param installation
 */
void INS_SetCurrentSelectedInstallation (const installation_t *installation)
{
	int i;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		installation_t *ins = INS_GetInstallationByIDX(i);
		if (ins == installation) {
			ins->selected = qtrue;
			if (!ins->founded)
				Com_Error(ERR_DROP, "The installation you are trying to select is not founded yet");
		} else
			ins->selected = qfalse;
	}

	if (installation) {
		B_SetCurrentSelectedBase(NULL);
		Cvar_Set("mn_installation_title", installation->name);
		Cvar_Set("mn_installation_type", installation->installationTemplate->id);
	} else {
		Cvar_Set("mn_installation_title", "");
		Cvar_Set("mn_installation_type", "");
	}
}

/**
 * @brief Finishes an installation
 * @param[in, out] installation Pointer to the installation to be finished
 * @SA INS_UpdateInstallationData
 * @SA INS_ConstructionFinished_f
 */
static void INS_FinishInstallation (installation_t *installation)
{
	if (!installation)
		Com_Error(ERR_DROP, "INS_FinishInstallation: No Installation.\n");
	if (!installation->installationTemplate)
		Com_Error(ERR_DROP, "INS_FinishInstallation: No Installation template.\n");
	if (!installation->founded)
		Com_Error(ERR_DROP, "INS_FinishInstallation: Finish a not founded Installation?\n");
	if (installation->installationStatus != INSTALLATION_UNDER_CONSTRUCTION) {
		Com_DPrintf(DEBUG_CLIENT, "INS_FinishInstallation: Installation is not being built.\n");
		return;
	}

	installation->installationStatus = INSTALLATION_WORKING;
	/* if Radar Tower */
	RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);
	/* if SAM Site */
	installation->numBatteries = installation->installationTemplate->maxBatteries;
	BDEF_InitialiseInstallationSlots(installation);
	/* if UFO Yard */
	installation->ufoCapacity.max = installation->installationTemplate->maxUFOsStored;
}

#ifdef DEBUG

/**
 * @brief Just lists all installations with their data
 * @note called with debug_listinstallation
 * @note Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void INS_InstallationList_f (void)
{
	int i;
	
	for (i = 0; i < ccs.numInstallations; i++) {
		const installation_t *installation = INS_GetInstallationByIDX(i);

		if (!installation->founded) {
			Com_Printf("Installation idx %i not founded\n\n", i);
			continue;
		}

		Com_Printf("Installation idx %i\n", installation->idx);
		Com_Printf("Installation name %s\n", installation->name);
		Com_Printf("Installation founded %i\n", installation->founded);
		Com_Printf("Installation pos %.02f:%.02f\n", installation->pos[0], installation->pos[1]);
		Com_Printf("Installation Alien interest %f\n", installation->alienInterest);

		Com_Printf("\nInstallation sensorWidth %i\n", installation->radar.range);
		Com_Printf("\nInstallation trackingWidth %i\n", installation->radar.trackingRange);
		Com_Printf("Installation numSensoredAircraft %i\n\n", installation->radar.numUFOs);

		Com_Printf("\nInstallation numBatteries %i\n", installation->numBatteries);
		/** @todo list batteries */

		Com_Printf("\nInstallation stored UFOs %i/%i\n", installation->ufoCapacity.cur, installation->ufoCapacity.max);
		/** @todo list stored Ufos*/
		
		Com_Printf("\n\n");
	}
}

/**
 * @brief Finishes the construction of an/all installation(s)
 */
static void INS_ConstructionFinished_f (void)
{
	int i;
	int idx = -1;

	if (Cmd_Argc() == 2) {
		idx = atoi(Cmd_Argv(1));
		if (idx < 0 || idx >= MAX_INSTALLATIONS) {
			Com_Printf("Usage: %s [installationIDX]\nWithout parameter it builds up all.\n", Cmd_Argv(0));
			return;
		}
	}

	for (i = 0; i < ccs.numInstallations; i++) {
		installation_t *ins;

		if (idx >= 0 && i != idx)
			continue;

		ins = INS_GetInstallationByIDX(i);

		if (ins && ins->founded)
			INS_FinishInstallation(ins);
	}
}
#endif


/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void INS_InitStartup (void)
{
	int idx;

	Com_DPrintf(DEBUG_CLIENT, "Reset installation\n");

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		ccs.installationTemplates[idx].id = NULL;
		ccs.installationTemplates[idx].name = NULL;
		ccs.installationTemplates[idx].cost = 0;
		ccs.installationTemplates[idx].radarRange = 0.0f;
		ccs.installationTemplates[idx].trackingRange = 0.0f;
		ccs.installationTemplates[idx].maxUFOsStored = 0;
		ccs.installationTemplates[idx].maxBatteries = 0;
	}

	/* add commands and cvars */
#ifdef DEBUG
	Cmd_AddCommand("debug_listinstallation", INS_InstallationList_f, "Print installation information to the game console");
	Cmd_AddCommand("debug_finishinstallation", INS_ConstructionFinished_f, "Finish construction of a specified or every installation");
#endif
}

/**
 * @brief Check if some installation are build.
 * @note Daily called.
 */
void INS_UpdateInstallationData (void)
{
	int instIdx;

	for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(instIdx);
		if (!installation)
			continue;

		if ((installation->installationStatus == INSTALLATION_UNDER_CONSTRUCTION)
		 && installation->buildStart
		 && installation->buildStart + installation->installationTemplate->buildTime <= ccs.date.day) {
		
		 	INS_FinishInstallation(installation);

			Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Construction of installation %s finished."), installation->name);
			MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDFINISH, _("Installation finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
		}
	}
}

static const value_t installation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(installationTemplate_t, name), 0},
	{"radar_range", V_INT, offsetof(installationTemplate_t, radarRange), MEMBER_SIZEOF(installationTemplate_t, radarRange)},
	{"radar_tracking_range", V_INT, offsetof(installationTemplate_t, trackingRange), MEMBER_SIZEOF(installationTemplate_t, trackingRange)},
	{"max_batteries", V_INT, offsetof(installationTemplate_t, maxBatteries), MEMBER_SIZEOF(installationTemplate_t, maxBatteries)},
	{"max_ufo_stored", V_INT, offsetof(installationTemplate_t, maxUFOsStored), MEMBER_SIZEOF(installationTemplate_t, maxUFOsStored)},
	{"max_damage", V_INT, offsetof(installationTemplate_t, maxDamage), MEMBER_SIZEOF(installationTemplate_t, maxDamage)},
	{"model", V_CLIENT_HUNK_STRING, offsetof(installationTemplate_t, model), 0},
	{"image", V_CLIENT_HUNK_STRING, offsetof(installationTemplate_t, image), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Copies an entry from the installation description file into the list of installation templates.
 * @note Parses one "installation" entry in the installation.ufo file and writes
 * it into the next free entry in installationTemplates.
 * @param[in] name Unique test-id of a installationTemplate_t.
 * @param[in] text @todo document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
 */
void INS_ParseInstallations (const char *name, const char **text)
{
	installationTemplate_t *installation;
	const char *errhead = "INS_ParseInstallations: unexpected end of file (names ";
	const char *token;
	const value_t *vp;
	int i;

	/* get id list body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("INS_ParseInstallations: installation \"%s\" without body ignored\n", name);
		return;
	}

	if (!name) {
		Com_Printf("INS_ParseInstallations: installation name not specified.\n");
		return;
	}

	if (ccs.numInstallationTemplates >= MAX_INSTALLATION_TEMPLATES) {
		Com_Printf("INS_ParseInstallations: too many installation templates\n");
		ccs.numInstallationTemplates = MAX_INSTALLATION_TEMPLATES;	/* just in case it's bigger. */
		return;
	}

	for (i = 0; i < ccs.numInstallationTemplates; i++) {
		if (!strcmp(ccs.installationTemplates[i].name, name)) {
			Com_Printf("INS_ParseInstallations: Second installation with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	/* new entry */
	installation = &ccs.installationTemplates[ccs.numInstallationTemplates];
	memset(installation, 0, sizeof(*installation));
	installation->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	Com_DPrintf(DEBUG_CLIENT, "...found installation %s\n", installation->id);

	ccs.numInstallationTemplates++;
	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = installation_vals; vp->string; vp++)
			if (!strcmp(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)installation + (int)vp->ofs), cp_campaignPool, 0);
					break;
				default:
					if (Com_EParseValue(installation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("INS_ParseInstallations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		/* other values */
		if (!vp->string) {
			if (!strcmp(token, "cost")) {
				char cvarname[MAX_VAR] = "mn_installation_";

				Q_strcat(cvarname, installation->id, MAX_VAR);
				Q_strcat(cvarname, "_cost", MAX_VAR);

				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->cost = atoi(token);

				Cvar_Set(cvarname, va(_("%d c"), atoi(token)));
			} else if (!strcmp(token, "buildtime")) {
				char cvarname[MAX_VAR];


				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->buildTime = atoi(token);

				Com_sprintf(cvarname, sizeof(cvarname), "mn_installation_%s_buildtime", installation->id);
				Cvar_Set(cvarname, va(ngettext("%d day\n", "%d days\n", atoi(token)), atoi(token)));
			}
		}
	} while (*text);
}

/**
 * @brief Save callback for savegames in xml
 * @sa INS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean INS_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;
	n = mxml_AddNode(p, "installations");
	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		const installation_t *inst = INS_GetInstallationByIDX(i);
		mxml_node_t *s, *ss;
		if (!inst->founded)
			continue;
		s = mxml_AddNode(n, "installation");
		mxml_AddInt(s, "idx", inst->idx);
		mxml_AddBool(s, "founded", inst->founded);
		mxml_AddString(s, "templateid", inst->installationTemplate->id);
		mxml_AddString(s, "name", inst->name);
		mxml_AddPos3(s, "pos", inst->pos);
		mxml_AddInt(s, "status", inst->installationStatus);
		mxml_AddInt(s, "damage", inst->installationDamage);
		mxml_AddFloat(s, "alieninterest", inst->alienInterest);
		mxml_AddInt(s, "buildstart", inst->buildStart);

		ss = mxml_AddNode(s, "batteries");
		mxml_AddInt(ss, "num", inst->numBatteries);
		B_SaveBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		/** @todo aircraft (don't save capacities, they should
		 * be recalculated after loading) */
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa INS_SaveXML
 * @sa SAV_GameLoadXML
 * @sa INS_LoadItemSlots
 */
qboolean INS_LoadXML (mxml_node_t *p)
{
	mxml_node_t *s;
	mxml_node_t *n = mxml_GetNode(p, "installations");
	if (!n)
		return qfalse;

	for (s = mxml_GetNode(n, "installation"); s ; s = mxml_GetNextNode(s,n, "installation")) {
		mxml_node_t *ss;
		const int idx = mxml_GetInt(s, "idx", 0);
		installation_t *inst = INS_GetInstallationByIDX(idx);
		inst->idx = INS_GetInstallationIDX(inst);
		inst->founded = mxml_GetBool(s, "founded", inst->founded);
		/* should never happen, we only save founded installations */
		if (!inst->founded)
			continue;
		inst->installationTemplate = INS_GetInstallationTemplateFromInstallationID(mxml_GetString(s, "templateid"));
		if (!inst->installationTemplate) {
			Com_Printf("Could not find installation template\n");
			return qfalse;
		}
		inst->ufoCapacity.max = inst->installationTemplate->maxUFOsStored;
		inst->ufoCapacity.cur = 0;
		ccs.numInstallations++;
		Q_strncpyz(inst->name, mxml_GetString(s, "name"), sizeof(inst->name));

		mxml_GetPos3(s, "pos", inst->pos);

		inst->installationStatus = mxml_GetInt(s, "status", 0);
		inst->installationDamage = mxml_GetInt(s, "damage", 0);
		inst->alienInterest = mxml_GetFloat(s, "alieninterest", 0.0);

		RADAR_InitialiseUFOs(&inst->radar);
		RADAR_Initialise(&(inst->radar), 0.0f, 0.0f, 1.0f, qtrue);
		RADAR_UpdateInstallationRadarCoverage(inst, inst->installationTemplate->radarRange, inst->installationTemplate->trackingRange);

		inst->buildStart = mxml_GetInt(s, "buildstart", 0);

		/* read battery slots */
		BDEF_InitialiseInstallationSlots(inst);

		ss = mxml_GetNode(s, "batteries");
		if (!ss) {
			Com_Printf("INS_LoadXML: Batteries not defined!\n");
			return qfalse;
		}
		inst->numBatteries = mxml_GetInt(ss, "num", 0);
		if (inst->numBatteries > inst->installationTemplate->maxBatteries) {
			Com_Printf("Installation has more batteries than possible, using upper bound\n");
			inst->numBatteries = inst->installationTemplate->maxBatteries;
		}
		B_LoadBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		/** @todo aircraft */
		/** @todo don't forget to recalc the capacities like we do for bases */
	}

	Cvar_Set("mn_installation_count", va("%i", ccs.numInstallations));

	return qtrue;
}
