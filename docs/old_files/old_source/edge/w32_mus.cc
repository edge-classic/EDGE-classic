//----------------------------------------------------------------------------
//  EDGE WIN32 MUS Handling Code
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

#include "i_defs.h"
#include "i_sdlinc.h"

#include "w32_sysinc.h"
#include "e_main.h"
#include "s_music.h"

// TODO: endianness handling (not a huge issue since the Win32
//       platform is predominantly for x86 architecture).

typedef unsigned short int UWORD;

typedef struct
{
	char ID[4];           // identifier - Always "MUS" 0x1A

	UWORD scorelen;       // Score Length
	UWORD scorestart;     // Score Start Offset in bytes
	UWORD channels;       // number of primary channels
	UWORD sec_channels;   // number of secondary channels
	UWORD instrcnt;       // Number of instruments
	UWORD dummy;
}
musheader_t;

typedef struct
{
	byte channel : 4;
	byte event   : 3;
	byte last    : 1;
}
museventdesc_t;

// MUS event types.
typedef enum 
{
	MUS_EV_RELEASE_NOTE,
	MUS_EV_PLAY_NOTE,
	MUS_EV_PITCH_WHEEL,
	MUS_EV_SYSTEM,                  // Valueless controller.
	MUS_EV_CONTROLLER,       
	MUS_EV_FIVE,                    // ?
	MUS_EV_SCORE_END,
	MUS_EV_SEVEN                    // ?
}
musevent_e;

// MUS controllers.
typedef enum 
{
	MUS_CTRL_INSTRUMENT,
	MUS_CTRL_BANK,
	MUS_CTRL_MODULATION,
	MUS_CTRL_VOLUME,
	MUS_CTRL_PAN,
	MUS_CTRL_EXPRESSION,
	MUS_CTRL_REVERB,
	MUS_CTRL_CHORUS,
	MUS_CTRL_SUSTAIN_PEDAL,
	MUS_CTRL_SOFT_PEDAL,

	MUS_CTRL_SOUNDS_OFF,
	MUS_CTRL_NOTES_OFF,
	MUS_CTRL_MONO,
	MUS_CTRL_POLY,
	MUS_CTRL_RESET_ALL,
	NUM_MUS_CTRLS
}
musctrl_e;

static char ctrl_mus2midi[NUM_MUS_CTRLS] =
{
	0,              // Not used.

	0,              // Bank select.
	1,              // Modulation.
	7,              // Volume.
	10,             // Pan.
	11,             // Expression.
	91,             // Reverb.
	93,             // Chorus.
	64,             // Sustain pedal.
	67,             // Soft pedal.

	120,            // All sounds off.
	123,            // All notes off.
	126,            // Mono.
	127,            // Poly.
	121             // Reset all controllers.
};


bool midiavailable = false;		// Available?

static HMIDIOUT midioutput;             // OUTPUT port...
static win32_mixer_t *mixer = NULL;

// Timer Control
#define ACTUAL_TIMER_HZ   70  // -AJA- was 140 !!

#define TIMER_RES 7                     // Timer resolution.
static int timerID;                     // Timer ID

static SDL_sem *semaphore;  // thread protection


class w32_mus_player_c : public abstract_music_c
{
private:
	byte *data;
	int length;

	const musheader_t *song_info;
	const byte *pos;        // The current play position.

	bool paused;            // The song is playing.
	bool looping;           // The song is looping.

	int waitticks;    

	byte chanVols[16];      // Last volume for each channel.

public:
	static w32_mus_player_c *current;

public:
	w32_mus_player_c(const byte *_dat, int _len) :
		length(_len), paused(true), looping(false), waitticks(0)
	{
		data = new byte[length];

		memcpy(data, _dat, length);

		song_info = (musheader_t*)data;

		// Go to the beginning of the song.
		pos = SongStartAddress();

		for (int ch = 0; ch < 16; ch++)
			chanVols[ch] = 0;  // TODO: what should it be???

		current = this;
	}

	~w32_mus_player_c()
	{
		SYS_ASSERT(current == this);

		// -AJA- funky stuff here to prevent the W32 system ticker
		//       callback from getting the rug pulled out from
		//       under its feet.
		SDL_SemWait(semaphore);

		current = NULL;

		Close();

		delete[] data;
		data = NULL;

		SDL_SemPost(semaphore);
	}

public:
//  bool IsPaused() { return paused; }

	void SysTicker(void);

public:
	virtual void Close(void);

	virtual void Play(bool loop);
	virtual void Stop(void);

	virtual void Pause(void);
	virtual void Resume(void);

