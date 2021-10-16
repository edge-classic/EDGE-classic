//----------------------------------------------------------------------------
//  EDGE MUS to Midi conversion
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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
//  Based on the MMUS2MID code from PrBoom (but heavily modified).
//
//  PrBoom a Doom port merged with LxDoom and LSDLDoom
//  based on BOOM, a modified and improved DOOM engine
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 1999-2000 by
//  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
//  
//----------------------------------------------------------------------------
//  
//  The PrBoom code was based on QMUS2MID by Sebastien Bacquet.
//
//----------------------------------------------------------------------------

#include "epi.h"
#include "bytearray.h"
#include "endianess.h"

#include "mus_2_midi.h"


namespace Mus2Midi
{
	const int MAX_TRACKS = 16;

	//
	// TRACK INFORMATION
	//
	class TrackInfo_c
	{
	public:
		TrackInfo_c() : data(), time(0), velocity(64), lastEvt(0) { }
		~TrackInfo_c() { }

		epi::bytearray_c data;  /* MIDI message stream */

		int  time;
		byte velocity;
		byte lastEvt;

		inline bool WriteByte(byte value)
		{
			data.Append(value);

			return true;
		}

		bool WriteVarLen(int value)
		{
			if (value < 0) value = 1;  // fatal error !

			int buffer = value & 0x7f;

			while ((value >>= 7) != 0)      // terminates because value unsigned
			{
				buffer <<= 8;               // note first value shifted in has bit 8 clear
				buffer |= 0x80;             // all succeeding values do not
				buffer += (value & 0x7f);
			}

			while (1)                     // write bytes out in opposite order
			{
				if (! WriteByte((byte)(buffer&0xff)))  // ensure buffer masked
					return false;

				if (buffer & 0x80)
					buffer >>= 8;
				else                        // terminate on the byte with bit 8 clear
					break;
			}

			return true;
		}

		bool WriteArray(const byte *values, int count)
		{
			for (; count > 0; values++, count--)
				if (! WriteByte(*values))
					return false;

			return true;
		}

		bool WriteEvent0(byte event, byte channel, bool nocomp)
		{
			event |= channel;

			if (event != lastEvt || nocomp)
			{
				lastEvt = event;

				return WriteByte(event);
			}

			return true;
		}

		bool WriteEvent1(byte event, byte channel, byte value, bool nocomp)
		{
			return WriteEvent0(event, channel, nocomp) && WriteByte(value);
		}

		bool WriteEvent2(byte event, byte channel, byte val1, byte val2, bool nocomp)
		{
			return WriteEvent0(event, channel, nocomp) &&
				WriteByte(val1) && WriteByte(val2);
		}
	};

	// 
	// MIDI INFORMATION
	//
	class MIDI_c                  /* a midi file */
	{
	public:
		MIDI_c()
		{
			for (int i = 0; i < MAX_TRACKS; i++)
				track[i] = new TrackInfo_c();
		}

		~MIDI_c()
		{
			for (int i = 0; i < MAX_TRACKS; i++)
				delete track[i];
		}

		int divisions;   /* number of ticks per quarter note */

		TrackInfo_c *track[MAX_TRACKS]; 
	};
}

// some macros to decode mus event bit fields

#define M_LAST(e)         ((byte)((e) & 0x80))
#define M_EVENT_TYPE(e)   ((byte)(((e) & 0x7F) >> 4))
#define M_CHANNEL(e)      ((byte)((e) & 0x0F))

// event types

typedef enum
{
	MUS_RELEASE_NOTE = 0,
	MUS_PLAY_NOTE    = 1,
	MUS_PITCH_WHEEL  = 2,
	MUS_SYSTEM       = 3,
	MUS_CONTROLLER   = 4,
	MUS_SCORE_END    = 6
}
mus_event_e;

// MUS format header structure

typedef struct
{
	char   ID[4];        // identifier "MUS"0x1A

	u16_t  score_len;  // length of music portion
	u16_t  score_start;   // offset of music portion
	u16_t  channels;     // count of primary channels
	u16_t  sec_channels;  // count of secondary channels
	u16_t  instr_count;     // number of instruments
	u16_t  dummy;
}
mus_header_t;

// MIDI event types

