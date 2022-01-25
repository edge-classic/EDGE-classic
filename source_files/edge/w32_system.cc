//----------------------------------------------------------------------------
//  EDGE Win32 Misc System Interface Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "i_sdlinc.h"
#include "i_net.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "con_main.h"
#include "dm_defs.h"
#include "e_main.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "w_wad.h"
#include "version.h"
#include "z_zone.h"

#include "w32_sysinc.h"

#define INTOLERANT_MATH 1  // -AJA- FIXME: temp fix to get to compile

extern FILE* debugfile;
extern FILE* logfile;

// output string buffer
#define MSGBUFSIZE 4096
static char msgbuf[MSGBUFSIZE];

// ================ INTERNALS =================

//
// FlushMessageQueue
//
// Hacktastic work around for SDL_Quit() 
//
void FlushMessageQueue()
{
	MSG msg;

	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		if ( msg.message == WM_QUIT )
			break;

		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
}

// ============ END OF INTERNALS ==============

void I_SetupSignalHandlers(bool allow_coredump)
{
	/* nothing needed */
}

void I_CheckAlreadyRunning(void)
{
	HANDLE edgemap;

	// Check we've not already running
	edgemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READONLY, 0, 8, TITLE);
	if (edgemap)
	{
		DWORD lasterror = GetLastError();
		if (lasterror == ERROR_ALREADY_EXISTS)
		{
			I_MessageBox("EDGE is already running!", "EDGE Error");
			I_CloseProgram(9);
		}
	}
}


//
// I_SystemStartup
//
// -ACB- 1998/07/11 Reformatted the code.
//
void I_SystemStartup(void)
{
	if (SDL_Init(0) < 0)
		I_Error("Couldn't init SDL!!\n%s\n", SDL_GetError());

	I_StartupGraphics(); // SDL requires this to be called first
	I_StartupControl();
	I_StartupSound();
	I_StartupMusic(); // Startup Music System
	I_StartupNetwork();

#ifndef INTOLERANT_MATH
	// -ACB- 2000/06/05 This switches off x87 signalling error
	_control87(EM_ZERODIVIDE | EM_INVALID, EM_ZERODIVIDE | EM_INVALID);
#endif /* INTOLERANT_MATH */

}

//
// I_DisplayExitScreen
//
void I_DisplayExitScreen(void)
{
}

//
// I_CloseProgram
//
void I_CloseProgram(int exitnum)
{
	exit(exitnum);
}

//
// I_TraceBack
//
// Like I_CloseProgram, but may display some sort of debugging information
// on some systems (typically the function call stack).
//
void I_TraceBack(void)
{
	I_CloseProgram(-1);
}


//
// I_Warning
//
// -AJA- 1999/09/10: added this.
//
void I_Warning(const char *warning,...)
{
	va_list argptr;

	va_start(argptr, warning);
	vsprintf(msgbuf, warning, argptr);

	I_Printf("WARNING: %s", msgbuf);
	va_end(argptr);
}

//
// I_Error
//
void I_Error(const char *error,...)
{
	va_list argptr;

	va_start(argptr, error);
	vsprintf(msgbuf, error, argptr);
	va_end(argptr);

	if (logfile)
	{
		fprintf(logfile, "ERROR: %s\n", msgbuf);
		fflush(logfile);
	}

	if (debugfile)
	{
		fprintf(debugfile, "ERROR: %s\n", msgbuf);
		fflush(debugfile);
	}

	I_SystemShutdown();

	I_MessageBox(msgbuf, "EDGE Error");

	I_CloseProgram(-1);
}

//
// I_Printf
//
void I_Printf(const char *message,...)
{
	va_list argptr;
	char *string;
	char printbuf[MSGBUFSIZE];

	// clear the buffer
	memset (printbuf, 0, MSGBUFSIZE);

	string = printbuf;

	va_start(argptr, message);

	// Print the message into a text string
	vsprintf(printbuf, message, argptr);

	L_WriteLog("%s", printbuf);

	// If debuging enabled, print to the debugfile
	L_WriteDebug("%s", printbuf);

	// Clean up \n\r combinations
	while (*string)
	{
		if (*string == '\n')
		{
			memmove(string + 2, string + 1, strlen(string));
			string[1] = '\r';
			string++;
		}
		string++;
	}

	// Send the message to the console.
	CON_Printf("%s", printbuf);

	va_end(argptr);
}


void I_MessageBox(const char *message, const char *title)
{
	MessageBox(NULL, message, title,
			   MB_ICONEXCLAMATION | MB_OK |
			   MB_SYSTEMMODAL | MB_SETFOREGROUND);
}


//
// I_PureRandom
//
int I_PureRandom(void)
{
	return ((int)time(NULL) ^ (int)I_ReadMicroSeconds()) & 0x7FFFFFFF;
}

//
// I_ReadMicroSeconds
//
u32_t I_ReadMicroSeconds(void)
{
	return (u32_t) (timeGetTime() * 1000);
}

//
// I_Sleep
//
void I_Sleep(int millisecs)
{
	::Sleep(millisecs);
}


//
// I_SystemShutdown
//
// -ACB- 1998/07/11 Tidying the code
//
void I_SystemShutdown(void)
{
	// make sure audio is unlocked (e.g. I_Error occurred)
	I_UnlockAudio();

	I_ShutdownNetwork();
	I_ShutdownMusic();
	I_ShutdownSound();
	I_ShutdownControl();
	I_ShutdownGraphics();

	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}

	// -KM- 1999/01/31 Close the debugfile
	if (debugfile != NULL)
	{
		fclose(debugfile);
		debugfile = NULL;
	}

	//ShowCursor(TRUE);
	FlushMessageQueue();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
