//----------------------------------------------------------------------------
//  EDGE Linux Misc System Code
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

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "timestamp.h"

#include "version.h"
#include "con_main.h"
#include "dm_state.h"
#include "e_main.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "r_modes.h"
#include "w_wad.h"
#include "z_zone.h"

#include "unx_sysinc.h"

// FIXME: Use file_c handles
extern FILE *logfile;
extern FILE *debugfile;

#ifdef USE_FLTK

// remove some problematic #defines
#undef VISIBLE
#undef INVISIBLE

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#endif // USE_FLTK

#ifndef USE_FLTK

static char cp437_to_ascii[160] =
{ 
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x00 - 0x07
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x08 - 0x0F
	'>', '<', '.', '.', '.', '.', '.', '.',   // 0x10 - 0x17
	'.', '.', '.', '<', '.', '.', 'A', 'V',   // 0x18 - 0x1F

	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x80 - 0x87
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x88 - 0x8F
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x90 - 0x97
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0x98 - 0x9F
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0xA0 - 0xA7
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0xA8 - 0xAF

	'.', '%', '.', '|', '+', '+', '+', '.',   // 0xB0 - 0xB7
	'.', '+', '|', '+', '+', '.', '.', '+',   // 0xB8 - 0xBF
	'+', '+', '+', '+', '-', '+', '+', '|',   // 0xC0 - 0xC7
	'+', '+', '+', '+', '+', '-', '+', '-',   // 0xC8 - 0xCF
	'+', '-', '.', '.', '.', '.', '.', '+',   // 0xD0 - 0xD7
	'+', '+', '+', '.', '.', '.', '.', '.',   // 0xD8 - 0xDF

	'.', '.', '.', '.', '.', '.', '.', '.',   // 0xE0 - 0xE7
	'.', '.', '.', '.', '.', '.', '.', '.',   // 0xE8 - 0xEF
	'.', '.', '>', '<', '.', '.', '.', '.',   // 0xF0 - 0xF7
	'.', '.', '.', '.', '.', '.', '.', '.'    // 0xF8 - 0xFF
};
#endif

// cleanup handling -- killough:

static void I_SignalHandler(int s)
{
	// CPhipps - report but don't crash on SIGPIPE
	if (s == SIGPIPE)
	{
		// -AJA- linux signals reset when raised.
		signal(SIGPIPE, I_SignalHandler);

		fprintf(stderr, "EDGE: Broken pipe\n");
		return;
	}

	signal(s, SIG_IGN);    // Ignore future instances of this signal.

	switch (s)
	{
		case SIGSEGV: I_Error("EDGE: Segmentation Violation"); break;
		case SIGINT:  I_Error("EDGE: Interrupted by User"); break;
		case SIGILL:  I_Error("EDGE: Illegal Instruction"); break;
		case SIGFPE:  I_Error("EDGE: Floating Point Exception"); break;
		case SIGABRT: I_Error("EDGE: Aborted"); break;
		case SIGTERM: I_Error("EDGE: Killed"); break;
	}

	I_Error("EDGE: Terminated by signal %d", s);
}


void I_SetupSignalHandlers(bool allow_coredump)
{
	signal(SIGPIPE, I_SignalHandler); // CPhipps - add SIGPIPE, as this is fatal

	if (allow_coredump)
	{
		// -AJA- Disable signal handlers, otherwise we don't get core dumps
		//       and core dumps are _DAMN_ useful for debugging.
		return;
	}

	signal(SIGSEGV, I_SignalHandler);
	signal(SIGTERM, I_SignalHandler);
	signal(SIGILL,  I_SignalHandler);
	signal(SIGFPE,  I_SignalHandler);
	signal(SIGILL,  I_SignalHandler);
	signal(SIGINT,  I_SignalHandler);  // killough 3/6/98: allow CTRL-BRK during init
	signal(SIGABRT, I_SignalHandler);
}

void I_CheckAlreadyRunning(void)
{
  /* nothing needed */
}


void I_WaitVBL (int count)
{
}

// Most of the following has been rewritten by Lee Killough
// and then by CPhipps
//
// I_GetTime
//

// CPhipps - believe it or not, it is possible with consecutive calls to 
// gettimeofday to receive times out of order, e.g you query the time twice and 
// the second time is earlier than the first. Cheap'n'cheerful fix here.
// NOTE: only occurs with bad kernel drivers loaded, e.g. pc speaker drv

unsigned long I_GetMicroSec (void)
{
	struct timeval tv;
	struct timezone tz;
	gettimeofday (&tv, &tz);
	return (tv.tv_sec * 1000000 + tv.tv_usec);
}


extern int autorun;  // Autorun state


bool microtimer_installed = 1;


static char errmsg[4096];  // buffer of error message -- killough

// killough 2/22/98: Add support for ENDBOOM, which is PC-specific

