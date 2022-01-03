//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Flat properties)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022 The EDGE-Classic community.
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

#ifndef __DDF_FLAT_H__
#define __DDF_FLAT_H__

#include "utility.h"

#include "types.h"

class flatdef_c
{
public:
	flatdef_c();
	~flatdef_c() {};

public:
	void Default(void);
	void CopyDetail(flatdef_c &src);

	// Member vars....
	epi::strent_c name;

	struct sfx_s *footstep;
	lumpname_c splash;
	//Lobo: item to spawn (or NULL).  The mobjdef pointer is only valid after
	// DDF_flatCleanUp() has been called.
	const mobjtype_c *effectobject;
	epi::strent_c effectobject_ref;

private:
	// disable copy construct and assignment operator
	explicit flatdef_c(flatdef_c &rhs) { }
	flatdef_c& operator=(flatdef_c &rhs) { return *this; }
};


// Our flatdefs container
class flatdef_container_c : public epi::array_c 
{
public:
	flatdef_container_c() : epi::array_c(sizeof(flatdef_c*)) {}
	~flatdef_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	flatdef_c* Find(const char *name);
	int GetSize() {	return array_entries; } 
	int Insert(flatdef_c *sw) { return InsertObject((void*)&sw); }
	flatdef_c* operator[](int idx) { return *(flatdef_c**)FetchObject(idx); } 
};

extern flatdef_container_c flatdefs; 	// -ACB- 2004/06/04 Implemented

bool DDF_ReadFlat(void *data, int size);

void DDF_ParseFLATS(const byte *data, int size);

#endif  /*__DDF_SWTH_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
