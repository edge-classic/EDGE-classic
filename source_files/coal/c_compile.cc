//----------------------------------------------------------------------
//  COAL COMPILER
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

// #include <sys/signal.h>

#include <vector>

#include "coal.h"


namespace coal
{

#include "c_local.h"
#include "c_compile.h"

#define MAX_ERRORS  200


compiling_c::compiling_c() :
	source_file(NULL), source_line(0),
	asm_dump(false),
	parse_p(NULL), line_start(NULL),
	error_count(0),
	global_scope(), all_modules(),
	all_types(), all_literals(),
	temporaries()
{ }

compiling_c::~compiling_c()
{ }



const char * punctuation[] =
// longer symbols must be before a shorter partial match
{"&&", "||", "<=", ">=","==", "!=", "++", "--", "...", "..",
 ":", ";", ",", "!",
 "*", "/", "%", "^","(", ")", "-", "+", "=",
 "[", "]", "{", "}", ".", "<", ">", "#", "&", "|",
 NULL
};

// simple types.  function types are dynamically allocated
static type_t	type_void = {ev_void};
static type_t	type_string = {ev_string};
static type_t	type_float = {ev_float};
static type_t	type_vector = {ev_vector};
static type_t	type_function = {ev_function, &type_void};
static type_t	type_module = {ev_module};

static int		type_size[8] = {1,1,1,3,1,1,1,1};


// definition used for void return functions
static def_t def_void = {&type_void, "VOID_SPACE", 0};

//
//  OPERATOR TABLE
//
typedef struct
{
	const char *name;
	int op;  // OP_XXX
	int priority;
	type_t *type_a, *type_b, *type_c;
}
opcode_t;

static opcode_t all_operators[] =
{
	{"!", OP_NOT_F, -1, &type_float, &type_void, &type_float},
	{"!", OP_NOT_V, -1, &type_vector, &type_void, &type_float},
	{"!", OP_NOT_S, -1, &type_string, &type_void, &type_float},
	{"!", OP_NOT_FNC, -1, &type_function, &type_void, &type_float},

	/* priority 1 is for function calls */

	{"^", OP_POWER_F, 2, &type_float, &type_float, &type_float},

	{"*", OP_MUL_F,  2, &type_float, &type_float, &type_float},
	{"*", OP_MUL_V,  2, &type_vector, &type_vector, &type_float},
	{"*", OP_MUL_FV, 2, &type_float, &type_vector, &type_vector},
	{"*", OP_MUL_VF, 2, &type_vector, &type_float, &type_vector},

	{"/", OP_DIV_F, 2, &type_float, &type_float, &type_float},
	{"/", OP_DIV_V, 2, &type_vector, &type_float, &type_vector},
	{"%", OP_MOD_F, 2, &type_float, &type_float, &type_float},

	{"+", OP_ADD_F,  3, &type_float, &type_float, &type_float},
	{"+", OP_ADD_V,  3, &type_vector, &type_vector, &type_vector},
	{"+", OP_ADD_S,  3, &type_string, &type_string, &type_string},
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
	{"<",  OP_LT, 4, &type_float, &type_float, &type_float},
	{">",  OP_GT, 4, &type_float, &type_float, &type_float},

	{"&&", OP_AND, 5, &type_float, &type_float, &type_float},
	{"||", OP_OR,  5, &type_float, &type_float, &type_float},

	{"&", OP_BITAND, 2, &type_float, &type_float, &type_float},
	{"|", OP_BITOR,  2, &type_float, &type_float, &type_float},

	{NULL}  // end of list
};

#define	TOP_PRIORITY	6
#define	NOT_PRIORITY	1


void real_vm_c::LEX_NewLine()
{
	// Called when *comp.parse_p == '\n'

	comp.source_line++;

	comp.line_start = comp.parse_p + 1;
	comp.fol_level = 0;
}


//
// Aborts the current function parse.
// The given message should have a trailing \n
//
void real_vm_c::CompileError(const char *error, ...)
{
	va_list argptr;
	char	buffer[1024];

	va_start(argptr,error);
	vsprintf(buffer,error,argptr);
	va_end(argptr);

	printer("%s:%i: %s", comp.source_file, comp.source_line, buffer);

//  raise(11);
	throw parse_error_x();
}


void real_vm_c::LEX_String()
{
	// Parses a quoted string

	comp.parse_p++;

	int len = 0;

	for (;;)
	{
		int c = *comp.parse_p++;
		if (!c || c=='\n')
			CompileError("unfinished string\n");

		if (c=='\\') // escape char
		{
			c = *comp.parse_p++;
			if (!c || !isprint(c))
				CompileError("bad escape in string\n");

			if (c == 'n')
				c = '\n';
			else if (c == '"')
				c = '"';
			else
				CompileError("unknown escape char: %c\n", c);
		}
		else if (c=='\"')
		{
			comp.token_buf[len] = 0;
			comp.token_type = tt_literal;
			comp.literal_type = &type_string;
			strcpy(comp.literal_buf, comp.token_buf);
			return;
		}

		comp.token_buf[len++] = c;
	}
}


float real_vm_c::LEX_Number()
{
	int len = 0;
	int c = *comp.parse_p;

	do
	{
		comp.token_buf[len++] = c;

		comp.parse_p++;
		c = *comp.parse_p;
	}
	while ((c >= '0' && c<= '9') || c == '.');

	comp.token_buf[len] = 0;

	return atof(comp.token_buf);
}


void real_vm_c::LEX_Vector()
{
	// Parses a single quoted vector

	comp.parse_p++;
	comp.token_type = tt_literal;
	comp.literal_type = &type_vector;

	for (int i=0 ; i<3 ; i++)
	{
		// FIXME: check for digits etc!

		comp.literal_value[i] = LEX_Number();

		while (isspace(*comp.parse_p) && *comp.parse_p != '\n')
			comp.parse_p++;
	}

	if (*comp.parse_p != '\'')
		CompileError("bad vector\n");

	comp.parse_p++;
}


void real_vm_c::LEX_Name()
{
	// Parses an identifier

	int len = 0;
	int c = *comp.parse_p;

	do
	{
		comp.token_buf[len++] = c;

		comp.parse_p++;
		c = *comp.parse_p;
	}
	while (isalnum(c) || c == '_');

	comp.token_buf[len] = 0;
	comp.token_type = tt_name;
}


void real_vm_c::LEX_Punctuation()
{
	comp.token_type = tt_punct;

	const char *p;

	char ch = *comp.parse_p;

	for (int i=0 ; (p = punctuation[i]) != NULL ; i++)
	{
		int len = strlen(p);

		if (strncmp(p, comp.parse_p, len) == 0)
		{
			// found it
			strcpy(comp.token_buf, p);

			if (p[0] == '{')
				comp.bracelevel++;
			else if (p[0] == '}')
				comp.bracelevel--;

			comp.parse_p += len;
			return;
		}
	}

	CompileError("unknown punctuation: %c\n", ch);
}


void real_vm_c::LEX_Whitespace(void)
{
	int c;

	for (;;)
	{
		// skip whitespace
		while ( (c = *comp.parse_p) <= ' ')
		{
			if (c == 0) // end of file?
				return;

			if (c=='\n')
				LEX_NewLine();

			comp.parse_p++;
		}

		// skip // comments
		if (c=='/' && comp.parse_p[1] == '/')
		{
			while (*comp.parse_p && *comp.parse_p != '\n')
				comp.parse_p++;

			LEX_NewLine();

			comp.parse_p++;
			continue;
		}

		// skip /* */ comments
		if (c=='/' && comp.parse_p[1] == '*')
		{
			do
			{
				comp.parse_p++;
				
				if (comp.parse_p[0]=='\n')
					LEX_NewLine();

				if (comp.parse_p[1] == 0)
					return;

			} while (comp.parse_p[-1] != '*' || comp.parse_p[0] != '/');

			comp.parse_p++;
			continue;
		}

		break;		// a real character has been found
	}
}



//
// Parse the next token in the file.
// Sets token_type and token_buf, and possibly the literal_xxx fields
//
void real_vm_c::LEX_Next()
{
	assert(comp.parse_p);

	LEX_Whitespace();

	comp.token_buf[0] = 0;
	comp.token_is_first = (comp.fol_level == 0);

	comp.fol_level++;

	int c = *comp.parse_p;

	if (!c)
	{
		comp.token_type = tt_eof;
		strcpy(comp.token_buf, "(EOF)");
		return;
	}

// handle quoted strings as a unit
	if (c == '\"')
	{
		LEX_String();
		return;
	}

// handle quoted vectors as a unit
	if (c == '\'')
	{
		LEX_Vector();
		return;
	}

// if the first character is a valid identifier, parse until a non-id
// character is reached
	if ( (c >= '0' && c <= '9') || ( c=='-' && comp.parse_p[1]>='0' && comp.parse_p[1] <='9') )
	{
		comp.token_type = tt_literal;
		comp.literal_type = &type_float;
		comp.literal_value[0] = LEX_Number();
		return;
	}

	if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' )
	{
		LEX_Name();
		return;
	}

// parse symbol strings until a non-symbol is found
	LEX_Punctuation();
}


//
// Issues an error if the current token isn't what we want.
// On success, automatically skips to the next token.
//
void real_vm_c::LEX_Expect(const char *str)
{
	if (strcmp(comp.token_buf, str) != 0)
		CompileError("expected %s got %s\n", str, comp.token_buf);

	LEX_Next();
}


//
// Checks that the current token matches what we want
// (which can be a keyword or symbol).
//
// Returns true on a match (skipping to the next token),
// otherwise returns false and does nothing.
//
bool real_vm_c::LEX_Check(const char *str)
{
	if (strcmp(comp.token_buf, str) != 0)
		return false;

	LEX_Next();

	return true;
}


//
// ERROR RECOVERY
//
// This is very simple, we just jump to the end of the line.
// The error may have occured inside a string, making checks for
// other stuff (like semicolons and comments) unreliable.
//
// We cannot use LEX_Next() here because it can throw another
// error exception.
//
void real_vm_c::LEX_SkipPastError()
{
	for (; *comp.parse_p && *comp.parse_p != '\n'; comp.parse_p++)
	{
#if 0
		if (*parse_p == ';' || *parse_p == '}' ||
			(parse_p[0] == '/' && parse_p[1] == '/') ||
			(parse_p[0] == '/' && parse_p[1] == '*'))
			break;
#endif
	}

	comp.token_type = tt_error;
	comp.token_buf[0] = 0;
	comp.token_is_first = false;
}


//
// Checks to see if the current token is a valid name
//
char * real_vm_c::ParseName()
{
	static char	ident[MAX_NAME];

	if (comp.token_type != tt_name)
		CompileError("expected identifier, got %s\n", comp.token_buf);

	if (strlen(comp.token_buf) >= MAX_NAME-1)
		CompileError("identifier too long\n");

	strcpy(ident, comp.token_buf);

	LEX_Next();

	return ident;
}


//=============================================================================


//
// Returns a preexisting complex type that matches the parm, or allocates
// a new one and copies it out.
//
type_t * real_vm_c::FindType(type_t *type)
{
	for (int k = 0; k < (int)comp.all_types.size(); k++)
	{
		type_t *check = comp.all_types[k];

		if (check->type != type->type
			|| check->aux_type != type->aux_type
			|| check->parm_num != type->parm_num)
			continue;

		int i;
		for (i=0 ; i< type->parm_num ; i++)
			if (check->parm_types[i] != type->parm_types[i])
				break;

		if (i == type->parm_num)
			return check;
	}

	// allocate a new one
	type_t *t_new = new type_t;
	*t_new = *type;

	comp.all_types.push_back(t_new);

	return t_new;
}


//
// Parses a variable type, including field and functions types
//
type_t * real_vm_c::ParseType()
{
	type_t	t_new;
	type_t	*type;
	char	*name;

	if (!strcmp(comp.token_buf, "float") )
		type = &type_float;
	else if (!strcmp(comp.token_buf, "vector") )
		type = &type_vector;
	else if (!strcmp(comp.token_buf, "float") )
		type = &type_float;
//	else if (!strcmp(comp.token_buf, "entity") )
//		type = &type_entity;
	else if (!strcmp(comp.token_buf, "string") )
		type = &type_string;
	else if (!strcmp(comp.token_buf, "void") )
		type = &type_void;
	else
	{
		CompileError("unknown type: %s\n", comp.token_buf);
		type = &type_float;	// shut up compiler warning
	}
	LEX_Next();

	if (! LEX_Check("("))
		return type;

// function type
	memset(&t_new, 0, sizeof(t_new));
	t_new.type = ev_function;
	t_new.aux_type = type;	// return type
	t_new.parm_num = 0;

	if (! LEX_Check(")"))
	{
		if (LEX_Check("..."))
			t_new.parm_num = -1;	// variable args
		else
			do
			{
				type = ParseType();
				name = ParseName();
				
				strcpy(comp.parm_names[t_new.parm_num], name);

				t_new.parm_types[t_new.parm_num] = type;
				t_new.parm_num++;
			} while (LEX_Check(","));

		LEX_Expect(")");
	}

	return FindType(&t_new);
}


//===========================================================================


int real_vm_c::EmitCode(short op, int a, int b, int c)
{
	// TODO: if last statement was OP_NULL, overwrite it instead of
	//       creating a new one.

	int ofs = op_mem.alloc((int)sizeof(statement_t));

	statement_t *st = REF_OP(ofs);

	st->op = op;
	st->line = comp.source_line - comp.function_line;

	st->a = a;
	st->b = b;
	st->c = c;

	comp.last_statement = ofs;

	return ofs;
}


int real_vm_c::EmitMove(type_t *type, int a, int b)
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


def_t * real_vm_c::NewGlobal(type_t *type)
{
	int tsize = type_size[type->type];

	def_t * var = new def_t;
	memset(var, 0, sizeof(def_t));

	var->ofs  = global_mem.alloc(tsize * sizeof(double));
	var->type = type;

	// clear it 
	memset(global_mem.deref(var->ofs), 0, tsize * sizeof(double));

	return var;
}


def_t * real_vm_c::NewLocal(type_t *type)
{
	def_t * var = new def_t;
	memset(var, 0, sizeof(def_t));

	var->ofs = -(comp.locals_end+1);
	var->type = type;

	comp.locals_end += type_size[type->type];

	return var;
}


def_t * real_vm_c::NewTemporary(type_t *type)
{
	def_t *var;

	std::vector<def_t *>::iterator TI;

	for (TI = comp.temporaries.begin(); TI != comp.temporaries.end(); TI++)
	{
		var = *TI;

		// make sure it fits
		if (type_size[var->type->type] < type_size[type->type])
			continue;

		if (! (var->flags & DF_FreeTemp))
			continue;

		// found a match, so re-use it!
		var->flags &= ~DF_FreeTemp;
		var->type = type;

		return var;
	}

	var = NewLocal(type);

	var->flags |= DF_Temporary;
	comp.temporaries.push_back(var);

	return var;
}


void real_vm_c::FreeTemporaries()
{
	std::vector<def_t *>::iterator TI;

	for (TI = comp.temporaries.begin(); TI != comp.temporaries.end(); TI++)
	{
		def_t *tvar = *TI;

		tvar->flags |= DF_FreeTemp;
	}
}


def_t * real_vm_c::FindLiteral()
{
	// check for a constant with the same value
	for (int i = 0; i < (int)comp.all_literals.size(); i++)
	{
		def_t *cn = comp.all_literals[i];

		if (cn->type != comp.literal_type)
			continue;

		if (comp.literal_type == &type_string)
		{
			if (strcmp(G_STRING(cn->ofs), comp.literal_buf) == 0)
				return cn;
		}
		else if (comp.literal_type == &type_float)
		{
			if (G_FLOAT(cn->ofs) == comp.literal_value[0])
				return cn;
		}
		else if	(comp.literal_type == &type_vector)
		{
			if (G_FLOAT(cn->ofs)   == comp.literal_value[0] &&
				G_FLOAT(cn->ofs+1) == comp.literal_value[1] &&
				G_FLOAT(cn->ofs+2) == comp.literal_value[2])
			{
				return cn;
			}
		}
	}

	return NULL;  // not found
}


void real_vm_c::StoreLiteral(int ofs)
{
	double *p = REF_GLOBAL(ofs);

	if (comp.literal_type == &type_string)
	{
		*p = (double) InternaliseString(comp.literal_buf);
	}
	else if (comp.literal_type == &type_vector)
	{
		p[0] = comp.literal_value[0];
		p[1] = comp.literal_value[1];
		p[2] = comp.literal_value[2];
	}
	else
	{
		*p = comp.literal_value[0];
	}
}


def_t * real_vm_c::EXP_Literal()
{
	// Looks for a preexisting constant
	def_t *cn = FindLiteral();

	if (! cn)
	{
		// allocate a new one
		cn = NewGlobal(comp.literal_type);

		cn->name = "CONSTANT VALUE";

		cn->flags |= DF_Constant;
		cn->scope = NULL;   // literals are "scope-less"

		// copy the literal to the global area
		StoreLiteral(cn->ofs);

		comp.all_literals.push_back(cn);
	}

	LEX_Next();
	return cn;
}


def_t * real_vm_c::EXP_FunctionCall(def_t *func)
{
	type_t * t = func->type;

	if (t->type != ev_function)
		CompileError("not a function before ()\n");
	
//??	function_t *df = &functions[func->ofs];

	
	// evaluate all parameters
	def_t * exprs[MAX_PARMS];

	int arg = 0;

	if (! LEX_Check(")"))
	{
		do
		{
			if (arg >= t->parm_num)
				CompileError("too many parameters (expected %d)\n", t->parm_num);

			assert(arg < MAX_PARMS);

			def_t * e = EXP_Expression(TOP_PRIORITY);

			if (e->type != t->parm_types[arg])
				CompileError("type mismatch on parameter %i\n", arg+1);

			assert (e->type->type != ev_void);

			exprs[arg++] = e;
		}
		while (LEX_Check(","));

		LEX_Expect(")");

		if (arg != t->parm_num)
			CompileError("too few parameters (needed %d)\n", t->parm_num);
	}


	def_t *result = NULL;

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
		EmitMove(result->type, OFS_RETURN*8, result->ofs);
		return result;
	}

