//----------------------------------------------------------------------
//  COAL COMPILER
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

[[noreturn]] extern void FatalError(const char *error, ...);

namespace coal
{

static constexpr const char *punctuation[] =
    // longer symbols must be before a shorter partial match
    {"&&", "||", "<=", ">=", "==", "!=", "++", "--", "...", "..", ":", ";", ",", "!", "*", "/", "%",
     "^",  "(",  ")",  "-",  "+",  "=",  "[",  "]",  "{",   "}",  ".", "<", ">", "#", "&", "|", nullptr};

// simple types.  function types are dynamically Allocated
static Type type_void     = {ev_void, nullptr, 0, {nullptr}};
static Type type_string   = {ev_string, nullptr, 0, {nullptr}};
static Type type_float    = {ev_float, nullptr, 0, {nullptr}};
static Type type_vector   = {ev_vector, nullptr, 0, {nullptr}};
static Type type_function = {ev_function, &type_void, 0, {nullptr}};
static Type type_module   = {ev_module, nullptr, 0, {nullptr}};
static Type type_null     = {ev_null, nullptr, 0, {nullptr}};

static constexpr int type_size[10] = {1, 1, 1, 3, 1, 1, 1, 1, 1, 1};

// definition used for void return functions
static Definition def_void = {&type_void, "VOID_SPACE", 0, nullptr, 0, nullptr};

//
//  OPERATOR TABLE
//
struct OpCode
{
    const char *name;
    int         op; // OP_XXX
    int         priority;
    Type       *type_a, *type_b, *type_c;
};

static OpCode all_operators[] = {
    {"!", OP_NOT_F, -1, &type_float, &type_void, &type_float},
    {"!", OP_NOT_V, -1, &type_vector, &type_void, &type_float},
    {"!", OP_NOT_S, -1, &type_string, &type_void, &type_float},
    {"!", OP_NOT_FNC, -1, &type_function, &type_void, &type_float},

    /* priority 1 is for function calls */

    {"^", OP_POWER_F, 2, &type_float, &type_float, &type_float},

    {"*", OP_MUL_F, 2, &type_float, &type_float, &type_float},
    {"*", OP_MUL_V, 2, &type_vector, &type_vector, &type_float},
    {"*", OP_MUL_FV, 2, &type_float, &type_vector, &type_vector},
    {"*", OP_MUL_VF, 2, &type_vector, &type_float, &type_vector},

    {"/", OP_DIV_F, 2, &type_float, &type_float, &type_float},
    {"/", OP_DIV_V, 2, &type_vector, &type_float, &type_vector},
    {"%", OP_MOD_F, 2, &type_float, &type_float, &type_float},

    {"+", OP_ADD_F, 3, &type_float, &type_float, &type_float},
    {"+", OP_ADD_V, 3, &type_vector, &type_vector, &type_vector},
    {"+", OP_ADD_S, 3, &type_string, &type_string, &type_string},
    {"+", OP_ADD_SF, 3, &type_string, &type_float, &type_string},
    {"+", OP_ADD_SV, 3, &type_string, &type_vector, &type_string},

    {"-", OP_SUB_F, 3, &type_float, &type_float, &type_float},
    {"-", OP_SUB_V, 3, &type_vector, &type_vector, &type_vector},

    {"==", OP_EQ_F, 4, &type_float, &type_float, &type_float},
    {"==", OP_EQ_V, 4, &type_vector, &type_vector, &type_float},
    {"==", OP_EQ_S, 4, &type_string, &type_string, &type_float},
    {"==", OP_EQ_FNC, 4, &type_function, &type_function, &type_float},

    {"!=", OP_NE_F, 4, &type_float, &type_float, &type_float},
    {"!=", OP_NE_V, 4, &type_vector, &type_vector, &type_float},
    {"!=", OP_NE_S, 4, &type_string, &type_string, &type_float},
    {"!=", OP_NE_FNC, 4, &type_function, &type_function, &type_float},

    {"<=", OP_LE, 4, &type_float, &type_float, &type_float},
    {">=", OP_GE, 4, &type_float, &type_float, &type_float},
    {"<", OP_LT, 4, &type_float, &type_float, &type_float},
    {">", OP_GT, 4, &type_float, &type_float, &type_float},

    {"&&", OP_AND, 5, &type_float, &type_float, &type_float},
    {"||", OP_OR, 5, &type_float, &type_float, &type_float},

    {"&", OP_BITAND, 2, &type_float, &type_float, &type_float},
    {"|", OP_BITOR, 2, &type_float, &type_float, &type_float},

    {nullptr, 0, 0, nullptr, nullptr, nullptr} // end of list
};

static constexpr uint8_t kTopPriority = 6;
static constexpr uint8_t kNotPriority = 1;

void RealVM::LexNewLine()
{
    // Called when *comp_.parse_p == '\n'

    comp_.source_line++;

    comp_.line_start = comp_.parse_p + 1;
    comp_.fol_level  = 0;
}

//
// Aborts the current function parse.
// The given message should have a trailing \n
//
void RealVM::CompileError(const char *error, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, error);
    stbsp_vsprintf(buffer, error, argptr);
    va_end(argptr);

    FatalError("%s:%i: %s", comp_.source_file, comp_.source_line, buffer);
}

void RealVM::LexString()
{
    // Parses a quoted string

    comp_.parse_p++;

    int len = 0;

    for (;;)
    {
        int c = *comp_.parse_p++;
        if (!c || c == '\n')
            CompileError("unfinished string\n");

        if (c == '\\') // escape char
        {
            c = *comp_.parse_p++;
            if (!c || !(c > 0x1F && c < 0x7F))
                CompileError("bad escape in string\n");

            if (c == 'n')
                c = '\n';
            else if (c == '"')
                c = '"';
            else
                CompileError("unknown escape char: %c\n", c);
        }
        else if (c == '\"')
        {
            comp_.token_buf[len] = 0;
            comp_.token_type     = tt_literal;
            comp_.literal_type   = &type_string;
            strcpy(comp_.literal_buf, comp_.token_buf);
            return;
        }

        comp_.token_buf[len++] = c;
    }
}

