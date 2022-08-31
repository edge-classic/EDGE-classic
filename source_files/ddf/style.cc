//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Styles)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2005  The EDGE Team.
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
// Style Setup and Parser Code
//

#include "local.h"

#include "utility.h"

#include "font.h"
#include "style.h"

#undef  DF
#define DF  DDF_FIELD

styledef_c *default_style;

static void DDF_StyleGetSpecials(const char *info, void *storage);

styledef_container_c styledefs;

#undef  DDF_CMD_BASE
#define DDF_CMD_BASE  dummy_bgstyle
static backgroundstyle_c dummy_bgstyle;

static const commandlist_t background_commands[] =
{
	DF("COLOUR", colour, DDF_MainGetRGB),
	DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),
    DF("IMAGE", image_name, DDF_MainGetString),
    DF("SCALE",  scale,  DDF_MainGetFloat),
    DF("ASPECT", aspect, DDF_MainGetFloat),

	DDF_CMD_END
};


#undef  DDF_CMD_BASE
#define DDF_CMD_BASE  dummy_textstyle
static textstyle_c  dummy_textstyle;

static const commandlist_t text_commands[] =
{
    DF("COLOURMAP", colmap, DDF_MainGetColourmap),
	DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),
    DF("FONT",   font,   DDF_MainLookupFont),
    DF("SCALE",  scale,  DDF_MainGetFloat),
    DF("ASPECT", aspect, DDF_MainGetFloat),

	DDF_CMD_END
};


#undef  DDF_CMD_BASE
#define DDF_CMD_BASE  dummy_soundstyle
static soundstyle_c dummy_soundstyle;

static const commandlist_t sound_commands[] =
{
	DF("BEGIN",  begin,  DDF_MainLookupSound),
	DF("END",    end,    DDF_MainLookupSound),
	DF("SELECT", select, DDF_MainLookupSound),
	DF("BACK",   back,   DDF_MainLookupSound),
	DF("ERROR",  error,  DDF_MainLookupSound),
	DF("MOVE",   move,   DDF_MainLookupSound),
	DF("SLIDER", slider, DDF_MainLookupSound),

	DDF_CMD_END
};


static styledef_c* dynamic_style;

#undef  DDF_CMD_BASE
#define DDF_CMD_BASE  dummy_style
static styledef_c dummy_style;

static const commandlist_t style_commands[] =
{
	// sub-commands
	DDF_SUB_LIST("BACKGROUND", bg, background_commands),
	DDF_SUB_LIST("TEXT",  text[0], text_commands),
	DDF_SUB_LIST("ALT",   text[1], text_commands),
	DDF_SUB_LIST("TITLE", text[2], text_commands),
	DDF_SUB_LIST("HELP",  text[3], text_commands),
	DDF_SUB_LIST("SOUND",  sounds, sound_commands),

    DF("SPECIAL", special, DDF_StyleGetSpecials),

	DDF_CMD_END
};


//
//  DDF PARSE ROUTINES
//

static void StyleStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New style entry is missing a name!");
		name = "STYLE_WITH_NO_NAME";
	}

	// replaces an existing entry?
	dynamic_style = styledefs.Lookup(name);

	if (extend)
	{
		if (! dynamic_style)
			DDF_Error("Unknown style to extend: %s\n", name);
		return;
	}

	if (dynamic_style)
	{
		dynamic_style->Default();
		return;
	}

	// not found, create a new one
	dynamic_style = new styledef_c;
	dynamic_style->name = name;

	styledefs.Insert(dynamic_style);
}


