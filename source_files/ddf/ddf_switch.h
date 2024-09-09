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

#pragma once

#include "ddf_types.h"

class Image;

struct SwitchCache
{
    const Image *image[2];
};

class SwitchDefinition
{
  public:
    SwitchDefinition();
    ~SwitchDefinition() {};

  public:
    void Default(void);
    void CopyDetail(SwitchDefinition &src);

    // Member vars....
    std::string name_;

    std::string on_name_;
    std::string off_name_;

    struct SoundEffect *on_sfx_;
    struct SoundEffect *off_sfx_;

    int time_;

    SwitchCache cache_;

  private:
    // disable copy construct and assignment operator
    explicit SwitchDefinition(SwitchDefinition &rhs)
    {
    }
    SwitchDefinition &operator=(SwitchDefinition &rhs)
    {
        return *this;
    }
};

// Our switchdefs container
class SwitchDefinitionContainer : public std::vector<SwitchDefinition *>
{
  public:
    SwitchDefinitionContainer()
    {
    }
    ~SwitchDefinitionContainer()
    {
        for (std::vector<SwitchDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            SwitchDefinition *s = *iter;
            delete s;
            s = nullptr;
        }
    }

  public:
    SwitchDefinition *Find(const char *name);
};

extern SwitchDefinitionContainer switchdefs; // -ACB- 2004/06/04 Implemented

void DDFReadSwitch(const std::string &data);

// handle the BOOM lump
void DDFConvertSwitchesLump(const uint8_t *data, int size);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