	return &def_void;
}


void real_vm_c::STAT_Return(void)
{
	def_t *func_def = comp.scope->def;

	if (comp.token_is_first || comp.token_buf[0] == '}' || LEX_Check(";"))
	{
		if (func_def->type->aux_type->type != ev_void)
			CompileError("missing value for return\n");

		EmitCode(OP_RET);
		return;
	}

	def_t * e = EXP_Expression(TOP_PRIORITY);

	if (func_def->type->aux_type->type == ev_void)
		CompileError("return with value in void function\n");

	if (func_def->type->aux_type != e->type)
		CompileError("type mismatch for return\n");

	EmitMove(func_def->type->aux_type, e->ofs, OFS_RETURN*8);

	EmitCode(OP_RET);

	// -AJA- optional semicolons
	if (! (comp.token_is_first || comp.token_buf[0] == '}'))
		LEX_Expect(";");
}


def_t * real_vm_c::FindDef(type_t *type, char *name, scope_c *scope)
{
	for (def_t * def = scope->names ; def ; def=def->next)
	{
		if (strcmp(def->name, name) != 0)
			continue;

		if (type && def->type != type)
			CompileError("type mismatch on redeclaration of %s\n", name);

		return def;
	}

	return NULL;
}


def_t * real_vm_c::DeclareDef(type_t *type, char *name, scope_c *scope)
{
	// A new def will be allocated if it can't be found

	assert(type);

	def_t *def = FindDef(type, name, scope);
	if (def)
		return def;

	// allocate a new def

	if (scope->kind == 'f')
		def = NewLocal(type);
	else
		def = NewGlobal(type);

	def->name = strdup(name);

	// link into list
	scope->push_back(def);

	return def;
}


