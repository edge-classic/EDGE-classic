//----------------------------------------------------------------------
//  COAL LOCAL DEFS
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

#ifndef __COAL_LOCAL_DEFS_H__
#define __COAL_LOCAL_DEFS_H__

#include "c_memory.h"

typedef unsigned char byte;

typedef int	func_t;
typedef int	string_t;


#define	MAX_NAME	64

#define	MAX_PARMS	16


typedef enum
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
	ev_pointer
}
etype_t;


typedef struct
{
	short op;
	short line;  // offset from start of function

	int a, b, c;
}
statement_t;


enum
{
	OP_NULL = 0,

	OP_CALL,
	OP_RET,

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


typedef struct
{
	const char *name;

	// where it was defined (last)
	const char *source_file;
	int source_line;

	int		return_size;

	int		parm_num;
	short	parm_ofs[MAX_PARMS];
	short	parm_size[MAX_PARMS];

	int		locals_ofs;
	int		locals_size;
	int		locals_end;

	int		first_statement;	// negative numbers are builtins
	int		last_statement;
}
function_t;

// offset in global data block (if > 0)
// when < 0, it is offset into local stack frame
typedef int	gofs_t;



//=============================================================================


#define OFS_NULL		0
#define OFS_RETURN		1
#define OFS_DEFAULT		4


#define REF_OP(ofs)      ((statement_t *)op_mem.deref(ofs))
#define REF_GLOBAL(ofs)  ((double *)global_mem.deref(ofs))
#define REF_STRING(ofs)  ((ofs)==0 ? "" :   \
                          (ofs) < 0 ? (char *)temp_strings.deref(-(1+(ofs))) :   \
                          (char *)string_mem.deref(ofs))

#define	G_FLOAT(ofs)    (* REF_GLOBAL(ofs))
#define	G_VECTOR(ofs)   REF_GLOBAL(ofs)
#define	G_STRING(ofs)   REF_STRING((int) G_FLOAT(ofs))


class parse_error_x
{
public:
	int foo;

	 parse_error_x() { }
	~parse_error_x() { }
};



class exec_error_x
{
public:
	int foo;

	 exec_error_x() { }
	~exec_error_x() { }
};


typedef struct
{
	const char *name;
	native_func_t func;
}
reg_native_func_t;


//============================================================//

#include "c_compile.h"
#include "c_execute.h"


class real_vm_c : public vm_c
{
public:
	/* API functions */

	 real_vm_c();
	~real_vm_c();

	void SetPrinter(print_func_t func);

	void AddNativeModule(const char *name);
	void AddNativeFunction(const char *name, native_func_t func);

	bool CompileFile(char *buffer, const char *filename);
	void ShowStats();

	void SetAsmDump(bool enable);
	void SetTrace  (bool enable);

	int FindFunction(const char *name);
	int FindVariable(const char *name);

	int Execute(int func_id);

	double     * AccessParam(int p);
	const char * AccessParamString(int p);

	void ReturnFloat(double f);
	void ReturnVector(double *v);
	void ReturnString(const char *s, int len = -1);

private:
	print_func_t printer;

	bmaster_c op_mem;
	bmaster_c global_mem;
	bmaster_c string_mem;
	bmaster_c temp_strings;

	std::vector< function_t* > functions;
	std::vector< reg_native_func_t* > native_funcs;

	compiling_c comp;
	execution_c exec;

	// c_compile.cc
private:
	void GLOB_Globals();
	void GLOB_Module();
	void GLOB_Constant();
	void GLOB_Variable();
	void GLOB_Function();
	int  GLOB_FunctionBody(def_t *func_def, type_t *type, const char *func_name);

	void STAT_Statement(bool allow_def);
	void STAT_Assignment(def_t *e);
	void STAT_If_Else();
	void STAT_Assert();
	void STAT_WhileLoop();
	void STAT_RepeatLoop();
	void STAT_ForLoop();
	void STAT_Return();

	def_t * EXP_Expression(int priority, bool *lvalue = NULL);
	def_t * EXP_FieldQuery(def_t *e, bool lvalue);
	def_t * EXP_ShortCircuit(def_t *e, int n);
	def_t * EXP_Term();
	def_t * EXP_VarValue();
	def_t * EXP_FunctionCall(def_t *func);
	def_t * EXP_Literal();

	def_t * DeclareDef(type_t *type, char *name, scope_c *scope);
	def_t * FindDef   (type_t *type, char *name, scope_c *scope);

	void StoreLiteral(int ofs);
	def_t * FindLiteral();

	def_t * NewTemporary(type_t *type);
	void FreeTemporaries();

	def_t * NewGlobal(type_t *type);
	def_t * NewLocal(type_t *type);

	char *   ParseName();
	type_t * ParseType();
	type_t * FindType(type_t *type);

	int EmitCode(short op, int a=0, int b=0, int c=0);
	int EmitMove(type_t *type, int a, int b);


	void LEX_Next();
	void LEX_Whitespace();
	void LEX_NewLine();
	void LEX_SkipPastError();

	bool LEX_Check (const char *str);
	void LEX_Expect(const char *str);

	void LEX_String();
	float LEX_Number();
	void LEX_Vector();
	void LEX_Name();
	void LEX_Punctuation();

	void CompileError(const char *error, ...);


	// c_execute.cc
private:
	void DoExecute(int func_id);

	void EnterNative  (int func, int argc);
	void EnterFunction(int func);
	void LeaveFunction();

	int GetNativeFunc(const char *name, const char *module);
	int	InternaliseString(const char *new_s);

	int STR_Concat(const char * s1, const char * s2);
	int STR_ConcatFloat (const char * s, double f);
	int STR_ConcatVector(const char * s, double *v);

	void RunError(const char *error, ...);

	void StackTrace();
	void PrintStatement(function_t *f, int s);
	const char * RegString(statement_t *st, int who);

	void ASM_DumpFunction(function_t *f);
	void ASM_DumpAll();

	static void default_printer(const char *msg, ...);
	static void default_aborter(const char *msg, ...);
};


#endif /* __COAL_LOCAL_DEFS_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
