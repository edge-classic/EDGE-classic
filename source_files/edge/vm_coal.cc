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
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "m_random.h"
#include "main.h"
#include "n_network.h"
#include "r_modes.h"
#include "version.h"
#include "w_wad.h"

extern ConsoleVariable double_framerate;

// user interface VM
coal::vm_c *ui_vm = nullptr;

void CoalPrinter(const char *msg, ...)
{
    static char buffer[1024];

    va_list argptr;

    va_start(argptr, msg);
    vsnprintf(buffer, sizeof(buffer), msg, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    LogPrint("COAL: %s", buffer);
}

// CoalGetFloat/CoalGetString/CoalGetVector usage:
// mod_name = nullptr to search global scope or a module name such as "hud",
// "math", etc var_name = Variable name, without the module prefix, i.e.
// "custom_stbar" instead of "hud.custom_stbar"

double CoalGetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name)
{
    return vm->GetFloat(mod_name, var_name);
}

const char *CoalGetString(coal::vm_c *vm, const char *mod_name,
                          const char *var_name)
{
    return vm->GetString(mod_name, var_name);
}

double *CoalGetVector(coal::vm_c *vm, const char *mod_name,
                      const char *var_name)
{
    return vm->GetVector(mod_name, var_name);
}

double CoalGetVectorX(coal::vm_c *vm, const char *mod_name,
                      const char *var_name)
{
    return vm->GetVectorX(mod_name, var_name);
}

double CoalGetVectorY(coal::vm_c *vm, const char *mod_name,
                      const char *var_name)
{
    return vm->GetVectorY(mod_name, var_name);
}

double CoalGetVectorZ(coal::vm_c *vm, const char *mod_name,
                      const char *var_name)
{
    return vm->GetVectorZ(mod_name, var_name);
}

// CoalSetFloat/CoalSetString/CoalSetVector usage:
// mod_name = nullptr to search global scope or a module name such as "hud",
// "math", etc var_name = Variable name, without the module prefix, i.e.
// "custom_stbar" instead of "hud.custom_stbar" value = Whatever is appropriate.
// SetString should allow a nullptr string as this may be desired

void CoalSetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name,
                  double value)
{
    vm->SetFloat(mod_name, var_name, value);
}

void CoalSetString(coal::vm_c *vm, const char *mod_name, const char *var_name,
                   const char *value)
{
    vm->SetString(mod_name, var_name, value);
}

void CoalSetVector(coal::vm_c *vm, const char *mod_name, const char *var_name,
                   double val_1, double val_2, double val_3)
{
    vm->SetVector(mod_name, var_name, val_1, val_2, val_3);
}

void CoalSetVectorX(coal::vm_c *vm, const char *mod_name, const char *var_name,
                    double val)
{
    vm->SetVectorX(mod_name, var_name, val);
}

void CoalSetVectorY(coal::vm_c *vm, const char *mod_name, const char *var_name,
                    double val)
{
    vm->SetVectorY(mod_name, var_name, val);
}

void CoalSetVectorZ(coal::vm_c *vm, const char *mod_name, const char *var_name,
                    double val)
{
    vm->SetVectorZ(mod_name, var_name, val);
}

void CoalCallFunction(coal::vm_c *vm, const char *name)
{
    int func = vm->FindFunction(name);

    if (func == coal::vm_c::NOT_FOUND)
        FatalError("Missing coal function: %s\n", name);

    if (vm->Execute(func) != 0)
        FatalError("Coal script terminated with an error.\n");
}

//------------------------------------------------------------------------
//  SYSTEM MODULE
//------------------------------------------------------------------------

// sys.error(str)
//
static void SYS_error(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    FatalError("%s\n", s);
}

// sys.print(str)
//
static void SYS_print(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    LogPrint("%s\n", s);
}

// sys.debug_print(str)
//
static void SYS_debug_print(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    LogDebug("%s\n", s);
}

// sys.edge_version()
//
static void SYS_edge_version(coal::vm_c *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat(edgeversion.f_);
}

//------------------------------------------------------------------------
//  MATH MODULE
//------------------------------------------------------------------------

// math.rint(val)
static void MATH_rint(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(RoundToInteger(val));
}

// math.floor(val)
static void MATH_floor(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(floor(val));
}

// math.ceil(val)
static void MATH_ceil(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(ceil(val));
}

