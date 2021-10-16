//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Music Playlist Handling)
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

#include "local.h"

#include "playlist.h"

static pl_entry_c *dynamic_plentry;

pl_entry_container_c playlist;

//
// DDF_MusicParseInfo
//
// Parses the music information given.
//
static void DDF_MusicParseInfo(const char *info)
{
	static const char *const musstrtype[] = { "UNKNOWN", "CD", "MIDI", "MUS", "OGG", "MP3", NULL };
	static const char *const musinftype[] = { "UNKNOWN", "TRACK", "LUMP", "FILE", NULL };

	char charbuff[256];
	int pos,i;

	// Get the music type
	i=0;
	pos=0;

	while (info[pos] != ':' && i<255)
	{
		if (info[i] == '\0')
			DDF_Error("DDF_MusicParseInfo: Premature end of music info\n");

		charbuff[i] = info[pos];

		i++;
		pos++;
	}

	if (i==255)
		DDF_Error("DDF_MusicParseInfo: Music info too big\n");

	// -AJA- terminate charbuff with trailing \0.
	charbuff[i] = 0;

	i=MUS_UNKNOWN;
	while (i!=ENDOFMUSTYPES && stricmp(charbuff, musstrtype[i]) != 0)
		i++;

	if (i==ENDOFMUSTYPES)
		DDF_Error("DDF_MusicParseInfo: Unknown music type: '%s'\n", charbuff);
	else
		dynamic_plentry->type = (musictype_t)i;

	// Data Type
	i=0;
	pos++;
	while (info[pos] != ':' && i<255)
	{
		if (info[pos] == '\0')
			DDF_Error("DDF_MusicParseInfo: Premature end of music info\n");

		charbuff[i] = info[pos];

		pos++;
		i++;
	}

	if (i==255)
		DDF_Error("DDF_MusicParseInfo: Music info too big\n");

	// -AJA- terminate charbuff with trailing \0.
	charbuff[i] = 0;

	i=MUSINF_UNKNOWN;
	while (musinftype[i] != NULL && stricmp(charbuff, musinftype[i]) != 0)
		i++;

	if (i==ENDOFMUSINFTYPES)
		DDF_Error("DDF_MusicParseInfo: Unknown music info: '%s'\n", charbuff);
	else
		dynamic_plentry->infotype = (musicinftype_e)i; // technically speaking this is proper

	// Remained is the string reference: filename/lumpname/track-number
	pos++;
	dynamic_plentry->info.Set(&info[pos]);

	return;
}


//
//  DDF PARSE ROUTINES
//

static void PlaylistStartEntry(const char *name, bool extend)
{	
	int number = MAX(0, atoi(name));

	if (number == 0)
		DDF_Error("Bad music number in playlist.ddf: %s\n", name);

	dynamic_plentry = playlist.Find(number);

	if (extend)
	{
		if (! dynamic_plentry)
			DDF_Error("Unknown playlist to extend: %s\n", name);
		return;
	}

	// replaces an existing entry
	if (dynamic_plentry)
	{
		dynamic_plentry->Default();
		return;
	}

	// not found, create a new entry
	dynamic_plentry = new pl_entry_c;

	dynamic_plentry->number = number;

	playlist.Insert(dynamic_plentry);
}


static void PlaylistParseField(const char *field, const char *contents,
		int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("PLAYLIST_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_CompareName(field, "MUSICINFO") == 0)
	{
		DDF_MusicParseInfo(contents);
		return;
	}

	DDF_WarnError("Unknown playlist.ddf command: %s\n", field);
}


static void PlaylistFinishEntry(void)
{
	// nothing needed
}


static void PlaylistClearAll(void)
{
	// 100% safe to just remove all entries
	playlist.Clear();
}


//
// DDF_ReadMusicPlaylist
//
bool DDF_ReadMusicPlaylist(void *data, int size)
{
	readinfo_t playlistinfo;

	playlistinfo.memfile = (char*)data;
	playlistinfo.memsize = size;
	playlistinfo.tag = "PLAYLISTS";
	playlistinfo.entries_per_dot = 3;

	if (playlistinfo.memfile)
	{
		playlistinfo.message  = NULL;
		playlistinfo.filename = NULL;
		playlistinfo.lumpname = "DDFPLAY";
	}
	else
	{
		playlistinfo.message  = "DDF_InitMusicPlaylist";
		playlistinfo.filename = "playlist.ddf";
		playlistinfo.lumpname = NULL;
	}

	playlistinfo.start_entry  = PlaylistStartEntry;
	playlistinfo.parse_field  = PlaylistParseField;
	playlistinfo.finish_entry = PlaylistFinishEntry;
	playlistinfo.clear_all    = PlaylistClearAll;

	return DDF_MainReadFile(&playlistinfo);
}

//
// DDF_MusicPlaylistInit
//
void DDF_MusicPlaylistInit(void)
{
	// -ACB- 2004/05/04 Use container 
	playlist.Clear();
}

//
// DDF_MusicPlaylistCleanUp
//
void DDF_MusicPlaylistCleanUp(void)
{
	// -ACB- 2004/05/04 Cut our playlist down to size
	playlist.Trim();
}

// --> pl_entry_c class

//
// pl_entry_c constructor
//
pl_entry_c::pl_entry_c() : number(0)
{
	Default();
}

//
// pl_entry_c destructor
//
pl_entry_c::~pl_entry_c()
{
}


//
// pl_entry_c::CopyDetail()
//
// Copy everything with exception ddf identifier
//
void pl_entry_c::CopyDetail(pl_entry_c &src)
{
	type = src.type;
	infotype = src.infotype;
	info = src.info;
}

//
// pl_entry_c::Default()
// 
void pl_entry_c::Default()
{
	type = MUS_UNKNOWN;     
	infotype = MUSINF_UNKNOWN;
	info.clear();             
}


// --> pl_entry_containter_c class

//
// pl_entry_container_c::CleanupObject()
//
void pl_entry_container_c::CleanupObject(void *obj)
{
	pl_entry_c *p = *(pl_entry_c**)obj;

	if (p)
		delete p;

	return;
}

//
// pl_entry_c* pl_entry_container_c::Find()
//
pl_entry_c* pl_entry_container_c::Find(int number)
{
	epi::array_iterator_c it;
	pl_entry_c *p;

	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		p = ITERATOR_TO_TYPE(it, pl_entry_c*);
		if (p->number == number)
			return p;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
