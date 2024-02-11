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

#ifndef __DDF_ANIM_H_
#define __DDF_ANIM_H_

#include "epi.h"
#include "types.h"

//
// source animation definition
//
// -KM- 98/07/31 Anims.ddf
//
class animdef_c
{
   public:
    animdef_c();
    ~animdef_c(){};

   public:
    void Default(void);
    void CopyDetail(animdef_c &src);

    // Member vars....
    std::string name;

    enum  // types
    {
        A_Flat = 0,
        A_Texture,
        A_Graphic
    };

    int type;

    // -AJA- 2004/10/27: new SEQUENCE command for anims
    std::vector<std::string> pics;

    // first and last names in TEXTURE1/2 lump
    std::string startname;
    std::string endname;

    // how many 1/35s ticks each frame lasts
    int speed;

   private:
    // disable copy construct and assignment operator
    explicit animdef_c(animdef_c &rhs) { (void)rhs; }
    animdef_c &operator=(animdef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our animdefs container
class animdef_container_c : public std::vector<animdef_c *>
{
   public:
    animdef_container_c() {}
    ~animdef_container_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            animdef_c *anim = *iter;
            delete anim;
            anim = nullptr;
        }
    }
};

extern animdef_container_c animdefs;  // -ACB- 2004/06/03 Implemented

void DDF_ReadAnims(const std::string &data);

// handle the BOOM lump
void DDF_ConvertANIMATED(const uint8_t *data, int size);

#endif /* __DDF_ANIM__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
