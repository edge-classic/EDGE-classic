//------------------------------------------------------------------------
//  COAL General Stuff
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2009  The EDGE Team.
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
//------------------------------------------------------------------------

#include "i_defs.h"

#include "coal.h"

#include "file.h"
#include "filesystem.h"
#include "path.h"

#include "main.h"

#include "vm_coal.h"
#include "dm_state.h"
#include "e_main.h"
#include "g_game.h"
#include "version.h"

#include "e_player.h"
#include "hu_font.h"
#include "hu_draw.h"
#include "r_modes.h"
#include "w_wad.h"
#include "z_zone.h"

#include "m_random.h"


// user interface VM
coal::vm_c *ui_vm;


void VM_Printer(const char *msg, ...)
{
	static char buffer[1024];

	va_list argptr;

	va_start(argptr, msg);
	vsnprintf(buffer, sizeof(buffer), msg, argptr);
	va_end(argptr);

	buffer[sizeof(buffer)-1] = 0;

	I_Printf("COAL: %s", buffer);
}

// VM_GetFloat/VM_GetString/VM_GetVector usage:
// mod_name = NULL to search global scope or a module name such as "hud", "math", etc
// var_name = Variable name, without the module prefix, i.e. "custom_stbar" instead of "hud.custom_stbar"

double VM_GetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name)
{
	return vm->GetFloat(mod_name, var_name);
}

const char *VM_GetString(coal::vm_c *vm, const char *mod_name, const char *var_name)
{
	return vm->GetString(mod_name, var_name);
}

double *VM_GetVector(coal::vm_c *vm, const char *mod_name, const char *var_name)
{
	return vm->GetVector(mod_name, var_name);
}

// VM_SetFloat/VM_SetString/VM_SetVector usage:
// mod_name = NULL to search global scope or a module name such as "hud", "math", etc
// var_name = Variable name, without the module prefix, i.e. "custom_stbar" instead of "hud.custom_stbar"
// value = Whatever is appropriate. SetString should allow a NULL string as this may be desired

void VM_SetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name, double value)
{
	vm->SetFloat(mod_name, var_name, value);
}

void VM_SetString(coal::vm_c *vm, const char *mod_name, const char *var_name, const char *value)
{
	vm->SetString(mod_name, var_name, value);
}

void VM_SetVector(coal::vm_c *vm, const char *mod_name, const char *var_name, double val_1, double val_2, double val_3)
{
	vm->SetVector(mod_name, var_name, val_1, val_2, val_3);
}

void VM_CallFunction(coal::vm_c *vm, const char *name)
{
	int func = vm->FindFunction(name);

	if (func == coal::vm_c::NOT_FOUND)
		I_Error("Missing coal function: %s\n", name);

	if (vm->Execute(func) != 0)
		I_Error("Coal script terminated with an error.\n");
}


//------------------------------------------------------------------------
//  SYSTEM MODULE
//------------------------------------------------------------------------


// sys.error(str)
//
static void SYS_error(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	I_Error("%s\n", s);
}

// sys.print(str)
//
static void SYS_print(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	I_Printf("%s\n", s);
}

// sys.debug_print(str)
//
static void SYS_debug_print(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	I_Debugf("%s\n", s);
}


// sys.edge_version()
//
static void SYS_edge_version(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(EDGEVER / 100.0);
}



//------------------------------------------------------------------------
//  MATH MODULE
//------------------------------------------------------------------------


// math.rint(val)
static void MATH_rint(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(I_ROUND(val));
}

// math.floor(val)
static void MATH_floor(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(floor(val));
}

// math.ceil(val)
static void MATH_ceil(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(ceil(val));
}


// math.random()
static void MATH_random(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(C_Random() / double(0x10000));
}


//Lobo November 2021: math.random2() always between 0 and 10
static void MATH_random2(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(C_Random() % 11);
}

// math.cos(val)
static void MATH_cos(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(cos(val * M_PI / 180.0));
}

// math.sin(val)
static void MATH_sin(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(sin(val * M_PI / 180.0));
}

// math.tan(val)
static void MATH_tan(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(tan(val * M_PI / 180.0));
}

// math.acos(val)
static void MATH_acos(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(acos(val) * 180.0 / M_PI);
}

// math.asin(val)
static void MATH_asin(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(asin(val) * 180.0 / M_PI);
}

// math.atan(val)
static void MATH_atan(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);
	vm->ReturnFloat(atan(val) * 180.0 / M_PI);
}

// math.atan2(x, y)
static void MATH_atan2(coal::vm_c *vm, int argc)
{
	double x = *vm->AccessParam(0);
	double y = *vm->AccessParam(1);

	vm->ReturnFloat(atan2(y, x) * 180.0 / M_PI);
}

// math.log(val)
static void MATH_log(coal::vm_c *vm, int argc)
{
	double val = *vm->AccessParam(0);

	if (val <= 0)
		I_Error("math.log: illegal input: %g\n", val);

	vm->ReturnFloat(log(val));
}



