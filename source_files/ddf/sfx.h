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

#ifndef __DDF_SFX_H__
#define __DDF_SFX_H__

#include "utility.h"

#include "types.h"


#define S_CLOSE_DIST     160.0f
#define S_CLIPPING_DIST  4000.0f


// ----------------------------------------------------------------
// ------------------------ SOUND EFFECTS -------------------------
// ----------------------------------------------------------------

// -KM- 1998/10/29
typedef struct sfx_s
{
	int num;
	int sounds[1]; // -ACB- 1999/11/06 Zero based array is not ANSI compliant
	// -AJA- I'm also relying on the [1] within sfxdef_c.
	int last_rand_played;
}
sfx_t;

#define sfx_None (sfx_t*) NULL

// Sound Effect Definition Class
class sfxdef_c
{
public:
	sfxdef_c();
	~sfxdef_c();
	
public:
	void Default(void);
	void CopyDetail(sfxdef_c &src);

	// Member vars....
	epi::strent_c name;

    // full sound lump name (or file name)
	lumpname_c lump_name;
	epi::strent_c file_name;

	// sfxinfo ID number
	// -AJA- Changed to a sfx_t.  It serves two purposes: (a) hold the
	//       sound ID, like before, (b) better memory usage, as we don't
	//       need to allocate a new sfx_t for non-wildcard sounds.
	sfx_t normal;

    // Sfx singularity (only one at a time), or 0 if not singular
	int singularity;

	// Sfx priority
	int priority;

	// volume adjustment (100% is normal, lower is quieter)
	percent_t volume;

	// -KM- 1998/09/01  Looping: for non NULL origins
	bool looping;

	// -AJA- 2000/04/19: Prefer to play the whole sound rather than
	//       chopping it off with a new sound.
	bool precious;
 
    // distance limit, if the hearer is further away than `max_distance'
    // then the this sound won't be played at all.
	float max_distance;

private:
	// disable copy construct and assignment operator
	explicit sfxdef_c(sfxdef_c &rhs) { }
	sfxdef_c& operator= (sfxdef_c &rhs) { return *this; }
};


// Our sound effect definition container
class sfxdef_container_c : public epi::array_c 
{
public:
	sfxdef_container_c() : epi::array_c(sizeof(sfxdef_c*))
	{ }

	~sfxdef_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);
	
public:
	// List management
	int GetSize() { return array_entries; } 
	int Insert(sfxdef_c *s) { return InsertObject((void*)&s); }
	sfxdef_c* operator[](int idx) { return *(sfxdef_c**)FetchObject(idx); } 
	
	// Lookup functions
	sfx_t* GetEffect(const char *name, bool error = true);
	sfxdef_c* Lookup(const char *name);
};

// ----------EXTERNALISATIONS----------

extern sfxdef_container_c sfxdefs;			// -ACB- 2004/07/25 Implemented

bool DDF_ReadSFX(void *data, int size);

#endif // __DDF_SFX_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
