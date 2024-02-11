//----------------------------------------------------------------------------
//  EDGE Basic Types
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __DDF_TYPE_H__
#define __DDF_TYPE_H__

#include "epi.h"
#include "collection.h"

#include "math_color.h"
#include "math_bam.h"

class mobjtype_c;

// percentage type.  Ranges from 0.0f - 1.0f
typedef float percent_t;

#define PERCENT_MAKE(val)     ((val) / 100.0f)
#define PERCENT_2_FLOAT(perc) (perc)

class mobj_strref_c
{
  public:
    mobj_strref_c() : name(), def(nullptr)
    {
    }
    mobj_strref_c(const char *s) : name(s), def(nullptr)
    {
    }
    mobj_strref_c(const mobj_strref_c &rhs) : name(rhs.name), def(nullptr)
    {
    }
    ~mobj_strref_c(){};

  private:
    std::string name;

    const mobjtype_c *def;

  public:
    const char *GetName() const
    {
        return name.c_str();
    }

    const mobjtype_c *GetRef();
    // Note: this returns nullptr if not found, in which case you should
    // produce an error, since future calls will do the search again.

    mobj_strref_c &operator=(mobj_strref_c &rhs)
    {
        if (&rhs != this)
        {
            name = rhs.name;
            def  = nullptr;
        }

        return *this;
    }
};

#endif /*__DDF_TYPE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
