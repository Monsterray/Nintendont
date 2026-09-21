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
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "font.h"
#include "Config.h"
#include "menu.h"
#include "SettingsMenu.h"

/**********************************************************************
 * Layout
 **********************************************************************/

// Row helper (shared with menu.c).
extern u32 SettingY(u32 row);

#define SM_ROW_TABS		0	// Category tab bar.
#define SM_ROW_FIRST		2	// First setting row.
#define SM_MAX_ROWS		14	// Setting rows shown at once.
#define SM_ROW_HELP		17	// Button help (2 lines).

#define SM_X_CURSOR		(MENU_POS_X + 30)
#define SM_X_ITEM		(MENU_POS_X + 50)
#define SM_X_DESC		(MENU_POS_X + 320)

#define SM_NAME_WIDTH		17	// Longest name is "Force Progressive" / "Wiimote CC Rumble".

// Colors.
#define SM_COLOR_TAB_ON		DARK_BLUE
#define SM_COLOR_TAB_OFF	GRAY
#define SM_COLOR_ITEM		BLACK
#define SM_COLOR_VALUE		DARK_BLUE
#define SM_COLOR_HELP		GRAY

/**********************************************************************
 * Setting definitions
 **********************************************************************/

typedef enum
{
	CAT_GENERAL = 0,
	CAT_VIDEO,
	CAT_INPUT,
	CAT_MEMCARD,
	CAT_NETWORK,
	CAT_ADVANCED,

	CAT_COUNT
} SettingCategory;

static const char *const CategoryNames[CAT_COUNT] =
{
	"General",
	"Video",
	"Input",
	"Memcard",
	"Network",
	"Advanced",
};

typedef enum
{
	KIND_BOOL = 0,	// Two states. Labels default to Off/On.
	KIND_ENUM,	// N labelled states.
	KIND_INT,	// Integer range [min, max]; formatted by a callback.
} SettingKind;

typedef enum
{
	STORE_CONFIG = 0,	// Bit(s) in ncfg->Config, given by mask.
	STORE_VIDEO,		// Bit(s) in ncfg->VideoMode, given by mask.
	STORE_CUSTOM,		// get()/set() callbacks.
} SettingStore;

enum SettingFlags
{
	SF_NONE		= 0,
	SF_WII_ONLY	= (1 << 0),	// Hidden on Wii U / Wii VC.
	SF_WIIU_ONLY	= (1 << 1),	// Hidden on original Wii.
};

typedef struct SettingDef
{
	const char *name;		// <= SM_NAME_WIDTH characters.
	u8 category;			// SettingCategory
	u8 kind;			// SettingKind
	u8 store;			// SettingStore
	u8 flags;			// SettingFlags

	// Additional visibility condition. NULL == always visible.
	bool (*visible)(void);

	// STORE_CONFIG / STORE_VIDEO: bit mask.
	u32 mask;

	// STORE_CUSTOM: value accessors.
	// KIND_BOOL/KIND_ENUM return an index; KIND_INT returns the value.
	s32 (*get)(void);
	void (*set)(s32 value);

	// KIND_BOOL / KIND_ENUM: labels, and how many states there are.
	// A KIND_BOOL may leave both unset and gets "Off"/"On".
	const char *const *labels;
	u32 count;

	// KIND_INT: range and formatter.
	s32 min, max;
	void (*format)(s32 value, char *buf, size_t len);

	// Called after the value changes. (optional)
	void (*onChange)(void);

	// Description lines, NULL-terminated. (optional)
	const char *const *desc;
} SettingDef;

/** Visibility predicates. **/

static bool Vis_MemCardEmu(void)
{
	return !!(ncfg->Config & NIN_CFG_MEMCARDEMU);
}

static bool Vis_VideoForce(void)
{
	return !!(ncfg->VideoMode & NIN_VID_FORCE);
}

static bool Vis_PatchPAL50(void)
{
	// Only meaningful when a video mode is being forced.
	return (ncfg->VideoMode & NIN_VID_FORCE) || (ncfg->Config & NIN_CFG_FORCE_PROG);
}

