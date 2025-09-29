//------------------------------------------------------------------------
//  MAIN Program
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_music.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_things.h"
#include "deh_wad.h"
#include "deh_weapons.h"
#include "epi.h"

namespace dehacked
{

std::vector<InputBuffer *> input_buffers;

bool quiet_mode;

void Init()
{
    System_Startup();

    ammo ::Init();
    frames ::Init();
    miscellaneous ::Init();
    rscript::Init();
    sounds ::Init();
    music ::Init();
    sprites::Init();
    text_strings::Init();
    things ::Init();
    weapons::Init();

    /* reset parameters */

    quiet_mode = false;
}

void FreeInputBuffers(void)
{
    for (size_t i = 0; i < input_buffers.size(); i++)
    {
        delete input_buffers[i];
    }

    input_buffers.clear();
}

DehackedResult Convert(void)
{
    DehackedResult result;

    // load DEH patch file(s)
    for (size_t i = 0; i < input_buffers.size(); i++)
    {
        result = patch::Load(input_buffers[i]);

        if (result != kDehackedConversionOK)
            return result;
    }

    // do conversions into DDF...

    sprites::SpriteDependencies();
    frames ::StateDependencies();
    ammo ::AmmoDependencies();

    // things and weapons must be before attacks
    weapons::ConvertWEAP();
    things ::ConvertTHING();
    things ::ConvertATK();

    // rscript must be after things (for A_BossDeath)
    text_strings::ConvertLDF();
    rscript::ConvertRAD();

    // sounds must be after things/weapons/attacks
    sounds::ConvertSFX();
    music ::ConvertMUS();

    LogPrint("\n");

    return kDehackedConversionOK;
}

void Shutdown()
{
    ammo ::Shutdown();
    frames ::Shutdown();
    miscellaneous ::Shutdown();
    rscript::Shutdown();
    sounds ::Shutdown();
    music ::Shutdown();
    sprites::Shutdown();
    text_strings::Shutdown();
    things ::Shutdown();
    weapons::Shutdown();

    FreeInputBuffers();
}

} // namespace dehacked

//------------------------------------------------------------------------

void DehackedStartup()
{
    dehacked::Init();

    LogPrint("*** DeHackEd -> EDGE Conversion ***\n");
}

const char *DehackedGetError(void)
{
    return dehacked::GetErrorMsg();
}

DehackedResult DehackedSetQuiet(int quiet)
{
    dehacked::quiet_mode = (quiet != 0);

    return kDehackedConversionOK;
}

DehackedResult DehackedAddLump(const char *data, int length)
{
    dehacked::InputBuffer *buffer = new dehacked::InputBuffer(data, length);

    dehacked::input_buffers.push_back(buffer);

    return kDehackedConversionOK;
}

DehackedResult DehackedRunConversion(std::vector<DDFFile> *dest)
{
    dehacked::wad::dest_container = dest;

    return dehacked::Convert();
}

void DehackedShutdown(void)
{
    dehacked::Shutdown();
    dehacked::wad::dest_container = nullptr;
}
