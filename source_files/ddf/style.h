//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Styles)
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

#ifndef __DDF_STYLE_H__
#define __DDF_STYLE_H__

#include "utility.h"

#include "types.h"

class fontdef_c;
class colourmap_c;

//
// -AJA- 2004/11/14 Styles.ddf
//
class backgroundstyle_c
{
public:
	backgroundstyle_c();
	backgroundstyle_c(const backgroundstyle_c& rhs);
	~backgroundstyle_c();

	void Default();
	backgroundstyle_c& operator= (const backgroundstyle_c& rhs);

	rgbcol_t colour;
	percent_t translucency;

	epi::strent_c image_name;

	float scale;
	float aspect;
};

class textstyle_c
{
public:
	textstyle_c();
	textstyle_c(const textstyle_c& rhs);
	~textstyle_c();

	void Default();
	textstyle_c& operator= (const textstyle_c& rhs);

	const colourmap_c *colmap;

	percent_t translucency;

	fontdef_c *font;

	float scale;
	float aspect;

	// FIXME: horizontal and vertical alignment
};

class soundstyle_c
{
public:
	soundstyle_c();
	soundstyle_c(const soundstyle_c& rhs);
	~soundstyle_c();

	void Default();
	soundstyle_c& operator= (const soundstyle_c& rhs);

	struct sfx_s *begin;
	struct sfx_s *end;
	struct sfx_s *select;
	struct sfx_s *back;
	struct sfx_s *error;
	struct sfx_s *move;
	struct sfx_s *slider;

};

typedef enum
{
	SYLSP_Tiled = 0x0001,  // bg image should tile (otherwise covers whole area)
	SYLSP_TiledNoScale = 0x0002,  // bg image should tile (1:1 pixels)
}
style_special_e;

class styledef_c
{
public:
	styledef_c();
	~styledef_c();

public:
	void Default(void);
	void CopyDetail(const styledef_c &src);

	// Member vars....
	epi::strent_c name;

	backgroundstyle_c bg;

	// the four text styles
	enum
	{
		T_TEXT = 0, // main text style
		T_ALT,      // alternative text style
		T_TITLE,    // title style
		T_HELP,     // for help messages

		NUM_TXST
	};

	textstyle_c text[NUM_TXST];

	soundstyle_c sounds;

	style_special_e special;

private:
	// disable copy construct and assignment operator
	explicit styledef_c(styledef_c &rhs) { }
	styledef_c& operator=(styledef_c &rhs) { return *this; }
};


// Our styledefs container
class styledef_container_c : public epi::array_c 
{
public:
	styledef_container_c() : epi::array_c(sizeof(styledef_c*)) {}
	~styledef_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	int GetSize() {	return array_entries; } 
	int Insert(styledef_c *a) { return InsertObject((void*)&a); }
	styledef_c* operator[](int idx) { return *(styledef_c**)FetchObject(idx); } 

	// Search Functions
	styledef_c* Lookup(const char* refname);
};

extern styledef_container_c styledefs;
extern styledef_c *default_style;

bool DDF_ReadStyles(void *data, int size);

#endif  /*__DDF_STYLE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
