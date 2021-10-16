//----------------------------------------------------------------------------
//  EDGE Humidity Music Player (HEADER)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2005-2009  The EDGE Team.
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

#ifndef __HUMDINGER__
#define __HUMDINGER__

#include "i_defs_al.h"

#include "humidity/humidity.h"

class humdinger_c
{
public:
	humdinger_c(Humidity::output_device_c *dev);
	~humdinger_c();

	enum status_e { NOT_LOADED, PLAYING, PAUSED, STOPPED };

private:
	static const int NUM_BUFFERS = 16;
	static const int BUFFER_SIZE = 16384;

	Humidity::output_device_c *hum_dev;

	Humidity::midi_song_c *cur_song;

	char *pcm_buf;

	ALuint buffers[NUM_BUFFERS];
	ALenum format;

	bool looping;
	int status;

    static ALuint music_source;

    bool CreateMusicSource();
    void DeleteMusicSource();
	bool StreamIntoBuffer(int buffer);

public:
	void SetVolume(float gain);

	int Open(byte *data, int length);  // -1 on error

	void Play(bool loop, float gain);
	void Pause(void);
	void Resume(void);
	void Stop(void);
	void Ticker(void);

	void Close(void);
};

humdinger_c *HumDingerInit(void);
void HumDingerTerm(void);

const char *HumDingerGetError(void);  // when HumDingerInit fails.

#endif  // __HUMDINGER__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
