//----------------------------------------------------------------------------
//  EDGE Strings
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __DOOM_STRINGS__
#define __DOOM_STRINGS__

// Misc. other strings.
#define REQUIREDWAD    "edge"
#define EDGECONFIGFILE "edge.cfg"
#define EDGELOGFILE    "edge.log"

#define EDGEGWAEXT     "gwa"
#define EDGEHWAEXT     "hwa"
#define EDGEWADEXT     "wad"

#define SAVEGAMEBASE   "save"
#define SAVEGAMEEXT    "esg"
#define SAVEGAMEMODE   0755

#define CACHEDIR       "cache"
#define SAVEGAMEDIR    "savegame"
#define SCRNSHOTDIR    "screenshot"

#define HUBDIR         "hubs"
#define HUBBASE        "hub"

#ifdef WIN32
#define EDGEHOMESUBDIR  "Application Data\\Edge"
#elif MACOSX
#define EDGEHOMESUBDIR  "Library/Application Support/EDGE"
#else // Linux
#define EDGEHOMESUBDIR  ".edge"
#endif

#endif // __DOOM_STRINGS__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
