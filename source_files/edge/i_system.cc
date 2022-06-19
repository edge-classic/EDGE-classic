//----------------------------------------------------------------------------
//  EDGE-Classic Misc System Interface Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
//  Copyright (c) 2022 The EDGE-Classic Community
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

#include <chrono>
#include <thread>

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

extern FILE* debugfile;
extern FILE* logfile;

// output string buffer
#define MSGBUFSIZE 4096
static char msgbuf[MSGBUFSIZE];

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
	I_StartupMusic();
	I_StartupNetwork();
}

//
// I_CloseProgram
//
void I_CloseProgram(int exitnum)
{
	std::exit(exitnum);
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
	va_end(argptr);
	I_Printf("WARNING: %s", msgbuf);

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

	I_CloseProgram(EXIT_FAILURE);
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
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
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
	return (u32_t) std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

//
// I_Sleep
//
void I_Sleep(int millisecs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(millisecs));
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
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