static void StyleParseField(const char *field, const char *contents,
							int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("STYLE_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_MainParseField(style_commands, field, contents, (byte *)dynamic_style))
		return;  // OK

	DDF_WarnError("Unknown styles.ddf command: %s\n", field);
}


static void StyleFinishEntry(void)
{
	// TODO: check stuff
}


static void StyleClearAll(void)
{
	I_Warning("Ignoring #CLEARALL in styles.ddf\n");
}


bool DDF_ReadStyles(void *data, int size)
{
	readinfo_t styles;

	styles.memfile = (char*)data;
	styles.memsize = size;
	styles.tag = "STYLES";
	styles.entries_per_dot = 2;

	if (styles.memfile)
	{
		styles.message = NULL;
		styles.filename = NULL;
		styles.lumpname = "DDFSTYLE";
	}
	else
	{
		styles.message = "DDF_InitStyles";
		styles.filename = "styles.ddf";
		styles.lumpname = NULL;
	}

	styles.start_entry  = StyleStartEntry;
	styles.parse_field  = StyleParseField;
	styles.finish_entry = StyleFinishEntry;
	styles.clear_all    = StyleClearAll;

	return DDF_MainReadFile(&styles);
}


void DDF_StyleInit(void)
{
	styledefs.Clear();
}


void DDF_StyleCleanUp(void)
{
	if (styledefs.GetSize() == 0)
		I_Error("There are no styles defined in DDF !\n");
	
	default_style = styledefs.Lookup("DEFAULT");

	if (! default_style)
		I_Error("Styles.ddf is missing the [DEFAULT] style.\n");
	else if (! default_style->text[0].font)
		I_Warning("The [DEFAULT] style is missing TEXT.FONT\n");

	styledefs.Trim();
}


static specflags_t style_specials[] =
{
    {"TILED", SYLSP_Tiled, 0},
    {"TILED_NOSCALE", SYLSP_TiledNoScale, 0},
	{"STRETCH_FULLSCREEN", SYLSP_StretchFullScreen, 0},
    {NULL, 0, 0}
};

void DDF_StyleGetSpecials(const char *info, void *storage)
{
	style_special_e *dest = (style_special_e *)storage;

	int flag_value;

	switch (DDF_MainCheckSpecialFlag(info, style_specials,
	 								 &flag_value, true, false))
	{
		case CHKF_Positive:
			*dest = (style_special_e)(*dest | flag_value);
			break;

		case CHKF_Negative:
			*dest = (style_special_e)(*dest & ~flag_value);
			break;

		case CHKF_User:
		case CHKF_Unknown:
			DDF_WarnError("Unknown style special: %s", info);
			break;
	}
}

// --> backgroundstyle_c definition class

//
// backgroundstyle_c Constructor
//
backgroundstyle_c::backgroundstyle_c()
{
	Default();
}

//
// backgroundstyle_c Copy constructor
//
backgroundstyle_c::backgroundstyle_c(const backgroundstyle_c &rhs)
{
	*this = rhs;
}

//
// backgroundstyle_c Destructor
//
backgroundstyle_c::~backgroundstyle_c()
{
}

//
// backgroundstyle_c::Default()
//
void backgroundstyle_c::Default()
{
	colour = RGB_NO_VALUE;
	translucency = PERCENT_MAKE(100);

	image_name.clear();	

	scale  = 1.0f;
	aspect = 1.0f;
}

//
// backgroundstyle_c assignment operator
//
backgroundstyle_c& backgroundstyle_c::operator= (const backgroundstyle_c &rhs)
{
	if (&rhs != this)
	{
		colour = rhs.colour;
		translucency = rhs.translucency;

		image_name = rhs.image_name;

		scale   = rhs.scale;
		aspect  = rhs.aspect;
	}
		
	return *this;
}

// --> textstyle_c definition class

//
// textstyle_c Constructor
//
textstyle_c::textstyle_c()
{
	Default();
}

//
// textstyle_c Copy constructor
//
textstyle_c::textstyle_c(const textstyle_c &rhs)
{
	*this = rhs;
}

//
// textstyle_c Destructor
//
textstyle_c::~textstyle_c()
{
}

//
// textstyle_c::Default()
//
void textstyle_c::Default()
{
	colmap = NULL;
	translucency = PERCENT_MAKE(100);

	font   = NULL;
	scale  = 1.0f;
	aspect = 1.0f;
}

//
// textstyle_c assignment operator
//
textstyle_c& textstyle_c::operator= (const textstyle_c &rhs)
{
	if (&rhs != this)
	{
		colmap = rhs.colmap;
		translucency = rhs.translucency;

		font   = rhs.font;
		scale  = rhs.scale;
		aspect = rhs.aspect;
	}
		
	return *this;
}

// --> soundstyle_c definition class

//
// soundstyle_c Constructor
//
soundstyle_c::soundstyle_c()
{
	Default();
}

//
// soundstyle_c Copy constructor
//
soundstyle_c::soundstyle_c(const soundstyle_c &rhs)
{
	*this = rhs;
}

//
// soundstyle_c Destructor
//
soundstyle_c::~soundstyle_c()
{
}

//
// soundstyle_c::Default()
//
void soundstyle_c::Default()
{
	begin  = NULL;
	end    = NULL;
	select = NULL;
	back   = NULL;
	error  = NULL;
	move   = NULL;
	slider = NULL;
}

//
// soundstyle_c assignment operator
//
soundstyle_c& soundstyle_c::operator= (const soundstyle_c &rhs)
{
	if (&rhs != this)
	{
		begin  = rhs.begin;
		end    = rhs.end;
		select = rhs.select;
		back   = rhs.back;
		error  = rhs.error;
		move   = rhs.move;
		slider = rhs.slider;
	}

	return *this;
}

// --> style definition class

//
// styledef_c Constructor
//
styledef_c::styledef_c() : name()
{
	Default();
}

//
// styledef_c Destructor
//
styledef_c::~styledef_c()
{
}


//
// styledef_c::CopyDetail()
//
void styledef_c::CopyDetail(const styledef_c &src)
{
	bg = src.bg;

	for (int T = 0; T < NUM_TXST; T++)
		text[T] = src.text[T];

	sounds = src.sounds;

	special = src.special;
}

//
// styledef_c::Default()
//
void styledef_c::Default()
{
	bg.Default();

	for (int T = 0; T < NUM_TXST; T++)
		text[T].Default();

	sounds.Default();

	special = (style_special_e) 0;
}


// --> map definition container class

//
// styledef_container_c::CleanupObject()
//
void styledef_container_c::CleanupObject(void *obj)
{
	styledef_c *m = *(styledef_c**)obj;

	if (m) delete m;
}

//
// styledef_container_c::Lookup()
//
// Finds a styledef by name, returns NULL if it doesn't exist.
//
styledef_c* styledef_container_c::Lookup(const char *refname)
{
	epi::array_iterator_c it;
	styledef_c *m;

	if (!refname || !refname[0])
		return NULL;

	for (it = GetTailIterator(); it.IsValid(); it--)
	{
		m = ITERATOR_TO_TYPE(it, styledef_c*);
		if (DDF_CompareName(m->name.c_str(), refname) == 0)
			return m;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
