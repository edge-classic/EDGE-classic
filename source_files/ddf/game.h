//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __DDF_GAME_H__
#define __DDF_GAME_H__

#include "utility.h"

#include "types.h"

// ------------------------------------------------------------------
// -----------------------GAME DEFINITIONS---------------------------
// ------------------------------------------------------------------

class wi_mapposdef_c
{
public:
	lumpname_c name;

	int x, y;

public:
	wi_mapposdef_c();
	wi_mapposdef_c(wi_mapposdef_c &rhs);
	~wi_mapposdef_c();

public:
	wi_mapposdef_c& operator=(wi_mapposdef_c &rhs);

private:
	void Copy(wi_mapposdef_c &src);

};


class wi_mapposdef_container_c : public epi::array_c
{
public:
	wi_mapposdef_container_c();
	wi_mapposdef_container_c(wi_mapposdef_container_c &rhs);
	~wi_mapposdef_container_c();
	
private:
	void CleanupObject(void *obj);
	void Copy(wi_mapposdef_container_c &src);

public:
	int GetSize() {	return array_entries; } 
	int Insert(wi_mapposdef_c *m) { return InsertObject((void*)&m); }
	wi_mapposdef_container_c& operator=(wi_mapposdef_container_c &rhs);
	wi_mapposdef_c* operator[](int idx) { return *(wi_mapposdef_c**)FetchObject(idx); } 
};

class wi_framedef_c
{
public:
	lumpname_c pic;		// Name of pic to display.
	int tics;			// Tics on this frame
	int x, y;			// Position on screen where this goes

public:
	wi_framedef_c();	
	wi_framedef_c(wi_framedef_c &rhs); 
	~wi_framedef_c();
	
public:
	void Default(void);
	wi_framedef_c& operator=(wi_framedef_c &rhs);

private:
	void Copy(wi_framedef_c &src);
};

class wi_framedef_container_c : public epi::array_c
{
public:
	wi_framedef_container_c();
	wi_framedef_container_c(wi_framedef_container_c &rhs);
	~wi_framedef_container_c();
		
private:
	void CleanupObject(void *obj);
	void Copy(wi_framedef_container_c &rhs);

public:
	int GetSize() {	return array_entries; } 
	int Insert(wi_framedef_c *f) { return InsertObject((void*)&f); }
	wi_framedef_container_c& operator=(wi_framedef_container_c &rhs);
	wi_framedef_c* operator[](int idx) { return *(wi_framedef_c**)FetchObject(idx); } 
};


class wi_animdef_c
{
public:
 	enum animtype_e { WI_NORMAL, WI_LEVEL };

	animtype_e type;

	lumpname_c level;

	wi_framedef_container_c frames;

public:
	wi_animdef_c();
	wi_animdef_c(wi_animdef_c &rhs);
	~wi_animdef_c();

public:
	wi_animdef_c& operator=(wi_animdef_c &rhs);
	void Default(void);

private:
	void Copy(wi_animdef_c &rhs);
};

class wi_animdef_container_c : public epi::array_c
{
public:
	wi_animdef_container_c();
	wi_animdef_container_c(wi_animdef_container_c &rhs);
	~wi_animdef_container_c();

private:
	void CleanupObject(void *obj);
	void Copy(wi_animdef_container_c &src);

public:
	int GetSize() {	return array_entries; } 
	int Insert(wi_animdef_c *a) { return InsertObject((void*)&a); }
	wi_animdef_container_c& operator=(wi_animdef_container_c &rhs);
	wi_animdef_c* operator[](int idx) { return *(wi_animdef_c**)FetchObject(idx); } 
};

typedef enum
{
	// standard Doom shading
	LMODEL_Doom = 0,

	// Doom shading without the brighter N/S, darker E/W walls
	LMODEL_Doomish = 1,

	// flat lighting (no shading at all)
	LMODEL_Flat = 2,

	// vertex lighting
	LMODEL_Vertex = 3,

 	// Invalid (-ACB- 2003/10/06: MSVC wants the invalid value as part of the enum)
 	LMODEL_Invalid = 999
}
lighting_model_e;

// Game definition file
class gamedef_c
{
public:
	gamedef_c();
	~gamedef_c();

public:
	void Default(void);
	void CopyDetail(gamedef_c &src);

	// Member vars....
	epi::strent_c name;

	wi_animdef_container_c anims;
	wi_mapposdef_container_c mappos;

	lumpname_c background;
	lumpname_c splatpic;
	lumpname_c yah[2];

	// -AJA- 1999/10/22: background cameras.
	epi::strent_c bg_camera;

	int music;
	struct sfx_s *percent;
	struct sfx_s *done;
	struct sfx_s *endmap;
	struct sfx_s *nextmap;
	struct sfx_s *accel_snd;
	struct sfx_s *frag_snd;

	lumpname_c firstmap;
	lumpname_c namegraphic;
	
	epi::strlist_c titlepics;
	
	int titlemusic;
	int titletics;
	int special_music;

	lighting_model_e lighting;

private:
	// disable copy construct and assignment operator
	explicit gamedef_c(gamedef_c &rhs) { }
	gamedef_c& operator= (gamedef_c &rhs) { return *this; }
};


class gamedef_container_c : public epi::array_c
{
public:
	gamedef_container_c();
	~gamedef_container_c();

private:
	void CleanupObject(void *obj);

public:
	// List management
	int GetSize() {	return array_entries; } 
	int Insert(gamedef_c *g) { return InsertObject((void*)&g); }
	gamedef_c* operator[](int idx) { return *(gamedef_c**)FetchObject(idx); } 
	
	// Search Functions
	gamedef_c* Lookup(const char* refname);
};


// -------EXTERNALISATIONS-------

extern gamedef_container_c gamedefs;		// -ACB- 2004/06/21 Implemented

bool DDF_ReadGames(void *data, int size);

#endif // __DDF_GAME_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