static bool Vis_BBAProfile(void)
{
	// Profiles are managed by the Wii U Menu on Wii U.
	return !IsWiiU() && (ncfg->Config & NIN_CFG_BBA_EMU);
}

/** Custom accessors. **/

static const char *const LanguageLabels[] =
{
	"English", "German", "French", "Spanish", "Italian", "Dutch", "Auto",
};
static s32 Get_Language(void)
{
	const s32 lang = (s32)ncfg->Language;
	if (lang < NIN_LAN_FIRST || lang >= NIN_LAN_LAST)
		return NIN_LAN_LAST;	// Auto
	return lang;
}
static void Set_Language(s32 idx)
{
	ncfg->Language = (idx >= NIN_LAN_LAST) ? (u32)NIN_LAN_AUTO : (u32)idx;
}

static const char *const VideoModeLabels[] = { "Auto", "Force", "Force (Deflicker)", "None" };
static const u32 VideoModeValues[] = {
	NIN_VID_AUTO, NIN_VID_FORCE, NIN_VID_FORCE | NIN_VID_FORCE_DF, NIN_VID_NONE
};
static s32 Get_VideoMode(void)
{
	u32 i;
	for (i = 0; i < 4; i++)
	{
		if ((ncfg->VideoMode & NIN_VID_MASK) == VideoModeValues[i])
			return (s32)i;
	}
	return 0;	// Auto
}
static void Set_VideoMode(s32 idx)
{
	ncfg->VideoMode &= ~NIN_VID_MASK;
	ncfg->VideoMode |= VideoModeValues[idx];
}

static const char *const ForcedModeLabels[] = { "PAL50", "PAL60", "NTSC", "MPAL" };
static s32 Get_ForcedMode(void)
{
	switch (ncfg->VideoMode & NIN_VID_FORCE_MASK)
	{
		case NIN_VID_FORCE_PAL50:	return 0;
		case NIN_VID_FORCE_PAL60:	return 1;
		case NIN_VID_FORCE_MPAL:	return 3;
		case NIN_VID_FORCE_NTSC:
		default:			return 2;
	}
}
static void Set_ForcedMode(s32 idx)
{
	ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
	ncfg->VideoMode |= (NIN_VID_FORCE_PAL50 << idx);
}

// Video width: index 0 == Auto; index k (1..41) == scale 38+2k (40..120).
#define VIDWIDTH_STEPS 41
static s32 Get_VideoWidth(void)
{
	const s32 scale = ncfg->VideoScale;
	if (scale < 40 || scale > 120 || (scale & 1))
		return 0;
	return (scale - 38) / 2;
}
static void Set_VideoWidth(s32 idx)
{
	ncfg->VideoScale = (idx <= 0) ? 0 : (s8)(38 + 2*idx);
}
static void Fmt_VideoWidth(s32 idx, char *buf, size_t len)
{
	if (idx <= 0)
		snprintf(buf, len, "Auto");
	else
		snprintf(buf, len, "%d", 600 + 38 + 2*idx);
}

static s32 Get_ScreenPos(void)
{
	const s32 off = ncfg->VideoOffset;
	return (off < -20 || off > 20) ? 0 : off;
}
static void Set_ScreenPos(s32 v)
{
	ncfg->VideoOffset = (s8)v;
}
static void Fmt_Signed(s32 v, char *buf, size_t len)
{
	snprintf(buf, len, "%+d", v);
}

static void OnChange_Video(void)
{
	ReconfigVideo(rmode);
}

static s32 Get_MaxPads(void)
{
	return (ncfg->MaxPads > NIN_CFG_MAXPAD) ? NIN_CFG_MAXPAD : (s32)ncfg->MaxPads;
}
static void Set_MaxPads(s32 v)
{
	ncfg->MaxPads = (u32)v;
}
static void Fmt_Plain(s32 v, char *buf, size_t len)
{
	snprintf(buf, len, "%d", v);
}

static s32 Get_GamepadSlot(void)
{
	return (ncfg->WiiUGamepadSlot > NIN_CFG_MAXPAD) ? NIN_CFG_MAXPAD : (s32)ncfg->WiiUGamepadSlot;
}
static void Set_GamepadSlot(s32 v)
{
	ncfg->WiiUGamepadSlot = (u32)v;
}
static void Fmt_GamepadSlot(s32 v, char *buf, size_t len)
{
	if (v >= NIN_CFG_MAXPAD)
		snprintf(buf, len, "None");
	else
		snprintf(buf, len, "%d", v + 1);
}

