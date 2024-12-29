//----------------------------------------------------------------------
//  COAL EXECUTION ENGINE
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

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "AlmostEquals.h"
#include "c_local.h"
#include "epi.h"
#include "stb_sprintf.h"

extern void FatalError(const char *error, ...);

namespace coal
{

static constexpr uint32_t kMaximumRunaway = 1000000;

int RealVM::GetNativeFunc(const char *name, const char *module)
{
    char buffer[256];

    if (module)
        stbsp_sprintf(buffer, "%s.%s", module, name);
    else
        strcpy(buffer, name);

    for (int i = 0; i < (int)native_funcs_.size(); i++)
        if (strcmp(native_funcs_[i]->name, buffer) == 0)
            return i;

    return -1; // NOT FOUND
}

void RealVM::AddNativeFunction(const char *name, NativeFunction func)
{
    // already registered?
    int prev = GetNativeFunc(name, nullptr);

    if (prev >= 0)
    {
        native_funcs_[prev]->func = func;
        return;
    }

    RegisteredNativeFunction *reg = new RegisteredNativeFunction;

    reg->name = strdup(name);
    reg->func = func;

    native_funcs_.push_back(reg);
}

void RealVM::SetTrace(bool enable)
{
    exec_.tracing = enable;
}

int RealVM::FindFunction(const char *func_name)
{
    for (int i = (int)functions_.size() - 1; i >= 1; i--)
    {
        Function *f = functions_[i];

        if (strcmp(f->name, func_name) == 0)
            return i;
    }

    return VM::NOT_FOUND;
}

int RealVM::FindVariable(const char *var_name)
{
    // FIXME
    EPI_UNUSED(var_name);
    return VM::NOT_FOUND;
}

// returns an offset from the string heap
int RealVM::InternaliseString(const char *new_s)
{
    if (new_s[0] == 0)
        return 0;

    int ofs = string_mem_.Alloc(strlen(new_s) + 1);
    strcpy((char *)string_mem_.Deref(ofs), new_s);

    return ofs;
}

double *RealVM::AccessParam(int p)
{
    EPI_ASSERT(exec_.func);

    if (p >= functions_[exec_.func]->parm_num)
        RunError("PR_Parameter: p=%d out of range\n", p);

    if (AlmostEquals(exec_.stack[exec_.stack_depth + functions_[exec_.func]->parm_ofs[p]], (double)(-FLT_MAX)))
        return nullptr;
    else
        return &exec_.stack[exec_.stack_depth + functions_[exec_.func]->parm_ofs[p]];
}

const char *RealVM::AccessParamString(int p)
{
    double *d = AccessParam(p);

    if (d)
        return COAL_REF_STRING((int)*d);
    else
        return nullptr;
}

void RealVM::ReturnFloat(double f)
{
    COAL_G_FLOAT(kReturnOffset * 8) = f;
}

void RealVM::ReturnVector(double *v)
{
    double *c = COAL_G_VECTOR(kReturnOffset * 8);

    c[0] = v[0];
    c[1] = v[1];
    c[2] = v[2];
}

void RealVM::ReturnString(const char *s, int len)
{
    // TODO: turn this code into a utility function

    if (len < 0)
        len = strlen(s);

    if (len == 0)
    {
        COAL_G_FLOAT(kReturnOffset * 8) = 0;
    }
    else
    {
        int index = temp_strings_.Alloc(len + 1);

        char *s3 = (char *)temp_strings_.Deref(index);

        memcpy(s3, s, (size_t)len);
        s3[len] = 0;

        COAL_G_FLOAT(kReturnOffset * 8) = -(1 + index);
    }
}

//
// Aborts the currently executing functions
//
void RealVM::RunError(const char *error, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, error);
    stbsp_vsnprintf(buffer, sizeof(buffer), error, argptr);
    va_end(argptr);

    Printer("COAL ERROR: %s\n", buffer);

    if (exec_.call_depth > 0)
        StackTrace();

    /* clear the stack so SV/Host_Error can shutdown functions */
    exec_.call_depth = 0;

