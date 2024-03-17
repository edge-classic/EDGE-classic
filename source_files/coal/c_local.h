//----------------------------------------------------------------------
//  COAL LOCAL DEFS
//----------------------------------------------------------------------
//
//  Copyright (C) 2021-2024 The EDGE Team
//  Copyright (C) 2009-2021  Andrew Apted
//  Copyright (C) 1996-1997  Id Software, Inc.
//
//  Coal is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 3
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

#pragma once

#include <stdint.h>

#include <vector>

#include "coal.h"

namespace coal
{

struct MemoryBlock
{
    int used = 0;

// Prevent SIGBUS errors on ARM32 and heap alignment issues with emscripten
#if defined(__arm__) || defined(EMSCRIPTEN)
    double data[4096];
#else
    char data[4096];
#endif
};

class MemoryBlockGroup
{
    friend class MemoryManager;

  private:
    int pos_;

    MemoryBlock *blocks_[256];

  public:
    MemoryBlockGroup();
    ~MemoryBlockGroup();

    int TryAlloc(int len);

    void Reset();

    int UsedMemory() const;
    int TotalMemory() const;
};

class MemoryManager
{
  private:
    int pos_;

    MemoryBlockGroup *groups_[256];

  public:
    MemoryManager();
    ~MemoryManager();

    int Alloc(int len);

    inline void *Deref(int index) const
    {
        MemoryBlockGroup *grp = groups_[index >> 20];
        index &= ((1 << 20) - 1);

        MemoryBlock *blk = grp->blocks_[index >> 12];
        index &= ((1 << 12) - 1);

        return blk->data + index;
    }

    // forget all the previously stored items.  May not actually
    // free any memory.
    void Reset();

    // compute the total amount of memory used.  The second form
    // includes all the extra/free/wasted space.
    int UsedMemory() const;
    int TotalMemory() const;
};

constexpr uint8_t kMaximumNameLength = 64;
constexpr uint8_t kMaximumParameters = 16;

enum BasicType
{
    ev_INVALID = -1,

    ev_void = 0,
    ev_string,
    ev_float,
    ev_vector,
    ev_entity,
    ev_field,
    ev_function,
    ev_module,
    ev_pointer,
    ev_null
};

struct Statement
{
    int16_t op;
    int16_t line; // offset from start of function
    int     a, b, c;
};

enum OperationType
{
    OP_NULL = 0,

    OP_CALL,
    OP_RET,

    OP_PARM_NULL,
    OP_PARM_F,
    OP_PARM_V,

    OP_IF,
    OP_IFNOT,
    OP_GOTO,
    OP_ERROR,

    OP_MOVE_F,
    OP_MOVE_V,
    OP_MOVE_S,
    OP_MOVE_FNC,

    // ---- mathematical ops from here on --->

    OP_NOT_F,
    OP_NOT_V,
    OP_NOT_S,
    OP_NOT_FNC,

    OP_INC,
    OP_DEC,

    OP_POWER_F,
    OP_MUL_F,
    OP_MUL_V,
    OP_MUL_FV,
    OP_MUL_VF,

    OP_DIV_F,
    OP_DIV_V,
    OP_MOD_F,

    OP_ADD_F,
    OP_ADD_V,
    OP_ADD_S,
    OP_ADD_SF,
    OP_ADD_SV,

    OP_SUB_F,
    OP_SUB_V,

    OP_EQ_F,
    OP_EQ_V,
    OP_EQ_S,
    OP_EQ_FNC,

    OP_NE_F,
    OP_NE_V,
    OP_NE_S,
    OP_NE_FNC,

    OP_LE,
    OP_GE,
    OP_LT,
    OP_GT,

    OP_AND,
    OP_OR,
    OP_BITAND,
    OP_BITOR,

    NUM_OPERATIONS
};

struct Function
{
    const char *name;

    // where it was defined (last)
    const char *source_file;
    int         source_line;

    int return_size;

    int     parm_num;
    int16_t parm_ofs[kMaximumParameters];
    int16_t parm_size[kMaximumParameters];
    int     optional_parm_start; // Param nums equal or higher to this are optional

    int locals_ofs;
    int locals_size;
    int locals_end;

