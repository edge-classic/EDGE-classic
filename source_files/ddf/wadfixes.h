//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (WAD-specific fixes)
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

#ifndef __DDF_WADFIXES_H__
#define __DDF_WADFIXES_H__

#include "epi.h"

#include "types.h"

class fixdef_c
{
  public:
    fixdef_c();
    ~fixdef_c(){};

  public:
    void Default(void);
    void CopyDetail(fixdef_c &src);

    // Member vars....
    std::string name;

    std::string md5_string; // Fixes are likely to be for finalized WADs that won't be updated anymore,
                            // but other qualifiers like unique lumps might be added if necessary

  private:
    // disable copy construct and assignment operator
    explicit fixdef_c(fixdef_c &rhs)
    {
    }
    fixdef_c &operator=(fixdef_c &rhs)
    {
        return *this;
    }
};

// Our fixdefs container
class fixdef_container_c : public std::vector<fixdef_c *>
{
  public:
    fixdef_container_c()
    {
    }
    ~fixdef_container_c()
    {
      for (auto iter = begin(); iter != end(); iter++)
      {
          fixdef_c *f= *iter;
          delete f;
          f = nullptr;
      }
    }

  public:
    fixdef_c *Find(const char *name);
};

extern fixdef_container_c fixdefs; // -DASHO- 2022 Implemented

void DDF_ReadFixes(const std::string &data);

#endif /*__DDF_WADFIXES_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