static s32 Get_CardSize(void)
{
	return (ncfg->MemCardBlocks > MEM_CARD_MAX) ? 0 : (s32)ncfg->MemCardBlocks;
}
static void Set_CardSize(s32 v)
{
	ncfg->MemCardBlocks = (u8)v;
}
static void Fmt_CardSize(s32 v, char *buf, size_t len)
{
	snprintf(buf, len, "%d%s", MEM_CARD_BLOCKS(v), (v > 2) ? " (unstable)" : "");
}

static s32 Get_NetProfile(void)
{
	return (s32)(ncfg->NetworkProfile & 3);
}
static void Set_NetProfile(s32 v)
{
	ncfg->NetworkProfile = (u8)(v & 3);
}
static void Fmt_NetProfile(s32 v, char *buf, size_t len)
{
	if (v == 0)
		snprintf(buf, len, "Auto");
	else
		snprintf(buf, len, "%d", v);
}

/** Labels. **/
static const char *const OnOffLabels[]		= { "Off", "On" };
static const char *const NoYesLabels[]		= { "No", "Yes" };

/** Descriptions. Keep lines <= 29 characters. **/

static const char *const desc_autoboot[] = {
	"Boot the selected game",
	"immediately when Nintendont",
	"starts.",
	"",
	"Hold B while Nintendont is",
	"loading to cancel and return",
	"to this menu.",
	NULL
};
static const char *const desc_language[] = {
	"Set the system language.",
	"",
	"This option is normally only",
	"found on PAL GameCubes, so",
	"it usually won't have an",
	"effect on NTSC games.",
	NULL
};
static const char *const desc_cheats[] = {
	"Load Gecko/Ocarina cheat",
	"codes from /codes/GAMEID.gct",
	"on the storage device.",
	NULL
};
static const char *const desc_readspeed[] = {
	"Disc read speed is normally",
	"limited to the speed of the",
	"original GameCube drive.",
	"",
	"Unlocking it allows faster",
	"load times, but can cause",
	"problems in games that are",
	"sensitive to disc timing.",
	NULL
};
static const char *const desc_skip_ipl[] = {
	"Skip loading the GameCube",
	"IPL, even if it's present",
	"on the storage device.",
	NULL
};
static const char *const desc_led[] = {
	"Use the drive slot LED as a",
	"disk activity indicator.",
	"",
	"The LED will be turned on",
	"when reading from or writing",
	"to the storage device.",
	NULL
};

static const char *const desc_video_mode[] = {
	"Auto: pick the video mode",
	"from the game's region.",
	"",
	"Force: patch the game to the",
	"mode selected below.",
	"",
	"None: leave the game's own",
	"video mode untouched.",
	NULL
};
static const char *const desc_forced_mode[] = {
	"Video mode to force when",
	"Video Mode is set to Force.",
	NULL
};
static const char *const desc_force_prog[] = {
	"Patch games to always output",
	"480p (progressive scan).",
	"",
	"Requires component cables, or",
	"an HDMI cable on Wii U.",
	NULL
};
static const char *const desc_patch_pal50[] = {
	"Also patch PAL50 (576i) video",
	"modes when forcing a mode.",
	"",
	"Some PAL games only work",
	"correctly with this off.",
	NULL
};
static const char *const desc_force_wide[] = {
	"Patch games to use a 16:9",
	"aspect ratio. (widescreen)",
	"",
	"Not all games support this",
	"option. The patches will not",
	"be applied to games that have",
	"built-in support for 16:9;",
	"use the game's options screen",
	"to set the display mode.",
	NULL
};
static const char *const desc_wiiu_wide[] = {
	"On Wii U, Nintendont sets the",
	"display to 4:3, which results",
	"in bars on the sides of the",
	"screen. If the game supports",
	"widescreen, turn this on to",
	"set the display back to 16:9.",
	NULL
};
static const char *const desc_video_width[] = {
	"Horizontal picture width in",
	"pixels. Use this to remove",
	"black borders or fix an",
	"overscanned picture.",
	"",
	"Auto keeps the game's value.",
	NULL
};
static const char *const desc_screen_pos[] = {
	"Shift the picture left or",
	"right by up to 20 pixels.",
	NULL
};

