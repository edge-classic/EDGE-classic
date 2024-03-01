//----------------------------------------------------------------------------
//  EDGE UMAPINFO Parsing Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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
//  Based on the UMAPINFO reference implementation, released by Christoph
//  Oelckers under the following copyright:
//
//    Copyright 2017 Christoph Oelckers
//
//----------------------------------------------------------------------------

#pragma once

#include <string>

struct BossAction
{
    int type;
    int special;
    int tag;
};

struct MapEntry
{
    char *mapname;
    char *levelname;
    char *label;
    char *intertext;
    char *intertextsecret;
    char *authorname;
    char  levelpic[9];
    char  next_map[9];
    char  nextsecret[9];
    char  music[9];
    char  skytexture[9];
    char  endpic[9];
    char  exitpic[9];
    char  enterpic[9];
    char  interbackdrop[9];
    char  intermusic[9];
    int   partime;
    int   nointermission;
    int   numbossactions;
    int   docast;
    int   dobunny;
    int   endgame;
    char *specialaction;

    struct BossAction *bossactions;
};

struct MapList
{
    unsigned int     mapcount;
    struct MapEntry *maps;
};

extern struct MapList Maps;

void ParseUmapinfo(const std::string &buffer);

void FreeMapList();