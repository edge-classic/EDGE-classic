/*
    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	 This program is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    timidity.h
*/

#ifndef __TIMIDITY_API_H__
#define __TIMIDITY_API_H__

extern bool Timidity_Init(const char *conf_file, int rate, int channels);
extern void Timidity_Close(void);

extern struct MidiSong *Timidity_LoadSong(const byte *data, int length);
extern void Timidity_FreeSong(struct MidiSong *song);

extern void Timidity_Start(struct MidiSong *song);
extern void Timidity_Stop(void);
extern int  Timidity_Active(void);

extern void Timidity_SetVolume(int volume);
extern void Timidity_QuietFactor(int factor);
extern int  Timidity_PlaySome(s16_t *stream, int samples);

/// extern const char *Timidity_Error(void);

#endif /* __TIMIDITY_API_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
