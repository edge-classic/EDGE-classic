//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Switch textures)
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

#ifndef __DDF_SWTH_H__
#define __DDF_SWTH_H__

#include "epi.h"

#include "types.h"

class image_c;

//
// SWITCHES
//
#define BUTTONTIME 35

typedef struct switchcache_s
{
    const image_c *image[2];
} switchcache_t;

class switchdef_c
{
  public:
    switchdef_c();
    ~switchdef_c(){};

  public:
    void Default(void);
    void CopyDetail(switchdef_c &src);

    // Member vars....
    std::string name;

    std::string on_name;
    std::string off_name;

    struct sfx_s *on_sfx;
    struct sfx_s *off_sfx;

    int time;

    switchcache_t cache;

  private:
    // disable copy construct and assignment operator
    explicit switchdef_c(switchdef_c &rhs)
    {
    }
    switchdef_c &operator=(switchdef_c &rhs)
    {
        return *this;
    }
};

// Our switchdefs container
class switchdef_container_c : public std::vector<switchdef_c *>
{
  public:
    switchdef_container_c()
    {
    }
    ~switchdef_container_c()
    {
      for (auto iter = begin(); iter != end(); iter++)
      {
          switchdef_c *s= *iter;
          delete s;
          s = nullptr;
      }
    }

  public:
    switchdef_c *Find(const char *name);
};

extern switchdef_container_c switchdefs; // -ACB- 2004/06/04 Implemented

void DDF_ReadSwitch(const std::string &data);

// handle the BOOM lump
void DDF_ConvertSWITCHES(const byte *data, int size);

#endif /*__DDF_SWTH_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