def_t * real_vm_c::EXP_VarValue()
{
	char *name = ParseName();

	// look through the defs
	scope_c *scope = comp.scope;

	for (;;)
	{
		def_t *d = FindDef(NULL, name, scope);
		if (d)
			return d;

		if (scope->kind == 'g')
			CompileError("unknown identifier: %s\n", name);

		// move to outer scope
		scope = scope->def->scope;
	}
}


def_t * real_vm_c::EXP_Term()
{
	// if the token is a literal, allocate a constant for it
	if (comp.token_type == tt_literal)
		return EXP_Literal();

	if (comp.token_type == tt_name)
		return EXP_VarValue();

	if (LEX_Check("("))
	{
		def_t *e = EXP_Expression(TOP_PRIORITY);
		LEX_Expect(")");
		return e;
	}

	// unary operator?

	for (int n = 0; all_operators[n].name; n++)
	{
		opcode_t *op = &all_operators[n];

		if (op->priority != -1)
			continue;

		if (! LEX_Check(op->name))
			continue;

		def_t * e = EXP_Expression(NOT_PRIORITY);

		for (int i = 0; i == 0 || strcmp(op->name, op[i].name) == 0; i++)
		{
			if (op[i].type_a->type != e->type->type)
				continue;

			def_t *result = NewTemporary(op[i].type_c);

			EmitCode(op[i].op, e->ofs, 0, result->ofs);

			return result;
		}

		CompileError("type mismatch for %s\n", op->name);
		break;
	}

	CompileError("expected value, got %s\n", comp.token_buf);
	return NULL; /* NOT REACHED */
}


