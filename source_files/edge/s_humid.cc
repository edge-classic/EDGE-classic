//----------------------------------------------------------------------------
//  EDGE Humidity Music Player
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
//
//  Based on "oggplayer" code (Andrew Baker)
//
//  TODO: the mechanics here are very similar to the oggplayer_c class
//        (e.g. Play and Stop methods are identical).  Maybe factor out
//        this common behaviour (e.g. base class or mixin).
//
//  FIXME: doesn't handle HARDWARE devices properly (don't need OpenAL).
//

#include "i_defs.h"
#include "i_defs_al.h"

#include "endianess.h"
#include "mus_2_midi.h"

#include "humidity/hum_buffer.h"

#include "s_humid.h"

static bool humding_inited = false;
ALuint humdinger_c::music_source = NO_SOURCE;

//
// humdinger_c Constructor
//
humdinger_c::humdinger_c(Humidity::output_device_c *dev) :
	hum_dev(dev), cur_song(NULL), status(NOT_LOADED)
{
	pcm_buf = new char[BUFFER_SIZE];
}

//
// humdinger_c Deconstructor
//
humdinger_c::~humdinger_c()
{
	Close();

	Humidity::CloseDevice(hum_dev);

	delete[] pcm_buf;
	pcm_buf = NULL;
}

//
// humdinger_c::CreateMusicSource()
//
bool humdinger_c::CreateMusicSource() 
{          
    if (music_source != NO_SOURCE)
        DeleteMusicSource();
        
    alGetError();  
    alGenSources(1, &music_source);  
     
    if (alGetError() != AL_NO_ERROR)  
    {  
        music_source = NO_SOURCE;
        return false;
    }

    alSource3f(music_source, AL_POSITION,        0.0, 0.0, 0.0);  
    alSource3f(music_source, AL_VELOCITY,        0.0, 0.0, 0.0);  
    alSource3f(music_source, AL_DIRECTION,       0.0, 0.0, 0.0);  
    alSourcef (music_source, AL_ROLLOFF_FACTOR,  0.0          );  
    alSourcei (music_source, AL_SOURCE_RELATIVE, AL_TRUE      );  

    return true;
}

//
// humdinnger_c::DeleteMusicSource
//
void humdinger_c::DeleteMusicSource()
{
    alDeleteSources(1, &music_source);
    music_source = NO_SOURCE;
}

//
// humdinger_c::StreamIntoBuffer()
//
bool humdinger_c::StreamIntoBuffer(int buffer)
{
	int p_res = hum_dev->PlaySome(pcm_buf, BUFFER_SIZE / 4 /* FIXME */, Humidity::S16);

	bool eof = (p_res == Humidity::PLAY_DONE);

	if (p_res == Humidity::PLAY_ERR)
	{
		// Construct an error message
		std::string err_msg("[humdinger_c::StreamIntoBuffer] Failed: ");

		err_msg += hum_dev->GetError();

		// FIXME: this is too harsh
		I_Error("%s\n", err_msg.c_str());
		return false; /* NOT REACHED */
    }

    alBufferData(buffer, format, pcm_buf, BUFFER_SIZE, 22050);
    int al_err = alGetError();

	if (al_err != AL_NO_ERROR)
		I_Error("[humdinger_c::StreamIntoBuffer] alBufferData() Failed\n");

	return ! eof;
}

//
// humdinger_c::SetVolume()
//
void humdinger_c::SetVolume(float gain)
{
	if (music_source == NO_SOURCE)
		return;

	alSourcef(music_source, AL_GAIN, gain);
}

//
// humdinger_c::Open()
//
int humdinger_c::Open(byte *data, int length)
{
	if (length == 0 || ! data)
		return -1;

	if (status != NOT_LOADED)
		Close();

	if (!CreateMusicSource())
		return -1;

	L_WriteDebug("humdinger_c::Open (%d bytes)\n", length);

	// check for MUS format
	bool need_free_midi = false;

	if (length > 4 &&
		data[0] == 'M' && data[1] == 'U' && data[2] == 'S' && data[3] == 0x1A)
	{
		L_WriteDebug("Format is MUS : doing conversion...\n");

		byte *midi_data;
		int midi_len;

		if (! Mus2Midi::Convert(data, length, &midi_data, &midi_len,
				Mus2Midi::DOOM_DIVIS, true))
		{
			I_Warning("Unable to convert MUS to MIDI !\n");
			return -1;
		}

		data = midi_data;
		length = midi_len;
		need_free_midi = true;

		L_WriteDebug("Conversion done: new length is %d\n", length);
	}

	// FIXME IN HUMIDITY: cur_song = Humidity::LoadSong(data, length);

	cur_song = new Humidity::midi_song_c();

	if (cur_song->Decode(data, length) != Humidity::midi_song_c::LOAD_OK)
	{
		I_Warning("Unable to decode MIDI !\n");

		delete cur_song;
		cur_song = NULL;

		return -1;
	}

	L_WriteDebug("Decoded MIDI successfully\n");

	if (need_free_midi)
	{
		Mus2Midi::Free(data);
		data = NULL;
	}

	if (! hum_dev->Start(cur_song, true /* looping */))
	{
		I_Warning("Unable to play MIDI: %s\n", hum_dev->GetError());

		delete cur_song;
		cur_song = NULL;

		return -1;
	}

	L_WriteDebug("Started song successfully\n");

	// clear any previous AL error
	alGetError();

	format = AL_FORMAT_STEREO16; //!!! FIXME for mono

    alGenBuffers(NUM_BUFFERS, buffers);
    int result = alGetError();
	if (result != AL_NO_ERROR)
		I_Error("[humdinger_c::Open] alGenBuffers() Failed\n");

	// Loaded, but not playing
	status = STOPPED;

//	L_WriteDebug("humdinger_c::Open DONE\n");

	return 2;  // dummy value
}