//------------------------------------------------------------------------
//  STRINGS MODULE
//------------------------------------------------------------------------


// strings.len(s)
//
static void STRINGS_len(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	vm->ReturnFloat(strlen(s));
}


//Lobo: December 2021
// strings.find(s,TextToFind)
// returns substring position or -1 if not found
//
static void STRINGS_find(coal::vm_c *vm, int argc)
{
	const char * s1 = vm->AccessParamString(0);
	const char * s2 = vm->AccessParamString(1);

	std::string str (s1);
  	std::string str2 (s2);

	int found = str.find(str2);

	vm->ReturnFloat(found);
}

// strings.sub(s, start, end)
//
static void STRINGS_sub(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	int start = (int) *vm->AccessParam(1);
	int end   = (int) *vm->AccessParam(2);
	int len   = strlen(s);

	// negative values are relative to END of the string (-1 = last character)
	if (start < 0) start += len + 1;
	if (end   < 0) end   += len + 1;

	if (start < 1) start = 1;
	if (end > len) end = len;

	if (end < start)
	{
		vm->ReturnString("");
		return;
	}

	SYS_ASSERT(end >= 1 && start <= len);

	// translate into C talk
	start--;
	end--;

	int new_len = (end - start + 1);

	vm->ReturnString(s + start, new_len);
}


// strings.tonumber(s)
//
static void STRINGS_tonumber(coal::vm_c *vm, int argc)
{
	const char * s = vm->AccessParamString(0);

	vm->ReturnFloat(atof(s));
}


//------------------------------------------------------------------------

void VM_RegisterBASE(coal::vm_c *vm)
{
	// SYSTEM
    vm->AddNativeFunction("sys.error",       SYS_error);
    vm->AddNativeFunction("sys.print",       SYS_print);
    vm->AddNativeFunction("sys.debug_print", SYS_debug_print);
    vm->AddNativeFunction("sys.edge_version", SYS_edge_version);

	// MATH
    vm->AddNativeFunction("math.rint",      MATH_rint);
    vm->AddNativeFunction("math.floor",     MATH_floor);
    vm->AddNativeFunction("math.ceil",      MATH_ceil);
    vm->AddNativeFunction("math.random",    MATH_random);
	//Lobo November 2021
	vm->AddNativeFunction("math.random2",    MATH_random2);

    vm->AddNativeFunction("math.cos",       MATH_cos);
    vm->AddNativeFunction("math.sin",       MATH_sin);
    vm->AddNativeFunction("math.tan",       MATH_tan);
    vm->AddNativeFunction("math.acos",      MATH_acos);
    vm->AddNativeFunction("math.asin",      MATH_asin);
    vm->AddNativeFunction("math.atan",      MATH_atan);
    vm->AddNativeFunction("math.atan2",     MATH_atan2);
    vm->AddNativeFunction("math.log",       MATH_log);

	// STRINGS
    vm->AddNativeFunction("strings.len",    STRINGS_len);
    vm->AddNativeFunction("strings.sub",    STRINGS_sub);
    vm->AddNativeFunction("strings.tonumber", STRINGS_tonumber);

    //Lobo December 2021
    vm->AddNativeFunction("strings.find",    STRINGS_find);
}


void VM_InitCoal()
{
	E_ProgressMessage("Starting COAL VM...");

	ui_vm = coal::CreateVM();

	ui_vm->SetPrinter(VM_Printer);

	VM_RegisterBASE(ui_vm);
	VM_RegisterHUD();
	VM_RegisterPlaysim();
}

void VM_QuitCoal()
{
	if (ui_vm)
	{
		delete ui_vm;
		ui_vm = NULL;
	}
}


void VM_LoadCoalFire(const char *filename)
{
	epi::file_c *F = epi::FS_Open(filename, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

	if (! F)
		I_Error("Could not open coal script: %s\n", filename);

	I_Printf("Compiling COAL script: %s\n", filename);

	byte *data = F->LoadIntoMemory();

	if (! ui_vm->CompileFile((char *)data, filename))
		I_Error("Errors compiling coal script: %s\nPlease see debug.txt for details.", filename);

	delete[] data;
	delete F;
}

void VM_LoadLumpOfCoal(int lump)
{
	const char *name = W_GetLumpName(lump);

	int length;
	byte *data = W_ReadLumpAlloc(lump, &length);

	I_Printf("Compiling %s lump\n", name);

	if (! ui_vm->CompileFile((char *)data, name))
		I_Error("Errors compiling %s lump.\nPlease see debug.txt for details.", name);

	delete[] data;
}


void VM_LoadScripts()
{
	W_ReadCoalLumps();
	VM_SetFloat(ui_vm, "sys", "gametic", gametic);
	if (W_IsLumpInPwad("STBAR"))
	{
		VM_SetFloat(ui_vm, "hud", "custom_stbar", 1);
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