def_t * real_vm_c::EXP_ShortCircuit(def_t *e, int n)
{
	opcode_t *op = &all_operators[n];

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

	def_t *result = NewTemporary(op->type_c);

	EmitCode(OP_MOVE_F, e->ofs, result->ofs);

	int patch;

	if (op->name[0] == '&')
		patch = EmitCode(OP_IFNOT, result->ofs);
	else
		patch = EmitCode(OP_IF, result->ofs);

	def_t *e2 = EXP_Expression(op->priority - 1);
	if (e2->type->type != ev_float)
		CompileError("type mismatch for %s\n", op->name);

	EmitCode(OP_MOVE_F, e2->ofs, result->ofs);

	REF_OP(patch)->b = EmitCode(OP_NULL);

	return result;
}


def_t * real_vm_c::EXP_FieldQuery(def_t *e, bool lvalue)
{
	char *name = ParseName();

	if (e->type->type == ev_vector)
		CompileError("vector x/y/z access not yet implemented\n");

	if (e->type->type == ev_module)
	{
		scope_c * mod = comp.all_modules[e->ofs];

		def_t *d = FindDef(NULL, name, mod);
		if (! d)
			CompileError("unknown identifier: %s.%s\n", e->name, name);

		return d;
	}

	CompileError("type mismatch with . operator\n");
	return NULL; // NOT REACHED
}


