//----------------------------------------------------------------------------
//  DDF Collections
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
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

#ifndef __DDF_COLLECTION_H__
#define __DDF_COLLECTION_H__

#include <string>
#include <vector>

// types of DDF lumps / files
enum ddf_type_e
{
	DDF_UNKNOWN = -1,

	DDF_Anim = 0,
	DDF_Attack,
	DDF_ColourMap,
	DDF_Flat,
	DDF_Font,
	DDF_Game,
	DDF_Image,
	DDF_Language,
	DDF_Level,
	DDF_Line,
	DDF_Playlist,
	DDF_SFX,
	DDF_Sector,
	DDF_Style,
	DDF_Switch,
	DDF_Thing,
	DDF_Weapon,

	// not strictly DDF, but useful sometimes
	DDF_RadScript,

	DDF_NUM_TYPES
};


class ddf_file_c
{
public:
	ddf_type_e  type;
	std::string source;
	std::string data;

	ddf_file_c(ddf_type_e _t, const std::string& _s) : type(_t), source(_s), data()
	{ }

	ddf_file_c(ddf_type_e _t, const std::string& _s, std::string& _d) : type(_t), source(_s), data(_d)
	{ }

	~ddf_file_c()
	{}
};


class ddf_collection_c
{
public:
	std::vector<ddf_file_c> files;

	ddf_collection_c() : files()
	{ }

	~ddf_collection_c()
	{ }
};


#endif /*__DDF_COLLECTION_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