float RealVM::LexNumber()
{
    int len = 0;
    int c   = *comp_.parse_p;

    do
    {
        comp_.token_buf[len++] = c;

        comp_.parse_p++;
        c = *comp_.parse_p;
    } while ((c >= '0' && c <= '9') || c == '.');

    comp_.token_buf[len] = 0;

    return atof(comp_.token_buf);
}

void RealVM::LexVector()
{
    // Parses a single quoted vector

    comp_.parse_p++;
    comp_.token_type   = tt_literal;
    comp_.literal_type = &type_vector;

    for (int i = 0; i < 3; i++)
    {
        // FIXME: check for digits etc!

        comp_.literal_value[i] = LexNumber();

        while (((*comp_.parse_p > 0x8 && *comp_.parse_p < 0xE) || *comp_.parse_p == 0x20) && *comp_.parse_p != '\n')
            comp_.parse_p++;
    }

    if (*comp_.parse_p != '\'')
        CompileError("bad vector\n");

    comp_.parse_p++;
}

void RealVM::LexName()
{
    // Parses an identifier

    int len = 0;
    int c   = *comp_.parse_p;

    do
    {
        comp_.token_buf[len++] = c;

        comp_.parse_p++;
        c = *comp_.parse_p;
    } while (((c > '@' && c < '[') || (c > '`' && c < '{') || (c > '/' && c < ':')) || c == '_');

    comp_.token_buf[len] = 0;
    comp_.token_type     = tt_name;
}

void RealVM::LexPunctuation()
{
    comp_.token_type = tt_punct;

    const char *p;

    char ch = *comp_.parse_p;

    for (int i = 0; (p = punctuation[i]) != nullptr; i++)
    {
        int len = strlen(p);

        if (strncmp(p, comp_.parse_p, len) == 0)
        {
            // found it
            strcpy(comp_.token_buf, p);

            if (p[0] == '{')
                comp_.bracelevel++;
            else if (p[0] == '}')
                comp_.bracelevel--;

            comp_.parse_p += len;
            return;
        }
    }

    CompileError("unknown punctuation: %c\n", ch);
}

void RealVM::LexWhitespace(void)
{
    int c;

    for (;;)
    {
        // skip whitespace
        while ((c = *comp_.parse_p) <= ' ')
        {
            if (c == 0) // end of file?
                return;

            if (c == '\n')
                LexNewLine();

            comp_.parse_p++;
        }

        // skip // comments
        if (c == '/' && comp_.parse_p[1] == '/')
        {
            while (*comp_.parse_p && *comp_.parse_p != '\n')
                comp_.parse_p++;

            LexNewLine();

            comp_.parse_p++;
            continue;
        }

        // skip /* */ comments
        if (c == '/' && comp_.parse_p[1] == '*')
        {
            do
            {
                comp_.parse_p++;

                if (comp_.parse_p[0] == '\n')
                    LexNewLine();

                if (comp_.parse_p[1] == 0)
                    return;

            } while (comp_.parse_p[-1] != '*' || comp_.parse_p[0] != '/');

            comp_.parse_p++;
            continue;
        }

        break; // a real character has been found
    }
}

