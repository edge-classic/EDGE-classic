//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

// ----------------------------------------------------------------
// ------------------------ SOUND EFFECTS -------------------------
// ----------------------------------------------------------------

// -KM- 1998/10/29
struct SoundEffect
{
    int num;
    int sounds[1]; // -ACB- 1999/11/06 Zero based array is not ANSI compliant
                   // -AJA- I'm also relying on the [1] within
                   // SoundEffectDefinition.
};

class SoundEffectDefinition
{
  public:
    SoundEffectDefinition();
    ~SoundEffectDefinition();

  public:
    void Default(void);
    void CopyDetail(SoundEffectDefinition &src);

    // Member vars....
    std::string name_;

    // full sound lump name (or file name)
    std::string lump_name_;
    std::string file_name_;
    std::string pack_name_;

    // PC Speaker equivalent sound
    std::string pc_speaker_sound_;

    // sfxinfo ID number
    // -AJA- Changed to a SoundEffect.  It serves two purposes: (a) hold the
    //       sound ID, like before, (b) better memory usage, as we don't
    //       need to allocate a new SoundEffect for non-wildcard sounds.
    SoundEffect normal_;

    // Sfx singularity (only one at a time), or 0 if not singular
    int singularity_;

    // Sfx priority
    int priority_;

    // volume adjustment (100% is normal, lower is quieter)
    float volume_;

    // -KM- 1998/09/01  Looping: for non nullptr origins
    bool looping_;

    // -AJA- 2000/04/19: Prefer to play the whole sound rather than
    //       chopping it off with a new sound.
    bool precious_;

    // distance limit, if the hearer is further away than `max_distance'
    // then the this sound won't be played at all.
    float max_distance_;

  private:
    // disable copy construct and assignment operator
    explicit SoundEffectDefinition(SoundEffectDefinition &rhs)
    {
        (void)rhs;
    }
    SoundEffectDefinition &operator=(SoundEffectDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our sound effect definition container
class SoundEffectDefinitionContainer : public std::vector<SoundEffectDefinition *>
{
  public:
    SoundEffectDefinitionContainer()
    {
    }

    ~SoundEffectDefinitionContainer()
    {
        for (std::vector<SoundEffectDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            SoundEffectDefinition *s = *iter;
            delete s;
            s = nullptr;
        }
    }

  public:
    // Lookup functions
    SoundEffect           *GetEffect(const char *name, bool error = true);
    SoundEffectDefinition *Lookup(const char *name);
};

// ----------EXTERNALISATIONS----------

extern SoundEffectDefinitionContainer sfxdefs; // -ACB- 2004/07/25 Implemented

void DdfReadSFX(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