typedef enum
{
	MID_RELEASE_NOTE  = 0x80,
	MID_PLAY_NOTE     = 0x90,
	MID_KEY_PRESSURE  = 0xA0,
	MID_CONTROLLER    = 0xB0,
	MID_PROG_CHANGE   = 0xC0,
	MID_CHAN_PRESSURE = 0xD0,
	MID_PITCH_WHEEL   = 0xE0
}
midi_event_e;



namespace Mus2Midi
{
	// lookup table MUS -> MID controls 
	byte mus_control_to_midi[15] = 
	{
		0,     // Program change - not a MIDI control change

		0x00,  // Bank select               
		0x01,  // Modulation pot              
		0x07,  // Volume                  
		0x0A,  // Pan pot                 
		0x0B,  // Expression pot              
		0x5B,  // Reverb depth                
		0x5D,  // Chorus depth                
		0x40,  // Sustain pedal               
		0x43,  // Soft pedal                

		0x78,  // All sounds off              
		0x7B,  // All notes off               
		0x7E,  // Mono                    
		0x7F,  // Poly                    
		0x79   // Reset all controllers           
	};

	int mus_channel_to_midi[16] =
	{
		/* swaps channels 10 and 16 (Drum channels) */

		0, 1, 2, 3, 4, 5, 6, 7,
		8, 15, 10, 11, 12, 13, 14, 9
	};

	// some strings of bytes used in the midi format 

	const byte midikey[]   = { 0x00,0xff,0x59,0x02,0x00,0x00 };        // C major
	const byte miditempo[] = { 0x00,0xff,0x51,0x03,0x09,0xa3,0x1a };   // uS/qnote

	const byte midihdr[]   = { 'M','T','h','d', 0,0,0,6, 0,1 };  // length 6, format 1

	const byte trackhdr[]  = { 'M','T','r','k' };
	const byte trackend[]  = { 0x00,0xff,0x2f,0x00 };
}


namespace Mus2Midi
{
	// --- local variables and functions ---

	// Error Description
	char error_desc[256] = "FOO";
	char scratch_error[256];

	void SetError(const char *err, ...) GCCATTR((format (printf,1,2)));

	int ReadTime(const byte **musptrp);

	bool MidiEvent(TrackInfo_c *T, byte event, byte channel, int nocomp);

	void TWriteWord(byte *ptr, int value);
	void TWriteLong(byte *ptr, int value);

	void FixEndian(mus_header_t& header);
	bool DecodeMUS(const byte *mus, MIDI_c& info, int division, int nocomp);
	bool CompactMidi(MIDI_c& info, byte **mid, int *midlen);
}

void Mus2Midi::SetError(const char *err, ...)
{
	va_list argptr;

	error_desc[sizeof(error_desc)-1] = 0;

	// put actual error message on first line
	va_start(argptr, err);
	vsprintf(error_desc, err, argptr);
	va_end(argptr);

	// check for buffer overflow
	// ASSERT(error_desc[sizeof(error_desc)-1] == 0);
}

//
// ReadTime()
//
// Read a time value from the MUS buffer, advancing the position in it
//
// A time value is a variable length sequence of 8 bit bytes, with all
// but the last having bit 8 set.
//
// Passed a pointer to the pointer to the MUS buffer
// Returns the integer unsigned long time value there and advances the pointer
//
int Mus2Midi::ReadTime(const byte **musptrp)
{
	int timeval = 0;
	int cur_byte;

	do    // shift each byte read up in the result until a byte with bit 8 clear
	{
		cur_byte = *(*musptrp)++;
		timeval = (timeval << 7) + (cur_byte & 0x7F);
	}
	while(cur_byte & 0x80);

	return timeval;
}

void Mus2Midi::FixEndian(mus_header_t& header)
{
	header.score_len    = EPI_LE_U16(header.score_len);
	header.score_start  = EPI_LE_U16(header.score_start);
	header.channels     = EPI_LE_U16(header.channels);
	header.sec_channels = EPI_LE_U16(header.sec_channels);
	header.instr_count  = EPI_LE_U16(header.instr_count);
}