static const char *const desc_native_si[] = {
	"Native Control allows use of",
	"GBA link cables on original",
	"Wii systems.",
	"",
	"NOTE: Enabling Native Control",
	"will disable Bluetooth and",
	"USB HID controllers.",
	NULL
};
static const char *const desc_max_pads[] = {
	"Set the maximum number of",
	"native GameCube controller",
	"ports to use on Wii.",
	"",
	"This should usually be kept",
	"at 4 to enable all ports.",
	NULL
};
static const char *const desc_cc_rumble[] = {
	"Enable rumble on Wii Remotes",
	"when using the Wii Classic",
	"Controller or Wii Classic",
	"Controller Pro.",
	NULL
};
static const char *const desc_gamepad_slot[] = {
	"GameCube controller port the",
	"Wii U GamePad is mapped to.",
	"",
	"Set to None to disable the",
	"GamePad as a controller.",
	NULL
};

static const char *const desc_mcemu[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
	NULL
};
static const char *const desc_card_size[] = {
	"Size for newly created",
	"memory card images, in",
	"blocks.",
	"",
	"NOTE: Sizes larger than 251",
	"blocks are known to cause",
	"issues.",
	NULL
};
static const char *const desc_mc_multi[] = {
	"Nintendont usually uses one",
	"emulated memory card image",
	"per game.",
	"",
	"Multi Card switches this",
	"to one image for all USA and",
	"PAL games and one image for",
	"all JPN games.",
	NULL
};

static const char *const desc_bba[] = {
	"Enable BBA Emulation in the",
	"following supported titles",
	"including all their regions:",
	"",
	"Mario Kart: Double Dash!!",
	"Kirby Air Ride",
	"1080 Avalanche",
	"PSO Episode 1&2",
	"PSO Episode III",
	"Homeland",
	NULL
};
static const char *const desc_netprof[] = {
	"Force a Network Profile",
	"to use for BBA Emulation.",
	"",
	"Auto uses the profile that",
	"is currently active. Profiles",
	"that cannot connect to the",
	"internet can also be used.",
	NULL
};

static const char *const desc_debugger[] = {
	"Enable the Gecko debugger",
	"hook. Requires a USB Gecko",
	"in memory card Slot B.",
	NULL
};
static const char *const desc_debugwait[] = {
	"Pause at game start until",
	"the debugger connects.",
	NULL
};
static const char *const desc_osreport[] = {
	"Print the game's OSReport",
	"debug output to the USB",
	"Gecko / log.",
	NULL
};
static const char *const desc_log[] = {
	"Write the kernel debug log",
	"to /ndebug.log on the",
	"storage device.",
	"",
	"Slows down loading. Only",
	"enable this when asked for",
	"a bug report.",
	NULL
};
static const char *const desc_cheat_path[] = {
	"Use the cheat file path",
	"passed in by the launching",
	"application instead of",
	"/codes/GAMEID.gct.",
	NULL
};
static const char *const desc_tri_arcade[] = {
	"Arcade Mode re-enables the",
	"coin slot functionality of",
	"Triforce games.",
	"",
	"To insert a coin, move the",
	"C stick in any direction.",
	NULL
};

/**
 * The settings table.
 * Order within a category is the display order.
 */