	virtual void Ticker(void);
	virtual void Volume(float gain);

private:
	const byte *SongStartAddress(void)
	{
		// Address of the start of the MUS track.

		SYS_ASSERT(data);

		return data + song_info->scorestart;
	}
};

w32_mus_player_c * w32_mus_player_c::current = NULL;


void w32_mus_player_c::Close(void)
{
	Stop();
}


void w32_mus_player_c::Play(bool loop)
{
	SYS_ASSERT(pos);

	paused  = false;
	looping = loop;
}

void w32_mus_player_c::Pause(void)
{
	paused = true;

	// Stop all the notes.
	midiOutReset(midioutput);
}


void w32_mus_player_c::Resume()
{
	paused = false;
}


void w32_mus_player_c::Stop()
{
	if (!data || !pos)
		return;

	paused = true;

	// reset pitchwheel on all channels
	for (int i=0; i < 16; i++)              
		midiOutShortMsg(midioutput, i | 0xe0 | (0x40 << 16)); 

	// send RESET_ALL_CONTROLLERS message
	midiOutShortMsg(midioutput, 0xb0 | (121<<8));

	midiOutReset(midioutput); 
}


void w32_mus_player_c::Volume(float gain)
{
	if (!mixer)
		return;

	gain = CLAMP(0, gain, 1);

	// Calculate a given value in range
	DWORD actualvol;

	actualvol = int(gain * (mixer->maxvol - mixer->minvol));
	actualvol += mixer->minvol;

	I_MusicSetMixerVol(mixer, actualvol);
}


void w32_mus_player_c::Ticker()
{
	// this ticker function does nothing.  The 70Hz timer
	// callback (SysTicker) does all the work.
}


//
// Called at 140Hz, interprets the MUS data to MIDI events.
//
void w32_mus_player_c::SysTicker(void)
{
	if (paused)
		return;

	waitticks--;
	if (waitticks > 0)
		return;

	bool scoreEnd = false;

	//
	// We assume playpos is OK. We'll keep playing events until the
	// 'last' bit is set, and then the waitticks is set.
	// (AJA: Note the looping limit in case of corrupt file).
	//
	for (int iloop=0; iloop < 50; iloop++)
	{
		museventdesc_t *evDesc = (museventdesc_t *)pos++;

		byte midiStatus = 0;
		byte midiChan   = 0;
		byte midiParm1  = 0;
		byte midiParm2  = 0;

		// Construct the MIDI event.
		switch (evDesc->event)
		{
			case MUS_EV_RELEASE_NOTE:
			{
				midiStatus = 0x80;
				midiParm1  = *pos++;   // Note
				midiParm2  = 0x40;     // Velocity (64)
				break;
			}

			case MUS_EV_PLAY_NOTE:
			{
				midiStatus = 0x90;
				midiParm1 = *pos++;

				// Is the volume there, too?
				if (midiParm1 & 0x80) 
					chanVols[evDesc->channel] = *pos++;

				midiParm1 &= 0x7f;
				midiParm2 = chanVols[evDesc->channel];
				break;                   
			}

			case MUS_EV_PITCH_WHEEL:
			{
				// Convert to 14-bit MIDI pitch value
				int pitchwheel = (int)(*pos++) << 6;

				midiStatus = 0xe0;
				midiParm1 = pitchwheel & 127;
				midiParm2 = pitchwheel >> 7;
				break;
			}

			case MUS_EV_SYSTEM:
			{
				byte scratch = *pos++;

				midiStatus = 0xb0;
				midiParm1 = ctrl_mus2midi[scratch];
				midiParm2 = (scratch==12) ? song_info->instrcnt+1 : 0;
				break;
			}

			case MUS_EV_CONTROLLER:
			{
				midiStatus = 0xb0;
				midiParm1 = *pos++;
				midiParm2 = *pos++;

				// The instrument control is mapped to another kind of MIDI event.
				if (midiParm1 == MUS_CTRL_INSTRUMENT)
				{
					midiStatus = 0xc0;
					midiParm1  = midiParm2;
					midiParm2  = 0;
				}
				else
				{
					// Use the conversion table.
					midiParm1 = ctrl_mus2midi[midiParm1];
				}
				break;
			}

			case MUS_EV_SCORE_END:
			{
				// We're done.
				scoreEnd = true;
				break;
			}

			default:
			{
				I_Warning("MUS_SongPlayer: Unhandled MUS event %d.\n",evDesc->event);
				break;
			}
		}

		if (scoreEnd)
			break;

		// Choose the channel.
		midiChan = evDesc->channel;

		// Redirect MUS channel 15 to MIDI channel 9 (drums).
		if (midiChan == 15)
			midiChan = 9;
		else if (midiChan == 9)
			midiChan = 15;

		// Send out the MIDI event.
		midiOutShortMsg(midioutput, midiChan | midiStatus | (midiParm1<<8) | (midiParm2<<16));

		// Check if this was the last event in a group.
		if (evDesc->last)
			break;
	}

	waitticks = 0;

	// Check for end of score.
	if (scoreEnd)
	{
		if (! looping)
		{
			paused = true;  // TODO: stopped/playing/paused trichotomy

			// Reset the MIDI output so no notes are left playing when the song ends.
			midiOutReset(midioutput);
		}

		pos = SongStartAddress();
		return;
	}

	// Read the number of ticks to wait.
	for (int jloop = 0; jloop < 10; jloop++)
	{
		byte val = *pos++;

		waitticks = (waitticks * 128) + (val & 0x7f);

		if ((val & 0x80) == 0)
			break;
	}
}