def_t * real_vm_c::EXP_Expression(int priority, bool *lvalue)
{
	if (priority == 0)
		return EXP_Term();

	def_t * e = EXP_Expression(priority-1, lvalue);

	// loop through a sequence of same-priority operators
	bool found;

	do
	{
		found = false;

		while (priority == 1 && LEX_Check("."))
			e = EXP_FieldQuery(e, lvalue);

		if (priority == 1 && LEX_Check("("))
		{
			if (lvalue)
				*lvalue = false;
			return EXP_FunctionCall(e);
		}

		if (lvalue)
			return e;

		for (int n = 0; all_operators[n].name; n++)
		{
			opcode_t *op = &all_operators[n];

			if (op->priority != priority)
				continue;

			if (! LEX_Check(op->name))
				continue;

			found = true;

			if (strcmp(op->name, "&&") == 0 ||
			    strcmp(op->name, "||") == 0)
			{
				e = EXP_ShortCircuit(e, n);
				break;
			}

			def_t * e2 = EXP_Expression(priority-1);

			// type check

			etype_t type_a = e->type->type;
			etype_t type_b = e2->type->type;
			etype_t type_c = ev_void;

			opcode_t * oldop = op;

			while (type_a != op->type_a->type ||
				   type_b != op->type_b->type ||
				   (type_c != ev_void && type_c != op->type_c->type) )
			{
				op++;
				if (!op->name || strcmp(op->name , oldop->name) != 0)
					CompileError("type mismatch for %s\n", oldop->name);
			}

			if (type_a == ev_pointer && type_b != e->type->aux_type->type)
				CompileError("type mismatch for %s\n", op->name);

			def_t *result = NewTemporary(op->type_c);

			EmitCode(op->op, e->ofs, e2->ofs, result->ofs);

			e = result;
			break;
		}
	}
	while (found);

	return e;
}


void real_vm_c::STAT_If_Else()
{
	LEX_Expect("(");
	def_t * e = EXP_Expression(TOP_PRIORITY);
	LEX_Expect(")");

	int patch = EmitCode(OP_IFNOT, e->ofs);

	STAT_Statement(false);
	FreeTemporaries();

	if (LEX_Check("else"))
	{
		// use GOTO to skip over the else statements
		int patch2 = EmitCode(OP_GOTO);

		REF_OP(patch)->b = EmitCode(OP_NULL);

		patch = patch2;

		STAT_Statement(false);
		FreeTemporaries();
	}

	REF_OP(patch)->b = EmitCode(OP_NULL);
}


void real_vm_c::STAT_Assert()
{
	// TODO: only internalise the filename ONCE
	int file_str = InternaliseString(comp.source_file);
	int line_num = comp.source_line;

	LEX_Expect("(");
	def_t * e = EXP_Expression(TOP_PRIORITY);
	LEX_Expect(")");

	int patch = EmitCode(OP_IF, e->ofs);

	EmitCode(OP_ERROR, file_str, line_num);
	FreeTemporaries();

	REF_OP(patch)->b = EmitCode(OP_NULL);
}