//
// Decode()
//
// Convert a memory buffer contain MUS data to an Allegro MIDI structure
// with specified time division and compression.
//
// Passed a pointer to the buffer containing MUS data, a pointer to the
// Allegro MIDI structure, the divisions, and a flag whether to compress.
//
// Returns 0 if successful, otherwise an error code (see mmus2mid.h).
//
bool Mus2Midi::DecodeMUS(const byte *mus, MIDI_c& info, int division, int nocomp)
{
	int MUSchannel, MIDIchannel=0;
	int i;
	const byte *musptr;
	size_t muslen;
	static mus_header_t header;

	// copy the MUS header and byte-swap it if necessary
	memcpy(&header, mus, sizeof(mus_header_t));
	FixEndian(header);

	// check some things and set length of MUS buffer from internal data
	muslen = header.score_len + header.score_start;

	if (muslen <= 0)
	{
		SetError("Bad MUS file: is empty !\n");
		return false;
	}

	if (header.channels > 15)       // MUSchannels + drum channel > 16
	{
		SetError("Bad MUS file: too many channels (%d)\n", header.channels);
		return false;
	}

	musptr = mus + header.score_start; // init musptr to start of score

	if (division == 0)
		division = Mus2Midi::DEF_DIVIS;

	// set the divisions (ticks per quarter note)
	info.divisions = division;

	// use track[0] for midi tempo/key track

	TrackInfo_c *T0 = info.track[0];

	if (! T0->WriteArray(midikey, sizeof(midikey)) ||    // key C major
		! T0->WriteArray(miditempo, sizeof(miditempo)))  // tempo uS/qnote 
	{
		return false;
	}

	// process the MUS events in the MUS buffer

	byte evt = 0;

	while ((size_t)(musptr - mus) < muslen)
	{
		// get a mus event, decode its type and channel fields

		byte raw_event = *musptr++;

		evt = M_EVENT_TYPE(raw_event);

		if (evt == MUS_SCORE_END)  // jff 1/23/98 use symbol
			break;

		MUSchannel = M_CHANNEL(raw_event);

		MIDIchannel = mus_channel_to_midi[MUSchannel];

		TrackInfo_c *T = info.track[MIDIchannel];

		T->WriteVarLen(T->time);
		T->time = 0;

		switch (evt)
		{
			case MUS_RELEASE_NOTE:
			{
				byte note = (*musptr++) & 0x7f;

				// killough 10/7/98: Fix noise problems by not allowing compression
				T->WriteEvent2(MID_PLAY_NOTE, MIDIchannel, note, 0, true);
				break;
			}

			case MUS_PLAY_NOTE:
			{
				byte note = *musptr++;

				if (note & 0x80)
				{
					note &= 0x7f;
					T->velocity = (*musptr++) & 0x7f;
				}

				T->WriteEvent2(MID_PLAY_NOTE, MIDIchannel, note, T->velocity, nocomp?true:false);
				break;
			}

			case MUS_PITCH_WHEEL:
			{
				int pitch = (int)(*musptr++) << 6;
				T->WriteEvent2(MID_PITCH_WHEEL, MIDIchannel, pitch & 0x7f,
						(pitch >> 7), nocomp?true:false);
				break;
			}

			case MUS_SYSTEM:
			{
				byte sys_ev = *musptr++;

				if (sys_ev < 10 || sys_ev > 14)
				{
					SetError("Bad MUS file (system event %d)", sys_ev);
					return false;
				}

				byte sys_mid = mus_control_to_midi[sys_ev];

				if (sys_ev == 12)  // Mono
				{
					// -AJA- 2003/10/31: replaced channels+1 with instr_count+1 
					T->WriteEvent2(MID_CONTROLLER, MIDIchannel, sys_mid,
						(byte)(header.instr_count+1), nocomp?true:false);
				}
				else
				{
					T->WriteEvent2(MID_CONTROLLER, MIDIchannel, sys_mid, 0, nocomp?true:false);
				}
				break;
			}

			case MUS_CONTROLLER:
			{
				byte ctl = *musptr++;
				byte param = (*musptr++) & 0x7f;

				if (ctl > 9)
				{
					SetError("Bad MUS file (control change %d)", ctl);
					return false;
				}

				if (ctl == 0)  // CTRL_INSTRUMENT
				{
					T->WriteEvent1(MID_PROG_CHANGE, MIDIchannel, param, nocomp?true:false);
				}
				else
				{
					T->WriteEvent2(MID_CONTROLLER, MIDIchannel,
						mus_control_to_midi[ctl], param, nocomp?true:false);
				}
				break;
			}

			default:
				SetError("Bad MUS file (illegal event %d)", evt);
				return false;
		}

		if (M_LAST(raw_event))
		{
			int delta_time = ReadTime(&musptr); // killough 10/7/98: make local

			// jff 3/13/98 update all tracks whether allocated yet or not
			for (i = 0; i < MAX_TRACKS; i++)
				info.track[i]->time += delta_time;
		}
	} 

	if (evt != MUS_SCORE_END)
	{
		SetError("MUS data corrupt ! (missing end)");
		return false;
	}

	// Now add an end of track to each mididata track

	for (i = 0; i < MAX_TRACKS; i++)
	{
		TrackInfo_c *T = info.track[i];

		if (T->data.Length() == 0)
			continue;

		if (! T->WriteArray(trackend, sizeof(trackend)))
		{
			SetError("Mus2Midi: Out of memory");
			return false;
		}
	}

	return true;
}


