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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_system.h"

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


//
// System_Startup
//
void System_Startup(void)
{
	has_error_msg = false;

	progress.Reset();

	Debug_Startup();
}

//
// System_Shutdown
//
void System_Shutdown(void)
{
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

}  // Deh_Edge