void real_vm_c::STAT_WhileLoop()
{
	int begin = EmitCode(OP_NULL);

	LEX_Expect("(");
	def_t * e = EXP_Expression(TOP_PRIORITY);
	LEX_Expect(")");

	int patch = EmitCode(OP_IFNOT, e->ofs);

	STAT_Statement(false);
	FreeTemporaries();

	EmitCode(OP_GOTO, 0, begin);

	REF_OP(patch)->b = EmitCode(OP_NULL);
}


void real_vm_c::STAT_RepeatLoop()
{
	int begin = EmitCode(OP_NULL);

	STAT_Statement(false);
	FreeTemporaries();

	LEX_Expect("until");
	LEX_Expect("(");

	def_t * e = EXP_Expression(TOP_PRIORITY);

	EmitCode(OP_IFNOT, e->ofs, begin);

	LEX_Expect(")");

	// -AJA- optional semicolons
	if (! (comp.token_is_first || comp.token_buf[0] == '}'))
		LEX_Expect(";");
}


void real_vm_c::STAT_ForLoop()
{
	LEX_Expect("(");

	char * var_name = strdup(ParseName());

	def_t * var = FindDef(&type_float, var_name, comp.scope);

	if (! var || (var->flags & DF_Constant))
		CompileError("unknown variable in for loop: %s\n", var_name);

	LEX_Expect("=");

	def_t * e1 = EXP_Expression(TOP_PRIORITY);
	if (e1->type != var->type)
		CompileError("type mismatch in for loop\n");

	// assign first value to the variable
	EmitCode(OP_MOVE_F, e1->ofs, var->ofs);

	LEX_Expect(",");

	def_t * e2 = EXP_Expression(TOP_PRIORITY);
	if (e2->type != var->type)
		CompileError("type mismatch in for loop\n");

	// create local to contain second value
	def_t * target = NewLocal(&type_float);
	EmitCode(OP_MOVE_F, e2->ofs, target->ofs);

	LEX_Expect(")");

	def_t * cond_temp = NewTemporary(&type_float);

	int begin = EmitCode(OP_LE, var->ofs, target->ofs, cond_temp->ofs);
	int patch = EmitCode(OP_IFNOT, cond_temp->ofs);

	STAT_Statement(false);
	FreeTemporaries();

	// increment the variable
	EmitCode(OP_INC, var->ofs, 0, var->ofs);
	EmitCode(OP_GOTO, 0, begin);

	REF_OP(patch)->b = EmitCode(OP_NULL);
}


void real_vm_c::STAT_Assignment(def_t *e)
{
	if (e->flags & DF_Constant)
		CompileError("assignment to a constant\n");

	def_t *e2 = EXP_Expression(TOP_PRIORITY);

	if (e2->type != e->type)
		CompileError("type mismatch in assignment\n");

	EmitMove(e->type, e2->ofs, e->ofs);
}


void real_vm_c::STAT_Statement(bool allow_def)
{
	if (allow_def && LEX_Check("var"))
	{
		GLOB_Variable();
		return;
	}

	if (allow_def && LEX_Check("function"))
	{
		CompileError("functions must be global\n");
		return;
	}

	if (allow_def && LEX_Check("constant"))
	{
		CompileError("constants must be global\n");
		return;
	}

	if (LEX_Check("{"))
	{
		do {
			STAT_Statement(true);
			FreeTemporaries();
		}
		while (! LEX_Check("}"));

		return;
	}

	if (LEX_Check("return"))
	{
		STAT_Return();
		return;
	}

	if (LEX_Check("if"))
	{
		STAT_If_Else();
		return;
	}

	if (LEX_Check("assert"))
	{
		STAT_Assert();
		return;
	}

	if (LEX_Check("while"))
	{
		STAT_WhileLoop();
		return;
	}

	if (LEX_Check("repeat"))
	{
		STAT_RepeatLoop();
		return;
	}

	if (LEX_Check("for"))
	{
		STAT_ForLoop();
		return;
	}

	bool lvalue = true;
	def_t * e = EXP_Expression(TOP_PRIORITY, &lvalue);

	// lvalue is false for a plain function call

	if (lvalue)
	{
		LEX_Expect("=");

		STAT_Assignment(e);
	}

	// -AJA- optional semicolons
	if (! (comp.token_is_first || comp.token_buf[0] == '}'))
		LEX_Expect(";");
}