static const SettingDef Settings[] =
{
	/** General **/
	{ .name = "Auto Boot", .category = CAT_GENERAL, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_AUTO_BOOT, .desc = desc_autoboot },
	{ .name = "Language", .category = CAT_GENERAL, .kind = KIND_ENUM, .store = STORE_CUSTOM,
	  .get = Get_Language, .set = Set_Language, .labels = LanguageLabels, .count = 7, .desc = desc_language },
	{ .name = "Cheats", .category = CAT_GENERAL, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_CHEATS, .desc = desc_cheats },
	{ .name = "Unlock Read Speed", .category = CAT_GENERAL, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_REMLIMIT, .desc = desc_readspeed },
	{ .name = "Skip IPL", .category = CAT_GENERAL, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_SKIP_IPL, .labels = NoYesLabels, .count = 2, .desc = desc_skip_ipl },
	{ .name = "Drive Access LED", .category = CAT_GENERAL, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .flags = SF_WII_ONLY, .mask = NIN_CFG_LED, .desc = desc_led },

	/** Video **/
	{ .name = "Video Mode", .category = CAT_VIDEO, .kind = KIND_ENUM, .store = STORE_CUSTOM,
	  .get = Get_VideoMode, .set = Set_VideoMode, .labels = VideoModeLabels, .count = 3, .desc = desc_video_mode },
	{ .name = "Forced Mode", .category = CAT_VIDEO, .kind = KIND_ENUM, .store = STORE_CUSTOM,
	  .visible = Vis_VideoForce, .get = Get_ForcedMode, .set = Set_ForcedMode,
	  .labels = ForcedModeLabels, .count = 4, .desc = desc_forced_mode },
	{ .name = "Force Progressive", .category = CAT_VIDEO, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_FORCE_PROG, .desc = desc_force_prog },
	{ .name = "Patch PAL50", .category = CAT_VIDEO, .kind = KIND_BOOL, .store = STORE_VIDEO,
	  .visible = Vis_PatchPAL50, .mask = NIN_VID_PATCH_PAL50, .desc = desc_patch_pal50 },
	{ .name = "Force Widescreen", .category = CAT_VIDEO, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_FORCE_WIDE, .desc = desc_force_wide },
	{ .name = "WiiU Widescreen", .category = CAT_VIDEO, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .flags = SF_WIIU_ONLY, .mask = NIN_CFG_WIIU_WIDE, .desc = desc_wiiu_wide },
	{ .name = "Video Width", .category = CAT_VIDEO, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .get = Get_VideoWidth, .set = Set_VideoWidth, .min = 0, .max = VIDWIDTH_STEPS,
	  .format = Fmt_VideoWidth, .onChange = OnChange_Video, .desc = desc_video_width },
	{ .name = "Screen Position", .category = CAT_VIDEO, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .get = Get_ScreenPos, .set = Set_ScreenPos, .min = -20, .max = 20,
	  .format = Fmt_Signed, .onChange = OnChange_Video, .desc = desc_screen_pos },

	/** Input **/
	{ .name = "Native Control", .category = CAT_INPUT, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .flags = SF_WII_ONLY, .mask = NIN_CFG_NATIVE_SI, .desc = desc_native_si },
	{ .name = "Max Pads", .category = CAT_INPUT, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .flags = SF_WII_ONLY, .get = Get_MaxPads, .set = Set_MaxPads, .min = 0, .max = NIN_CFG_MAXPAD,
	  .format = Fmt_Plain, .desc = desc_max_pads },
	{ .name = "Wiimote CC Rumble", .category = CAT_INPUT, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_CC_RUMBLE, .desc = desc_cc_rumble },
	{ .name = "GamePad Slot", .category = CAT_INPUT, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .flags = SF_WIIU_ONLY, .get = Get_GamepadSlot, .set = Set_GamepadSlot, .min = 0, .max = NIN_CFG_MAXPAD,
	  .format = Fmt_GamepadSlot, .desc = desc_gamepad_slot },

	/** Memory Card **/
	{ .name = "Emulation", .category = CAT_MEMCARD, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_MEMCARDEMU, .desc = desc_mcemu },
	{ .name = "Card Size", .category = CAT_MEMCARD, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .visible = Vis_MemCardEmu, .get = Get_CardSize, .set = Set_CardSize, .min = 0, .max = MEM_CARD_MAX,
	  .format = Fmt_CardSize, .desc = desc_card_size },
	{ .name = "Multi Card", .category = CAT_MEMCARD, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .visible = Vis_MemCardEmu, .mask = NIN_CFG_MC_MULTI, .desc = desc_mc_multi },

	/** Network **/
	{ .name = "BBA Emulation", .category = CAT_NETWORK, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_BBA_EMU, .desc = desc_bba },
	{ .name = "Network Profile", .category = CAT_NETWORK, .kind = KIND_INT, .store = STORE_CUSTOM,
	  .visible = Vis_BBAProfile, .get = Get_NetProfile, .set = Set_NetProfile, .min = 0, .max = 3,
	  .format = Fmt_NetProfile, .desc = desc_netprof },

	/** Advanced **/
	{ .name = "Debugger", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .flags = SF_WII_ONLY, .mask = NIN_CFG_DEBUGGER, .desc = desc_debugger },
	{ .name = "Debugger Wait", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .flags = SF_WII_ONLY, .mask = NIN_CFG_DEBUGWAIT, .desc = desc_debugwait },
	{ .name = "OSReport", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_OSREPORT, .desc = desc_osreport },
	{ .name = "Log to File", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_LOG, .desc = desc_log },
	{ .name = "Cheat Path", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_CHEAT_PATH, .desc = desc_cheat_path },
	{ .name = "TRI Arcade Mode", .category = CAT_ADVANCED, .kind = KIND_BOOL, .store = STORE_CONFIG,
	  .mask = NIN_CFG_ARCADE_MODE, .desc = desc_tri_arcade },
};
#define NUM_SETTINGS (sizeof(Settings) / sizeof(Settings[0]))

