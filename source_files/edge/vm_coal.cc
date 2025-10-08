//------------------------------------------------------------------------
//  COAL General Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 2006-2024 The EDGE Team.
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

#include "vm_coal.h"

#include <stdarg.h>

#include "coal.h"
#include "ddf_main.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "m_random.h"
#include "n_network.h"
#include "r_modes.h"
#include "stb_sprintf.h"
#include "version.h"
#include "w_wad.h"

// user interface VM
coal::VM *ui_vm = nullptr;

void COALPrinter(const char *msg, ...)
{
    static char buffer[1024];

    va_list argptr;

    va_start(argptr, msg);
    stbsp_vsnprintf(buffer, sizeof(buffer), msg, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    LogPrint("COAL: %s", buffer);
}

// COALGetFloat/COALGetString/COALGetVector usage:
// mod_name = nullptr to search global scope or a module name such as "hud",
// "math", etc var_name = Variable name, without the module prefix, i.e.
// "custom_stbar" instead of "hud.custom_stbar"

double COALGetFloat(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetFloat(mod_name, var_name);
}

const char *COALGetString(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetString(mod_name, var_name);
}

double *COALGetVector(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetVector(mod_name, var_name);
}

double COALGetVectorX(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetVectorX(mod_name, var_name);
}

double COALGetVectorY(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetVectorY(mod_name, var_name);
}

double COALGetVectorZ(coal::VM *vm, const char *mod_name, const char *var_name)
{
    return vm->GetVectorZ(mod_name, var_name);
}

// COALSetFloat/COALSetString/COALSetVector usage:
// mod_name = nullptr to search global scope or a module name such as "hud",
// "math", etc var_name = Variable name, without the module prefix, i.e.
// "custom_stbar" instead of "hud.custom_stbar" value = Whatever is appropriate.
// SetString should allow a nullptr string as this may be desired

void COALSetFloat(coal::VM *vm, const char *mod_name, const char *var_name, double value)
{
    vm->SetFloat(mod_name, var_name, value);
}

void COALSetString(coal::VM *vm, const char *mod_name, const char *var_name, const char *value)
{
    vm->SetString(mod_name, var_name, value);
}

void COALSetVector(coal::VM *vm, const char *mod_name, const char *var_name, double val_1, double val_2, double val_3)
{
    vm->SetVector(mod_name, var_name, val_1, val_2, val_3);
}

void COALSetVectorX(coal::VM *vm, const char *mod_name, const char *var_name, double val)
{
    vm->SetVectorX(mod_name, var_name, val);
}

void COALSetVectorY(coal::VM *vm, const char *mod_name, const char *var_name, double val)
{
    vm->SetVectorY(mod_name, var_name, val);
}

void COALSetVectorZ(coal::VM *vm, const char *mod_name, const char *var_name, double val)
{
    vm->SetVectorZ(mod_name, var_name, val);
}

void COALCallFunction(coal::VM *vm, const char *name)
{
    int func = vm->FindFunction(name);

    if (func == coal::VM::NOT_FOUND)
        FatalError("Missing coal function: %s\n", name);

    if (vm->Execute(func) != 0)
        FatalError("COAL script terminated with an error.\n");
}

//------------------------------------------------------------------------
//  SYSTEM MODULE
//------------------------------------------------------------------------

// sys.error(str)
//
static void SYS_error(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    FatalError("%s\n", s);
}

// sys.print(str)
//
static void SYS_print(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    LogPrint("%s\n", s);
}

// sys.debug_print(str)
//
static void SYS_debug_print(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    LogDebug("%s\n", s);
}

// sys.edge_version()
//
static void SYS_edge_version(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    vm->ReturnFloat(edge_version.f_);
}

//------------------------------------------------------------------------
//  MATH MODULE
//------------------------------------------------------------------------

// math.rint(val)
static void MATH_rint(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(RoundToInteger(val));
}

// math.floor(val)
static void MATH_floor(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(floor(val));
}

// math.ceil(val)
static void MATH_ceil(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(ceil(val));
}

// math.random()
static void MATH_random(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    vm->ReturnFloat(RandomShort() / double(0x10000));
}

// Lobo November 2021: math.random2() always between 0 and 10
static void MATH_random2(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    vm->ReturnFloat(RandomShort() % 11);
}

// math.cos(val)
static void MATH_cos(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(cos(val * HMM_PI / 180.0));
}

// math.sin(val)
static void MATH_sin(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(sin(val * HMM_PI / 180.0));
}

// math.tan(val)
static void MATH_tan(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(tan(val * HMM_PI / 180.0));
}

// math.acos(val)
static void MATH_acos(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(acos(val) * 180.0 / HMM_PI);
}

// math.asin(val)
static void MATH_asin(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(asin(val) * 180.0 / HMM_PI);
}

// math.atan(val)
static void MATH_atan(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(atan(val) * 180.0 / HMM_PI);
}

// math.atan2(x, y)
static void MATH_atan2(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double x = *vm->AccessParam(0);
    double y = *vm->AccessParam(1);

    vm->ReturnFloat(atan2(y, x) * 180.0 / HMM_PI);
}

// math.log(val)
static void MATH_log(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    double val = *vm->AccessParam(0);

    if (val <= 0)
        FatalError("math.log: illegal input: %g\n", val);

    vm->ReturnFloat(log(val));
}

//------------------------------------------------------------------------
//  STRINGS MODULE
//------------------------------------------------------------------------

// strings.len(s)
//
static void STRINGS_len(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    vm->ReturnFloat(strlen(s));
}

// Lobo: December 2021
//  strings.find(s,TextToFind)
//  returns substring position or -1 if not found
//
static void STRINGS_find(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s1 = vm->AccessParamString(0);
    const char *s2 = vm->AccessParamString(1);

    std::string str(s1);
    std::string str2(s2);

    int found = str.find(str2);

    vm->ReturnFloat(found);
}

// strings.sub(s, start, end)
//
static void STRINGS_sub(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    int start = (int)*vm->AccessParam(1);
    int end   = (int)*vm->AccessParam(2);
    int len   = strlen(s);

    // negative values are relative to END of the string (-1 = last character)
    if (start < 0)
        start += len + 1;
    if (end < 0)
        end += len + 1;

    if (start < 1)
        start = 1;
    if (end > len)
        end = len;

    if (end < start)
    {
        vm->ReturnString("");
        return;
    }

    EPI_ASSERT(end >= 1 && start <= len);

    // translate into C talk
    start--;
    end--;

    int new_len = (end - start + 1);

    vm->ReturnString(s + start, new_len);
}

// strings.tonumber(s)
//
static void STRINGS_tonumber(coal::VM *vm, int argc)
{
    EPI_UNUSED(argc);

    const char *s = vm->AccessParamString(0);

    vm->ReturnFloat(atof(s));
}

void COALRegisterBASE(coal::VM *vm)
{
    // SYSTEM
    vm->AddNativeFunction("sys.error", SYS_error);
    vm->AddNativeFunction("sys.print", SYS_print);
    vm->AddNativeFunction("sys.debug_print", SYS_debug_print);
    vm->AddNativeFunction("sys.edge_version", SYS_edge_version);

    // MATH
    vm->AddNativeFunction("math.rint", MATH_rint);
    vm->AddNativeFunction("math.floor", MATH_floor);
    vm->AddNativeFunction("math.ceil", MATH_ceil);
    vm->AddNativeFunction("math.random", MATH_random);
    // Lobo November 2021
    vm->AddNativeFunction("math.random2", MATH_random2);

    vm->AddNativeFunction("math.cos", MATH_cos);
    vm->AddNativeFunction("math.sin", MATH_sin);
    vm->AddNativeFunction("math.tan", MATH_tan);
    vm->AddNativeFunction("math.acos", MATH_acos);
    vm->AddNativeFunction("math.asin", MATH_asin);
    vm->AddNativeFunction("math.atan", MATH_atan);
    vm->AddNativeFunction("math.atan2", MATH_atan2);
    vm->AddNativeFunction("math.log", MATH_log);

    // STRINGS
    vm->AddNativeFunction("strings.len", STRINGS_len);
    vm->AddNativeFunction("strings.sub", STRINGS_sub);
    vm->AddNativeFunction("strings.tonumber", STRINGS_tonumber);

    // Lobo December 2021
    vm->AddNativeFunction("strings.find", STRINGS_find);
}

//------------------------------------------------------------------------

struct PendingCOALScript
{
    int         type   = 0;
    std::string data   = "";
    std::string source = "";
};

static std::vector<PendingCOALScript> unread_scripts;

void InitializeCOAL()
{
    StartupProgressMessage("Starting COAL VM...");

    ui_vm = coal::CreateVM();

    ui_vm->SetPrinter(COALPrinter);

    COALRegisterBASE(ui_vm);
    COALRegisterHUD();
    COALRegisterPlaysim();
}

void ShutdownCOAL()
{
    if (ui_vm)
        coal::DeleteVM(ui_vm);
}

void COALAddScript(int type, std::string &data, const std::string &source)
{
    unread_scripts.push_back(PendingCOALScript{type, "", source});

    // transfer the caller's data
    unread_scripts.back().data.swap(data);
}

void COALLoadScripts()
{
    for (PendingCOALScript &info : unread_scripts)
    {
        const char *name = info.source.c_str();
        char       *data = (char *)info.data.c_str(); // FIXME make param to CompileFile be a std::string&

        LogPrint("Compiling: %s\n", name);

        if (!ui_vm->CompileFile(data, name))
            FatalError("Errors compiling %s\nPlease see debug.txt for details.", name);
    }

    unread_scripts.clear();

    COALSetFloat(ui_vm, "sys", "gametic", game_tic);

    if (IsLumpInPwad("STBAR"))
    {
        COALSetFloat(ui_vm, "hud", "custom_stbar", 1);
    }
}

static bool coal_detected = false;
void        SetCOALDetected(bool detected)
{
    coal_detected = detected;
}

bool GetCOALDetected()
{
    return coal_detected;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
