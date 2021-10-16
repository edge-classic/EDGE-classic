//----------------------------------------------------------------------
//  COAL EXECUTION ENGINE
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

#include "coal.h"

#include <vector>


namespace coal
{

#include "c_local.h"
#include "c_execute.h"


#define MAX_RUNAWAY  (1000*1000)

#define MAX_PRINTMSG  1024


execution_c::execution_c() :
	s(0), func(0), tracing(false),
	stack_depth(0), call_depth(0)
{ }

execution_c::~execution_c()
{ }


void real_vm_c::default_printer(const char *msg, ...)
{
	// does nothing
}

void real_vm_c::default_aborter(const char *msg, ...)
{
	exit(66);
}


int real_vm_c::GetNativeFunc(const char *name, const char *module)
{
	char buffer[256];

	if (module)
		sprintf(buffer, "%s.%s", module, name);
	else
		strcpy(buffer, name);

	for (int i = 0; i < (int)native_funcs.size(); i++)
		if (strcmp(native_funcs[i]->name, buffer) == 0)
			return i;
	
	return -1;  // NOT FOUND
}

void real_vm_c::AddNativeFunction(const char *name, native_func_t func)
{
	// already registered?
	int prev = GetNativeFunc(name, NULL);

	if (prev >= 0)
	{
		native_funcs[prev]->func = func;
		return;
	}

	reg_native_func_t *reg = new reg_native_func_t;

	reg->name = strdup(name);
	reg->func = func;

	native_funcs.push_back(reg);
}


void real_vm_c::SetTrace(bool enable)
{
	exec.tracing = enable;
}


int real_vm_c::FindFunction(const char *func_name)
{
	for (int i = (int)functions.size()-1; i >= 1; i--)
	{
		function_t *f = functions[i];

		if (strcmp(f->name, func_name) == 0)
			return i;
	}

	return vm_c::NOT_FOUND;
}

int real_vm_c::FindVariable(const char *var_name)
{
	// FIXME

	return vm_c::NOT_FOUND;
}

// returns an offset from the string heap
int	real_vm_c::InternaliseString(const char *new_s)
{
	if (new_s[0] == 0)
		return 0;

	int ofs = string_mem.alloc(strlen(new_s) + 1);
	strcpy((char *)string_mem.deref(ofs), new_s);

	return ofs;
}


double * real_vm_c::AccessParam(int p)
{
	assert(exec.func);

	if (p >= functions[exec.func]->parm_num)
		RunError("PR_Parameter: p=%d out of range\n", p);

	return &exec.stack[exec.stack_depth + functions[exec.func]->parm_ofs[p]];
}


const char * real_vm_c::AccessParamString(int p)
{
	double *d = AccessParam(p);

	return REF_STRING((int) *d);
}


void real_vm_c::ReturnFloat(double f)
{
	G_FLOAT(OFS_RETURN*8) = f;
}

void real_vm_c::ReturnVector(double *v)
{
	double *c = G_VECTOR(OFS_RETURN*8);

	c[0] = v[0];
	c[1] = v[1];
	c[2] = v[2];
}

void real_vm_c::ReturnString(const char *s, int len)
{
	// TODO: turn this code into a utility function

	if (len < 0)
		len = strlen(s);

	if (len == 0)
	{
		G_FLOAT(OFS_RETURN*8) = 0;
	}
	else
	{
		int index = temp_strings.alloc(len + 1);

		char *s3  = (char *) temp_strings.deref(index);

		memcpy(s3, s, (size_t)len);
		s3[len] = 0;

		G_FLOAT(OFS_RETURN*8) = -(1 + index);
	}
}


//
// Aborts the currently executing functions
//
void real_vm_c::RunError(const char *error, ...)
{
    va_list argptr;
    char buffer[MAX_PRINTMSG];

    va_start(argptr, error);
    vsnprintf(buffer, sizeof(buffer), error, argptr);
    va_end(argptr);

    printer("ERROR: %s\n", buffer);

	if (exec.call_depth > 0)
		StackTrace();

    /* clear the stack so SV/Host_Error can shutdown functions */
    exec.call_depth = 0;

//  raise(11);
    throw exec_error_x();
}


int real_vm_c::STR_Concat(const char * s1, const char * s2)
{
	int len1 = strlen(s1);
	int len2 = strlen(s2);

	if (len1 == 0 && len2 == 0)
		return 0;

	int index = temp_strings.alloc(len1 + len2 + 1);
	char *s3  = (char *) temp_strings.deref(index);

	strcpy(s3, s1);
	strcpy(s3 + len1, s2);

	return -(1 + index);

}

int real_vm_c::STR_ConcatFloat(const char * s, double f)
{
	char buffer[100];

	if (f == (int)f)
	{
		sprintf(buffer, "%1.0f", f);
	}
	else
	{
		sprintf(buffer, "%8.6f", f);
	}

	return STR_Concat(s, buffer);
}

int real_vm_c::STR_ConcatVector(const char * s, double *v)
{
	char buffer[200];

	if (v[0] == (int)v[0] && v[1] == (int)v[1] && v[2] == (int)v[2])
	{
		sprintf(buffer, "'%1.0f %1.0f %1.0f'", v[0], v[1], v[2]);
	}
	else
	{
		sprintf(buffer, "'%6.4f %6.4f %6.4f'", v[0], v[1], v[2]);
	}

	return STR_Concat(s, buffer);
}


//================================================================
//  EXECUTION ENGINE
//================================================================

void real_vm_c::EnterFunction(int func)
{
	assert(func > 0);

	function_t *new_f = functions[func];

	// NOTE: the saved 's' value points to the instruction _after_ OP_CALL

    exec.call_stack[exec.call_depth].s    = exec.s;
    exec.call_stack[exec.call_depth].func = exec.func;

    exec.call_depth++;
	if (exec.call_depth >= MAX_CALL_STACK)
		RunError("stack overflow");

	if (exec.func)
		exec.stack_depth += functions[exec.func]->locals_end;

	if (exec.stack_depth + new_f->locals_end >= MAX_LOCAL_STACK)
		RunError("PR_ExecuteProgram: locals stack overflow\n");

	exec.s    = new_f->first_statement;
	exec.func = func;
}


void real_vm_c::LeaveFunction()
{
	if (exec.call_depth <= 0)
		RunError("stack underflow");

	exec.call_depth--;

	exec.s    = exec.call_stack[exec.call_depth].s;
	exec.func = exec.call_stack[exec.call_depth].func;

	if (exec.func)
		exec.stack_depth -= functions[exec.func]->locals_end;
}


void real_vm_c::EnterNative(int func, int argc)
{
	function_t *newf = functions[func];

	int n = -(newf->first_statement + 1);
	assert(n < (int)native_funcs.size());

	exec.stack_depth += functions[exec.func]->locals_end;
	{
		int old_func = exec.func;
		{
			exec.func = func;
			native_funcs[n]->func (this, argc);
		}
		exec.func = old_func;
	}
	exec.stack_depth -= functions[exec.func]->locals_end;
}


#define Operand(a)  (   \
	((a) > 0) ? REF_GLOBAL(a) :   \
    ((a) < 0) ? &exec.stack[exec.stack_depth - ((a) + 1)] :   \
	NULL)


