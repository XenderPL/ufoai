/**
 * @file cp_messageoptions_callbacks.c
 * @todo Remove direct access to nodes
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

#include "../client.h"
#include "../menu/m_main.h"
#include "../menu/m_data.h"
#include "../menu/node/m_node_text.h"
#include "cp_campaign.h"
#include "cp_messageoptions.h"
#include "cp_messageoptions_callbacks.h"

static msoMenuState_t msoMenuState = MSO_MSTATE_REINIT;
static int messageList_size = 0; /**< actual messageSettings list size */
static int messageList_scroll = 0; /**< actual messageSettings list begin index due to scrolling */
static int visibleMSOEntries = 0; /**< actual visible entry count */
messageSettings_t backupMessageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding backup message settings (used for restore function in options popup) */

menuOption_t *messageSettingOptions;

/**
 * @brief Initializes menu texts for scrollable area
 * @sa MSO_Init_f
 */
static void MSO_InitList (void)
{
	menuOption_t *current;
	menuOption_t *lastCategory = NULL;
	menuOption_t *lastElement = NULL;
	int idx;

	/* option already allocated, nothing to do */
	if (messageSettingOptions)
		return;

	messageSettingOptions = (menuOption_t *) Mem_PoolAlloc(sizeof(*messageSettingOptions) * ccs.numMsgCategoryEntries, cp_campaignPool, 0);
	current = messageSettingOptions;

	MN_ResetData(TEXT_MESSAGEOPTIONS);
	for (idx = 0; idx < ccs.numMsgCategoryEntries; idx++) {
		const msgCategoryEntry_t *entry = &ccs.msgCategoryEntries[idx];
		const char *id = va("%d", idx);
		MN_InitOption(current, id, entry->notifyType, id);

		if (entry->isCategory) {
			if (lastCategory)
				lastCategory->next = current;
			lastCategory = current;
			lastElement = NULL;
		} else {
			if (lastElement)
				lastElement->next = current;
			else if (lastCategory)
				lastCategory->firstChild = current;
			else
				Sys_Error("MSO_InitList: The first entry must be a category");
			lastElement = current;
		}
		current++;
	}
	MN_RegisterOption(TEXT_MESSAGEOPTIONS, messageSettingOptions);
	MSO_SetMenuState(MSO_MSTATE_PREPARED, qfalse, qtrue);
}

/**
 * @brief Executes confuncs to update visible message options lines.
 * @sa MSO_Init_f
 */
static void MSO_UpdateVisibleButtons (void)
{
	int visible;/* current line */
	menuOptionIterator_t iterator;

	MN_InitOptionIteratorAtIndex(messageList_scroll, messageSettingOptions, &iterator);

	/* update visible button lines based on current displayed values */
	for (visible = 0; visible < messageList_size; visible++) {
		const menuOption_t *option = iterator.option;
		int idx;
		msgCategoryEntry_t *entry;

		if (!option)
			break;
		idx = atoi(option->value);

		entry = &ccs.msgCategoryEntries[idx];
		if (!entry)
			break;
		if (entry->isCategory) {
			/* category is visible anyway*/
			MN_ExecuteConfunc("ms_disable %i", visible);

		} else {
			assert(entry->category);
			MN_ExecuteConfunc("ms_enable %i", visible);
			MN_ExecuteConfunc("ms_btnstate %i %i %i %i", visible, entry->settings->doPause,
				entry->settings->doNotify, entry->settings->doSound);
		}

		MN_OptionIteratorNextOption(&iterator);
	}

	for (; visible < messageList_size; visible++)
		MN_ExecuteConfunc("ms_disable %i", visible);
}

/**
 * @brief initializes message options menu by showing as much button lines as needed.
 * @note This must only be done once as long as settings are changed in menu, so
 * messageOptionsInitialized is checked whether this is done yet. This function will be
 * reenabled if settings are changed via MSO_Set_f. If text list is not initialized
 * after parsing, MSO_InitTextList will be called first.
 */
static void MSOCB_Init (void)
{
	if (msoMenuState < MSO_MSTATE_PREPARED) {
		MSO_InitList();
		msoMenuState = MSO_MSTATE_PREPARED;
	}

	if (msoMenuState < MSO_MSTATE_INITIALIZED) {
		MSO_UpdateVisibleButtons();
		msoMenuState = MSO_MSTATE_INITIALIZED;
	}
}