//
// humdinger_c::Close()
//
void humdinger_c::Close()
{
	L_WriteDebug("humdinger_c::Close\n");

	if (status == NOT_LOADED)
		return;

	// Stop playback
	Stop();

	// Release Resource
	alDeleteBuffers(NUM_BUFFERS, buffers);
    DeleteMusicSource();

	if (cur_song)
	{
		Humidity::FreeSong(cur_song);
		cur_song = NULL;
	}

	status = NOT_LOADED;

//	L_WriteDebug("humdinger_c::Close DONE\n");
}

//
// humdinger_c::Pause()
//
void humdinger_c::Pause()
{
	if (status != PLAYING)
		return;

	alSourcePause(music_source);
	status = PAUSED;
}

//
// humdinger_c::Resume()
//
void humdinger_c::Resume()
{
	if (status != PAUSED)
		return;

	alSourcePlay(music_source);
	status = PLAYING;
}

//
// humdinger_c::Play()
//
void humdinger_c::Play(bool loop, float gain)
{
    if (status != NOT_LOADED && status != STOPPED)
        return;

	if (nosound)
		return;

	L_WriteDebug("humdinger_c::Play\n");

	// clear any previous AL error
	alGetError();

	// Load up initial buffer data
	int buffer;
	for (buffer=0; buffer < NUM_BUFFERS; buffer++)
	{
		if (!StreamIntoBuffer(buffers[buffer]))
			I_Error("[humdinger_c::Play] Failed to load into buffers\n");
	}

	SetVolume(gain);

	// Update OpenAL
    alSourceQueueBuffers(music_source, NUM_BUFFERS, buffers);
    alSourcePlay(music_source);
    
	looping = loop;
	status = PLAYING;

//	L_WriteDebug("humdinger_c::Play DONE\n");
}

//
// humdinger_c::Stop()
//
void humdinger_c::Stop()
{
	ALuint buffer;
	int queued;

	if (status != PLAYING && status != PAUSED)
		return;

	// Stop playback
	alSourceStop(music_source);
	
	// Remove any queue buffer
	alGetSourcei(music_source, AL_BUFFERS_QUEUED, &queued);
	while (queued)
	{
		alSourceUnqueueBuffers(music_source, 1, &buffer);	
		queued--;
	}
	status = STOPPED;
}

//
// humdinger_c::Ticker()
//
void humdinger_c::Ticker()
{
	if (status != PLAYING)
		return;

//	L_WriteDebug("humdinger_c::Ticker\n");

	int result;
	int processed;
	bool active = true;

	// clear any previous AL error
	alGetError();

	alGetSourcei(music_source, AL_BUFFERS_PROCESSED, &processed);

	while (processed--)
	{
		ALuint buffer;
		
		alSourceUnqueueBuffers(music_source, 1, &buffer);
		result = alGetError();

		if (result != AL_NO_ERROR)
			I_Error("[humdinger_c::Ticker] alSourceUnqueueBuffers() Failed\n");

		active = StreamIntoBuffer(buffer);

		alSourceQueueBuffers(music_source, 1, &buffer);
		result = alGetError();

		if (result != AL_NO_ERROR)
			I_Error("[humdinger_c::Ticker] alSourceQueueBuffers() Failed\n");
	}

	// Check for underrun
	if (active)
	{
		int state; 
		
		alGetSourcei(music_source, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
		{
			L_WriteDebug("humdinger_c::Ticker - UNDERRUN\n");

			alGetSourcei(music_source, AL_BUFFERS_PROCESSED, &processed);
			alSourcePlay(music_source);
		}	
	}		
	else
	{
		L_WriteDebug("humdinger_c::Ticker - END OF SONG\n");

		// No more data. Seek to beginning and stop if not looping

		if (! looping)
			Stop();
	}

//	L_WriteDebug("humdinger_c::Ticker DONE\n");
}

//----------------------------------------------------------------------------

//
// HumDingerInit
//
humdinger_c *HumDingerInit(void)
{
	L_WriteDebug("HumDingerInit...\n");

	Humidity::Init();

	Humidity::output_device_c **dev_list = Humidity::GetDeviceList();
	int dev_count = 0;

	while (dev_list[dev_count] != NULL)
		dev_count++;
	
	if (dev_count == 0)
	{
		// FIXME: store for HumDingerGetError
		I_Warning("Humidity: no devices are available.\n");
		Humidity::Exit();
		return NULL;
	}

	// !!! FIXME: some control over device selection
	//            (e.g. SW or HW, FM or ! FM).
	//            Using zero gives us the highest priority device.

	Humidity::output_device_c *dev = dev_list[0];
	SYS_ASSERT(dev);

	// !!! FIXME: some control over stereoness, sample rate

	if (! dev->Open(true, 22050))
	{
		// FIXME: store for HumDingerGetError (OR NOT ??)
		I_Warning("Error opening humidity device: %s\n", dev->GetError());
		Humidity::Exit();
		return NULL;
	}

	I_Printf("Humdinger: using device '%s'\n", dev->InfoName());

	humding_inited = true;

	return new humdinger_c(dev);
}

//
// HumDingerTerm
//
void HumDingerTerm(void)
{
	L_WriteDebug("HumDingerTerm...\n");

	if (humding_inited)
	{
		Humidity::Exit();
	}
}

//
// HumDingerGetError
//
const char *HumDingerGetError(void)
{
	return "Unknown problem"; //!!!! FIXME
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
