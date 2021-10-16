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

#ifndef __COAL_EXECUTION_STUFF_H__
#define __COAL_EXECUTION_STUFF_H__


#define	MAX_CALL_STACK		96
#define	MAX_LOCAL_STACK		2048

struct call_stack_c
{
	int s;
	int func;
};


class execution_c
{
public:
	// code pointer
	int s;
	int func;

	bool tracing;

	double stack[MAX_LOCAL_STACK];
	int stack_depth;

	call_stack_c call_stack[MAX_CALL_STACK+1];
	int call_depth;

public:
	 execution_c();
	~execution_c();
};

#endif /* __COAL_EXECUTION_STUFF_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