    int first_statement; // negative numbers are builtins
    int last_statement;
};

//=============================================================================

constexpr uint8_t kReturnOffset  = 1;
constexpr uint8_t kDefaultOffset = 4;

#define COAL_REF_OP(ofs)     ((Statement *)op_mem_.Deref(ofs))
#define COAL_REF_GLOBAL(ofs) ((double *)global_mem_.Deref(ofs))
#define COAL_REF_STRING(ofs)                                                                                           \
    ((ofs) == 0 ? "" : (ofs) < 0 ? (char *)temp_strings_.Deref(-(1 + (ofs))) : (char *)string_mem_.Deref(ofs))

#define COAL_G_FLOAT(ofs)  (*COAL_REF_GLOBAL(ofs))
#define COAL_G_VECTOR(ofs) COAL_REF_GLOBAL(ofs)
#define COAL_G_STRING(ofs) COAL_REF_STRING((int)COAL_G_FLOAT(ofs))

struct RegisteredNativeFunction
{
    const char    *name;
    NativeFunction func;
};

//============================================================//

class Scope;

enum Token
{
    tt_eof,     // end of file reached
    tt_name,    // an alphanumeric name token
    tt_punct,   // code punctuation
    tt_literal, // string, float, vector
    tt_error    // an error occured (so get next token)
};

struct Type
{
    BasicType type;

    // function types are more complex
    Type *aux_type;                       // return type or field type

    int   parm_num;                       // -1 = variable args
    Type *parm_types[kMaximumParameters]; // only [parm_num] allocated
};

struct Definition
{
    Type       *type;
    const char *name;

    // offset in global data block (if > 0)
    // when < 0, it is offset into local stack frame
    int ofs;

    Scope *scope;

    int flags;

    Definition *next;
};

enum DefinitionFlag
{
    DF_Constant  = (1 << 1),
    DF_Temporary = (1 << 2),
    DF_FreeTemp  = (1 << 3), // temporary can be re-used
};

class Scope
{
  public:
    char kind_;         // 'g' global, 'f' function, 'm' module

    Definition *names_; // functions, vars, constants, parameters

    Definition *def_;   // parent scope is def->scope

  public:
    Scope() : kind_('g'), names_(nullptr), def_(nullptr)
    {
    }
    ~Scope()
    {
    }

    void PushBack(Definition *def_in)
    {
        def_in->scope = this;
        def_in->next  = names_;
        names_        = def_in;
    }
};

struct Compiler
{
    const char *source_file = nullptr;
    int         source_line = 0;
    int         function_line;

    bool asm_dump = false;

    // current parsing position
    char *parse_p    = nullptr;
    char *line_start = nullptr; // start of current source line
    int   bracelevel;
    int   fol_level;            // fol = first on line

    // current token (from LEX_Next)
    char  token_buf[2048];
    Token token_type;
    bool  token_is_first;

    char   literal_buf[2048];
    Type  *literal_type;
    double literal_value[3];

    // parameter names (when parsing a function def)
    char parm_names[kMaximumParameters][kMaximumNameLength];

    int error_count = 0;

    Scope global_scope;

    std::vector<Scope *>      all_modules;
    std::vector<Type *>       all_types;
    std::vector<Definition *> all_literals;

    // all temporaries for current function
    std::vector<Definition *> temporaries;

    // the function/module being parsed, or nullptr
    Scope *scope;

    // for tracking local variables vs temps
    int locals_end;
    int last_statement;
};

constexpr uint8_t  kMaximumCallStack  = 96;
constexpr uint16_t kMaximumLocalStack = 2048;

struct CallStack
{
    int s;
    int func;
};

struct Execution
{
    // code pointer
    int s    = 0;
    int func = 0;

    bool tracing = false;

    double stack[kMaximumLocalStack];
    int    stack_depth = 0;

    CallStack call_stack[kMaximumCallStack + 1];
    int       call_depth = 0;
};

class RealVm : public Vm
{
  public:
    /* API functions */

    RealVm();
    ~RealVm();

    void SetPrinter(PrintFunction func);

    void AddNativeFunction(const char *name, NativeFunction func);

