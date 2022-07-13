//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Colourmaps)
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

#ifndef __DDF_COLORMAP_H__
#define __DDF_COLORMAP_H__

#include "utility.h"

#include "types.h"

// -AJA- 1999/07/09: colmap.ddf structures.

typedef enum
{
	// Default value
	COLSP_None     = 0x0000,
	
	// don't apply gun-flash type effects (looks silly for fog)
	COLSP_NoFlash  = 0x0001,

	// for fonts, apply the FONTWHITEN mapping first
	COLSP_Whiten   = 0x0002
}
colourspecial_e;

typedef struct colmapcache_s
{
	byte *data;
}
colmapcache_t;

class colourmap_c
{
public:
	colourmap_c();
	~colourmap_c();

public:
	void CopyDetail(colourmap_c &src);
	void Default();
	
	// Member vars...
	epi::strent_c name;

	lumpname_c lump_name;

	int start;
	int length;

	colourspecial_e special;

	// colours for GL renderer
	rgbcol_t gl_colour;


	rgbcol_t font_colour;  // (computed only, not in DDF)

	colmapcache_t cache;

	void *analysis;

	bool needs_doom_palette; // For built in colormaps like text whitening, night vision, etc

private:
	// disable copy construct and assignment operator
	explicit colourmap_c(colourmap_c &rhs) { }
	colourmap_c& operator=(colourmap_c &rhs) { return *this; }
};


// Colourmap container
class colourmap_container_c : public epi::array_c
{
public:
	colourmap_container_c();
	~colourmap_container_c();

private:
	void CleanupObject(void *obj);

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(colourmap_c *c) { return InsertObject((void*)&c); }
	colourmap_c* operator[](int idx) { return *(colourmap_c**)FetchObject(idx); } 

	// Search Functions
	colourmap_c* Lookup(const char* refname);
};

extern colourmap_container_c colourmaps;	// -ACB- 2004/06/10 Implemented

bool DDF_ReadColourMaps(void *data, int size);

void DDF_ColourmapAddRaw(const char *lump_name, int size);

#endif /* __DDF_COLORMAP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