//
// Parse the next token in the file.
// Sets token_type and token_buf, and possibly the literal_xxx fields
//
void RealVM::LexNext()
{
    EPI_ASSERT(comp_.parse_p);

    LexWhitespace();

    comp_.token_buf[0]   = 0;
    comp_.token_is_first = (comp_.fol_level == 0);

    comp_.fol_level++;

    int c = *comp_.parse_p;

    if (!c)
    {
        comp_.token_type = tt_eof;
        strcpy(comp_.token_buf, "(EOF)");
        return;
    }

    // handle quoted strings as a unit
    if (c == '\"')
    {
        LexString();
        return;
    }

    // handle quoted vectors as a unit
    if (c == '\'')
    {
        LexVector();
        return;
    }

    // if the first character is a valid identifier, parse until a non-id
    // character is reached
    if ((c >= '0' && c <= '9') || (c == '-' && comp_.parse_p[1] >= '0' && comp_.parse_p[1] <= '9'))
    {
        comp_.token_type       = tt_literal;
        comp_.literal_type     = &type_float;
        comp_.literal_value[0] = LexNumber();
        return;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
    {
        LexName();
        return;
    }

    // parse symbol strings until a non-symbol is found
    LexPunctuation();
}

//
// Issues an error if the current token isn't what we want.
// On success, automatically skips to the next token.
//
void RealVM::LexExpect(const char *str)
{
    if (strcmp(comp_.token_buf, str) != 0)
        CompileError("expected %s got %s\n", str, comp_.token_buf);

    LexNext();
}

//
// Checks that the current token matches what we want
// (which can be a keyword or symbol).
//
// Returns true on a match (skipping to the next token),
// otherwise returns false and does nothing.
//
bool RealVM::LexCheck(const char *str)
{
    if (strcmp(comp_.token_buf, str) != 0)
        return false;

    LexNext();

    return true;
}

//
// Checks to see if the current token is a valid name
//
char *RealVM::ParseName()
{
    static char ident[kMaximumNameLength];

    if (comp_.token_type != tt_name)
        CompileError("expected identifier, got %s\n", comp_.token_buf);

    if (strlen(comp_.token_buf) >= kMaximumNameLength - 1)
        CompileError("identifier too long\n");

    strcpy(ident, comp_.token_buf);

    LexNext();

    return ident;
}

//=============================================================================

//
// Returns a preexisting complex type that matches the parm, or Allocates
// a new one and copies it out.
//
Type *RealVM::FindType(Type *type)
{
    for (int k = 0; k < (int)comp_.all_types.size(); k++)
    {
        Type *check = comp_.all_types[k];

        if (check->type != type->type || check->aux_type != type->aux_type || check->parm_num != type->parm_num)
            continue;

        int i;
        for (i = 0; i < type->parm_num; i++)
            if (check->parm_types[i] != type->parm_types[i])
                break;

        if (i == type->parm_num)
            return check;
    }

    // Allocate a new one
    Type *t_new = new Type;
    *t_new      = *type;

    comp_.all_types.push_back(t_new);

    return t_new;
}

//
// Parses a variable type, including field and functions types
//
Type *RealVM::ParseType()
{
    Type  t_new;
    Type *type;
    char *name;

    if (!strcmp(comp_.token_buf, "float"))
        type = &type_float;
    else if (!strcmp(comp_.token_buf, "vector"))
        type = &type_vector;
    else if (!strcmp(comp_.token_buf, "float"))
        type = &type_float;
    //	else if (!strcmp(comp_.token_buf, "entity") )
    //		type = &type_entity;
    else if (!strcmp(comp_.token_buf, "string"))
        type = &type_string;
    else if (!strcmp(comp_.token_buf, "void"))
        type = &type_void;
    else
    {
        type = &type_float; // shut up compiler warning
        CompileError("unknown type: %s\n", comp_.token_buf);
    }
    LexNext();

    if (!LexCheck("("))
        return type;

    // function type
    EPI_CLEAR_MEMORY(&t_new, Type, 1);
    t_new.type     = ev_function;
    t_new.aux_type = type; // return type
    t_new.parm_num = 0;

    if (!LexCheck(")"))
    {
        if (LexCheck("..."))
            t_new.parm_num = -1; // variable args
        else
            do
            {
                type = ParseType();
                name = ParseName();

                strcpy(comp_.parm_names[t_new.parm_num], name);

                t_new.parm_types[t_new.parm_num] = type;
                t_new.parm_num++;
            } while (LexCheck(","));

        LexExpect(")");
    }

    return FindType(&t_new);
}

//===========================================================================

int RealVM::EmitCode(int16_t op, int a, int b, int c)
{
    // TODO: if last statement was OP_NULL, overwrite it instead of
    //       creating a new one.

    int ofs = op_mem_.Alloc((int)sizeof(Statement));

    Statement *st = COAL_REF_OP(ofs);

    st->op   = op;
    st->line = comp_.source_line - comp_.function_line;

    st->a = a;
    st->b = b;
    st->c = c;

    comp_.last_statement = ofs;

    return ofs;
}

int RealVM::EmitMove(Type *type, int a, int b)
{
    switch (type->type)
    {
    case ev_string:
        return EmitCode(OP_MOVE_S, a, b);

    case ev_vector:
        return EmitCode(OP_MOVE_V, a, b);

    default:
        return EmitCode(OP_MOVE_F, a, b);
    }
}

Definition *RealVM::NewGlobal(Type *type)
{
    int tsize = type_size[type->type];

    Definition *var = new Definition;
    EPI_CLEAR_MEMORY(var, Definition, 1);

    var->ofs  = global_mem_.Alloc(tsize * sizeof(double));
    var->type = type;

    // clear it
    EPI_CLEAR_MEMORY((double *)global_mem_.Deref(var->ofs), double, tsize);

    return var;
}

Definition *RealVM::NewLocal(Type *type)
{
    Definition *var = new Definition;
    EPI_CLEAR_MEMORY(var, Definition, 1);

    var->ofs  = -(comp_.locals_end + 1);
    var->type = type;

    comp_.locals_end += type_size[type->type];

    return var;
}

Definition *RealVM::NewTemporary(Type *type)
{
    Definition *var;

    std::vector<Definition *>::iterator TI;

    for (TI = comp_.temporaries.begin(); TI != comp_.temporaries.end(); TI++)
    {
        var = *TI;

        // make sure it fits
        if (type_size[var->type->type] < type_size[type->type])
            continue;

        if (!(var->flags & DF_FreeTemp))
            continue;

        // found a match, so re-use it!
        var->flags &= ~DF_FreeTemp;
        var->type = type;

        return var;
    }

    var = NewLocal(type);

    var->flags |= DF_Temporary;
    comp_.temporaries.push_back(var);

    return var;
}

void RealVM::FreeTemporaries()
{
    std::vector<Definition *>::iterator TI;

    for (TI = comp_.temporaries.begin(); TI != comp_.temporaries.end(); TI++)
    {
        Definition *tvar = *TI;

        tvar->flags |= DF_FreeTemp;
    }
}

Definition *RealVM::FindLiteral()
{
    // check for a constant with the same value
    for (int i = 0; i < (int)comp_.all_literals.size(); i++)
    {
        Definition *cn = comp_.all_literals[i];

        if (cn->type != comp_.literal_type)
            continue;

        if (comp_.literal_type == &type_string)
        {
            if (strcmp(COAL_G_STRING(cn->ofs), comp_.literal_buf) == 0)
                return cn;
        }
        else if (comp_.literal_type == &type_float)
        {
            if (AlmostEquals(COAL_G_FLOAT(cn->ofs), comp_.literal_value[0]))
                return cn;
        }
        else if (comp_.literal_type == &type_vector)
        {
            if (AlmostEquals(COAL_G_FLOAT(cn->ofs), comp_.literal_value[0]) &&
                AlmostEquals(COAL_G_FLOAT(cn->ofs + 1), comp_.literal_value[1]) &&
                AlmostEquals(COAL_G_FLOAT(cn->ofs + 2), comp_.literal_value[2]))
            {
                return cn;
            }
        }
    }

    return nullptr; // not found
}

void RealVM::StoreLiteral(int ofs)
{
    double *p = COAL_REF_GLOBAL(ofs);

    if (comp_.literal_type == &type_string)
    {
        *p = (double)InternaliseString(comp_.literal_buf);
    }
    else if (comp_.literal_type == &type_vector)
    {
        p[0] = comp_.literal_value[0];
        p[1] = comp_.literal_value[1];
        p[2] = comp_.literal_value[2];
    }
    else
    {
        *p = comp_.literal_value[0];
    }
}

Definition *RealVM::EXPLiteral()
{
    // Looks for a preexisting constant
    Definition *cn = FindLiteral();

    if (!cn)
    {
        // Allocate a new one
        cn = NewGlobal(comp_.literal_type);

        cn->name = "CONSTANT VALUE";

        cn->flags |= DF_Constant;
        cn->scope = nullptr; // literals are "scope-less"

        // copy the literal to the global area
        StoreLiteral(cn->ofs);

        comp_.all_literals.push_back(cn);
    }

    LexNext();
    return cn;
}

Definition *RealVM::EXPFunctionCall(Definition *func)
{
    Type *t = func->type;

    if (t->type != ev_function)
        CompileError("not a function before ()\n");

    Function *df = functions_[(int)*COAL_REF_GLOBAL(func->ofs)];

    // evaluate all parameters
    Definition *exprs[kMaximumParameters];

    int arg = 0;

    if (!LexCheck(")"))
    {
        do
        {
            if (arg >= t->parm_num)
                CompileError("too many parameters (expected %d)\n", t->parm_num);

            EPI_ASSERT(arg < kMaximumParameters);

            Definition *e = EXPExpression(kTopPriority);

            if (e->type != t->parm_types[arg])
                CompileError("type mismatch on parameter %i\n", arg + 1);

            EPI_ASSERT(e->type->type != ev_void);

            exprs[arg++] = e;
        } while (LexCheck(","));

        LexExpect(")");

        if (arg != t->parm_num)
        {
            if (df->optional_parm_start == -1)
                CompileError("COAL: Too few parameters for function %s (needed %d)\n", df->name, t->parm_num);
            else
            {
                if (t->parm_num <= df->optional_parm_start)
                    CompileError("COAL: Too few parameters for function %s (needed %d)\n", df->name, t->parm_num);
                for (int i = arg; i < t->parm_num; i++)
                {
                    exprs[i] = NewTemporary(&type_null);
                    arg++;
                }
            }
        }
    }

    Definition *result = nullptr;

    if (t->aux_type->type != ev_void)
    {
        result = NewTemporary(t->aux_type);
    }

    // copy parameters
    int parm_ofs = 0;

    for (int k = 0; k < arg; k++)
    {
        if (exprs[k]->type->type == ev_vector)
        {
            EmitCode(OP_PARM_V, exprs[k]->ofs, parm_ofs);
            parm_ofs += 3;
        }
        else if (exprs[k]->type->type == ev_null)
        {
            EmitCode(OP_PARM_NULL, 0, parm_ofs);
            parm_ofs++;
        }
        else
        {
            EmitCode(OP_PARM_F, exprs[k]->ofs, parm_ofs);
            parm_ofs++;
        }
    }

    // Note: local vars are setup where they are declared, and
    //       temporaries do not need any default value.

    EmitCode(OP_CALL, func->ofs, arg);

    if (result)
    {
        EmitMove(result->type, kReturnOffset * 8, result->ofs);
        return result;
    }

    return &def_void;
}

void RealVM::STATReturn(void)
{
    Definition *func_def = comp_.scope->def_;

    if (comp_.token_is_first || comp_.token_buf[0] == '}' || LexCheck(";"))
    {
        if (func_def->type->aux_type->type != ev_void)
            CompileError("missing value for return\n");

        EmitCode(OP_RET);
        return;
    }

    Definition *e = EXPExpression(kTopPriority);

    if (func_def->type->aux_type->type == ev_void)
        CompileError("return with value in void function\n");

    if (func_def->type->aux_type != e->type)
        CompileError("type mismatch for return\n");

    EmitMove(func_def->type->aux_type, e->ofs, kReturnOffset * 8);

    EmitCode(OP_RET);

    // -AJA- optional semicolons
    if (!(comp_.token_is_first || comp_.token_buf[0] == '}'))
        LexExpect(";");
}

Definition *RealVM::FindDef(Type *type, char *name, Scope *scope)
{
    for (Definition *def = scope->names_; def; def = def->next)
    {
        if (strcmp(def->name, name) != 0)
            continue;

        if (type && def->type != type)
            CompileError("type mismatch on redeclaration of %s\n", name);

        return def;
    }

    return nullptr;
}

Definition *RealVM::DeclareDef(Type *type, char *name, Scope *scope)
{
    // A new def will be Allocated if it can't be found

    EPI_ASSERT(type);

    Definition *def = FindDef(type, name, scope);
    if (def)
        return def;

    // Allocate a new def

    if (scope->kind_ == 'f')
        def = NewLocal(type);
    else
        def = NewGlobal(type);

    def->name = strdup(name);

    // link into list
    scope->PushBack(def);

    return def;
}

Definition *RealVM::EXPVarValue()
{
    char *name = ParseName();

    // look through the defs
    Scope *scope = comp_.scope;

    for (;;)
    {
        Definition *d = FindDef(nullptr, name, scope);
        if (d)
            return d;

        if (scope->kind_ == 'g')
            CompileError("unknown identifier: %s\n", name);

        // move to outer scope
        scope = scope->def_->scope;
    }
}

Definition *RealVM::EXPTerm()
{
    // if the token is a literal, Allocate a constant for it
    if (comp_.token_type == tt_literal)
        return EXPLiteral();

    if (comp_.token_type == tt_name)
        return EXPVarValue();

    if (LexCheck("("))
    {
        Definition *e = EXPExpression(kTopPriority);
        LexExpect(")");
        return e;
    }

    // unary operator?

    for (int n = 0; all_operators[n].name; n++)
    {
        OpCode *op = &all_operators[n];

        if (op->priority != -1)
            continue;

        if (!LexCheck(op->name))
            continue;

        Definition *e = EXPExpression(kNotPriority);

        for (int i = 0; i == 0 || strcmp(op->name, op[i].name) == 0; i++)
        {
            if (op[i].type_a->type != e->type->type)
                continue;

            Definition *result = NewTemporary(op[i].type_c);

            EmitCode(op[i].op, e->ofs, 0, result->ofs);

            return result;
        }

        CompileError("type mismatch for %s\n", op->name);
        break;
    }

    CompileError("expected value, got %s\n", comp_.token_buf);
}

Definition *RealVM::EXPShortCircuit(Definition *e, int n)
{
    OpCode *op = &all_operators[n];

    if (e->type->type != ev_float)
        CompileError("type mismatch for %s\n", op->name);

    // Instruction stream for &&
    //
    //		... calc a ...
    //		MOVE a --> c
    //		IF c == 0 GOTO label
    //		... calc b ...
    //		MOVE b --> c
    //		label:

    Definition *result = NewTemporary(op->type_c);

    EmitCode(OP_MOVE_F, e->ofs, result->ofs);

    int patch;

    if (op->name[0] == '&')
        patch = EmitCode(OP_IFNOT, result->ofs);
    else
        patch = EmitCode(OP_IF, result->ofs);

    Definition *e2 = EXPExpression(op->priority - 1);
    if (e2->type->type != ev_float)
        CompileError("type mismatch for %s\n", op->name);

    EmitCode(OP_MOVE_F, e2->ofs, result->ofs);

    COAL_REF_OP(patch)->b = EmitCode(OP_NULL);

    return result;
}

Definition *RealVM::EXPFieldQuery(Definition *e, bool lvalue)
{
    EPI_UNUSED(lvalue);
    char *name = ParseName();

    if (e->type->type == ev_vector)
    {
        Definition *vec = FindDef(&type_vector, (char *)e->name, e->scope);
        if (vec)
        {
            Definition *element = NewTemporary(&type_float);
            if (strcmp(name, "x") == 0)
                element->ofs = vec->ofs;
            else if (strcmp(name, "y") == 0)
                element->ofs = vec->ofs + sizeof(double);
            else if (strcmp(name, "z") == 0)
                element->ofs = vec->ofs + (2 * sizeof(double));
            else
                CompileError("Bad element access!\n");

            return element;
        }
        else
            CompileError("unknown identifier: %s.%s\n", e->name, name);
    }

    if (e->type->type == ev_module)
    {
        Scope *mod = comp_.all_modules[e->ofs];

        Definition *d = FindDef(nullptr, name, mod);
        if (!d)
            CompileError("unknown identifier: %s.%s\n", e->name, name);

        return d;
    }

    CompileError("type mismatch with . operator\n");
}

Definition *RealVM::EXPExpression(int priority, bool *lvalue)
{
    if (priority == 0)
        return EXPTerm();

    Definition *e = EXPExpression(priority - 1, lvalue);

    // loop through a sequence of same-priority operators
    bool found;

    do
    {
        found = false;

        while (priority == 1 && LexCheck("."))
            e = EXPFieldQuery(e, lvalue);

        if (priority == 1 && LexCheck("("))
        {
            if (lvalue)
                *lvalue = false;
            return EXPFunctionCall(e);
        }

        if (lvalue)
            return e;

        for (int n = 0; all_operators[n].name; n++)
        {
            OpCode *op = &all_operators[n];

            if (op->priority != priority)
                continue;

            if (!LexCheck(op->name))
                continue;

            found = true;

            if (strcmp(op->name, "&&") == 0 || strcmp(op->name, "||") == 0)
            {
                e = EXPShortCircuit(e, n);
                break;
            }

            Definition *e2 = EXPExpression(priority - 1);

            // type check

            BasicType type_a = e->type->type;
            BasicType type_b = e2->type->type;
            BasicType type_c = ev_void;

            OpCode *oldop = op;

            while (type_a != op->type_a->type || type_b != op->type_b->type ||
                   (type_c != ev_void && type_c != op->type_c->type))
            {
                op++;
                if (!op->name || strcmp(op->name, oldop->name) != 0)
                    CompileError("type mismatch for %s\n", oldop->name);
            }

            if (type_a == ev_pointer && type_b != e->type->aux_type->type)
                CompileError("type mismatch for %s\n", op->name);

            Definition *result = NewTemporary(op->type_c);

            EmitCode(op->op, e->ofs, e2->ofs, result->ofs);

            e = result;
            break;
        }
    } while (found);

    return e;
}

void RealVM::STATIf_Else()
{
    LexExpect("(");
    Definition *e = EXPExpression(kTopPriority);
    LexExpect(")");

    int patch = EmitCode(OP_IFNOT, e->ofs);

    STATStatement(false);
    FreeTemporaries();

    if (LexCheck("else"))
    {
        // use GOTO to skip over the else statements
        int patch2 = EmitCode(OP_GOTO);

        COAL_REF_OP(patch)->b = EmitCode(OP_NULL);

        patch = patch2;

        STATStatement(false);
        FreeTemporaries();
    }

    COAL_REF_OP(patch)->b = EmitCode(OP_NULL);
}

void RealVM::STATAssert()
{
    // TODO: only internalise the filename ONCE
    int file_str = InternaliseString(comp_.source_file);
    int line_num = comp_.source_line;

    LexExpect("(");
    Definition *e = EXPExpression(kTopPriority);
    LexExpect(")");

    int patch = EmitCode(OP_IF, e->ofs);

    EmitCode(OP_ERROR, file_str, line_num);
    FreeTemporaries();

    COAL_REF_OP(patch)->b = EmitCode(OP_NULL);
}

void RealVM::STATWhileLoop()
{
    int begin = EmitCode(OP_NULL);

    LexExpect("(");
    Definition *e = EXPExpression(kTopPriority);
    LexExpect(")");

    int patch = EmitCode(OP_IFNOT, e->ofs);

    STATStatement(false);
    FreeTemporaries();

    EmitCode(OP_GOTO, 0, begin);

    COAL_REF_OP(patch)->b = EmitCode(OP_NULL);
}

void RealVM::STATRepeatLoop()
{
    int begin = EmitCode(OP_NULL);

    STATStatement(false);
    FreeTemporaries();

    LexExpect("until");
    LexExpect("(");

    Definition *e = EXPExpression(kTopPriority);

    EmitCode(OP_IFNOT, e->ofs, begin);

    LexExpect(")");

    // -AJA- optional semicolons
    if (!(comp_.token_is_first || comp_.token_buf[0] == '}'))
        LexExpect(";");
}

void RealVM::STATForLoop()
{
    LexExpect("(");

    char *var_name = strdup(ParseName());

    Definition *var = FindDef(&type_float, var_name, comp_.scope);

    if (!var || (var->flags & DF_Constant))
        CompileError("unknown variable in for loop: %s\n", var_name);

    LexExpect("=");

    Definition *e1 = EXPExpression(kTopPriority);
    if (e1->type != var->type)
        CompileError("type mismatch in for loop\n");

    // assign first value to the variable
    EmitCode(OP_MOVE_F, e1->ofs, var->ofs);

    LexExpect(",");

    Definition *e2 = EXPExpression(kTopPriority);
    if (e2->type != var->type)
        CompileError("type mismatch in for loop\n");

    // create local to contain second value
    Definition *target = NewLocal(&type_float);
    EmitCode(OP_MOVE_F, e2->ofs, target->ofs);

    LexExpect(")");

    Definition *cond_temp = NewTemporary(&type_float);

    int begin = EmitCode(OP_LE, var->ofs, target->ofs, cond_temp->ofs);
    int patch = EmitCode(OP_IFNOT, cond_temp->ofs);

    STATStatement(false);
    FreeTemporaries();

    // increment the variable
    EmitCode(OP_INC, var->ofs, 0, var->ofs);
    EmitCode(OP_GOTO, 0, begin);

    COAL_REF_OP(patch)->b = EmitCode(OP_NULL);
}

void RealVM::STATAssignment(Definition *e)
{
    if (e->flags & DF_Constant)
        CompileError("assignment to a constant\n");

    Definition *e2 = EXPExpression(kTopPriority);

    if (e2->type != e->type)
        CompileError("type mismatch in assignment\n");

    EmitMove(e->type, e2->ofs, e->ofs);
}

void RealVM::STATStatement(bool allow_def)
{
    if (allow_def && LexCheck("var"))
    {
        GLOBVariable();
        return;
    }

    if (allow_def && LexCheck("function"))
    {
        CompileError("functions must be global\n");
        return;
    }

    if (allow_def && LexCheck("constant"))
    {
        CompileError("constants must be global\n");
        return;
    }

    if (LexCheck("{"))
    {
        do
        {
            STATStatement(true);
            FreeTemporaries();
        } while (!LexCheck("}"));

        return;
    }

    if (LexCheck("return"))
    {
        STATReturn();
        return;
    }

    if (LexCheck("if"))
    {
        STATIf_Else();
        return;
    }

    if (LexCheck("assert"))
    {
        STATAssert();
        return;
    }

    if (LexCheck("while"))
    {
        STATWhileLoop();
        return;
    }

    if (LexCheck("repeat"))
    {
        STATRepeatLoop();
        return;
    }

    if (LexCheck("for"))
    {
        STATForLoop();
        return;
    }

    bool        lvalue = true;
    Definition *e      = EXPExpression(kTopPriority, &lvalue);

    // lvalue is false for a plain function call

    if (lvalue)
    {
        LexExpect("=");

        STATAssignment(e);
    }

    // -AJA- optional semicolons
    if (!(comp_.token_is_first || comp_.token_buf[0] == '}'))
        LexExpect(";");
}

int RealVM::GLOBFunctionBody(Definition *func_def, Type *type, const char *func_name)
{
    // Returns the first_statement value

    comp_.temporaries.clear();

    comp_.function_line = comp_.source_line;

    //
    // check for native function definition
    //
    if (LexCheck("native"))
    {
        const char *module = nullptr;
        if (func_def->scope->kind_ == 'm')
            module = func_def->scope->def_->name;

        int native = GetNativeFunc(func_name, module);

        if (native < 0)
        {
            // fix scope (must not stay in function scope)
            comp_.scope = func_def->scope;

            CompileError("no such native function: %s%s%s\n", module ? module : "", module ? "." : "", func_name);
        }

        return -(native + 1);
    }

    //
    // create the parmeters as locals
    //

    for (int i = 0; i < type->parm_num; i++)
    {
        if (FindDef(type->parm_types[i], comp_.parm_names[i], comp_.scope))
            CompileError("parameter %s redeclared\n", comp_.parm_names[i]);

        DeclareDef(type->parm_types[i], comp_.parm_names[i], comp_.scope);
    }

    int code = EmitCode(OP_NULL);

    //
    // parse regular statements
    //
    LexExpect("{");

    while (!LexCheck("}"))
    {
        if (comp_.token_type == tt_error)
            LexNext();
        else
            STATStatement(true);

        if (comp_.token_type == tt_eof)
            CompileError("unfinished function body (hit EOF)\n");

        FreeTemporaries();
    }

    Statement *last = COAL_REF_OP(comp_.last_statement);

    if (last->op != OP_RET)
    {
        if (type->aux_type->type == ev_void)
            EmitCode(OP_RET);
        else
            CompileError("missing return at end of function %s\n", func_name);
    }

    return code;
}

void RealVM::GLOBFunction()
{
    char *func_name = strdup(ParseName());

    LexExpect("(");

    Type t_new;

    EPI_CLEAR_MEMORY(&t_new, Type, 1);
    t_new.type         = ev_function;
    t_new.parm_num     = 0;
    t_new.aux_type     = &type_void;
    int optional_start = -1;

    if (!LexCheck(")"))
    {
        do
        {
            if (t_new.parm_num >= kMaximumParameters)
                CompileError("too many parameters (over %d)\n", kMaximumParameters);

            char *name = ParseName();

            if (strcmp(name, "optional") == 0)
            {
                if (optional_start == -1)
                    optional_start = t_new.parm_num;
                name = ParseName();
            }
            else
            {
                if (optional_start > -1)
                    CompileError("Function %s has required parameters declared after optional parameters!\n",
                                 func_name);
            }

            strcpy(comp_.parm_names[t_new.parm_num], name);

            // parameter type (defaults to float)
            if (LexCheck(":"))
                t_new.parm_types[t_new.parm_num] = ParseType();
            else
                t_new.parm_types[t_new.parm_num] = &type_float;

            t_new.parm_num++;
        } while (LexCheck(","));

        LexExpect(")");
    }

    // return type (defaults to void)
    if (LexCheck(":"))
    {
        t_new.aux_type = ParseType();
    }

    Type *func_type = FindType(&t_new);

    Definition *def = DeclareDef(func_type, func_name, comp_.scope);

    EPI_ASSERT(func_type->type == ev_function);

    LexExpect("=");

    // fill in the dfunction
    COAL_G_FLOAT(def->ofs) = (double)functions_.size();

    Function *df = new Function;
    EPI_CLEAR_MEMORY(df, Function, 1);

    functions_.push_back(df);

    df->name        = func_name; // already strdup'd
    df->source_file = strdup(comp_.source_file);
    df->source_line = comp_.source_line;

    int stack_ofs = 0;

    df->return_size = type_size[def->type->aux_type->type];
    if (def->type->aux_type->type == ev_void)
        df->return_size = 0;
    ///---	stack_ofs += df->return_size;

    df->parm_num = def->type->parm_num;

    df->optional_parm_start = optional_start;

    for (int i = 0; i < df->parm_num; i++)
    {
        df->parm_ofs[i]  = stack_ofs;
        df->parm_size[i] = type_size[def->type->parm_types[i]->type];
        if (def->type->parm_types[i]->type == ev_void)
            df->parm_size[i] = 0;

        stack_ofs += df->parm_size[i];
    }

    df->locals_ofs = stack_ofs;

    // parms are "reAlloc'd" by DeclareDef in FunctionBody
    comp_.locals_end = 0;

    Scope *OLD_scope = comp_.scope;

    comp_.scope        = new Scope;
    comp_.scope->kind_ = 'f';
    comp_.scope->def_  = def;
    //  {
    df->first_statement = GLOBFunctionBody(def, func_type, func_name);
    df->last_statement  = comp_.last_statement;
    //  }
    comp_.scope = OLD_scope;

    df->locals_size = comp_.locals_end - df->locals_ofs;
    df->locals_end  = comp_.locals_end;

    if (comp_.asm_dump)
        ASMDumpFunction(df);

    // debugprintf(stderr, "FUNCTION %s locals:%d\n", func_name, comp_.locals_end);
}

void RealVM::GLOBVariable()
{
    char *var_name = strdup(ParseName());

    Type *type = &type_float;

    if (LexCheck(":"))
    {
        type = ParseType();
    }

    Definition *def = DeclareDef(type, var_name, comp_.scope);

    if (def->flags & DF_Constant)
        CompileError("%s previously defined as a constant\n");

    if (LexCheck("="))
    {
        // global variables can only be initialised with a constant
        if (def->ofs > 0)
        {
            if (comp_.token_type != tt_literal)
                CompileError("expected value for var, got %s\n", comp_.token_buf);

            if (comp_.literal_type->type != type->type)
                CompileError("type mismatch for %s\n", var_name);

            StoreLiteral(def->ofs);

            LexNext();
        }
        else // local variables can take an expression
             // it is equivalent to: var XX ; XX = ...
        {
            Definition *e2 = EXPExpression(kTopPriority);

            if (e2->type != def->type)
                CompileError("type mismatch for %s\n", var_name);

            EmitMove(type, e2->ofs, def->ofs);
        }
    }
    else // set to default
    {
        // global vars are already zero (via NewGlobal)
        if (def->ofs < 0)
            EmitMove(type, kDefaultOffset * 8, def->ofs);
    }

    // -AJA- optional semicolons
    if (!(comp_.token_is_first || comp_.token_buf[0] == '}'))
        LexExpect(";");
}

void RealVM::GLOBConstant()
{
    char *const_name = strdup(ParseName());

    LexExpect("=");

    if (comp_.token_type != tt_literal)
        CompileError("expected value for constant, got %s\n", comp_.token_buf);

    Definition *cn = DeclareDef(comp_.literal_type, const_name, comp_.scope);

    cn->flags |= DF_Constant;

    StoreLiteral(cn->ofs);

    LexNext();

    // -AJA- optional semicolons
    if (!(comp_.token_is_first || comp_.token_buf[0] == '}'))
        LexExpect(";");
}

void RealVM::GLOBModule()
{
    if (comp_.scope->kind_ != 'g')
        CompileError("modules cannot contain other modules\n");

    char *mod_name = strdup(ParseName());

    Definition *def = FindDef(&type_module, mod_name, comp_.scope);

    Scope *mod = nullptr;

    if (def)
        mod = comp_.all_modules[def->ofs];
    else
    {
        def = new Definition;
        EPI_CLEAR_MEMORY(def, Definition, 1);

        def->name  = mod_name;
        def->type  = &type_module;
        def->ofs   = (int)comp_.all_modules.size();
        def->scope = comp_.scope;

        comp_.scope->PushBack(def);

        mod = new Scope;

        mod->kind_ = 'm';
        mod->def_  = def;

        comp_.all_modules.push_back(mod);
    }

    Scope *OLD_scope = comp_.scope;

    comp_.scope = mod;

    LexExpect("{");

    while (!LexCheck("}"))
    {
        // handle a previous error
        if (comp_.token_type == tt_error)
            LexNext();
        else
            GLOBGlobals();

        if (comp_.token_type == tt_eof)
            CompileError("unfinished module (hit EOF)\n");
    }

    comp_.scope = OLD_scope;
}

void RealVM::GLOBGlobals()
{
    if (LexCheck("function"))
    {
        GLOBFunction();
        return;
    }

    if (LexCheck("var"))
    {
        GLOBVariable();
        return;
    }

    if (LexCheck("constant"))
    {
        GLOBConstant();
        return;
    }

    if (LexCheck("module"))
    {
        GLOBModule();
        return;
    }

    CompileError("expected global definition, got %s\n", comp_.token_buf);
}

//
// compiles the NUL terminated text, adding definitions to the pr structure
//
bool RealVM::CompileFile(char *buffer, const char *filename)
{
    comp_.source_file   = filename;
    comp_.source_line   = 1;
    comp_.function_line = 0;

    comp_.parse_p    = buffer;
    comp_.line_start = buffer;
    comp_.bracelevel = 0;
    comp_.fol_level  = 0;

    LexNext(); // read first token

    while (comp_.token_type != tt_eof)
    {
        comp_.scope = &comp_.global_scope;

        // handle a previous error
        if (comp_.token_type == tt_error)
            LexNext();
        else
            GLOBGlobals();
    }

    comp_.source_file = nullptr;

    return (comp_.error_count == 0);
}

void RealVM::ShowStats()
{
    Printer("functions: %u\n", functions_.size());
    Printer("string memory: %d / %d\n", string_mem_.UsedMemory(), string_mem_.TotalMemory());
    Printer("instruction memory: %d / %d\n", op_mem_.UsedMemory(), op_mem_.TotalMemory());
    Printer("globals memory: %d / %d\n", global_mem_.UsedMemory(), global_mem_.TotalMemory());
}

RealVM::RealVM()
    : op_mem_(), global_mem_(), string_mem_(), temp_strings_(), functions_(), native_funcs_(), comp_(), exec_()
{
    // string #0 must be the empty string
    int ofs = string_mem_.Alloc(2);
    if (ofs != 0)
    {
        RunError("string #0 must be the empty string\n");
    }

    strcpy((char *)string_mem_.Deref(0), "");

    // function #0 is the "null function"
    Function *df = new Function;
    EPI_CLEAR_MEMORY(df, Function, 1);

    functions_.push_back(df);

    // statement #0 is never used
    ofs = EmitCode(OP_RET);
    if (ofs != 0)
    {
        RunError("statement #0 is never used\n");
    }

    // global #0 is never used (equivalent to nullptr)
    // global #1-#3 are reserved for function return values
    // global #4-#6 are reserved for a zero value
    ofs = global_mem_.Alloc(7 * sizeof(double));
    EPI_ASSERT(ofs == 0);
    EPI_CLEAR_MEMORY((double *)global_mem_.Deref(0), double, 7);
}

RealVM::~RealVM()
{
    // FIXME !!!!
}

void RealVM::SetPrinter(PrintFunction func)
{
    Printer = func;
}

void RealVM::SetAsmDump(bool enable)
{
    comp_.asm_dump = enable;
}

double RealVM::GetFloat(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetFloat failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_float, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_float, (char *)var_name, &comp_.global_scope);

    if (var)
        return COAL_G_FLOAT(var->ofs);

    RunError("GetFloat failed: Could not find variable %s\n", var_name);
}

