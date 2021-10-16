//------------------------------------------------------------------------
//  SYSTEM : System specific code
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "i_defs.h"

#include "dh_plugin.h"
#include "system.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace Deh_Edge
{

#define FATAL_COREDUMP  0

#define DEBUG_ENABLED   0
#define DEBUG_ENDIAN    0
#define DEBUG_PROGRESS  0

#define DEBUGGING_FILE  "deh_debug.log"


int cpu_big_endian = 0;
bool disable_progress = false;

char message_buf[1024];

char global_error_buf[1024];
bool has_error_msg = false;

#if (DEBUG_ENABLED)
FILE *debug_fp = NULL;
#endif


typedef struct
{
public:
	// current major range
	int low_perc;
	int high_perc;

	// current percentage
	int cur_perc;

	void Reset(void)
	{
		low_perc = high_perc = 0;
		cur_perc = 0;
	}

	void Major(int low, int high)
	{
		low_perc = cur_perc = low;
		high_perc = high;
	}

	bool Minor(int count, int limit)
	{
		int new_perc = low_perc + (high_perc - low_perc) * count / limit;

		if (new_perc > cur_perc)
		{
			cur_perc = new_perc;
			return true;
		}

		return false;
	}
}
progress_info_t;

progress_info_t progress;


// forward decls
void Debug_Startup(void);
void Debug_Shutdown(void);
void Endian_Startup(void);


//
// System_Startup
//
void System_Startup(void)
{
	has_error_msg = false;

	if (! cur_funcs)
	{
		setbuf(stdout, NULL);

#if defined(LINUX) || defined(UNIX)
		// no whirling baton if stderr is redirected
		if (! isatty(2))
			disable_progress = true;
#endif
	}
	
	progress.Reset();

	Debug_Startup();
	Endian_Startup();
}

//
// System_Shutdown
//
void System_Shutdown(void)
{
//	if (! cur_funcs) ClearProgress();

	Debug_Shutdown();
}


/* -------- text output code ----------------------------- */

//
// PrintMsg
//
void PrintMsg(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(message_buf, str, args);
	va_end(args);

	if (cur_funcs)
		cur_funcs->print_msg("%s", message_buf);
	else
	{
		printf("%s", message_buf);
		fflush(stdout);
	}

#if (DEBUG_ENABLED)
	Debug_PrintMsg("> %s", message_buf);
#endif
}

//
// PrintWarn
//
void PrintWarn(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(message_buf, str, args);
	va_end(args);

	if (! quiet_mode)
	{
		if (cur_funcs)
			cur_funcs->print_msg("- Warning: %s", message_buf);
		else
		{
			printf("- Warning: %s", message_buf);
			fflush(stdout);
		}
	}

#if (DEBUG_ENABLED)
	Debug_PrintMsg("> Warning: %s", message_buf);
#endif
}

//
// FatalError
//
void FatalError(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(message_buf, str, args);
	va_end(args);

	if (cur_funcs)
	{
		cur_funcs->fatal_error("Error: %s\n", message_buf);
		/* NOT REACHED */
	}
	else
	{
		printf("\nError: %s\n", message_buf);
		fflush(stdout);
	}

	System_Shutdown();

#if (FATAL_COREDUMP && defined(UNIX))
	raise(SIGSEGV);
#endif

	exit(5);
}

//
// InternalError
//
void InternalError(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(message_buf, str, args);
	va_end(args);

	if (cur_funcs)
	{
		cur_funcs->fatal_error("INTERNAL ERROR: %s\n", message_buf);
		/* NOT REACHED */
	}
	else
	{
		printf("\nINTERNAL ERROR: %s\n", message_buf);
		fflush(stdout);
	}

	System_Shutdown();

#if (FATAL_COREDUMP && defined(UNIX))
	raise(SIGSEGV);
#endif

	exit(5);
}

void SetErrorMsg(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(global_error_buf, str, args);
	va_end(args);

	has_error_msg = true;
}

const char *GetErrorMsg(void)
{
	if (! has_error_msg)
		return "";
	
	has_error_msg = false;

	return global_error_buf;
}


/* --------- progress handling -------------------------------- */

//
// ProgressMajor
//
// Called for major elements, i.e. each patch file to process and
// also the final save.
//
void ProgressMajor(int low_perc, int high_perc)
{
	progress.Major(low_perc, high_perc);

	if (cur_funcs)
		cur_funcs->progress_bar(progress.cur_perc);
#if (DEBUG_PROGRESS)
	else
		fprintf(stderr, "PROGRESS %d%% (to %d%%)\n", progress.low_perc,
			progress.high_perc);
#endif
}

//
// ProgressMinor
//
// Called for the progress of a single element (patch file loading,
// hwa file saving).  The count value should range from 0 to limit-1.
// 
void ProgressMinor(int count, int limit)
{
	if (progress.Minor(count, limit))
	{
		if (cur_funcs)
			cur_funcs->progress_bar(progress.cur_perc);
#if (DEBUG_PROGRESS)
		else
			fprintf(stderr, " Progress Minor %d%%\n", progress.cur_perc);
#endif
	}
}

//
// ProgressText
//
void ProgressText(const char *str)
{
	if (cur_funcs)
		cur_funcs->progress_text(str);
#if (DEBUG_PROGRESS)
	else
		fprintf(stderr, "------ %s ------\n", str);
#endif
}

#if 0
//
// ClearProgress
//
void ClearProgress(void)
{
	fprintf(stderr, "                \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

	progress_shown = -1;
}

//
// ShowProgress
//
void ShowProgress(int count, int limit)
{
	if (disable_progress)
		return;

	int perc = count * 100 / limit;

	if (perc == progress_shown)
		return;

	fprintf(stderr, "--%3d%%--\b\b\b\b\b\b\b\b", perc);

	progress_shown = perc;
}
#endif


/* -------- debugging code ----------------------------- */

void Debug_Startup(void)
{
#if (DEBUG_ENABLED)
	debug_fp = fopen(DEBUGGING_FILE, "w");

	if (! debug_fp)
		PrintMsg("Unable to open DEBUG FILE: %s\n", DEBUGGING_FILE);

	Debug_PrintMsg("=== START OF DEBUG FILE ===\n");
#endif
}

void Debug_Shutdown(void)
{
#if (DEBUG_ENABLED)
	if (debug_fp)
	{
		Debug_PrintMsg("=== END OF DEBUG FILE ===\n");

		fclose(debug_fp);
		debug_fp = NULL;
	}
#endif
}

//
// Debug_PrintMsg
//
void Debug_PrintMsg(const char *str, ...)
{
#if (DEBUG_ENABLED)
  if (debug_fp)
  {
    va_list args;

    va_start(args, str);
    vfprintf(debug_fp, str, args);
    va_end(args);

    fflush(debug_fp);
  }
#else
  (void) str;
#endif
}


/* -------- endian code ----------------------------- */

//
// Endian_Startup
//
// Parts inspired by the Yadex endian.cc code.
//
void Endian_Startup(void)
{
	volatile union
	{
		unsigned char mem[32];
		unsigned int val;
	}
	u_test;

	/* sanity-check type sizes */

	int size_8  = sizeof(unsigned char);
	int size_16 = sizeof(unsigned short);
	int size_32 = sizeof(unsigned int);

	if (size_8 != 1)
		FatalError("Sanity check failed: sizeof(uint8) = %d\n", size_8);

	if (size_16 != 2)
		FatalError("Sanity check failed: sizeof(uint16) = %d\n", size_16);

	if (size_32 != 4)
		FatalError("Sanity check failed: sizeof(uint32) = %d\n", size_32);

	/* check endianness */

	memset((unsigned int *) u_test.mem, 0, sizeof(u_test.mem));

	u_test.mem[0] = 0x70;  u_test.mem[1] = 0x71;
	u_test.mem[2] = 0x72;  u_test.mem[3] = 0x73;

#if (DEBUG_ENDIAN)
	Debug_PrintMsg("Endianness magic value: 0x%08x\n", u_test.val);
#endif

	if (u_test.val == 0x70717273)
		cpu_big_endian = 1;
	else if (u_test.val == 0x73727170)
		cpu_big_endian = 0;
	else
		FatalError("Sanity check failed: weird endianness (0x%08x)\n", u_test.val);

#if (DEBUG_ENDIAN)
	Debug_PrintMsg("Endianness = %s\n", cpu_big_endian ? "BIG" : "LITTLE");

	Debug_PrintMsg("Endianness check: 0x1234 --> 0x%04x\n", 
			(int) Endian_U16(0x1234));

	Debug_PrintMsg("Endianness check: 0x11223344 --> 0x%08x\n", 
			Endian_U32(0x11223344));
#endif
}

//
// Endian_U16
//
unsigned short Endian_U16(unsigned short x)
{
	if (cpu_big_endian)
		return (x >> 8) | (x << 8);
	else
		return x;
}

//
// Endian_U32
//
unsigned int Endian_U32(unsigned int x)
{
	if (cpu_big_endian)
		return (x >> 24) | ((x >> 8) & 0xff00) |
			((x << 8) & 0xff0000) | (x << 24);
	else
		return x;
}

}  // Deh_Edge