// math.random()
static void MATH_random(coal::vm_c *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat(RandomShort() / double(0x10000));
}

// Lobo November 2021: math.random2() always between 0 and 10
static void MATH_random2(coal::vm_c *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat(RandomShort() % 11);
}

// math.cos(val)
static void MATH_cos(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(cos(val * HMM_PI / 180.0));
}

// math.sin(val)
static void MATH_sin(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(sin(val * HMM_PI / 180.0));
}

// math.tan(val)
static void MATH_tan(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(tan(val * HMM_PI / 180.0));
}

// math.acos(val)
static void MATH_acos(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(acos(val) * 180.0 / HMM_PI);
}

// math.asin(val)
static void MATH_asin(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(asin(val) * 180.0 / HMM_PI);
}

// math.atan(val)
static void MATH_atan(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);
    vm->ReturnFloat(atan(val) * 180.0 / HMM_PI);
}

// math.atan2(x, y)
static void MATH_atan2(coal::vm_c *vm, int argc)
{
    (void)argc;

    double x = *vm->AccessParam(0);
    double y = *vm->AccessParam(1);

    vm->ReturnFloat(atan2(y, x) * 180.0 / HMM_PI);
}

// math.log(val)
static void MATH_log(coal::vm_c *vm, int argc)
{
    (void)argc;

    double val = *vm->AccessParam(0);

    if (val <= 0) FatalError("math.log: illegal input: %g\n", val);

    vm->ReturnFloat(log(val));
}

//------------------------------------------------------------------------
//  STRINGS MODULE
//------------------------------------------------------------------------

// strings.len(s)
//
static void STRINGS_len(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    vm->ReturnFloat(strlen(s));
}

// Lobo: December 2021
//  strings.find(s,TextToFind)
//  returns substring position or -1 if not found
//
static void STRINGS_find(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s1 = vm->AccessParamString(0);
    const char *s2 = vm->AccessParamString(1);

    std::string str(s1);
    std::string str2(s2);

    int found = str.find(str2);

    vm->ReturnFloat(found);
}

// strings.sub(s, start, end)
//
static void STRINGS_sub(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    int start = (int)*vm->AccessParam(1);
    int end   = (int)*vm->AccessParam(2);
    int len   = strlen(s);

    // negative values are relative to END of the string (-1 = last character)
    if (start < 0) start += len + 1;
    if (end < 0) end += len + 1;

    if (start < 1) start = 1;
    if (end > len) end = len;

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
static void STRINGS_tonumber(coal::vm_c *vm, int argc)
{
    (void)argc;

    const char *s = vm->AccessParamString(0);

    vm->ReturnFloat(atof(s));
}

void CoalRegisterBASE(coal::vm_c *vm)
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

struct PendingCoalScript
{
    int         type   = 0;
    std::string data   = "";
    std::string source = "";
};

static std::vector<PendingCoalScript> unread_scripts;

void CoalInitialize()
{
    StartupProgressMessage("Starting COAL VM...");

    ui_vm = coal::CreateVM();

    ui_vm->SetPrinter(CoalPrinter);

    CoalRegisterBASE(ui_vm);
    CoalRegisterHud();
    CoalRegisterPlaysim();
}

void CoalAddScript(int type, std::string &data, const std::string &source)
{
    unread_scripts.push_back(PendingCoalScript{type, "", source});

    // transfer the caller's data
    unread_scripts.back().data.swap(data);
}

void CoalLoadScripts()
{
    for (PendingCoalScript &info : unread_scripts)
    {
        const char *name = info.source.c_str();
        char       *data =
            (char *)info.data
                .c_str();  // FIXME make param to CompileFile be a std::string&

        LogPrint("Compiling: %s\n", name);

        if (!ui_vm->CompileFile(data, name))
            FatalError("Errors compiling %s\nPlease see debug.txt for details.",
                       name);
    }

    unread_scripts.clear();

    CoalSetFloat(ui_vm, "sys", "gametic",
                 game_tic / (double_framerate.d_ ? 2 : 1));

    if (IsLumpInPwad("STBAR"))
    {
        CoalSetFloat(ui_vm, "hud", "custom_stbar", 1);
    }
}

static bool coal_detected = false;
void        SetCoalDetected(bool detected)
{
    // check whether redundant call, once enabled stays enabled
    if (coal_detected) { return; }

    coal_detected = detected;
}

bool GetCoalDetected() { return coal_detected; }

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