const char *RealVM::GetString(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetString failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_string, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_string, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        return COAL_G_STRING(var->ofs);
    }

    RunError("GetString failed: Could not find variable %s\n", var_name);
}

double *RealVM::GetVector(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetVector failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        return COAL_G_VECTOR(var->ofs);
    }

    RunError("GetVector failed: Could not find variable %s\n", var_name);
}

double RealVM::GetVectorX(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetVectorX failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        return COAL_G_VECTOR(var->ofs)[0];
    }

    RunError("GetVectorX failed: Could not find variable %s\n", var_name);
}

double RealVM::GetVectorY(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetVectorY failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        return COAL_G_VECTOR(var->ofs)[1];
    }

    RunError("GetVectorY failed: Could not find variable %s\n", var_name);
}

double RealVM::GetVectorZ(const char *mod_name, const char *var_name)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            RunError("GetVectorZ failed: Could not find module %s\n", mod_name);
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        return COAL_G_VECTOR(var->ofs)[2];
    }

    RunError("GetVectorZ failed: Could not find variable %s\n", var_name);
}

void RealVM::SetFloat(const char *mod_name, const char *var_name, double value)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetFloat failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_float, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_float, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        COAL_G_FLOAT(var->ofs) = value;
        return;
    }

    Printer("SetFloat failed: Could not find variable %s\n", var_name);
    return;
}

