//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Flat properties)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#ifndef __DDF_FLAT_H__
#define __DDF_FLAT_H__

#include "epi.h"

#include "types.h"

class flatdef_c
{
  public:
    flatdef_c();
    ~flatdef_c(){};

  public:
    void Default(void);
    void CopyDetail(flatdef_c &src);

    // Member vars....
    std::string name;

    std::string liquid; // Values are "THIN" and "THICK" - determines swirl and shader params - Dasho

    struct sfx_s *footstep;
    std::string   splash;
    // Lobo: item to spawn (or nullptr).  The mobjdef pointer is only valid after
    //  DDF_flatCleanUp() has been called.
    const mobjtype_c *impactobject;
    std::string       impactobject_ref;

    const mobjtype_c *glowobject;
    std::string       glowobject_ref;

    percent_t sink_depth;
    percent_t bob_depth;

  private:
    // disable copy construct and assignment operator
    explicit flatdef_c(flatdef_c &rhs)
    {
        (void)rhs;
    }
    flatdef_c &operator=(flatdef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our flatdefs container
class flatdef_container_c : public std::vector<flatdef_c *>
{
  public:
    flatdef_container_c()
    {
    }
    ~flatdef_container_c()
    {
      for (auto iter = begin(); iter != end(); iter++)
      {
          flatdef_c *flt = *iter;
          delete flt;
          flt = nullptr;
      }
    }
    flatdef_c *Find(const char *name);
};

extern flatdef_container_c flatdefs; // -DASHO- 2022 Implemented

void DDF_ReadFlat(const std::string &data);

#endif /*__DDF_FLAT_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
