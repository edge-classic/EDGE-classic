//----------------------------------------------------------------------------
//  EDGE Platform Interface (EPI) Header
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#pragma once

#include <stdint.h>

#include <string>

//--------------------------------------------------------
//  SYSTEM functions.
//--------------------------------------------------------

// -ACB- 1999/09/20 Removed system specific attribs.

// This routine is responsible for getting things off the ground, in
// particular calling all the other platform initialisers (i.e.
// StartupControl, StartupGraphics, StartupMusic and
// StartupSound).  Does whatever else the platform code needs.
void SystemStartup(void);

#ifdef __GNUC__
void LogPrint(const char *message, ...) __attribute__((format(printf, 1, 2)));
void LogWarning(const char *warning, ...) __attribute__((format(printf, 1, 2)));
void LogDebug(const char *message, ...) __attribute__((format(printf, 1, 2)));
void FatalError(const char *error, ...) __attribute__((format(printf, 1, 2)));
#else
void LogPrint(const char *message, ...);
void LogWarning(const char *warning, ...);
void LogDebug(const char *message, ...);
void FatalError(const char *error, ...);
#endif

// The opposite of the SystemStartup routine.  This will shutdown
// everything running in the platform code, by calling the other
// termination functions (ShutdownSound, ShutdownMusic,
// ShutdownGraphics and ShutdownControl), and doing anything else
// the platform code needs to (e.g. freeing all other resources).
void SystemShutdown(void);

// Exit the program immediately, using the given `exitnum' as the
// program's exit status.  This is the very last thing done, and
// SystemShutdown() is guaranteed to have already been called.
[[noreturn]] void CloseProgram(int exitnum);

// -AJA- 2005/01/21: sleep for the given number of milliseconds.
void SleepForMilliseconds(int millisecs);

// -AJA- 2007/04/13: display a system message box with the
// given message (typically a serious error message).
void ShowMessageBox(const char *message, const char *title);

extern std::string executable_path;

//--------------------------------------------------------
//  INPUT functions.
//--------------------------------------------------------
//
// -ACB- 1999/09/19 moved from I_Input.H

// Initialises all control devices (i.e. input devices), such as the
// keyboard, mouse and joysticks.  Should be called from
// SystemStartup() -- the main code never calls this function.
void StartupControl(void);

// Causes all control devices to send their events to the engine via
// the PostEvent() function.
void ControlGetEvents(void);

// Shuts down all control devices.  This is the opposite of
// StartupControl().  Should be called from SystemShutdown(), the
// main code never calls this function.
void ShutdownControl(void);

// Returns a fairly random value, used as seed for EDGE's internal
// random engine.  If this function would return a constant value,
// everything would still work great, except that random events before
// the first tic of a level (like random RTS spawn) would be
// predictable.
int PureRandomNumber(void);

// Returns a value that increases monotonically over time.  The value
// should increase by TICRATE every second (TICRATE is currently 35).
// The starting value should be close to zero.
int GetTime(void);

// Returns a value that increases by 1000 every second (i.e. each unit is
// a single millisecond).  This timer begins at zero when the application
// is first begun, hence it won't normally overflow (unless the engine
// runs continuously for 24 days).
int GetMilliseconds(void);

// Returns a value that increases by 1000000 every second (i.e. each unit
// is a single microsecond).  Since this value will wrap-around regularly
// (roughly every 71 minutes), caller *MUST* check for this situation.
uint32_t GetMicroseconds(void);

//--------------------------------------------------------
//  MUSIC functions.
//--------------------------------------------------------
//
// -ACB- 1999/09/19 moved from I_Music.H

class AbstractMusicPlayer;

// This variable enables/disables music.  Initially false, it is set
// to true by the "-no_music" option.  Can also be set to true by the
// platform code when no working music device is found.
extern bool no_music;

// Initialises the music system.  Returns true if successful,
// otherwise false.  (You should set "no_music" to true if it fails).
// The main code never calls this function, it should be called by
// SystemStartup().
void StartupMusic(void);

//--------------------------------------------------------
//  SOUND functions.
//--------------------------------------------------------
//
// -ACB- 1999/09/20 Moved from I_Sound.H

// This variable enables/disables sound.  Initially false, it is set
// to true by the "-no_sound" option.  Can also be set to true by the
// platform code when no working sound device is found.
extern bool no_sound;

// Initialises the sound system.  Returns true if successful,
// otherwise false if something went wrong (NOTE: you must set no_sound
// to false when it fails).   The main code never calls this function,
// it should be called by SystemStartup().
void StartupAudio(void);

// Shuts down the sound system.  This is the companion function to
// StartupSound().  This must be called by SystemShutdown(), the
// main code never calls this function.
void AudioShutdown(void);

//--------------------------------------------------------
//  VIDEO functions.
//--------------------------------------------------------
//
// -ACB- 1999/09/20 Moved from I_Video.H

struct DisplayMode;

// Initialises the graphics system.  This should be called by
// SystemStartup(), the main code never calls this function
// directly.  This function should determine what video modes are
// available, and call V_AddAvailableResolution() for them.
void StartupGraphics(void);

// Shuts down the graphics system.  This is the companion function to
// StartupGraphics.  Note that this should be called by
// SystemStartup(), the main code never calls this function.
void ShutdownGraphics(void);

// Called to prepare the screen for rendering (if necessary).
void StartFrame(void);

// Called when the current frame has finished being rendered.  This
// routine typically copies the screen buffer to the video memory.  It
// may also handle double/triple buffering here.
void FinishFrame(void);

// Tries to set the video card to the given mode (or open a window).
// If there already was a valid mode (or open window), this call
// should replace it.  The previous contents (including the palette)
// is assumed to be lost.
//
// Returns true if successful, otherwise false.  The platform is free
// to select a working mode if the given mode was not possible, in
// which case the values of the global variables SCREENWIDTH,
// SCREENHEIGHT and SCREENBITS must be updated.
bool SetScreenSize(DisplayMode *mode);

void DeterminePixelAspect();

void GrabCursor(bool enable);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
