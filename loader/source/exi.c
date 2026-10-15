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
#include "global.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogc/usbgecko.h>

#define	EXI_BASE	0xCD006800
#define EXI			0xCD006814

#if 0
static bool wiiu_done = false;
#endif
// FIXME: Re-enable Wii U file logging after testing FatFs.
//static FILE *nl_log = NULL;
static u32 GeckoFound = 0;

void CheckForGecko( void )
{
	if( !IsWiiU() )
		GeckoFound = usb_isgeckoalive( 1 );
}

void EXISendByte( char byte )
{

loop:
	*(vu32*)EXI			= 0xD0;
	*(vu32*)(EXI+0x10)	= 0xB0000000 | (byte<<20);
	*(vu32*)(EXI+0x0C)	= 0x19;

	while( *(vu32*)(EXI+0x0C)&1 );

	u32 loop =  *(vu32*)(EXI+0x10)&0x4000000;
	
	*(vu32*)EXI	= 0;

	if( !loop )
		goto loop;

	return;
}

#ifdef NIN_TEST
#include "ff_utf8.h"
#include "exi.h"
/*
 * Test builds (make TEST=seconds): every line also goes to sd:/nlog.txt, so a hang that
 * needs a power cut still leaves the loader's last words. Opened and closed per line:
 * a file held open across the loader's device shutdown could damage the card.
 */
static void TestLog(const char *s)
{
	FIL f;
	UINT done;
	if (f_open_char(&f, "sd:/nlog.txt", FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
		return;	// before the SD is mounted, or after it is closed
	f_lseek(&f, f_size(&f));
	f_write(&f, s, strlen(s), &done);
	f_close(&f);
}

/*
 * Test builds also stream every line to the PC that launched the run, through
 * hbc-reborn's network log (the lab script registers for it with HBC). main.c closes
 * it before each IOS reload, which drops every socket, and opens it again after.
 */
#include "hbc_netlog.h"
void TestNetOpen(void)
{
	s32 r = hbc_netlog_init();
	gprintf("TEST: network log %s (%d)\r\n", r == 0 ? "open" : "unavailable", r);
}
void TestNetClose(void)
{
	hbc_netlog_close();
}
static void TestNetSend(const char *s, int len)
{
	if (len <= 0 || hbc_netlog_lock == LWP_MUTEX_NULL)
		return;	// nothing to send, or the log was never opened
	if (len > 4095)
		len = 4095;
	LWP_MutexLock(hbc_netlog_lock);
	hbc_netlog_send(s, len);
	LWP_MutexUnlock(hbc_netlog_lock);
}
#endif

/**
 * Log a debug message.
 * - Wii: Logs to USB Gecko.
 * - Wii U: Logs to a file on the root device.
 * @param str printf-style format string.
 * @param ... printf arguments.
 * @return vsnprintf() return value; 0 if nothing was written; or -1 on error.
 */
int gprintf( const char *str, ... )
{
	int ret = 0;

	if( IsWiiU() )
	{
		// FIXME: Re-enable Wii U file logging after testing FatFs.
#if 0
		if(wiiu_done == true)
			return 0;

		// We're running on a vWii, log the results to a file

		// Open the file if it hasn't been already
		if (nl_log == NULL)
		{
			char LogPath[20];
			snprintf(LogPath, "%s:/nloader.log", GetRootDevice());
			nl_log = fopen(LogPath, "w");
		}
		if (nl_log != NULL)
		{
			va_list ap;
			va_start(ap, str);
			ret = vfprintf(nl_log, str, ap); // No need for a buffer, goes straight to the file
			// Flushes the stream so we don't have to wait for the file to close or it to fill
			fflush(nl_log);
			va_end(ap);
		} else {
			// Couldn't open the file
			return -1;
		}
#endif
	} else {
		// We're running on a real Wii, send the results to a USB Gecko
#ifndef NIN_TEST
		if (!GeckoFound)
		{
			// No USB Gecko found
			return 0;
		}
#endif

		char astr[4096];
		va_list ap;
		va_start(ap, str);
		ret = vsnprintf(astr, sizeof(astr), str, ap);
		va_end(ap);
#ifdef NIN_TEST
		TestLog(astr);
		TestNetSend(astr, ret);
		if (!GeckoFound)
			return ret;
#endif

		// Send the string.
		const char *p = astr;
		for (; *p != '\0'; p++)
		{
			EXISendByte(*p);
		}
	}

	// Everything went okay
	return ret;
}

void closeLog(void)
{
	// FIXME: Re-enable Wii U file logging after testing FatFs.
#if 0
	wiiu_done = true;

	if(nl_log != NULL)
		fclose(nl_log);
	nl_log = NULL;
#endif
}
