
#ifndef __COMMON_CONFIG_STRINGS_H__
#define __COMMON_CONFIG_STRINGS_H__

// NOTE: Since config v11 the loader's Settings menu is table driven
// (loader/source/SettingsMenu.c) and no longer uses these arrays.
// They are kept for third-party tools; order must match CommonConfig.h.
const char* OptionStrings[] =
{
	"Cheats",
	"Debugger",
	"Debugger Wait",
	"Memcard Emulation",
	"Cheat Path",
	"Force Widescreen",
	"Force Progressive",
	"Auto Boot",
	"Unlock Read Speed",
	"OSReport",
	"WiiU Widescreen", //Replaces USB
	"Drive Access LED",
	"Log",
	
	"MaxPads",
	"Language",
	"Video",
	"Videomode",
	"Memcard Blocks",
	"Memcard Multi",
	"Native Control",
};

const char* LanguageStrings[] =
{
	"Eng",
	"Ger",
	"Fre",
	"Spa",
	"Ita",
	"Dut",

	"Auto",
};

const char* VideoStrings[] =
{
	"Auto",
	"Force",
	"None",
	"Invalid",
	"Force (Deflicker)", // Deprecated: deflicker is a separate setting since v11
};

const char* VideoModeStrings[] =
{
	"PAL50",
	"PAL60",
	"NTSC",
	"MPAL",
};

#endif