// this converts BIOS color codes to ANSI codes.  Its not pretty, but it
// does the job - rain
// CPhipps - made static

static inline int convert_colour(int colour, int *bold)
{
	*bold = 0;

	if (colour > 7)
	{
		colour &= 7;
		*bold = 1;
	}

	switch (colour)
	{
		case 1: return 4;
		case 3: return 6;
		case 4: return 1;
		case 6: return 3;
	}

	return colour;
}

// CPhipps - flags controlling ENDOOM behaviour
enum
{
	endoom_colours = 1,
	endoom_nonasciichars = 2,
	endoom_droplastline = 4
};
unsigned int endoom_mode;

//
// I_Warning
//
void I_Warning(const char *warning,...)
{
	va_list argptr;

	va_start (argptr, warning);
	vsprintf (errmsg, warning, argptr);
	va_end (argptr);

	I_Printf ("WARNING: %s", errmsg);
}

//
// I_Error
//
void I_Error(const char *error, ...)
{
	va_list argptr;

	va_start (argptr, error);
	vsprintf (errmsg, error, argptr);
	va_end (argptr);

	if (logfile)
	{
		fprintf(logfile, "ERROR: %s\n", errmsg);
		fflush(logfile);
	}

	if (debugfile)
	{
		fprintf(debugfile, "ERROR: %s\n", errmsg);
		fflush(debugfile);
	}

	// -AJA- Commit suicide, thereby producing a core dump which may
	//       come in handy for debugging the code that called I_Error().
	if (M_CheckParm("-core"))
	{
		fprintf(stderr, "%s\n", errmsg);

		I_GrabCursor(false);

		raise(11);
		/* NOTREACHED */
	}

	I_SystemShutdown();

	I_MessageBox(errmsg, "EDGE Error");

	I_CloseProgram(-1);
}

// -AJA- Routine which emulates IBM charset.
#ifndef USE_FLTK
static void PrintString(char *str)
{
	for (; *str; str++)
	{
		int ch = (unsigned char) *str;

		if (ch == 0x7F || ch == '\r')
			continue;

		if ((0x20 <= ch && ch <= 0x7E) ||
				ch == '\n' || ch == '\t')
		{
			putchar(ch);
			continue;
		}

		if (ch >= 0x80)
			ch -= 0x60;

		putchar(cp437_to_ascii[ch]);
	}

	fflush(stdout);
}
#endif

void I_Printf(const char *message,...)
{
	va_list argptr;
	char printbuf[2048];
	char *string = printbuf;

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
			memmove (string + 2, string + 1, strlen (string));
			string[1] = '\r';
			string++;
		}
		string++;
	}

	// Send the message to the console.
	CON_Printf("%s", printbuf);

	// And the text screen if in text mode
#ifndef USE_FLTK
	PrintString(printbuf);
#endif

	va_end(argptr);
}

void TextAttr (int attr)
{
	// Not supported in Linux without low-level termios manipulation
	// or ncurses, which I'd rather not link
	// textattr(attr);
}

void ClearScreen (void)
{  
	I_Printf("\n");
}

//
// I_DisplayExitScreen
//
void I_DisplayExitScreen(void)
{
	/* not implemented */
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
void I_TraceBack(void)
{
	I_CloseProgram(-1);
}

//
// I_SystemStartup
//
// -ACB- 1998/07/11 Reformatted the code.
//
void I_SystemStartup(void)
{
	Uint32 flags = 0;

	if (M_CheckParm("-core"))
		flags |= SDL_INIT_NOPARACHUTE;

	if (SDL_Init(flags) < 0)
		I_Error("Couldn't init SDL!!\n%s\n", SDL_GetError());

	I_StartupGraphics();
	I_StartupControl();
	I_StartupSound();    // -ACB- 1999/09/20 Sets nosound directly
	I_StartupMusic();
	I_StartupNetwork();
}

//
// I_SystemShutdown
//
// -ACB- 1998/07/11 Tidying the code
//
void I_SystemShutdown(void)
{
	// makre sure audio is unlocked (e.g. I_Error occurred)
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
	if (debugfile)
	{
		fclose(debugfile);
		debugfile = NULL;
	}
}

//
// I_PureRandom
//
// Returns as-random-as-possible 32 bit values.
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
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (u32_t)tv.tv_sec * 1000000 + (u32_t)tv.tv_usec;
}

//
// I_Sleep
//
void I_Sleep(int millisecs)
{
	//!!!! FIXME: use nanosleep ?
	usleep(millisecs * 1000);
}

void I_MessageBox(const char *message, const char *title)
{
#ifdef USE_FLTK
	Fl::scheme(NULL);
	fl_message_font(FL_HELVETICA /*_BOLD*/, 18);	
	fl_message("%s", message);

#else // USE_FLTK
	fprintf(stderr, "\n%s\n", message);
#endif // USE_FLTK
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
