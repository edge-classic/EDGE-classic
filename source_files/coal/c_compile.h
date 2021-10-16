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

#ifndef __COAL_COMPILER_BITS_H__
#define __COAL_COMPILER_BITS_H__

class scope_c;

typedef enum
{
	tt_eof,			// end of file reached
	tt_name, 		// an alphanumeric name token
	tt_punct, 		// code punctuation
	tt_literal,  	// string, float, vector
	tt_error        // an error occured (so get next token)
}
token_e;


typedef struct type_s
{
	etype_t			type;

// function types are more complex
	struct type_s	*aux_type;	// return type or field type

	int				parm_num;	// -1 = variable args
	struct type_s	*parm_types[MAX_PARMS];	// only [parm_num] allocated
}
type_t;


typedef struct def_s
{
	type_t	*type;
	const char *name;

	gofs_t	ofs;

	scope_c	*scope;

	int		flags;

	struct def_s *next;
}
def_t;

typedef enum
{
	DF_Constant    = (1 << 1),
	DF_Temporary   = (1 << 2),
	DF_FreeTemp    = (1 << 3),  // temporary can be re-used
}
def_flag_e;


class scope_c
{
public:
	char kind;  // 'g' global, 'f' function, 'm' module

	def_t * names;  // functions, vars, constants, parameters

	def_t * def;   // parent scope is def->scope

public:
	 scope_c() : kind('g'), names(NULL), def(NULL) { }
	~scope_c() { }

	void push_back(def_t *def)
	{
		def->scope = this;
		def->next = names; names = def;
	}
};


class compiling_c
{
public:
	const char *source_file;
	int source_line;
	int function_line;

	bool asm_dump;

	// current parsing position
	char *parse_p;
	char *line_start;	// start of current source line
	int bracelevel;
	int fol_level;    // fol = first on line

	// current token (from LEX_Next)
	char    token_buf[2048];
	token_e token_type;
	bool    token_is_first;

	char    literal_buf[2048];
	type_t *literal_type;
	double  literal_value[3];

	// parameter names (when parsing a function def)
	char parm_names[MAX_PARMS][MAX_NAME];

	int error_count;

	scope_c global_scope;

	std::vector<scope_c *> all_modules;
	std::vector<type_t  *> all_types;
	std::vector<def_t   *> all_literals;

	// all temporaries for current function
	std::vector<def_t *> temporaries;

	// the function/module being parsed, or NULL
	scope_c * scope;

	// for tracking local variables vs temps
	int locals_end;
	int last_statement;

public:
	 compiling_c();
	~compiling_c();
};

#endif /* __COAL_COMPILER_BITS_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