/**********************************************************************
 * Value access
 **********************************************************************/

static bool Setting_IsVisible(const SettingDef *d)
{
	if ((d->flags & SF_WII_ONLY) && IsWiiU())
		return false;
	if ((d->flags & SF_WIIU_ONLY) && !IsWiiU())
		return false;
	if (d->visible && !d->visible())
		return false;
	return true;
}

/**
 * Labels for a setting's states. KIND_BOOL rows normally leave .labels unset,
 * so most of the table does not have to repeat the Off/On pair.
 */
static const char *const *Setting_Labels(const SettingDef *d, u32 *count)
{
	if (d->labels)
	{
		*count = d->count;
		return d->labels;
	}
	*count = 2;
	return OnOffLabels;
}

static s32 Setting_Get(const SettingDef *d)
{
	switch (d->store)
	{
		case STORE_CONFIG:
			return (ncfg->Config & d->mask) ? 1 : 0;
		case STORE_VIDEO:
			return (ncfg->VideoMode & d->mask) ? 1 : 0;
		case STORE_CUSTOM:
		default:
			return d->get ? d->get() : 0;
	}
}

static void Setting_Set(const SettingDef *d, s32 value)
{
	switch (d->store)
	{
		case STORE_CONFIG:
			if (value) ncfg->Config |= d->mask;
			else       ncfg->Config &= ~d->mask;
			break;
		case STORE_VIDEO:
			if (value) ncfg->VideoMode |= d->mask;
			else       ncfg->VideoMode &= ~d->mask;
			break;
		case STORE_CUSTOM:
		default:
			if (d->set) d->set(value);
			break;
	}
	if (d->onChange)
		d->onChange();
}

/**
 * Step a setting by delta, wrapping around.
 * @return True if the value changed (or an action ran).
 */
static bool Setting_Adjust(const SettingDef *d, s32 delta)
{
	s32 min, count;
	switch (d->kind)
	{
		case KIND_INT:
			min = d->min;
			count = d->max - d->min + 1;
			break;
		case KIND_BOOL:
		case KIND_ENUM:
		default:
		{
			u32 n;
			Setting_Labels(d, &n);
			min = 0;
			count = (s32)n;
			break;
		}
	}
	if (count <= 1)
		return false;

	s32 idx = Setting_Get(d) - min;
	idx = (idx + delta) % count;
	if (idx < 0)
		idx += count;
	Setting_Set(d, idx + min);
	return true;
}

static void Setting_FormatValue(const SettingDef *d, char *buf, size_t len)
{
	const s32 v = Setting_Get(d);
	switch (d->kind)
	{
		case KIND_INT:
			if (d->format) d->format(v, buf, len);
			else snprintf(buf, len, "%d", v);
			break;
		case KIND_BOOL:
		case KIND_ENUM:
		default:
		{
			u32 n;
			const char *const *labels = Setting_Labels(d, &n);
			if (v >= 0 && (u32)v < n)
				snprintf(buf, len, "%s", labels[v]);
			else
				snprintf(buf, len, "?");
			break;
		}
	}
}

