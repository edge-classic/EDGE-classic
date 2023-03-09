//----------------------------------------------------------------------------
//  EDGE MAC OS X Native MUS Handling Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#include "i_defs.h"
#include "i_sdlinc.h"

#include "file.h"
#include "filesystem.h"
#include "mus_2_midi.h"
#include "path.h"

#include "dm_state.h"
#include "s_music.h"
#include "s_tsf.h"

#import <Cocoa/Cocoa.h>
#import <QTKit/QTMovie.h>

/* MAC Music Player */
class mac_mus_player_c : public abstract_music_c
{
public:
	mac_mus_player_c(NSString* _filename) 
    {
	    filename = [[NSString alloc] initWithString:_filename];
        movie = NULL;
    }

	~mac_mus_player_c() 
    { 
        Close();
    }

private:
    NSString *filename;
    QTMovie *movie;

public:
    NSError* Init() 
    {
        NSError *error = nil;
        movie = [QTMovie movieWithFile:filename error:&error]; 
        return error;
    }

	void Close(void) 
    {
        if (movie != nil)
        {
            [movie stop];
            [movie release];
        }

        if (filename != nil)
            [filename release];
    }

	void Play(bool loop) 
    {
        // Set looping
        NSNumber* value = [NSNumber numberWithInteger:(loop?YES:NO)];
        [movie setAttribute:value forKey:QTMovieLoopsAttribute];
        [value release];

        // Set play position
        [movie gotoBeginning];

        // Start it 
        [movie play]; 
    }

	void Stop(void) { [movie stop]; }

	void Pause(void) { [movie stop]; }
	void Resume(void) { [movie play]; }

	void Ticker(void) {}
	void Volume(float gain) { [movie setVolume:gain]; }
};

//
// I_PlayNativeMusic
//
abstract_music_c * I_PlayNativeMusic(const byte *data, int length, 
                                     float volume, bool loop)
{
    // verify format
	if (length < 16 ||
		! (data[0] == 'M' && 
           data[1] == 'U' && 
           data[2] == 'S' && 
           data[3] == 0x1A))
	{
		I_Warning("I_PlayNativeMusic: wrong format (not MUS)\n");
		return NULL;
	}

    // Convert from MUS to MIDI
	I_Printf("mac_music_player_c: Converting MUS format to MIDI...\n");

	byte *midi_data;
	int midi_len;

	if (!Mus2Midi::Convert(data, length, 
                           &midi_data, &midi_len,
				           Mus2Midi::DOOM_DIVIS, 
                           true))
	{
		delete [] data;

		I_Warning("Unable to convert MUS to MIDI !\n");
		return NULL;
	}

	delete[] data;

	I_Debugf("Conversion done: new length is %d\n", length);

    // Write to temporary file: QTKit doesn't like reading this
    // from memory.
    const char* TMP_MIDI_NAME = "tempmac.mid";
    std::string fn = epi::PATH_Join(home_dir.c_str(), TMP_MIDI_NAME);
    epi::file_c* tmpfile = FS_Open(fn.c_str(), 
                                   epi::file_c::ACCESS_WRITE);   
    tmpfile->Write(midi_data, midi_len); 
    delete tmpfile;

    delete [] midi_data;
    
    // Get the system default encoding
    const NSStringEncoding def_enc = [NSString defaultCStringEncoding];

    // Create an abstract music object
    NSString *tmp_fn = [NSString stringWithCString:fn.c_str()
                                 encoding:def_enc];

    mac_mus_player_c* music = new mac_mus_player_c(tmp_fn);

	[tmp_fn release];

    // Initialise the music player
    NSError *error = music->Init();
    if (error != nil) 
    {
        NSString *error_msg = [error localizedDescription];
        NSUInteger len = [error_msg lengthOfBytesUsingEncoding:def_enc] + 1;
        char *buf = new char[len];

        [error_msg getCString:buf maxLength:len encoding:def_enc];
        I_Printf("I_PlayNativeMusic: %s\n", buf);

        delete [] buf;
        [error_msg release];
        
        [error release];
        return NULL;
    }

    music->Volume(volume);
    music->Play(loop);
	return music;
}

void I_StartupMusic(void)
{
	if (nomusic) return;

#if 1
	if (! nosound)
	{
		if (S_StartupTSF())
		{
			I_Printf("I_StartupMusic: TinySoundfont Init OK\n");
		}
		else
		{
			I_Printf("I_StartupMusic: TinySoundfont Init FAILED\n");
		}
	}
	else
#endif
    {
		I_Printf("I_StartupMusic: TinySoundfont Disabled\n");
    }

	// Nothing went pear shaped
	return;
}

void I_ShutdownMusic(void)
{
}



//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab:syntax=cpp