//----------------------------------------------------------------------------

void CALLBACK SysTicker(UINT id, UINT msg, DWORD user, DWORD dw1, DWORD dw2)
{
	// Sanity Checks...
	if (!midiavailable)
		return;

	if (! (app_state & APP_STATE_ACTIVE))
		return;

	// -AJA- we don't block on the semaphore, as that may be a
	//       problem for the operating system (the MSDN docs
	//       say this timer event callback is called from a
	//       system multimedia thread).

	if (SDL_SemTryWait(semaphore) != 0)
		return;

	if (w32_mus_player_c::current)
	{
		w32_mus_player_c::current->SysTicker();
		w32_mus_player_c::current->SysTicker();
	}

	SDL_SemPost(semaphore);
}


bool I_StartupMUS()
{
	/* Returns true if no problems */

	if (midiavailable)
		return true; // Already initialized.

	semaphore = SDL_CreateSemaphore(1);
	SYS_ASSERT(semaphore);

	// Insert code here...
	mixer = I_MusicLoadMixer(MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER);
	if (!mixer)
	{
		I_Printf("I_StartupMUS: Couldn't load the midi mixer\n");
		return false;
	}

	// Open the midi stream.
	MMRESULT res = midiOutOpen(&midioutput, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
	if (res != MMSYSERR_NOERROR)
	{
		I_MusicReleaseMixer(mixer);
		mixer = NULL;
		
		I_Printf("I_StartupMUS: Couldn't open the midi device\n");
		return false;
	}

	if (!I_MusicGetMixerVol(mixer, &mixer->originalvol))
	{
		I_MusicReleaseMixer(mixer);
		mixer = NULL;
		
		midiOutClose(midioutput);

		I_Printf("I_StartupMUS: Unable to get original volume\n");
		return false;
	}
	
	// All is quiet on startup
	I_MusicSetMixerVol(mixer, 0);

	w32_mus_player_c::current = NULL;

	// Startup music clock
	int clockspeed = 1000 / ACTUAL_TIMER_HZ;

	timeBeginPeriod(TIMER_RES);
	timerID = timeSetEvent(clockspeed, TIMER_RES, (LPTIMECALLBACK)SysTicker, 0, TIME_PERIODIC);

	midiavailable = true;

	return true;
}


void I_ShutdownMUS(void)
{
	if (!midiavailable)
		return;

	midiavailable = false;

	// wait for the timer handler to finish (if active).
	SDL_SemWait(semaphore);
	SDL_SemPost(semaphore);

	// If there is a registered song, unregister it.
	if (w32_mus_player_c::current)
	{
		delete w32_mus_player_c::current;
	}

	// Kill timer
	timeKillEvent(timerID);
	timeEndPeriod(TIMER_RES);

	// Restore the original volume.

	// Close all our handled objects
	if (mixer)
	{
		I_MusicSetMixerVol(mixer, mixer->originalvol);
		I_MusicReleaseMixer(mixer);
		mixer = NULL;
	}

	// Switch off...
	midiOutClose(midioutput);
}


abstract_music_c * I_PlayNativeMusic(const byte *data, int length, float  volume, bool loop)
{
	if (!midiavailable)
		return NULL;

	// verify format
	if (length < 16 ||
		! (data[0] == 'M' && data[1] == 'U' && data[2] == 'S' && data[3] == 0x1A))
	{
		I_Warning("I_PlayNativeMusic: wrong format (not MUS)\n");
		return NULL;
	}

	// If there is a registered song, unregister it.
	if (w32_mus_player_c::current)
	{
		delete w32_mus_player_c::current;
	}

	// grab semaphore to prevent race conditions with I_MUSTicker
	SDL_SemWait(semaphore);

	w32_mus_player_c *player = new w32_mus_player_c(data, length);

	player->Volume(volume);
	player->Play(loop);

	SDL_SemPost(semaphore);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
