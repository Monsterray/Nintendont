/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

/**
 * Table-driven Settings menu.
 *
 * Every user-visible option lives in one table in SettingsMenu.c.
 * To add a setting:
 *   1. Add the storage (a NIN_CFG bit or field) in common/include/CommonConfig.h,
 *      bump NIN_CFG_VERSION and add a migration step to UpdateNinCFG() in global.c.
 *   2. Add one SettingDef entry to the table in SettingsMenu.c (name, category,
 *      kind, storage, description). Visibility / platform rules are flags there.
 *   3. Read it in the kernel with ConfigGetConfig() (or a ConfigGet* helper).
 * Nothing in menu.c needs to change.
 */
#ifndef __SETTINGS_MENU_H__
#define __SETTINGS_MENU_H__

#include <gctypes.h>

// Persistent cursor state for the Settings menu.
typedef struct SettingsMenuState
{
	s32 category;	// Index into the category table.
	s32 pos;	// -1 == category tab row; 0..n-1 == visible setting index.
	s32 scroll;	// First visible setting row.
} SettingsMenuState;

// One frame of (already debounced / key-repeated) input.
typedef struct SettingsMenuInput
{
	bool up;
	bool down;
	bool left;
	bool right;
	bool ok;	// A
	bool nextTab;	// Y / 2
} SettingsMenuInput;

// Return flags from SettingsMenu_Update().
enum
{
	SMENU_REDRAW	= (1 << 0),	// Screen needs to be redrawn.
	SMENU_CHANGED	= (1 << 1),	// A setting changed; save nincfg.bin.
};

/**
 * Reset the cursor to the first category / first item.
 */
void SettingsMenu_Reset(SettingsMenuState *st);

/**
 * Process one frame of input.
 * @return SMENU_* flags.
 */
u32 SettingsMenu_Update(SettingsMenuState *st, const SettingsMenuInput *in);

/**
 * Draw the Settings menu into the current frame.
 * Caller is responsible for PrintInfo() / GRRLIB_Render().
 */
void SettingsMenu_Draw(const SettingsMenuState *st);

#endif /* __SETTINGS_MENU_H__ */