void real_vm_c::DoExecute(int fnum)
{
	function_t *f = functions[fnum];

	int runaway = MAX_RUNAWAY;

	// make a stack frame
	int exitdepth = exec.call_depth;

	EnterFunction(fnum);

	for (;;)
	{
		statement_t *st = REF_OP(exec.s);

		if (exec.tracing)
			PrintStatement(f, exec.s);

		if (!--runaway)
			RunError("runaway loop error");

		// move code pointer to next statement
		exec.s += sizeof(statement_t);

		// handle exotic operations here (ones which store special
		// values in the a / b / c fields of statement_t).

		if (st->op < OP_MOVE_F) switch (st->op)
		{
			case OP_NULL:
				// no operation
				continue;

			case OP_CALL:
			{
				double * a = Operand(st->a);

				int fnum = (int)*a;
				if (fnum <= 0)
					RunError("NULL function");

				function_t *newf = functions[fnum];

				/* negative statements are built in functions */
				if (newf->first_statement < 0)
					EnterNative(fnum, st->b);
				else
					EnterFunction(fnum);
				continue;
			}

			case OP_RET:
			{
				LeaveFunction();

				// all done?
				if (exec.call_depth == exitdepth)
					return;

				continue;
			}

			case OP_PARM_F:
			{
				double *a = Operand(st->a);
				double *b = &exec.stack[exec.stack_depth + functions[exec.func]->locals_end + st->b];

				*b = *a;
				continue;
			}

			case OP_PARM_V:
			{
				double *a = Operand(st->a);
				double *b = &exec.stack[exec.stack_depth + functions[exec.func]->locals_end + st->b];

				b[0] = a[0];
				b[1] = a[1];
				b[2] = a[2];
				continue;
			}

			case OP_IFNOT:
			{
				if (! Operand(st->a)[0])
					exec.s = st->b;
				continue;
			}

			case OP_IF:
			{
				if (Operand(st->a)[0])
					exec.s = st->b;
				continue;
			}

			case OP_GOTO:
				exec.s = st->b;
				continue;

			case OP_ERROR:
				RunError("Assertion failed @ %s:%d\n",
				         REF_STRING(st->a), st->b);
				break; /* NOT REACHED */

			default:
				RunError("Bad opcode %i", st->op);
		}

		// handle mathematical ops here

		double * a = Operand(st->a);
		double * b = Operand(st->b);
		double * c = Operand(st->c);

		switch (st->op)
		{
			case OP_MOVE_F:
			case OP_MOVE_FNC:	// pointers
				*b = *a;
				break;

			case OP_MOVE_S:
				// temp strings must be internalised when assigned
				// to a global variable.
				if (*a < 0 && st->b > OFS_RETURN*8)
					*b = InternaliseString(REF_STRING((int)*a));
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
				*c = STR_Concat(REF_STRING((int)*a), REF_STRING((int)*b));
				// temp strings must be internalised when assigned
				// to a global variable.
				if (st->c > OFS_RETURN*8)
					*c = InternaliseString(REF_STRING((int)*c));
				break;

			case OP_ADD_SF:
				*c = STR_ConcatFloat(REF_STRING((int)*a), *b);
				if (st->c > OFS_RETURN*8)
					*c = InternaliseString(REF_STRING((int)*c));
				break;

			case OP_ADD_SV:
				*c = STR_ConcatVector(REF_STRING((int)*a), b);
				if (st->c > OFS_RETURN*8)
					*c = InternaliseString(REF_STRING((int)*c));
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
				if (*b == 0)
					RunError("Division by zero");
				*c = *a / *b;
				break;

			case OP_DIV_V:
				if (*b == 0)
					RunError("Division by zero");
				c[0] = a[0] / *b;
				c[1] = a[1] / *b;
				c[2] = a[2] / *b;
				break;

			case OP_MOD_F:
				if (*b == 0)
					RunError("Division by zero");
				else
				{
					float d = floorf(*a / *b);
					*c = *a - d * (*b);
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
				*c = *a == *b;
				break;
			case OP_EQ_V:
				*c = (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
				break;
			case OP_EQ_S:
				*c = (*a == *b) ? 1 :
					!strcmp(REF_STRING((int)*a), REF_STRING((int)*b));
				break;

			case OP_NE_F:
			case OP_NE_FNC:
				*c = *a != *b;
				break;
			case OP_NE_V:
				*c = (a[0] != b[0]) || (a[1] != b[1]) || (a[2] != b[2]);
				break;
			case OP_NE_S:
				*c = (*a == *b) ? 0 :
					!! strcmp(REF_STRING((int)*a), REF_STRING((int)*b));
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

int real_vm_c::Execute(int func_id)
{
	// re-use the temporary string space
	temp_strings.reset();

	try
	{
		if (func_id < 1 || func_id >= (int)functions.size())
		{
			RunError("vm_c::Execute: NULL function");
		}

		DoExecute(func_id);
	}
	catch (exec_error_x err)
	{
		return 9;
	}

	// printer("TEMP_STRINGs: %d / %d\n", temp_strings.usedMemory(), temp_strings.totalMemory());

	return 0;
}


//=================================================================
//  DEBUGGING STUFF
//=================================================================

const char * opcode_names[] =
{
	"NULL",
	"CALL",
	"RET",
	"PARM_F",
	"PARM_V",
	"IF",
	"IFNOT",
	"GOTO",
	"ERROR",

	"MOVE_F",
	"MOVE_V",
	"MOVE_S",
	"MOVE_FNC",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_FNC",

	"INC",
	"DEC",

	"POWER", 
	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",
	"DIV_F",
	"DIV_V",
	"MOD_F",

	"ADD_F",
	"ADD_V",
	"ADD_S",
	"ADD_SF",
	"ADD_SV",
	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_FNC",
	"NE_F",
	"NE_V",
	"NE_S",
	"NE_FNC",
	"LE",
	"GE",
	"LT",
	"GT",

	"AND",
	"OR",
	"BITAND",
	"BITOR",
};


static const char *OpcodeName(short op)
{
	if (op < 0 || op >= NUM_OPERATIONS)
		return "???";

	return opcode_names[op];
}


//
// Returns a string suitable for printing (no newlines, max 60 chars length)
// TODO: reimplement this
#if 0
char *DebugString(char *string)
{
	static char buf[80];
	char	*s;

	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}
#endif

//
// Returns a string describing *data in a type specific manner
// TODO: reimplement this
#if 0
char *Debug_ValueString(etype_t type, double *val)
{
	static char	line[256];
	def_t		*def;
	function_t	*f;

	line[0] = 0;

	switch (type)
	{
	case ev_string:
//!!!!		sprintf(line, "%s", PR_String(REF_STRING((int)*val)));
		break;
//	case ev_entity:
//		sprintf (line, "entity %i", *(int *)val);
//		break;
	case ev_function:
		f = functions + (int)*val;
		if (!f)
			sprintf(line, "undefined function");
//!!!!		else
//!!!!			sprintf(line, "%s()", REF_STRING(f->s_name));
		break;
//	case ev_field:
//		def = PR_DefForFieldOfs ( *(int *)val );
//		sprintf (line, ".%s", def->name);
//		break;
	case ev_void:
		sprintf(line, "void");
		break;
	case ev_float:
		sprintf(line, "%5.1f", (float) *val);
		break;
	case ev_vector:
		sprintf(line, "'%5.1f %5.1f %5.1f'", val[0], val[1], val[2]);
		break;
	case ev_pointer:
		sprintf(line, "pointer");
		break;
	default:
		sprintf(line, "bad type %i", type);
		break;
	}

	return line;
}
#endif


void real_vm_c::StackTrace()
{
	printer("Stack Trace:\n");

	exec.call_stack[exec.call_depth].func = exec.func;
	exec.call_stack[exec.call_depth].s    = exec.s;

	for (int i = exec.call_depth; i >= 1; i--)
	{
		int back = (exec.call_depth - i) + 1;

		function_t * f = functions[exec.call_stack[i].func];

		statement_t *st = REF_OP(exec.call_stack[i].s);

		if (f)
			printer("%-2d %s() at %s:%d\n", back, f->name, f->source_file, f->source_line + st->line);
		else
			printer("%-2d ????\n", back);
	}

	printer("\n");
}


const char * real_vm_c::RegString(statement_t *st, int who)
{
	static char buffer[100];

	int val = (who == 1) ? st->a : (who == 2) ? st->b : st->c;

	if (val == OFS_RETURN*8)
		return "result";

	if (val == OFS_DEFAULT*8)
		return "default";

	sprintf(buffer, "%s[%d]", (val < 0) ? "stack" : "glob", abs(val));
	return buffer;
}

void real_vm_c::PrintStatement(function_t *f, int s)
{
	statement_t *st = REF_OP(s);

	const char *op_name = OpcodeName(st->op);

	printer("  %06x: %-9s ", s, op_name);

	switch (st->op)
	{
		case OP_NULL:
		case OP_RET:
		case OP_ERROR:
			break;
	
		case OP_MOVE_F:
		case OP_MOVE_S:
		case OP_MOVE_FNC:	// pointers
		case OP_MOVE_V:
			printer("%s ",    RegString(st, 1));
			printer("-> %s",  RegString(st, 2));
			break;

		case OP_IFNOT:
		case OP_IF:
			printer("%s %08x", RegString(st, 1), st->b);
			break;

		case OP_GOTO:
			printer("%08x", st->b);
			// TODO
			break;

		case OP_CALL:
			printer("%s (%d) ", RegString(st, 1), st->b);

			if (! st->c)
				printer(" ");
			else
				printer("-> %s",   RegString(st, 3));
			break;

		case OP_PARM_F:
		case OP_PARM_V:
			printer("%s -> future[%d]", RegString(st, 1), st->b);
			break;

		case OP_NOT_F:
		case OP_NOT_FNC:
		case OP_NOT_V:
		case OP_NOT_S:
			printer("%s ",    RegString(st, 1));
			printer("-> %s",  RegString(st, 3));
			break;

		default:
			printer("%s + ",  RegString(st, 1));
			printer("%s ",    RegString(st, 2));
			printer("-> %s",  RegString(st, 3));
			break;
	}

	printer("\n");
}


void real_vm_c::ASM_DumpFunction(function_t *f)
{
	printer("Function %s()\n", f->name);

	if (f->first_statement < 0)
	{
		printer("  native #%d\n\n", - f->first_statement);
		return;
	}

	for (int s = f->first_statement; s <= f->last_statement; s += sizeof(statement_t))
	{
		PrintStatement(f, s);
	}

	printer("\n");
}

void real_vm_c::ASM_DumpAll()
{
	for (int i = 1; i < (int)functions.size(); i++)
	{
		function_t *f = functions[i];

		ASM_DumpFunction(f);
	}
}


}  // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
