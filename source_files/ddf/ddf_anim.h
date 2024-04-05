//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Animated textures)
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

class AnimationDefinition
{
  public:
    AnimationDefinition();
    ~AnimationDefinition(){};

  public:
    void Default(void);
    void CopyDetail(AnimationDefinition &src);

    std::string name_;

    enum AnimationType
    {
        kAnimationTypeFlat = 0,
        kAnimationTypeTexture,
        kAnimationTypeGraphic
    };

    int type_;

    // -AJA- 2004/10/27: new SEQUENCE command for anims
    std::vector<std::string> pics_;

    // first and last names in TEXTURE1/2 lump
    std::string start_name_;
    std::string end_name_;

    // how many 1/35s ticks each frame lasts
    int speed_;

  private:
    // disable copy construct and assignment operator
    explicit AnimationDefinition(AnimationDefinition &rhs)
    {
        (void)rhs;
    }
    AnimationDefinition &operator=(AnimationDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class AnimationDefinitionContainer : public std::vector<AnimationDefinition *>
{
  public:
    AnimationDefinitionContainer()
    {
    }
    ~AnimationDefinitionContainer()
    {
        for (std::vector<AnimationDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            AnimationDefinition *anim = *iter;
            delete anim;
            anim = nullptr;
        }
    }

  public:
    AnimationDefinition *Lookup(const char *refname);
};

extern AnimationDefinitionContainer animdefs; // -ACB- 2004/06/03 Implemented

void DDFReadAnims(const std::string &data);

// handle the BOOM lump
void DDFConvertAnimatedLump(const uint8_t *data, int size);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