/**********************************************************************
 * Visible list / categories
 **********************************************************************/

/**
 * Collect the visible settings of a category.
 * @param out Receives indices into Settings[].
 * @return Count.
 */
static u32 BuildVisibleList(s32 category, u8 *out, u32 outMax)
{
	u32 n = 0, i;
	for (i = 0; i < NUM_SETTINGS && n < outMax; i++)
	{
		if (Settings[i].category != category)
			continue;
		if (!Setting_IsVisible(&Settings[i]))
			continue;
		out[n++] = (u8)i;
	}
	return n;
}

static bool Category_IsVisible(s32 category)
{
	u8 tmp[1];
	return BuildVisibleList(category, tmp, 1) > 0;
}

static s32 NextCategory(s32 from, s32 dir)
{
	s32 c = from, i;
	for (i = 0; i < CAT_COUNT; i++)
	{
		c = (c + dir + CAT_COUNT) % CAT_COUNT;
		if (Category_IsVisible(c))
			return c;
	}
	return from;
}

static void ClampCursor(SettingsMenuState *st, u32 n)
{
	if (st->pos >= (s32)n)
		st->pos = (s32)n - 1;
	if (st->pos < -1)
		st->pos = -1;

	// Keep the cursor within the scrolled window.
	if (st->pos < 0) {
		st->scroll = 0;
	} else if (st->pos < st->scroll) {
		st->scroll = st->pos;
	} else if (st->pos >= st->scroll + SM_MAX_ROWS) {
		st->scroll = st->pos - SM_MAX_ROWS + 1;
	}
	if (st->scroll < 0)
		st->scroll = 0;
}

/**********************************************************************
 * Public API
 **********************************************************************/

void SettingsMenu_Reset(SettingsMenuState *st)
{
	st->category = CAT_GENERAL;
	if (!Category_IsVisible(st->category))
		st->category = NextCategory(st->category, +1);
	st->pos = 0;
	st->scroll = 0;
}

u32 SettingsMenu_Update(SettingsMenuState *st, const SettingsMenuInput *in)
{
	u32 ret = 0;
	u8 list[NUM_SETTINGS];
	u32 n = BuildVisibleList(st->category, list, NUM_SETTINGS);

	// Category switching.
	s32 catDelta = 0;
	if (in->nextTab)
		catDelta = +1;
	else if (st->pos < 0 && in->right)
		catDelta = +1;
	else if (st->pos < 0 && in->left)
		catDelta = -1;
	else if (st->pos < 0 && in->ok)
		catDelta = +1;

	if (catDelta != 0)
	{
		st->category = NextCategory(st->category, catDelta);
		n = BuildVisibleList(st->category, list, NUM_SETTINGS);
		st->pos = in->nextTab ? 0 : -1;
		st->scroll = 0;
		ret |= SMENU_REDRAW;
	}

	// Cursor movement.
	if (in->down)
	{
		st->pos++;
		if (st->pos >= (s32)n)
			st->pos = -1;	// Wrap to the tab row.
		ret |= SMENU_REDRAW;
	}
	else if (in->up)
	{
		st->pos--;
		if (st->pos < -1)
			st->pos = (s32)n - 1;
		ret |= SMENU_REDRAW;
	}

	// Value changes.
	if (st->pos >= 0 && st->pos < (s32)n)
	{
		const SettingDef *d = &Settings[list[st->pos]];
		bool changed = false;
		if (in->ok || in->right)
			changed = Setting_Adjust(d, +1);
		else if (in->left)
			changed = Setting_Adjust(d, -1);

		if (changed)
		{
			ret |= SMENU_CHANGED | SMENU_REDRAW;
			// Visibility may have changed (e.g. Show Advanced, Memcard Emulation).
			n = BuildVisibleList(st->category, list, NUM_SETTINGS);
			if (n == 0)
				st->category = NextCategory(st->category, +1);
		}
	}

	ClampCursor(st, n);
	return ret;
}

