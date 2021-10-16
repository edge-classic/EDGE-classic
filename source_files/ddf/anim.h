//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Animated textures)
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

#ifndef __DDF_ANIM_H_
#define __DDF_ANIM_H_

#include "utility.h"

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
	~animdef_c() {};

public:
	void Default(void);
	void CopyDetail(animdef_c &src);

	// Member vars....
	epi::strent_c name;

	enum  // types
	{
		A_Flat = 0,
		A_Texture,
		A_Graphic
	};

	int type;

	// -AJA- 2004/10/27: new SEQUENCE command for anims
	epi::strlist_c pics;

	// first and last names in TEXTURE1/2 lump
	lumpname_c startname;
	lumpname_c endname;

	// how many 1/35s ticks each frame lasts
	int speed;

private:
	// disable copy construct and assignment operator
	explicit animdef_c(animdef_c &rhs) { }
	animdef_c& operator= (animdef_c &rhs) { return *this; }
};


// Our animdefs container
class animdef_container_c : public epi::array_c 
{
public:
	animdef_container_c() : epi::array_c(sizeof(animdef_c*)) {}
	~animdef_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	int GetSize() {	return array_entries; } 
	int Insert(animdef_c *a) { return InsertObject((void*)&a); }
	animdef_c* operator[](int idx) { return *(animdef_c**)FetchObject(idx); } 
};

extern animdef_container_c animdefs;		// -ACB- 2004/06/03 Implemented 

bool DDF_ReadAnims(void *data, int size);

void DDF_ParseANIMATED(const byte *data, int size);

#endif  /* __DDF_ANIM__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