    FatalError("%s", buffer);
}

int RealVM::StringConcat(const char *s1, const char *s2)
{
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    if (len1 == 0 && len2 == 0)
        return 0;

    int   index = temp_strings_.Alloc(len1 + len2 + 1);
    char *s3    = (char *)temp_strings_.Deref(index);

    strcpy(s3, s1);
    strcpy(s3 + len1, s2);

    return -(1 + index);
}

int RealVM::StringConcatFloat(const char *s, double f)
{
    char buffer[100];

    if (AlmostEquals(f, round(f)))
    {
        stbsp_sprintf(buffer, "%1.0f", f);
    }
    else
    {
        stbsp_sprintf(buffer, "%8.6f", f);
    }

    return StringConcat(s, buffer);
}

int RealVM::StringConcatVector(const char *s, double *v)
{
    char buffer[200];

    if (AlmostEquals(v[0], round(v[0])) && AlmostEquals(v[1], round(v[1])) && AlmostEquals(v[2], round(v[2])))
    {
        stbsp_sprintf(buffer, "'%1.0f %1.0f %1.0f'", v[0], v[1], v[2]);
    }
    else
    {
        stbsp_sprintf(buffer, "'%6.4f %6.4f %6.4f'", v[0], v[1], v[2]);
    }

    return StringConcat(s, buffer);
}

//================================================================
//  EXECUTION ENGINE
//================================================================

void RealVM::EnterFunction(int func)
{
    EPI_ASSERT(func > 0);

    Function *new_f = functions_[func];

    // NOTE: the saved 's' value points to the instruction _after_ OP_CALL

    exec_.call_stack[exec_.call_depth].s    = exec_.s;
    exec_.call_stack[exec_.call_depth].func = exec_.func;

    exec_.call_depth++;
    if (exec_.call_depth >= kMaximumCallStack)
        RunError("stack overflow");

    if (exec_.func)
        exec_.stack_depth += functions_[exec_.func]->locals_end;

    if (exec_.stack_depth + new_f->locals_end >= kMaximumLocalStack)
        RunError("PR_ExecuteProgram: locals stack overflow\n");

    exec_.s    = new_f->first_statement;
    exec_.func = func;
}

void RealVM::LeaveFunction()
{
    if (exec_.call_depth <= 0)
        RunError("stack underflow");

    exec_.call_depth--;

    exec_.s    = exec_.call_stack[exec_.call_depth].s;
    exec_.func = exec_.call_stack[exec_.call_depth].func;

    if (exec_.func)
        exec_.stack_depth -= functions_[exec_.func]->locals_end;
}

void RealVM::EnterNative(int func, int argc)
{
    Function *newf = functions_[func];

    int n = -(newf->first_statement + 1);
    EPI_ASSERT(n < (int)native_funcs_.size());

    exec_.stack_depth += functions_[exec_.func]->locals_end;
    {
        int old_func = exec_.func;
        {
            exec_.func = func;
            native_funcs_[n]->func(this, argc);
        }
        exec_.func = old_func;
    }
    exec_.stack_depth -= functions_[exec_.func]->locals_end;
}

#define COAL_OPERAND(a)                                                                                                \
    (((a) > 0) ? COAL_REF_GLOBAL(a) : ((a) < 0) ? &exec_.stack[exec_.stack_depth - ((a) + 1)] : nullptr)