void SettingsMenu_Draw(const SettingsMenuState *st)
{
	u8 list[NUM_SETTINGS];
	const u32 n = BuildVisibleList(st->category, list, NUM_SETTINGS);
	u32 i;

	/** Category tab bar. **/
	int x = SM_X_ITEM;
	for (i = 0; i < CAT_COUNT; i++)
	{
		if (!Category_IsVisible(i))
			continue;
		const bool sel = ((s32)i == st->category);
		char tab[16];
		snprintf(tab, sizeof(tab), sel ? "[%s]" : " %s ", CategoryNames[i]);
		PrintFormat(MENU_SIZE, sel ? SM_COLOR_TAB_ON : SM_COLOR_TAB_OFF, x, SettingY(SM_ROW_TABS), "%s", tab);
		x += (strlen(tab) + 1) * 10;
	}
	if (st->pos < 0)
		PrintFormat(MENU_SIZE, SM_COLOR_ITEM, SM_X_CURSOR, SettingY(SM_ROW_TABS), ARROW_RIGHT);

	/** Settings list. **/
	for (i = st->scroll; i < n && i < (u32)(st->scroll + SM_MAX_ROWS); i++)
	{
		const SettingDef *d = &Settings[list[i]];
		const u32 row = SM_ROW_FIRST + (i - st->scroll);
		const bool sel = ((s32)i == st->pos);
		char value[24];
		Setting_FormatValue(d, value, sizeof(value));

		if (sel)
			PrintFormat(MENU_SIZE, SM_COLOR_ITEM, SM_X_CURSOR, SettingY(row), ARROW_RIGHT);

		if (sel && d->kind != KIND_BOOL)
		{
			// Adjustable with Left/Right: show arrows around the value.
			PrintFormat(MENU_SIZE, SM_COLOR_ITEM, SM_X_ITEM, SettingY(row), "%-*s: ", SM_NAME_WIDTH, d->name);
			PrintFormat(MENU_SIZE, SM_COLOR_VALUE, SM_X_ITEM + (SM_NAME_WIDTH + 2) * 10, SettingY(row),
				    ARROW_LEFT " %s " ARROW_RIGHT, value);
		}
		else
		{
			PrintFormat(MENU_SIZE, SM_COLOR_ITEM, SM_X_ITEM, SettingY(row), "%-*s: ", SM_NAME_WIDTH, d->name);
			PrintFormat(MENU_SIZE, sel ? SM_COLOR_VALUE : SM_COLOR_ITEM, SM_X_ITEM + (SM_NAME_WIDTH + 2) * 10, SettingY(row),
				    "%s", value);
		}
	}

	// Scroll indicators.
	if (st->scroll > 0)
		PrintFormat(MENU_SIZE, SM_COLOR_HELP, SM_X_ITEM, SettingY(SM_ROW_FIRST - 1), "...");
	if (n > (u32)(st->scroll + SM_MAX_ROWS))
		PrintFormat(MENU_SIZE, SM_COLOR_HELP, SM_X_ITEM, SettingY(SM_ROW_FIRST + SM_MAX_ROWS), "...");

	/** Description. **/
	const char *const *desc = NULL;
	if (st->pos < 0)
	{
		static const char *const desc_tabs[] = {
			"Left/Right: change category",
			"Down: edit its settings",
			NULL
		};
		desc = desc_tabs;
	}
	else if (st->pos < (s32)n)
	{
		desc = Settings[list[st->pos]].desc;
	}
	if (desc)
	{
		u32 row = SM_ROW_FIRST;
		for (; *desc && row < SM_ROW_HELP; desc++, row++)
		{
			if (**desc)
				PrintFormat(MENU_SIZE, SM_COLOR_ITEM, SM_X_DESC, SettingY(row), "%s", *desc);
		}
	}

	/** Button help. **/
	PrintFormat(MENU_SIZE, SM_COLOR_HELP, SM_X_ITEM, SettingY(SM_ROW_HELP),
		    "A: Change   Left/Right: Adjust   Y/2: Next Tab");
	PrintFormat(MENU_SIZE, SM_COLOR_HELP, SM_X_ITEM, SettingY(SM_ROW_HELP + 1),
		    "B: Game List   X/1: Update Nintendont");
}
