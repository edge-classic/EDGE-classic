//----------------------------------------------------------------------------
//  EPI Assertions
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __EPI_ASSERT_H__
#define __EPI_ASSERT_H__

// -------- the macros --------

// -ES- 2000/02/07 SYS_ASSERT_MSG fails if the condition is false.
// The second parameter is the entire I_Error argument list, surrounded
// by parentheses (which makes it possible to use an arbitrary number
// of parameters even without GCC)
#ifdef NDEBUG
#define SYS_ASSERT_MSG(cond, arglist)  ((void) 0)
#else
#define SYS_ASSERT_MSG(cond, arglist)  \
	((cond)?(void) 0 : I_Error arglist)
#endif

// -AJA- 2000/02/13: variation on a theme...
#ifdef NDEBUG
#define SYS_ASSERT(cond)  ((void) 0)
#else
#ifdef __GNUC__
#define SYS_ASSERT(cond)  \
	((cond)? (void)0 :I_Error("Assertion '%s' failed in %s (%s:%d).\n",  \
    #cond , __func__ , __FILE__ , __LINE__ ))
#else
#define SYS_ASSERT(cond)  \
	((cond)? (void)0 :I_Error("Assertion '%s' failed (%s:%d).\n",  \
    #cond , __FILE__ , __LINE__ ))
#endif
#endif // NDEBUG


#define SYS_NULL_CHECK(ptr)    SYS_ASSERT((ptr) != NULL)
#define SYS_ZERO_CHECK(value)  SYS_ASSERT((value) != 0)

#endif  /*__EPI_ASSERT_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
