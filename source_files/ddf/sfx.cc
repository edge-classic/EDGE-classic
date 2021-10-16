//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Sounds)
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
//
// -KM- 1998/09/27 Finished :-)
//

#include "local.h"

#include "sfx.h"

static sfxdef_c *dynamic_sfx;

sfxdef_container_c sfxdefs;

#define DDF_CMD_BASE  dummy_sfx
static sfxdef_c dummy_sfx;

static const commandlist_t sfx_commands[] =
{
	DDF_FIELD("LUMP_NAME", lump_name, DDF_MainGetLumpName),
	DDF_FIELD("FILE_NAME", file_name, DDF_MainGetString),
	DDF_FIELD("SINGULAR",  singularity, DDF_MainGetNumeric),
	DDF_FIELD("PRIORITY",  priority, DDF_MainGetNumeric),
	DDF_FIELD("VOLUME",    volume,   DDF_MainGetPercent),
	DDF_FIELD("LOOP",      looping,  DDF_MainGetBoolean),
	DDF_FIELD("PRECIOUS",  precious, DDF_MainGetBoolean),
	DDF_FIELD("MAX_DISTANCE", max_distance, DDF_MainGetFloat),

	DDF_CMD_END
};


//
//  DDF PARSE ROUTINES
//

static void SoundStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New sound entry is missing a name!");
		name = "SOUND_WITH_NO_NAME";
	}

	dynamic_sfx = sfxdefs.Lookup(name);

	if (extend)
	{
		if (! dynamic_sfx)
			DDF_Error("Unknown sound to extend: %s\n", name);
		return;
	}

	// replaces an existing entry?
	if (dynamic_sfx)
	{
		// maintain the internal ID
		int id = dynamic_sfx->normal.sounds[0];

		dynamic_sfx->Default();

		dynamic_sfx->normal.num = 1;
		dynamic_sfx->normal.sounds[0] = id;
		return;
	}

	// not found, create a new one
	dynamic_sfx = new sfxdef_c;

	dynamic_sfx->name = name;

	sfxdefs.Insert(dynamic_sfx);

	// give it a self-referencing ID number
	dynamic_sfx->normal.sounds[0] = sfxdefs.GetSize()-1;
	dynamic_sfx->normal.num = 1;
}


static void SoundParseField(const char *field, const char *contents,
    int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("SOUND_PARSE: %s = %s;\n", field, contents);
#endif

	// -AJA- ignore these for backwards compatibility
	if (DDF_CompareName(field, "BITS") == 0 ||
	    DDF_CompareName(field, "STEREO") == 0)
		return;

	if (DDF_MainParseField(sfx_commands, field, contents, (byte *)dynamic_sfx))
		return;  // OK

	DDF_WarnError("Unknown sounds.ddf command: %s\n", field);
}


static void SoundFinishEntry(void)
{
	if (!dynamic_sfx->lump_name[0] &&
		!(dynamic_sfx->file_name && dynamic_sfx->file_name[0]))
	{
		DDF_Error("Missing LUMP_NAME or FILE_NAME for sound.\n");
	}
}


static void SoundClearAll(void)
{
	I_Warning("Ignoring #CLEARALL in sounds.ddf\n");
}


bool DDF_ReadSFX(void *data, int size)
{
	readinfo_t sfx_r;

	sfx_r.memfile = (char*)data;
	sfx_r.memsize = size;
	sfx_r.tag = "SOUNDS";
	sfx_r.entries_per_dot = 8;

	if (sfx_r.memfile)
	{
		sfx_r.message = NULL;
		sfx_r.filename = NULL;
		sfx_r.lumpname = "DDFSFX";
	}
	else
	{
		sfx_r.message = "DDF_InitSounds";
		sfx_r.filename = "sounds.ddf";
		sfx_r.lumpname = NULL;
	}

	sfx_r.start_entry  = SoundStartEntry;
	sfx_r.parse_field  = SoundParseField;
	sfx_r.finish_entry = SoundFinishEntry;
	sfx_r.clear_all    = SoundClearAll;

	return DDF_MainReadFile(&sfx_r);
}

void DDF_SFXInit(void)
{
	sfxdefs.Clear();
}

void DDF_SFXCleanUp(void)
{
	sfxdefs.Trim();
}

//
// DDF_MainLookupSound
//
// Lookup the sound specified.
//
// -ACB- 1998/07/08 Checked the S_sfx table for sfx names.
// -ACB- 1998/07/18 Removed to the need set *currentcmdlist[commandref].data to -1
// -KM- 1998/09/27 Fixed this func because of sounds.ddf
// -KM- 1998/10/29 sfx_t finished
//
void DDF_MainLookupSound(const char *info, void *storage)
{
	sfx_t **dest = (sfx_t **)storage;

	SYS_ASSERT(info && storage);

	*dest = sfxdefs.GetEffect(info);
}