void RealVM::DoExecute(int fnum)
{
    Function *f = functions_[fnum];

    int runaway = kMaximumRunaway;

    // make a stack frame
    int exitdepth = exec_.call_depth;

    EnterFunction(fnum);

    for (;;)
    {
        Statement *st = COAL_REF_OP(exec_.s);

        if (exec_.tracing)
            PrintStatement(f, exec_.s);

        if (!--runaway)
            RunError("runaway loop error");

        // move code pointer to next statement
        exec_.s += sizeof(Statement);

        // handle exotic operations here (ones which store special
        // values in the a / b / c fields of Statement).

        if (st->op < OP_MOVE_F)
            switch (st->op)
            {
            case OP_NULL:
                // no operation
                continue;

            case OP_CALL: {
                double *a = COAL_OPERAND(st->a);

                int fnum_call = (int)*a;
                if (fnum_call <= 0)
                    RunError("NULL function");

                Function *newf = functions_[fnum_call];

                /* negative statements are built in functions */
                if (newf->first_statement < 0)
                    EnterNative(fnum_call, st->b);
                else
                    EnterFunction(fnum_call);
                continue;
            }

            case OP_RET: {
                LeaveFunction();

                // all done?
                if (exec_.call_depth == exitdepth)
                    return;

                continue;
            }

            case OP_PARM_NULL: {
                double *a = &exec_.stack[exec_.stack_depth + functions_[exec_.func]->locals_end + st->b];

                *a = -FLT_MAX; // Trying to pick a reliable but very unlikely value for a parameter - Dasho
                continue;
            }

            case OP_PARM_F: {
                double *a = COAL_OPERAND(st->a);
                double *b = &exec_.stack[exec_.stack_depth + functions_[exec_.func]->locals_end + st->b];

                *b = *a;
                continue;
            }

            case OP_PARM_V: {
                double *a = COAL_OPERAND(st->a);
                double *b = &exec_.stack[exec_.stack_depth + functions_[exec_.func]->locals_end + st->b];

                b[0] = a[0];
                b[1] = a[1];
                b[2] = a[2];
                continue;
            }

            case OP_IFNOT: {
                if (!COAL_OPERAND(st->a)[0])
                    exec_.s = st->b;
                continue;
            }

            case OP_IF: {
                if (COAL_OPERAND(st->a)[0])
                    exec_.s = st->b;
                continue;
            }

            case OP_GOTO:
                exec_.s = st->b;
                continue;

            case OP_ERROR:
                RunError("Assertion failed @ %s:%d\n", COAL_REF_STRING(st->a), st->b);
                break; /* NOT REACHED */

            default:
                RunError("Bad opcode %i", st->op);
            }

        // handle mathematical ops here

        double *a = COAL_OPERAND(st->a);
        double *b = COAL_OPERAND(st->b);
        double *c = COAL_OPERAND(st->c);

        switch (st->op)
        {
        case OP_MOVE_F:
        case OP_MOVE_FNC: // pointers
            *b = *a;
            break;

        case OP_MOVE_S:
            // temp strings must be internalised when assigned
            // to a global variable.
            if (*a < 0 && st->b > kReturnOffset * 8)
                *b = InternaliseString(COAL_REF_STRING((int)*a));
            else
                *b = *a;
            break;

        case OP_MOVE_V:
            b[0] = a[0];
            b[1] = a[1];
            b[2] = a[2];
            break;

        case OP_NOT_F:
        case OP_NOT_FNC:
            *c = !*a;
            break;
        case OP_NOT_V:
            *c = !a[0] && !a[1] && !a[2];
            break;
        case OP_NOT_S:
            *c = !*a;
            break;

        case OP_INC:
            *c = *a + 1;
            break;
        case OP_DEC:
            *c = *a - 1;
            break;

        case OP_ADD_F:
            *c = *a + *b;
            break;

        case OP_ADD_V:
            c[0] = a[0] + b[0];
            c[1] = a[1] + b[1];
            c[2] = a[2] + b[2];
            break;

        case OP_ADD_S:
            *c = StringConcat(COAL_REF_STRING((int)*a), COAL_REF_STRING((int)*b));
            // temp strings must be internalised when assigned
            // to a global variable.
            if (st->c > kReturnOffset * 8)
                *c = InternaliseString(COAL_REF_STRING((int)*c));
            break;

        case OP_ADD_SF:
            *c = StringConcatFloat(COAL_REF_STRING((int)*a), *b);
            if (st->c > kReturnOffset * 8)
                *c = InternaliseString(COAL_REF_STRING((int)*c));
            break;

        case OP_ADD_SV:
            *c = StringConcatVector(COAL_REF_STRING((int)*a), b);
            if (st->c > kReturnOffset * 8)
                *c = InternaliseString(COAL_REF_STRING((int)*c));
            break;

        case OP_SUB_F:
            *c = *a - *b;
            break;
        case OP_SUB_V:
            c[0] = a[0] - b[0];
            c[1] = a[1] - b[1];
            c[2] = a[2] - b[2];
            break;

        case OP_MUL_F:
            *c = *a * *b;
            break;
        case OP_MUL_V:
            *c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            break;
        case OP_MUL_FV:
            c[0] = a[0] * b[0];
            c[1] = a[0] * b[1];
            c[2] = a[0] * b[2];
            break;
        case OP_MUL_VF:
            c[0] = b[0] * a[0];
            c[1] = b[0] * a[1];
            c[2] = b[0] * a[2];
            break;

        case OP_DIV_F:
            if (AlmostEquals(*b, 0.0))
                RunError("Division by zero");
            *c = *a / *b;
            break;

        case OP_DIV_V:
            if (AlmostEquals(*b, 0.0))
                RunError("Division by zero");
            c[0] = a[0] / *b;
            c[1] = a[1] / *b;
            c[2] = a[2] / *b;
            break;

        case OP_MOD_F:
            if (AlmostEquals(*b, 0.0))
                RunError("Division by zero");
            else
            {
                float d = floorf(*a / *b);
                *c      = *a - d * (*b);
            }
            break;

        case OP_POWER_F:
            *c = powf(*a, *b);
            break;

        case OP_GE:
            *c = *a >= *b;
            break;
        case OP_LE:
            *c = *a <= *b;
            break;
        case OP_GT:
            *c = *a > *b;
            break;
        case OP_LT:
            *c = *a < *b;
            break;

        case OP_EQ_F:
        case OP_EQ_FNC:
            *c = AlmostEquals(*a, *b);
            break;
        case OP_EQ_V:
            *c = (AlmostEquals(a[0], b[0])) && (AlmostEquals(a[1], b[1])) && (AlmostEquals(a[2], b[2]));
            break;
        case OP_EQ_S:
            *c = (AlmostEquals(*a, *b)) ? 1 : !strcmp(COAL_REF_STRING((int)*a), COAL_REF_STRING((int)*b));
            break;

        case OP_NE_F:
        case OP_NE_FNC:
            *c = !AlmostEquals(*a, *b);
            break;
        case OP_NE_V:
            *c = (!AlmostEquals(a[0], b[0])) || (!AlmostEquals(a[1], b[1])) || (!AlmostEquals(a[2], b[2]));
            break;
        case OP_NE_S:
            *c = (AlmostEquals(*a, *b)) ? 0 : !!strcmp(COAL_REF_STRING((int)*a), COAL_REF_STRING((int)*b));
            break;

        case OP_AND:
            *c = *a && *b;
            break;
        case OP_OR:
            *c = *a || *b;
            break;

        case OP_BITAND:
            *c = (int)*a & (int)*b;
            break;
        case OP_BITOR:
            *c = (int)*a | (int)*b;
            break;

        default:
            RunError("Bad opcode %i", st->op);
        }
    }
}