    bool CompileFile(char *buffer, const char *filename);
    void ShowStats();

    void SetAsmDump(bool enable);
    void SetTrace(bool enable);

    double      GetFloat(const char *mod_name, const char *var_name);
    const char *GetString(const char *mod_name, const char *var_name);
    double     *GetVector(const char *mod_name, const char *var_name);
    double      GetVectorX(const char *mod_name, const char *var_name);
    double      GetVectorY(const char *mod_name, const char *var_name);
    double      GetVectorZ(const char *mod_name, const char *var_name);

    void SetFloat(const char *mod_name, const char *var_name, double value);
    void SetString(const char *mod_name, const char *var_name, const char *value);
    void SetVector(const char *mod_name, const char *var_name, double val_1, double val_2, double val_3);
    void SetVectorX(const char *mod_name, const char *var_name, double val);
    void SetVectorY(const char *mod_name, const char *var_name, double val);
    void SetVectorZ(const char *mod_name, const char *var_name, double val);

    int FindFunction(const char *name);
    int FindVariable(const char *name);

    int Execute(int func_id);

    double     *AccessParam(int p);
    const char *AccessParamString(int p);

    void ReturnFloat(double f);
    void ReturnVector(double *v);
    void ReturnString(const char *s, int len = -1);

  private:
    PrintFunction Printer;

    MemoryManager op_mem_;
    MemoryManager global_mem_;
    MemoryManager string_mem_;
    MemoryManager temp_strings_;

    std::vector<Function *>                 functions_;
    std::vector<RegisteredNativeFunction *> native_funcs_;

    Compiler  comp_;
    Execution exec_;

    // c_compile.cc
  private:
    void GLOB_Globals();
    void GLOB_Module();
    void GLOB_Constant();
    void GLOB_Variable();
    void GLOB_Function();
    int  GLOB_FunctionBody(Definition *func_def, Type *type, const char *func_name);

    void STAT_Statement(bool allow_def);
    void STAT_Assignment(Definition *e);
    void STAT_If_Else();
    void STAT_Assert();
    void STAT_WhileLoop();
    void STAT_RepeatLoop();
    void STAT_ForLoop();
    void STAT_Return();

    Definition *EXP_Expression(int priority, bool *lvalue = nullptr);
    Definition *EXP_FieldQuery(Definition *e, bool lvalue);
    Definition *EXP_ShortCircuit(Definition *e, int n);
    Definition *EXP_Term();
    Definition *EXP_VarValue();
    Definition *EXP_FunctionCall(Definition *func);
    Definition *EXP_Literal();

    Definition *DeclareDef(Type *type, char *name, Scope *scope);
    Definition *FindDef(Type *type, char *name, Scope *scope);

    void        StoreLiteral(int ofs);
    Definition *FindLiteral();

    Definition *NewTemporary(Type *type);
    void        FreeTemporaries();

    Definition *NewGlobal(Type *type);
    Definition *NewLocal(Type *type);

    char *ParseName();
    Type *ParseType();
    Type *FindType(Type *type);

    int EmitCode(int16_t op, int a = 0, int b = 0, int c = 0);
    int EmitMove(Type *type, int a, int b);

    void LEX_Next();
    void LEX_Whitespace();
    void LEX_NewLine();

    bool LEX_Check(const char *str);
    void LEX_Expect(const char *str);

    void  LEX_String();
    float LEX_Number();
    void  LEX_Vector();
    void  LEX_Name();
    void  LEX_Punctuation();

    void CompileError(const char *error, ...);

    // c_execute.cc
  private:
    void DoExecute(int func_id);

    void EnterNative(int func, int argc);
    void EnterFunction(int func);
    void LeaveFunction();

    int GetNativeFunc(const char *name, const char *module);
    int InternaliseString(const char *new_s);

    int STR_Concat(const char *s1, const char *s2);
    int STR_ConcatFloat(const char *s, double f);
    int STR_ConcatVector(const char *s, double *v);

    void RunError(const char *error, ...);

    void        StackTrace();
    void        PrintStatement(Function *f, int s);
    const char *RegString(Statement *st, int who);

    void ASM_DumpFunction(Function *f);
    void ASM_DumpAll();
};

} // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