int real_vm_c::GLOB_FunctionBody(def_t *func_def, type_t *type, const char *func_name)
{
	// Returns the first_statement value

	comp.temporaries.clear();

	comp.function_line = comp.source_line;

	//
	// check for native function definition
	//
	if (LEX_Check("native"))
	{
		const char *module = NULL;
		if (func_def->scope->kind == 'm')
			module = func_def->scope->def->name;

		int native = GetNativeFunc(func_name, module);

		if (native < 0)
		{
			// fix scope (must not stay in function scope)
			comp.scope = func_def->scope;

			CompileError("no such native function: %s%s%s\n",
				module ? module : "", module ? "." : "", func_name);
		}

		return -(native + 1);
	}

	//
	// create the parmeters as locals
	//
	def_t *defs[MAX_PARMS];

	for (int i=0 ; i < type->parm_num ; i++)
	{
		if (FindDef(type->parm_types[i], comp.parm_names[i], comp.scope))
			CompileError("parameter %s redeclared\n", comp.parm_names[i]);

		defs[i] = DeclareDef(type->parm_types[i], comp.parm_names[i], comp.scope);
	}

	int code = EmitCode(OP_NULL);

	//
	// parse regular statements
	//
	LEX_Expect("{");

	while (! LEX_Check("}"))
	{
		try
		{
			// handle a previous error
			if (comp.token_type == tt_error)
				LEX_Next();
			else
				STAT_Statement(true);
		}
		catch (parse_error_x err)
		{
			comp.error_count++;
			LEX_SkipPastError();
		}

		if (comp.token_type == tt_eof)
			CompileError("unfinished function body (hit EOF)\n");

		FreeTemporaries();
	}

	statement_t *last = REF_OP(comp.last_statement);

	if (last->op != OP_RET)
	{
		if (type->aux_type->type == ev_void)
			EmitCode(OP_RET);
		else
			CompileError("missing return at end of function %s\n", func_name);
	}

	return code;
}


void real_vm_c::GLOB_Function()
{
	char *func_name = strdup(ParseName());

	LEX_Expect("(");

	type_t t_new;

	memset(&t_new, 0, sizeof(t_new));
	t_new.type = ev_function;
	t_new.parm_num = 0;
	t_new.aux_type = &type_void;

	if (! LEX_Check(")"))
	{
		do
		{
			if (t_new.parm_num >= MAX_PARMS)
				CompileError("too many parameters (over %d)\n", MAX_PARMS);

			char *name = ParseName();

			strcpy(comp.parm_names[t_new.parm_num], name);

			// parameter type (defaults to float)
			if (LEX_Check(":"))
				t_new.parm_types[t_new.parm_num] = ParseType();
			else
				t_new.parm_types[t_new.parm_num] = &type_float;

			t_new.parm_num++;
		}
		while (LEX_Check(","));

		LEX_Expect(")");
	}

	// return type (defaults to void)
	if (LEX_Check(":"))
	{
		t_new.aux_type = ParseType();
	}

	type_t *func_type = FindType(&t_new);

	def_t *def = DeclareDef(func_type, func_name, comp.scope);

	assert(func_type->type == ev_function);


	LEX_Expect("=");

	// fill in the dfunction
	G_FLOAT(def->ofs) = (double)functions.size();

	function_t *df = new function_t;
	memset(df, 0, sizeof(function_t));

	functions.push_back(df);

	df->name = func_name;  // already strdup'd
	df->source_file = strdup(comp.source_file);
	df->source_line = comp.source_line;

	int stack_ofs = 0;

	df->return_size = type_size[def->type->aux_type->type];
	if (def->type->aux_type->type == ev_void) df->return_size = 0;
///---	stack_ofs += df->return_size;

	df->parm_num = def->type->parm_num;

	for (int i=0 ; i < df->parm_num ; i++)
	{
		df->parm_ofs[i]  = stack_ofs;
		df->parm_size[i] = type_size[def->type->parm_types[i]->type];
		if (def->type->parm_types[i]->type == ev_void) df->parm_size[i] = 0;

		stack_ofs += df->parm_size[i];
	}


	df->locals_ofs = stack_ofs;

	// parms are "realloc'd" by DeclareDef in FunctionBody
	comp.locals_end = 0;


	scope_c *OLD_scope = comp.scope;

	comp.scope = new scope_c;
	comp.scope->kind = 'f';
	comp.scope->def  = def;
	//  { 
		df->first_statement = GLOB_FunctionBody(def, func_type, func_name);
		df->last_statement  = comp.last_statement;
	//  }
	comp.scope = OLD_scope;

	df->locals_size = comp.locals_end - df->locals_ofs;
	df->locals_end  = comp.locals_end;

	if (comp.asm_dump)
		ASM_DumpFunction(df);

// debugprintf(stderr, "FUNCTION %s locals:%d\n", func_name, comp.locals_end);
}


void real_vm_c::GLOB_Variable()
{
	char *var_name = strdup(ParseName());

	type_t *type = &type_float;

	if (LEX_Check(":"))
	{
		type = ParseType();
	}

	def_t * def = DeclareDef(type, var_name, comp.scope);

	if (def->flags & DF_Constant)
		CompileError("%s previously defined as a constant\n");

	if (LEX_Check("="))
	{
		// global variables can only be initialised with a constant
		if (def->ofs > 0)
		{
			if (comp.token_type != tt_literal)
				CompileError("expected value for var, got %s\n", comp.token_buf);
			
			if (comp.literal_type->type != type->type)
				CompileError("type mismatch for %s\n", var_name);

			StoreLiteral(def->ofs);

			LEX_Next();
		}
		else  // local variables can take an expression
		 	  // it is equivalent to: var XX ; XX = ...
		{
			def_t *e2 = EXP_Expression(TOP_PRIORITY);

			if (e2->type != def->type)
				CompileError("type mismatch for %s\n", var_name);

			EmitMove(type, e2->ofs, def->ofs);
		}
	}
	else  // set to default
	{
		// global vars are already zero (via NewGlobal)
		if (def->ofs < 0)
			EmitMove(type, OFS_DEFAULT*8, def->ofs);
	}

	// -AJA- optional semicolons
	if (! (comp.token_is_first || comp.token_buf[0] == '}'))
		LEX_Expect(";");
}


