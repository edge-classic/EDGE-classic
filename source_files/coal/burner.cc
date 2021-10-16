//----------------------------------------------------------------------
//  BURNER : stand-alone Coal interpreter
//----------------------------------------------------------------------
// 
//  Copyright (C)      2009  Andrew Apted
//  Copyright (C) 1996-1997  Id Software, Inc.
//
//  Coal is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  Coal is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU General Public License for more details.
//
//----------------------------------------------------------------------
//
//  Based on QCC (the Quake-C Compiler) and the corresponding
//  execution engine from the Quake source code.
//
//----------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include <sys/signal.h>


#include "coal.h"


coal::vm_c * coalvm;


void Error(const char *msg, ...)
{
	va_list argptr;

	printf("************ ERROR ************\n");

	va_start(argptr, msg);
	vprintf(msg, argptr);
	va_end(argptr);
	
	printf("\n");

	exit(1);
}


void BurnPrint(const char *msg, ...)
{
	va_list argptr;

	va_start(argptr, msg);
	vprintf(msg, argptr);
	va_end(argptr);
}


//================================================================//


static int filelength(FILE *f)
{
	int pos = ftell(f);

	fseek(f, 0, SEEK_END);

	int end = ftell(f);

	fseek(f, pos, SEEK_SET);

	return end;
}


int LoadFile(char *filename, char **bufptr)
{
	FILE *f = fopen(filename, "rb");

	if (!f)
		Error("Error opening %s: %s", filename, strerror(errno));

	int length = filelength(f);
	*bufptr = (char *) malloc(length+1);

	if ( fread(*bufptr, 1, length, f) != (size_t)length)
		Error("File read failure");

	fclose(f);

	(*bufptr)[length] = 0;

	return length;
}

void FreeFile(char **bufptr)
{
	free((void *) *bufptr);
	*bufptr = NULL;
}


//==================================================================//


void PF_PrintStr(coal::vm_c * vm, int argc)
{
	const char * p = vm->AccessParamString(0);

    printf("%s\n", p);
}

void PF_PrintNum(coal::vm_c * vm, int argc)
{
	double * p = vm->AccessParam(0);

    printf("%1.5f\n", *p);
}

void PF_PrintVector(coal::vm_c * vm, int argc)
{
	double * vec = vm->AccessParam(0);

	printf("'%1.3f %1.3f %1.3f'\n", vec[0], vec[1], vec[2]);
}


//==================================================================//


int main(int argc, char **argv)
{
	char	*src2;
	char	filename[1024];

	int   k;

	if (argc <= 1 ||
	    (strcmp(argv[1], "-?") == 0) || (strcmp(argv[1], "-h") == 0) ||
		(strcmp(argv[1], "-help") == 0) || (strcmp(argv[1], "--help") == 0))
	{
		printf("USAGE: coal [OPTIONS] filename.ec ...\n");
		return 0;
	}

	coalvm = coal::CreateVM();

	assert(coalvm);

	coalvm->SetPrinter(BurnPrint);

	coalvm->AddNativeFunction("sys.print", PF_PrintStr);

	if (strcmp(argv[1], "-a") == 0)
	{
		coalvm->SetAsmDump(true);
		argv++; argc--;
	}

	if (strcmp(argv[1], "-t") == 0)
	{
		coalvm->SetTrace(true);
		argv++; argc--;
	}


	// compile all the files
	for (k = 1; k < argc; k++)
	{
		if (argv[k][0] == '-')
			Error("Bad filename: %s\n", argv[k]);

		sprintf(filename, "%s", argv[k]);

		printf("Compiling %s\n", filename);

		LoadFile(filename, &src2);

		if (! coalvm->CompileFile(src2, filename))
		{
			printf("Failed with errors.\n");
			exit(1);
		}

		FreeFile(&src2);
	}

	coalvm->ShowStats();


	// find 'main' function

	int main_func = coalvm->FindFunction("main");

	if (! main_func)
		Error("No main function!\n");

	if (coalvm->Execute(main_func) != 0)
	{
		fprintf(stderr, "\n*** script terminated by error\n");
		return 1;
	}

	return 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
