//----------------------------------------------------------------------
//  COAL PUBLIC API
//----------------------------------------------------------------------
//
//  Copyright (C) 2021-2024 The EDGE Team
//  Copyright (C) 2009-2021  Andrew Apted
//  Copyright (C) 1996-1997  Id Software, Inc.
//
//  COAL is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  COAL is distributed in the hope that it will be useful, but
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

#pragma once

namespace coal
{

class VM;

typedef void (*PrintFunction)(const char *msg, ...);

typedef void (*NativeFunction)(VM *vm, int argc);

class VM
{
    /* this is an abstract base class */

  public:
    VM()
    {
    }
    virtual ~VM()
    {
    }

    virtual void SetPrinter(PrintFunction func) = 0;

    virtual void AddNativeFunction(const char *name, NativeFunction func) = 0;

    virtual bool CompileFile(char *buffer, const char *filename) = 0;
    virtual void ShowStats()                                     = 0;

    virtual void SetAsmDump(bool enable) = 0;
    virtual void SetTrace(bool enable)   = 0;

    enum
    {
        NOT_FOUND = 0
    };

    virtual double      GetFloat(const char *mod_name, const char *var_name)   = 0;
    virtual const char *GetString(const char *mod_name, const char *var_name)  = 0;
    virtual double     *GetVector(const char *mod_name, const char *var_name)  = 0;
    virtual double      GetVectorX(const char *mod_name, const char *var_name) = 0;
    virtual double      GetVectorY(const char *mod_name, const char *var_name) = 0;
    virtual double      GetVectorZ(const char *mod_name, const char *var_name) = 0;

    virtual void SetFloat(const char *mod_name, const char *var_name, double value)                              = 0;
    virtual void SetString(const char *mod_name, const char *var_name, const char *value)                        = 0;
    virtual void SetVector(const char *mod_name, const char *var_name, double val_1, double val_2, double val_3) = 0;
    virtual void SetVectorX(const char *mod_name, const char *var_name, double val)                              = 0;
    virtual void SetVectorY(const char *mod_name, const char *var_name, double val)                              = 0;
    virtual void SetVectorZ(const char *mod_name, const char *var_name, double val)                              = 0;

    virtual int FindFunction(const char *name) = 0;
    virtual int FindVariable(const char *name) = 0;

    virtual int Execute(int func_id) = 0;

    virtual double     *AccessParam(int p)       = 0;
    virtual const char *AccessParamString(int p) = 0;

    virtual void ReturnFloat(double f)                     = 0;
    virtual void ReturnVector(double *v)                   = 0;
    virtual void ReturnString(const char *s, int len = -1) = 0;
};

// create a new COAL virtual machine
VM  *CreateVM();
void DeleteVM(VM *vm);

} // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
