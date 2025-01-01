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

#include "i_system.h"

#include <chrono>
#include <thread>

#include "con_main.h"
#include "dm_defs.h"
#include "e_main.h"
#include "epi.h"
#include "epi_sdl.h"
#include "epi_windows.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "version.h"
#include "w_wad.h"

extern FILE *debug_file;
extern FILE *log_file;

#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
extern HANDLE windows_timer;
#endif

// output string buffer
static constexpr int16_t kMessageBufferSize = 4096;
static char              message_buffer[kMessageBufferSize];

void SystemStartup(void)
{
    StartupGraphics(); // SDL requires this to be called first
    StartupControl();
    StartupAudio();
}

void CloseProgram(int exitnum)
{
    exit(exitnum);
}

void LogWarning(const char *warning, ...)
{
    va_list argptr;

    va_start(argptr, warning);
    stbsp_vsprintf(message_buffer, warning, argptr);
    va_end(argptr);

    LogPrint("WARNING: %s", message_buffer);
}

void FatalError(const char *error, ...)
{
    va_list argptr;

    va_start(argptr, error);
    stbsp_vsprintf(message_buffer, error, argptr);
    va_end(argptr);

    if (log_file)
    {
        fprintf(log_file, "ERROR: %s\n", message_buffer);
        fflush(log_file);
    }

    if (debug_file)
    {
        fprintf(debug_file, "ERROR: %s\n", message_buffer);
        fflush(debug_file);
    }

    SystemShutdown();

    ShowMessageBox(message_buffer, "EDGE-Classic Error");

#ifndef NDEBUG
    // trigger debugger
    abort();
#else
    CloseProgram(EXIT_FAILURE);
#endif    

}

void LogPrint(const char *message, ...)
{
    va_list argptr;

    char printbuf[kMessageBufferSize];
    printbuf[kMessageBufferSize - 1] = 0;

    va_start(argptr, message);
    stbsp_vsnprintf(printbuf, sizeof(printbuf), message, argptr);
    va_end(argptr);

    EPI_ASSERT(printbuf[kMessageBufferSize - 1] == 0);

    if (log_file)
    {
        fprintf(log_file, "%s", printbuf);
        fflush(log_file);
    }

    // If debuging enabled, print to the debug_file
    LogDebug("%s", printbuf);

    // Send the message to the console.
    ConsolePrint("%s", printbuf);

#ifdef EDGE_WEB
    // Send to debug console in browser
    printf("%s", printbuf);
#endif
}

void ShowMessageBox(const char *message, const char *title)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
}

int PureRandomNumber(void)
{
    int P1 = (int)time(nullptr);
    int P2 = (int)GetMicroseconds();

    return (P1 ^ P2) & 0x7FFFFFFF;
}

uint32_t GetMicroseconds(void)
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void SleepForMilliseconds(int millisecs)
{
#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    // On Windows use high resolution timer if available, the Sleep Win32 call
    // defaults to 15.6ms resolution and timeBeginPeriod is problematic
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

void SystemShutdown(void)
{
    ShutdownSound();
    ShutdownControl();
    ShutdownGraphics();

    if (log_file)
    {
        fclose(log_file);
        log_file = nullptr;
    }

    // -KM- 1999/01/31 Close the debug_file
    if (debug_file != nullptr)
    {
        fclose(debug_file);
        debug_file = nullptr;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
