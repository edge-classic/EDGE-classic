//----------------------------------------------------------------------------
//  DDF Collections
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

#pragma once

#include <string>

enum DdfType
{
    kDdfTypeUnknown = -1,
    kDdfTypeAnim    = 0,
    kDdfTypeAttack,
    kDdfTypeColourMap,
    kDdfTypeFlat,
    kDdfTypeFont,
    kDdfTypeGame,
    kDdfTypeImage,
    kDdfTypeLanguage,
    kDdfTypeLevel,
    kDdfTypeLine,
    kDdfTypeMovie,
    kDdfTypePlaylist,
    kDdfTypeSFX,
    kDdfTypeSector,
    kDdfTypeStyle,
    kDdfTypeSwitch,
    kDdfTypeThing,
    kDdfTypeWeapon,
    // not strictly DDF, but useful sometimes
    kDdfTypeRadScript,
    kTotalDdfTypes
};

struct DdfFile
{
    DdfType     type;
    std::string source;
    std::string data;
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