// --> Sound Effect Definition Class

//
// sfxdef_c Constructor
//
sfxdef_c::sfxdef_c() : name()
{
	Default();
}

//
// sfxdef_c Destructor
//
sfxdef_c::~sfxdef_c()
{
}

//
// sfxdef_c::CopyDetail()
//
void sfxdef_c::CopyDetail(sfxdef_c &src)
{
	lump_name = src.lump_name;
	file_name = src.file_name;

	// clear the internal sfx_t (ID would be wrong)
	normal.sounds[0] = 0;
	normal.num = 0;

	singularity = src.singularity;      // singularity
	priority = src.priority;    		// priority (lower is more important)
	volume = src.volume; 				// volume
	looping = src.looping;  			// looping
	precious = src.precious;  			// precious
	max_distance = src.max_distance; 	// max_distance
}

//
// sfxdef_c::Default()
//
void sfxdef_c::Default()
{
	lump_name.clear();
	file_name.clear();

	normal.sounds[0] = 0;
	normal.num = 0;

	singularity = 0;      			// singularity
	priority = 999;    				// priority (lower is more important)
	volume = PERCENT_MAKE(100); 	// volume
	looping = false;  				// looping
	precious = false;  				// precious
	max_distance = S_CLIPPING_DIST; // max_distance
}


// --> Sound Effect Definition Containter Class

//
// sfxdef_container_c::CleanupObject()
//
void sfxdef_container_c::CleanupObject(void *obj)
{
	sfxdef_c *s = *(sfxdef_c**)obj;

	if (s)
		delete s;

	return;
}

static int strncasecmpwild(const char *s1, const char *s2, int n)
{
	int i = 0;

	for (i = 0; s1[i] && s2[i] && i < n; i++)
	{
		if ((toupper(s1[i]) != toupper(s2[i])) && (s1[i] != '?') && (s2[i] != '?'))
			break;
	}
	// -KM- 1999/01/29 If strings are equal return equal.
	if (i == n)
		return 0;

	if (s1[i] == '?' || s2[i] == '?')
		return 0;

	return s1[i] - s2[i];
}


//
// sfxdef_container_c::GetEffect()
//
// FIXME!! Remove error param hack
// FIXME!! Cache results for those we create
//
sfx_t* sfxdef_container_c::GetEffect(const char *name, bool error)
{
	epi::array_iterator_c it, last;
	int count;

	sfxdef_c *si;
	sfx_t *r;

	// NULL Sound
	if (!name || !name[0] || DDF_CompareName(name, "NULL")==0)
		return NULL;

	// count them
	for (count=0, it=GetTailIterator(); 
		it.IsValid() && it.GetPos() >= 0; 
		it--)
	{
		si = ITERATOR_TO_TYPE(it, sfxdef_c*);
		
		if (strncasecmpwild(name, si->name.c_str(), 8) == 0)
		{
			count++;
			last = it;
		}
	}

	if (count == 0)
	{
		if (error)
			DDF_WarnError("Unknown SFX: '%.8s'\n", name);

		return NULL;
	}

	// -AJA- optimisation to save some memory
	if (count == 1)
	{
		si = ITERATOR_TO_TYPE(last, sfxdef_c*);
		r = &si->normal;

		SYS_ASSERT(r->num == 1);

		return r;
	}

	//
	// allocate elements.  Uses (count-1) since sfx_t already includes
	// the first integer.
	// 
	r = (sfx_t*) new byte[sizeof(sfx_t) + ((count-1) * sizeof(int))];

	// now store them
	for (r->num=0, it=GetTailIterator(); 
		it.IsValid() && it.GetPos() >= 0; 
		it--)
	{
		si = ITERATOR_TO_TYPE(it, sfxdef_c*);
		
		if (strncasecmpwild(name, si->name.c_str(), 8) == 0)
			r->sounds[r->num++] = it.GetPos();
	}

	SYS_ASSERT(r->num == count);

	return r;
}

//
// sfxdef_container_c::Lookup()
//
sfxdef_c* sfxdef_container_c::Lookup(const char *name)
{
	epi::array_iterator_c it;
	
	for (it=GetIterator(0); it.IsValid(); it++)
	{
		sfxdef_c *s = ITERATOR_TO_TYPE(it, sfxdef_c*);

		if (DDF_CompareName(s->name.c_str(), name) == 0)
			return s;
	}
	
	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
