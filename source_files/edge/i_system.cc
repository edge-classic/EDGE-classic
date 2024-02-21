//----------------------------------------------------------------------------
//  EDGE Misc System Interface Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------


#include "epi_sdl.h"
#include "epi_windows.h"

#include <chrono>
#include <thread>

#include "con_main.h"
#include "dm_defs.h"
#include "e_main.h"
#include "i_system.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "w_wad.h"
#include "version.h"

extern FILE *debugfile;
extern FILE *logfile;

#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
extern HANDLE windows_timer;
#endif

// output string buffer
#define MSGBUFSIZE 4096
static char msgbuf[MSGBUFSIZE];

void I_SystemStartup(void)
{
    I_StartupGraphics(); // SDL requires this to be called first
    I_StartupControl();
    I_StartupSound();
}

void I_CloseProgram(int exitnum)
{
    std::exit(exitnum);
}

void I_Warning(const char *warning, ...)
{
    va_list argptr;

    va_start(argptr, warning);
    vsprintf(msgbuf, warning, argptr);
    va_end(argptr);

    I_Printf("WARNING: %s", msgbuf);
}

void I_Error(const char *error, ...)
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

    I_MessageBox(msgbuf, "EDGE-Classic Error");

    I_CloseProgram(EXIT_FAILURE);
}

void I_Printf(const char *message, ...)
{
    va_list argptr;

    char printbuf[MSGBUFSIZE];

    va_start(argptr, message);
    vsnprintf(printbuf, sizeof(printbuf), message, argptr);
    va_end(argptr);

    I_Logf("%s", printbuf);

    // If debuging enabled, print to the debugfile
    I_Debugf("%s", printbuf);

    // Send the message to the console.
    ConsolePrintf("%s", printbuf);

#ifdef EDGE_WEB
    // Send to debug console in browser
    printf("%s", printbuf);
#endif
}

void I_MessageBox(const char *message, const char *title)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
}

int I_PureRandom(void)
{
    int P1 = (int)time(nullptr);
    int P2 = (int)I_GetMicros();

    return (P1 ^ P2) & 0x7FFFFFFF;
}

uint32_t I_GetMicros(void)
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void I_Sleep(int millisecs)
{

#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    // On Windows use high resolution timer if available, the Sleep Win32 call defaults to 15.6ms resolution and
    // timeBeginPeriod is problematic
    if (windows_timer != nullptr)
    {
        LARGE_INTEGER due_time;
        due_time.QuadPart = -((LONGLONG)(millisecs * 1000000) / 100);
        if (SetWaitableTimerEx(windows_timer, &due_time, 0, nullptr, nullptr, nullptr, 0)) 
        {
            WaitForSingleObject(windows_timer, INFINITE);
        }
        return;
    }
#endif

    SDL_Delay(millisecs);
}

void I_SystemShutdown(void)
{
    // make sure audio is unlocked (e.g. I_Error occurred)
    I_UnlockAudio();

    I_ShutdownSound();
    I_ShutdownControl();
    I_ShutdownGraphics();

    if (logfile)
    {
        fclose(logfile);
        logfile = nullptr;
    }

    // -KM- 1999/01/31 Close the debugfile
    if (debugfile != nullptr)
    {
        fclose(debugfile);
        debugfile = nullptr;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