/**
 * @brief initializes message options menu by showing as much button lines as needed.
 * @note First facultative param (from Cmd_Arg) init messageList_size (the number of rows of button)
 */
static void MSO_Init_f (void)
{
	if (Cmd_Argc() == 2) {
		messageList_size = atoi(Cmd_Argv(1));
	}

	MSOCB_Init();
}

/**
 * @brief Function for menu buttons to update message settings.
 * @sa MSO_Set
 */
static void MSO_Toggle_f (void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <listId> <pause|notify|sound>\n", Cmd_Argv(0));
	else {
		menuOptionIterator_t iterator;
		const int listIndex = atoi(Cmd_Argv(1));
		int idx;
		const msgCategoryEntry_t *selectedEntry;
		int optionType;
		qboolean activate;
		notify_t type;

		MN_InitOptionIteratorAtIndex(messageList_scroll + listIndex, messageSettingOptions, &iterator);
		if (!iterator.option)
			return;

		idx = atoi(iterator.option->value);
		selectedEntry = &ccs.msgCategoryEntries[idx];
		if (!selectedEntry)
			return;
		if (selectedEntry->isCategory) {
			Com_Printf("Toggle command with selected category entry ignored.\n");
			return;
		}
		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (!strcmp(nt_strings[type], selectedEntry->notifyType))
				break;
		}
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype during toggle '%s' ignored\n", selectedEntry->notifyType);
			return;
		}

		if (!strcmp(Cmd_Argv(2), "pause")) {
			optionType = MSO_PAUSE;
			activate = !selectedEntry->settings->doPause;
		} else if (!strcmp(Cmd_Argv(2), "notify")) {
			optionType = MSO_NOTIFY;
			activate = !selectedEntry->settings->doNotify;
		} else {
			optionType = MSO_SOUND;
			activate = !selectedEntry->settings->doSound;
		}
		MSO_Set(listIndex, type, optionType, activate, qtrue);
	}
}

/**
 * @brief Function to update message options menu after scrolling.
 * Updates all visible button lines based on configuration.
 */
static void MSO_Scroll_f (void)
{
	if (Cmd_Argc() < 2)
		return;

	/* no scrolling if visible entry count is less than max on page (due to folding) */
	messageList_scroll = atoi(Cmd_Argv(1));

	MSO_UpdateVisibleButtons();
}

/**
 * @brief Saves actual settings into backup settings variable.
 */
static void MSO_BackupSettings_f (void)
{
	memcpy(backupMessageSettings, messageSettings, sizeof(backupMessageSettings));
}

/**
 * @brief Restores actual settings from backup settings variable.
 * @note message options are caused to be re-initialized (for menu display)
 * @sa MSO_Init_f
 */
static void MSO_RestoreSettings_f (void)
{
	memcpy(messageSettings, backupMessageSettings, sizeof(messageSettings));
	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse,qtrue);
}

void MSO_SetMenuState (const msoMenuState_t newState, const qboolean callInit, const qboolean preserveIndex)
{
	msoMenuState = newState;
	if (newState == MSO_MSTATE_REINIT && !preserveIndex) {
		visibleMSOEntries = 0;
		messageList_scroll = 0;
	}
	if (callInit)
		MSOCB_Init();

}

void MSO_InitCallbacks (void)
{
	memset(backupMessageSettings, 1, sizeof(backupMessageSettings));
	Cmd_AddCommand("msgoptions_toggle", MSO_Toggle_f, "Toggles pause, notification or sound setting for a message category");
	Cmd_AddCommand("msgoptions_scroll", MSO_Scroll_f, "Scroll callback function for message options menu text");
	Cmd_AddCommand("msgoptions_init", MSO_Init_f, "Initializes message options menu");
	Cmd_AddCommand("msgoptions_backup", MSO_BackupSettings_f, "Backup message settings");
	Cmd_AddCommand("msgoptions_restore",MSO_RestoreSettings_f, "Restore message settings from backup");

}

void MSO_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("msgoptions_toggle");
	Cmd_RemoveCommand("msgoptions_scroll");
	Cmd_RemoveCommand("msgoptions_init");
	Cmd_RemoveCommand("msgoptions_backup");
	Cmd_RemoveCommand("msgoptions_restore");
	if (messageSettingOptions) {
		Mem_Free(messageSettingOptions);
		messageSettingOptions = NULL;
	}
}