int RealVM::Execute(int func_id)
{
    // re-use the temporary string space
    temp_strings_.Reset();

    if (func_id < 1 || func_id >= (int)functions_.size())
    {
        RunError("VM::Execute: NULL function");
    }

    DoExecute(func_id);

    return 0;
}

//=================================================================
//  DEBUGGING STUFF
//=================================================================

static constexpr const char *opcode_names[] = {
    "NULL",   "CALL",   "RET",    "PARM_F",   "PARM_V", "IF",    "IFNOT", "GOTO",   "ERROR",

    "MOVE_F", "MOVE_V", "MOVE_S", "MOVE_FNC",

    "NOT_F",  "NOT_V",  "NOT_S",  "NOT_FNC",

    "INC",    "DEC",

    "POWER",  "MUL_F",  "MUL_V",  "MUL_FV",   "MUL_VF", "DIV_F", "DIV_V", "MOD_F",

    "ADD_F",  "ADD_V",  "ADD_S",  "ADD_SF",   "ADD_SV", "SUB_F", "SUB_V",

    "EQ_F",   "EQ_V",   "EQ_S",   "EQ_FNC",   "NE_F",   "NE_V",  "NE_S",  "NE_FNC", "LE",    "GE", "LT", "GT",

    "AND",    "OR",     "BITAND", "BITOR",
};

