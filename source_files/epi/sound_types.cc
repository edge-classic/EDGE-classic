//------------------------------------------------------------------------
//  Sound Format Detection
//------------------------------------------------------------------------
// 
//  Copyright (c) 2022 - The EDGE Team.
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
//------------------------------------------------------------------------

#include "epi.h"

#include "path.h"
#include "sound_types.h"
#include "str_util.h"

#include "gme.h"
#include "xmp.h"

namespace epi
{

sound_format_e Sound_DetectFormat(byte *data, int song_len)
{
	// Start by trying the simple reliable header checks

	if (data[0] == 'R' && data[1] == 'I' &&
		data[2] == 'F'  && data[3] == 'F')
	{
		return FMT_WAV;
	}

	if (data[0] == 'f' && data[1] == 'L' &&
		data[2] == 'a'  && data[3] == 'C')
	{
		return FMT_FLAC;
	}

	if (data[0] == 'O' && data[1] == 'g' &&
		data[2] == 'g')
	{
		return FMT_OGG;
	}

	if ((data[0] == 'P' || data[0] == 'R') && data[1] == 'S' &&
		data[2] == 'I' && data[3] == 'D')
	{
		return FMT_SID;
	}

	if (data[0] == 'M' && data[1] == 'U' &&
		data[2] == 'S')
	{
		return FMT_MUS;
	}

	if (data[0] == 'M' && data[1] == 'T' &&
		data[2] == 'h' && data[3] == 'd')
	{
		return FMT_MIDI;
	}

	// XMI MIDI
	if (song_len > 12 && data[0] == 'F' && data[1] == 'O' &&
		data[2] == 'R' && data[3] == 'M' &&
		data[8] == 'X' && data[9] == 'D' &&
		data[10] == 'I' && data[11] == 'R')
	{
		return FMT_MIDI;
	}

	// GMF MIDI
	if (data[0] == 'G' && data[1] == 'M' &&
		data[2] == 'F' && data[3] == '\x1')
	{
		return FMT_MIDI;
	}

	// Electronic Arts MIDI
	if (song_len > data[0] && data[0] >= 0x5D)
	{
		int offset = data[0] - 0x10;
		if (data[offset] == 'r' && data[offset+1] == 's' && data[offset+2] == 'x' &&
			data[offset+3] == 'x' && data[offset+4] == '}' && data[offset+5] == 'u')
			return FMT_MIDI;
	}

	// Assume gzip format is VGZ and will be handled
	// by the VGM library
	if ((data[0] == 0x1f && data[1] == 0x8b &&
		data[2] == 0x08) || (data[0] == 'V' && data[1] == 'g' &&
		data[2] == 'm'  && data[3] == ' '))
	{
		return FMT_VGM;
	}

	// Moving on to more specialized or less reliable detections

	if (!std::string(gme_identify_header(data)).empty())
	{
		return FMT_GME;
	}

	if (xmp_test_module_from_memory(data, song_len, NULL) == 0)
		return FMT_XMP;

	if ((data[0] == 'I' && data[1] == 'D' && data[2] == '3') ||
		(data[0] == 0xFF && (data[1] >> 4 & 0xF)))
	{
		return FMT_MP3;
	}

	if (data[0] == 0x3)
	{
		return FMT_DOOM;
	}

	if (data[0] == 0x0)
	{
		return FMT_SPK;
	}

	return FMT_Unknown;
}

sound_format_e Sound_FilenameToFormat(const std::string& filename)
{
	std::string ext = epi::PATH_GetExtension(filename.c_str());

	str_lower(ext);

	if (ext == ".wav" || ext == ".wave")
		return FMT_WAV;

	if (ext == ".flac")
		return FMT_FLAC;

	if (ext == ".ogg")
		return FMT_OGG;

	if (ext == ".mp3")
		return FMT_MP3;

	if (ext == ".sid" || ext == ".psid")
		return FMT_SID;

	if (ext == ".mus")
		return FMT_MUS;

	if (ext == ".mid" || ext == ".midi" || ext == ".xmi" || ext == ".rmi" || ext == ".rmid")
		return FMT_MIDI;

	if (ext == ".mod" || ext == ".m15" || ext == ".flx" || ext == ".wow" || ext == ".dbm" ||
		ext == ".digi" || ext == ".emod" || ext == ".med" || ext == ".mtn" || ext == ".okt" ||
		ext == ".sfx" || ext == ".mgt" || ext == ".669" || ext == ".far" || ext == ".fnk" ||
		ext == ".imf" || ext == ".it" || ext == ".liq" || ext == ".mdl" || ext == ".mtm" || 
		ext == ".ptm" || ext == ".rtm" || ext == ".s3m" || ext == ".stm" || ext == ".ult" ||
		ext == ".xm" || ext == ".amf" || ext == ".gdm" || ext == ".stx" || ext == ".abk" ||
		ext == ".psm" || ext == ".j2b" || ext == ".mfp" || ext == ".smp" || ext == ".mmdc" ||
		ext == ".stim" || ext == ".umx")
		return FMT_XMP;

	if (ext == ".vgm" || ext == ".vgz")
		return FMT_VGM;

	if (ext == ".ay" || ext == ".gbs" || ext == ".gym" || ext == ".hes" || ext == ".nsf" ||
		ext == ".sap" || ext == ".spc")
		return FMT_GME;

	// Not sure if these will ever be encountered in the wild, but according to the VGMPF Wiki
	// they are valid DMX file extensions
	if (ext == ".dsp" || ext == ".pcs" || ext == ".gsp" || ext == ".gsw")
		return FMT_DOOM;

	return FMT_Unknown;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