void RealVM::SetString(const char *mod_name, const char *var_name, const char *value)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetString failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_string, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_string, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        *COAL_REF_GLOBAL(var->ofs) = (double)InternaliseString(value);
        return;
    }

    Printer("SetString failed: Could not find variable %s\n", var_name);
    return;
}

void RealVM::SetVector(const char *mod_name, const char *var_name, double val_1, double val_2, double val_3)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetVector failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        COAL_G_VECTOR(var->ofs)[0] = val_1;
        COAL_G_VECTOR(var->ofs)[1] = val_2;
        COAL_G_VECTOR(var->ofs)[2] = val_3;
        return;
    }

    Printer("SetVector failed: Could not find variable %s\n", var_name);
    return;
}

void RealVM::SetVectorX(const char *mod_name, const char *var_name, double val)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetVectorX failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        COAL_G_VECTOR(var->ofs)[0] = val;
        return;
    }

    Printer("SetVectorX failed: Could not find variable %s\n", var_name);
    return;
}

void RealVM::SetVectorY(const char *mod_name, const char *var_name, double val)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetVectorY failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        COAL_G_VECTOR(var->ofs)[1] = val;
        return;
    }

    Printer("SetVectorY failed: Could not find variable %s\n", var_name);
    return;
}

void RealVM::SetVectorZ(const char *mod_name, const char *var_name, double val)
{
    Definition *mod_def   = nullptr;
    Scope      *mod_scope = nullptr;
    if (mod_name)
    {
        mod_def = FindDef(&type_module, (char *)mod_name, &comp_.global_scope);
        if (!mod_def)
        {
            Printer("SetVectorZ failed: Could not find module %s\n", mod_name);
            return;
        }
        mod_scope = comp_.all_modules[mod_def->ofs];
    }

    Definition *var = nullptr;
    if (mod_scope)
        var = FindDef(&type_vector, (char *)var_name, mod_scope);
    else
        var = FindDef(&type_vector, (char *)var_name, &comp_.global_scope);

    if (var)
    {
        COAL_G_VECTOR(var->ofs)[2] = val;
        return;
    }

    Printer("SetVectorZ failed: Could not find variable %s\n", var_name);
    return;
}

VM *CreateVM()
{
    EPI_ASSERT(sizeof(double) == 8);

    return new RealVM;
}

} // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