static const char *OpcodeName(int16_t op)
{
    if (op < 0 || op >= NUM_OPERATIONS)
        return "???";

    return opcode_names[op];
}

void RealVM::StackTrace()
{
    Printer("Stack Trace:\n");

    exec_.call_stack[exec_.call_depth].func = exec_.func;
    exec_.call_stack[exec_.call_depth].s    = exec_.s;

    for (int i = exec_.call_depth; i >= 1; i--)
    {
        int back = (exec_.call_depth - i) + 1;

        Function *f = functions_[exec_.call_stack[i].func];

        Statement *st = COAL_REF_OP(exec_.call_stack[i].s);

        if (f)
            Printer("%-2d %s() at %s:%d\n", back, f->name, f->source_file, f->source_line + st->line);
        else
            Printer("%-2d ????\n", back);
    }

    Printer("\n");
}

const char *RealVM::RegString(Statement *st, int who)
{
    static char buffer[100];

    int val = (who == 1) ? st->a : (who == 2) ? st->b : st->c;

    if (val == kReturnOffset * 8)
        return "result";

    if (val == kDefaultOffset * 8)
        return "default";

    stbsp_sprintf(buffer, "%s[%d]", (val < 0) ? "stack" : "glob", abs(val));
    return buffer;
}

void RealVM::PrintStatement(Function *f, int s)
{
    EPI_UNUSED(f);
    Statement *st = COAL_REF_OP(s);

    const char *op_name = OpcodeName(st->op);

    Printer("  %06x: %-9s ", s, op_name);

    switch (st->op)
    {
    case OP_NULL:
    case OP_RET:
    case OP_ERROR:
        break;

    case OP_MOVE_F:
    case OP_MOVE_S:
    case OP_MOVE_FNC: // pointers
    case OP_MOVE_V:
        Printer("%s ", RegString(st, 1));
        Printer("-> %s", RegString(st, 2));
        break;

    case OP_IFNOT:
    case OP_IF:
        Printer("%s %08x", RegString(st, 1), st->b);
        break;

    case OP_GOTO:
        Printer("%08x", st->b);
        // TODO
        break;

    case OP_CALL:
        Printer("%s (%d) ", RegString(st, 1), st->b);

        if (!st->c)
            Printer(" ");
        else
            Printer("-> %s", RegString(st, 3));
        break;

    case OP_PARM_F:
    case OP_PARM_V:
        Printer("%s -> future[%d]", RegString(st, 1), st->b);
        break;

    case OP_NOT_F:
    case OP_NOT_FNC:
    case OP_NOT_V:
    case OP_NOT_S:
        Printer("%s ", RegString(st, 1));
        Printer("-> %s", RegString(st, 3));
        break;

    default:
        Printer("%s + ", RegString(st, 1));
        Printer("%s ", RegString(st, 2));
        Printer("-> %s", RegString(st, 3));
        break;
    }

    Printer("\n");
}

void RealVM::ASMDumpFunction(Function *f)
{
    Printer("Function %s()\n", f->name);

    if (f->first_statement < 0)
    {
        Printer("  native #%d\n\n", -f->first_statement);
        return;
    }

    for (int s = f->first_statement; s <= f->last_statement; s += sizeof(Statement))
    {
        PrintStatement(f, s);
    }

    Printer("\n");
}

void RealVM::ASMDumpAll()
{
    for (int i = 1; i < (int)functions_.size(); i++)
    {
        Function *f = functions_[i];

        ASMDumpFunction(f);
    }
}

} // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