void real_vm_c::GLOB_Constant()
{
	char *const_name = strdup(ParseName());

	LEX_Expect("=");

	if (comp.token_type != tt_literal)
		CompileError("expected value for constant, got %s\n", comp.token_buf);


	def_t * cn = DeclareDef(comp.literal_type, const_name, comp.scope);

	cn->flags |= DF_Constant;


	StoreLiteral(cn->ofs);

	LEX_Next();

	// -AJA- optional semicolons
	if (! (comp.token_is_first || comp.token_buf[0] == '}'))
		LEX_Expect(";");
}


void real_vm_c::GLOB_Module()
{
	if (comp.scope->kind != 'g')
		CompileError("modules cannot contain other modules\n");

	char *mod_name = strdup(ParseName());

	def_t * def = FindDef(&type_module, mod_name, comp.scope);

	scope_c * mod;

	if (def)
		mod = comp.all_modules[def->ofs];
	else
	{
		def = new def_t;
		memset(def, 0, sizeof(def_t));

		def->name = mod_name;
		def->type = &type_module;
		def->ofs  = (int)comp.all_modules.size();
		def->scope = comp.scope;

		comp.scope->push_back(def);

		mod = new scope_c;

		mod->kind = 'm';
		mod->def  = def;

		comp.all_modules.push_back(mod);
	}

	scope_c *OLD_scope = comp.scope;

	comp.scope = mod;

	LEX_Expect("{");

	while (! LEX_Check("}"))
	{
		try
		{
			// handle a previous error
			if (comp.token_type == tt_error)
				LEX_Next();
			else
				GLOB_Globals();
		}
		catch (parse_error_x err)
		{
			comp.error_count++;
			LEX_SkipPastError();
		}

		if (comp.token_type == tt_eof)
			CompileError("unfinished module (hit EOF)\n");
	}

	comp.scope = OLD_scope;
}


void real_vm_c::GLOB_Globals()
{
	if (LEX_Check("function"))
	{
		GLOB_Function();
		return;
	}

	if (LEX_Check("var"))
	{
		GLOB_Variable();
		return;
	}

	if (LEX_Check("constant"))
	{
		GLOB_Constant();
		return;
	}

	if (LEX_Check("module"))
	{
		GLOB_Module();
		return;
	}

	CompileError("expected global definition, got %s\n", comp.token_buf);
}


//
// compiles the NUL terminated text, adding definitions to the pr structure
//
bool real_vm_c::CompileFile(char *buffer, const char *filename)
{
	comp.source_file = filename;
	comp.source_line = 1;
	comp.function_line = 0;

	comp.parse_p = buffer;
	comp.line_start = buffer;
	comp.bracelevel = 0;
	comp.fol_level  = 0;

	LEX_Next();	// read first token

	while (comp.token_type != tt_eof &&
		   comp.error_count < MAX_ERRORS)
	{
		try
		{
			comp.scope = &comp.global_scope;

			// handle a previous error
			if (comp.token_type == tt_error)
				LEX_Next();
			else
				GLOB_Globals();
		}
		catch (parse_error_x err)
		{
			comp.error_count++;

			LEX_SkipPastError();
		}
	}

	comp.source_file = NULL;

	return (comp.error_count == 0);
}

void real_vm_c::ShowStats()
{
	printer("functions: %u\n", functions.size());
	printer("string memory: %d / %d\n",  string_mem.usedMemory(), string_mem.totalMemory());
	printer("instruction memory: %d / %d\n", op_mem.usedMemory(), op_mem.totalMemory());
	printer("globals memory: %d / %d\n", global_mem.usedMemory(), global_mem.totalMemory());
}


real_vm_c::real_vm_c() :
	printer(default_printer),
	op_mem(), global_mem(), string_mem(), temp_strings(),
	functions(), native_funcs(),
	comp(), exec()
{
	// string #0 must be the empty string
	int ofs = string_mem.alloc(2);
	assert(ofs == 0);
	strcpy((char *)string_mem.deref(0), "");


	// function #0 is the "null function" 
	function_t *df = new function_t;
	memset(df, 0, sizeof(function_t));

	functions.push_back(df);


	// statement #0 is never used
	ofs = EmitCode(OP_RET);
	assert(ofs == 0);


	// global #0 is never used (equivalent to NULL)
	// global #1-#3 are reserved for function return values
	// global #4-#6 are reserved for a zero value
	ofs = global_mem.alloc(7 * sizeof(double));
	assert(ofs == 0);
	memset(global_mem.deref(0), 0, 7 * sizeof(double));
}

real_vm_c::~real_vm_c()
{
	// FIXME !!!!
}


void real_vm_c::SetPrinter(print_func_t func)
{
	printer = func;
}

void real_vm_c::SetAsmDump(bool enable)
{
	comp.asm_dump = enable;
}



vm_c * CreateVM()
{
	assert(sizeof(double) == 8);

	return new real_vm_c;
}

}  // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