//
// TWriteLong()
//
// Write the length of a MIDI chunk to a midi buffer. The length is four
// bytes and is written byte-reversed for bigendian. The pointer to the
// midi buffer is advanced.
//
// Passed a pointer to the pointer to a midi buffer, and the length to write.
//
void Mus2Midi::TWriteWord(byte *ptr, int value)
{
	ptr[0] = (byte) ((value >>  8) & 0xff);
	ptr[1] = (byte) ((value)       & 0xff);
}

void Mus2Midi::TWriteLong(byte *ptr, int value)
{
	ptr[0] = (byte) ((value >> 24) & 0xff);
	ptr[1] = (byte) ((value >> 16) & 0xff);
	ptr[2] = (byte) ((value >>  8) & 0xff);
	ptr[3] = (byte) ((value)       & 0xff);
}

//
// CompactMidi()
//
// This routine converts an Allegro MIDI structure to a midi 1 format file
// in memory. It is used to support memory MUS -> MIDI conversion
//
// Passed a pointer to an Allegro MIDI structure, a pointer to a pointer to
// a buffer containing midi data, and a pointer to a length return.
//
// Returns true if successful, false on error (usually MEMALLOC)
//
bool Mus2Midi::CompactMidi(MIDI_c& info, byte **mid, int *midlen)
{
	// calculate how long the mid buffer must be, and allocate

	size_t total = 14;  // header

	int i;
	int num_track = 0;

	for (i=0; i< MAX_TRACKS; i++)
	{
		TrackInfo_c *T = info.track[i];

		if (T->data.Length() == 0)
			continue;

		total += 8 + T->data.Length();  // track header + track length
		num_track++;
	}

	if (num_track == 0)
	{
		SetError("Bad MUS file (no tracks)");
		return false;
	}

	*mid = new byte[total];

	if (*mid == NULL)
	{
		SetError("Mus2Midi: Out of memory");
		return false;
	}

	// write the midi header

	byte *midiptr = *mid;

	// fill in number of tracks and bigendian divisions (ticks/qnote)

	memcpy(midiptr, midihdr, sizeof(midihdr));
	midiptr += sizeof(midihdr);

	TWriteWord(midiptr, num_track);
	midiptr += 2;

	TWriteWord(midiptr, info.divisions);
	midiptr += 2;

	// write the tracks

	for (i=0; i < MAX_TRACKS; i++)
	{
		TrackInfo_c *T = info.track[i];

		if (T->data.Length() == 0)
			continue;

		memcpy(midiptr, trackhdr, sizeof(trackhdr));  // header
		midiptr += sizeof(trackhdr);

		TWriteLong(midiptr, T->data.Length());  // track length
		midiptr += 4;

		T->data.Read(0, midiptr, T->data.Length());  // track data
		midiptr += T->data.Length();
	}

	// return length information

	*midlen = (int)(midiptr - *mid);

	// ASSERT(midlen == total)

	return true;
}

//
// Convert
//
// Returns true if successful, false if an error occurred.
//
bool Mus2Midi::Convert(const byte *mus, int mus_len,
			byte **midi, int *midi_len, int division, bool nocomp)
{
	MIDI_c info;

	if (! DecodeMUS(mus, info, division, nocomp))
		return false;
	
	if (! CompactMidi(info, midi, midi_len))
		return false;

	return true;
}

//
// Free
//
void Mus2Midi::Free(byte *midi)
{
	if (midi)
		delete[] midi;
}

//
// ReturnError
//
const char *Mus2Midi::ReturnError(void)
{
	memcpy(scratch_error, error_desc, sizeof(scratch_error));
	memset(error_desc, '\0', sizeof(error_desc));

	return scratch_error;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
