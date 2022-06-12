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

#ifndef __DDF_MUS_H__
#define __DDF_MUS_H__

#include "utility.h"

#include "types.h"

// ----------------------------------------------------------------
// -------------------------MUSIC PLAYLIST-------------------------
// ----------------------------------------------------------------

// Playlist entry class

// FIXME: Move enums in pl_entry_c class?
typedef enum
{
	MUS_UNKNOWN   = 0,
	MUS_CD,
	MUS_MIDI,
	MUS_MUS,
	MUS_OGG,
	MUS_MP3,
	MUS_IBXM,
	MUS_GME,
	MUS_SID,
	ENDOFMUSTYPES
}
musictype_t;

typedef enum
{
	MUSINF_UNKNOWN   = 0,
	MUSINF_TRACK     = 1,
	MUSINF_LUMP      = 2,
	MUSINF_FILE      = 3,
	ENDOFMUSINFTYPES = 4
}
musicinftype_e;

class pl_entry_c
{
public:
	pl_entry_c();
	~pl_entry_c();

public:
	void Default(void);
	void CopyDetail(pl_entry_c &src);

	// Member vars....
	int number;

	musictype_t type;
	musicinftype_e infotype;

	epi::strent_c info;

private:
	// disable copy construct and assignment operator
	explicit pl_entry_c(pl_entry_c &rhs) { }
	pl_entry_c& operator= (pl_entry_c &rhs) { return *this; }
};


// Our playlist entry container
class pl_entry_container_c : public epi::array_c 
{
public:
	pl_entry_container_c() : epi::array_c(sizeof(pl_entry_c*)) {}
	~pl_entry_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	pl_entry_c* Find(int number);
	int GetSize() {	return array_entries; } 
	int Insert(pl_entry_c *p) { return InsertObject((void*)&p); }
	pl_entry_c* operator[](int idx) { return *(pl_entry_c**)FetchObject(idx); } 
};


// -------EXTERNALISATIONS-------

extern pl_entry_container_c playlist;		// -ACB- 2004/06/04 Implemented

bool DDF_ReadMusicPlaylist(void *data, int size);

#endif // __DDF_MUS_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
