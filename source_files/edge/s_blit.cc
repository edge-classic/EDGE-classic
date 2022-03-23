//----------------------------------------------------------------------------
//  Sound Blitter
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "i_sdlinc.h"

#include <list>

#include "m_misc.h"
#include "r_misc.h"   // R_PointToAngle
#include "p_local.h"  // P_ApproxDistance

#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"

#include "dm_state.h"

// Reverb and falloff stuff - Dasho
#include "p_blockmap.h"

// Sound must be clipped to prevent distortion (clipping is
// a kind of distortion of course, but it's much better than
// the "white noise" you get when values overflow).
//
// The more safe bits there are, the less likely the final
// output sum will overflow into white noise, but the less
// precision you have left for the volume multiplier.
#define SAFE_BITS  4
#define CLIP_THRESHHOLD  ((1L << (31-SAFE_BITS)) - 1)

// This value is how much breathing room there is until
// sounds start clipping.  More bits means less chance
// of clipping, but every extra bit makes the sound half
// as loud as before.
#define QUIET_BITS  0


#define MIN_CHANNELS    8
#define MAX_CHANNELS  128

mix_channel_c *mix_chan[MAX_CHANNELS];
int num_chan;

bool vacuum_sfx = false;
bool submerged_sfx = false;
bool outdoor_reverb = false;

static int *mix_buffer;
static int mix_buf_len;


#define MAX_QUEUE_BUFS  16

static std::list<epi::sound_data_c *> free_qbufs;
static std::list<epi::sound_data_c *> playing_qbufs;

static mix_channel_c *queue_chan;


int sfx_volume = 0;

static bool sfxpaused = false;

// these are analogous to viewx/y/z/angle
float listen_x;
float listen_y;
float listen_z;
angle_t listen_angle;

// -AJA- 2005/02/26: table to convert slider position to GAIN.
//       Curve was hand-crafted to give useful distinctions of
//       volume levels at the quiet end.  Entry zero always
//       means total silence (in the table for completeness).
float slider_to_gain[SND_SLIDER_NUM] =
{
	0.00000, 0.00200, 0.00400, 0.00800, 0.01600,
	0.03196, 0.05620, 0.08886, 0.12894, 0.17584,
	0.22855, 0.28459, 0.34761, 0.41788, 0.49553,
	0.58075, 0.67369, 0.77451, 0.88329, 1.00000
};

// FIXME: extern == hack
extern int dev_freq;
extern int dev_bits;
extern int dev_bytes_per_sample;
extern int dev_frag_pairs;

extern bool dev_signed;
extern bool dev_stereo;

mix_channel_c::mix_channel_c() : state(CHAN_Empty), data(NULL)
{ }

mix_channel_c::~mix_channel_c()
{ }

void mix_channel_c::ComputeDelta()
{
	// frequency close enough ?
	if (data->freq > (dev_freq - dev_freq/100) &&
		data->freq < (dev_freq + dev_freq/100))
	{
		delta = (1 << 10);
	}
	else
	{
		delta = (fixed22_t) floor((float)data->freq * 1024.0f / dev_freq);
	}
}

void mix_channel_c::ComputeVolume()
{
	float sep = 0.5f;
	float dist = 1.0f;
	

	if (pos && category >= SNCAT_Opponent)
	{
		if (dev_stereo)
		{
			angle_t angle = R_PointToAngle(listen_x, listen_y, pos->x, pos->y);

			// same equation from original DOOM 
			sep = 0.5f - 0.38f * M_Sin(angle - listen_angle);
		}

		if (! boss)
		{
			dist = P_ApproxDistance(listen_x - pos->x, listen_y - pos->y, listen_z - pos->z); 

			if (P_CheckSightToPoint(players[consoleplayer]->mo, pos->x, pos->y, pos->z))
				dist /= (100.0f * data->freq_factor);
			else
				dist /= (50.0f * data->freq_factor);
		}
	}

	float MAX_VOL = (1 << (16 - SAFE_BITS - (var_quiet_factor-1))) - 3;

	MAX_VOL = boss ? MAX_VOL : MIN(MAX_VOL, MAX_VOL / dist) * slider_to_gain[sfx_volume];

	if (def)
		MAX_VOL *= PERCENT_2_FLOAT(def->volume);

	// strictly linear equations
	volume_L = (int) (MAX_VOL * (1.0 - sep));
	volume_R = (int) (MAX_VOL * (0.0 + sep));

	if (var_sound_stereo == 2)  /* SWAP ! */
	{
		int tmp = volume_L;
		volume_L = volume_R;
		volume_R = tmp;
	}
}

void mix_channel_c::ComputeMusicVolume()
{
	// the MAX_VOL value here is equivalent to the 'NORMAL' quiet
	// factor.  If the music uses the full range, then the output
	// will also use the full range without being clipped
	// (assuming no other sounds are playing).
	//
	// We do not use 'quiet_factor' here because that mainly
	// applies to the mixing of game/UI sounds.  The music volume
	// slider allows for quieter output.

	float MAX_VOL = (1 << (16 - SAFE_BITS)) - 3;

 	MAX_VOL = MAX_VOL * slider_to_gain[mus_volume];

	volume_L = (int) MAX_VOL;
	volume_R = (int) MAX_VOL;
}

//----------------------------------------------------------------------------

static void BlitToU8(const int *src, u8_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u8_t) ((val >> (24-SAFE_BITS)) ^ 0x80);
	}
}

static void BlitToS8(const int *src, s8_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s8_t) (val >> (24-SAFE_BITS));
	}
}

static void BlitToU16(const int *src, u16_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u16_t) ((val >> (16-SAFE_BITS)) ^ 0x8000);
	}
}

static void BlitToS16(const int *src, s16_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s16_t) (val >> (16-SAFE_BITS));
	}
}


static void MixMono(mix_channel_c *chan, int *dest, int pairs)
{
	SYS_ASSERT(pairs > 0);

	s16_t *src_L;

	if (paused || menuactive)
		src_L = chan->data->data_L;
	else
	{
		
		if (!chan->data->is_sfx)
			src_L = chan->data->data_L;
		else
			src_L = chan->data->fx_data_L;
	}

	int *d_pos = dest;
	int *d_end = d_pos + pairs;

	fixed22_t offset = chan->offset;

	while (d_pos < d_end)
	{
		*d_pos++ += src_L[offset >> 10] * chan->volume_L;

		offset += chan->delta;
	}

	chan->offset = offset;

	SYS_ASSERT(offset - chan->delta < chan->length);
}

static void MixStereo(mix_channel_c *chan, int *dest, int pairs)
{
	SYS_ASSERT(pairs > 0);

	s16_t *src_L;
	s16_t *src_R;

	if (paused || menuactive)
	{
		src_L = chan->data->data_L;
		src_R = chan->data->data_R;
	}
	else
	{
		if (!chan->data->is_sfx)
		{
			src_L = chan->data->data_L;
			src_R = chan->data->data_R;
		}
		else
		{
			src_L = chan->data->fx_data_L;
			src_R = chan->data->fx_data_R;
		}
	}

	int *d_pos = dest;
	int *d_end = d_pos + pairs * 2;

	fixed22_t offset = chan->offset;

	while (d_pos < d_end)
	{
		*d_pos++ += src_L[offset >> 10] * chan->volume_L;
		*d_pos++ += src_R[offset >> 10] * chan->volume_R;

		offset += chan->delta;
	}

	chan->offset = offset;

	SYS_ASSERT(offset - chan->delta < chan->length);
}

static void MixInterleaved(mix_channel_c *chan, int *dest, int pairs)
{
	if (! dev_stereo)
		I_Error("INTERNAL ERROR: tried to mix an interleaved buffer in MONO mode.\n");

	SYS_ASSERT(pairs > 0);

	s16_t *src_L;

	if (paused || menuactive)
		src_L = chan->data->data_L;
	else
	{
		if (!chan->data->is_sfx)
			src_L = chan->data->data_L;
		else
			src_L = chan->data->fx_data_L;
	}

	int *d_pos = dest;
	int *d_end = d_pos + pairs * 2;

	fixed22_t offset = chan->offset;

	while (d_pos < d_end)
	{
		fixed22_t pos = (offset >> 9) & ~1;

		*d_pos++ += src_L[pos  ] * chan->volume_L;
		*d_pos++ += src_L[pos|1] * chan->volume_R;

		offset += chan->delta;
	}

	chan->offset = offset;

	SYS_ASSERT(offset - chan->delta < chan->length);
}

static void MixOneChannel(mix_channel_c *chan, int pairs)
{
	if (sfxpaused && chan->category >= SNCAT_Player)
		return;

	if (chan->volume_L == 0 && chan->volume_R == 0)
		return;

	SYS_ASSERT(chan->offset < chan->length);

	int *dest = mix_buffer;
	
	while (pairs > 0)
	{
		int count = pairs;

		// check if enough sound data is left
		if (chan->offset + count * chan->delta >= chan->length)
		{
			// find minimum number of samples we can play
			double avail = (chan->length - chan->offset + chan->delta - 1) /
				           (double)chan->delta;

			count = (int)floor(avail);

			SYS_ASSERT(count > 0);
			SYS_ASSERT(count <= pairs);

			SYS_ASSERT(chan->offset + count * chan->delta >= chan->length);
		}

		if (chan->data->mode == epi::SBUF_Interleaved)
			MixInterleaved(chan, dest, count);
		else if (dev_stereo)
			MixStereo(chan, dest, count);
		else
			MixMono(chan, dest, count);

		if (chan->offset >= chan->length)
		{
			if (! chan->loop)
			{
				chan->state = CHAN_Finished;
				break;
			}

			// we are looping, so clear flag.  The sound needs to
			// be "pumped" (played again) to continue looping.
			chan->loop = false;

			chan->offset = 0;
		}

		dest  += count * (dev_stereo ? 2 : 1);
		pairs -= count;
	}
}


static bool QueueNextBuffer(void)
{
	if (playing_qbufs.empty())
	{
		queue_chan->state = CHAN_Finished;
		queue_chan->data  = NULL;
		return false;
	}

	epi::sound_data_c *buf = playing_qbufs.front();

	queue_chan->data = buf;

	queue_chan->offset = 0;
	queue_chan->length = buf->length << 10;

	queue_chan->ComputeDelta();

	queue_chan->state = CHAN_Playing;
	return true;
}

static void MixQueues(int pairs)
{
	mix_channel_c *chan = queue_chan;

	if (! chan || chan->state != CHAN_Playing)
		return;

	if (chan->volume_L == 0 && chan->volume_R == 0)
		return;

	SYS_ASSERT(chan->offset < chan->length);

	int *dest = mix_buffer;

	while (pairs > 0)
	{
		int count = pairs;

		// check if enough sound data is left
		if (chan->offset + count * chan->delta >= chan->length)
		{
			// find minimum number of samples we can play
			double avail = (chan->length - chan->offset + chan->delta - 1) /
				           (double)chan->delta;

			count = (int)floor(avail);

			SYS_ASSERT(count > 0);
			SYS_ASSERT(count <= pairs);

			SYS_ASSERT(chan->offset + count * chan->delta >= chan->length);
		}

		if (chan->data->mode == epi::SBUF_Interleaved)
			MixInterleaved(chan, dest, count);
		else if (dev_stereo)
			MixStereo(chan, dest, count);
		else
			MixMono(chan, dest, count);

		if (chan->offset >= chan->length)
		{
			// reached end of current queued buffer.
			// Place current buffer onto free list,
			// and enqueue the next buffer to play.

			SYS_ASSERT(! playing_qbufs.empty());

			epi::sound_data_c *buf = playing_qbufs.front();
			playing_qbufs.pop_front();

			free_qbufs.push_back(buf);

			if (! QueueNextBuffer())
				break;
		}

		dest  += count * (dev_stereo ? 2 : 1);
		pairs -= count;
	}
}


void S_MixAllChannels(void *stream, int len)
{
	if (nosound || len <= 0)
		return;

	int pairs = len / dev_bytes_per_sample;

	int samples = pairs;
	if (dev_stereo)
		samples *= 2;

	// check that we're not getting too much data
	SYS_ASSERT(pairs <= dev_frag_pairs);

	SYS_ASSERT(mix_buffer && samples <= mix_buf_len);

	// clear mixer buffer
	memset(mix_buffer, 0, mix_buf_len * sizeof(int));

#if 0  // TESTING.. TESTING..
	mix_buffer[ 0] =  CLIP_THRESHHOLD;
	mix_buffer[33] = -CLIP_THRESHHOLD;
#endif

	// add each channel
	for (int i=0; i < num_chan; i++)
	{
		if (mix_chan[i]->state == CHAN_Playing)
		{
			MixOneChannel(mix_chan[i], pairs);
		}
	} 

	MixQueues(pairs);

	// blit to the SDL stream
	if (dev_bits == 8)
	{
		if (dev_signed)
			BlitToS8(mix_buffer, (s8_t *)stream, samples);
		else
			BlitToU8(mix_buffer, (u8_t *)stream, samples);
	}
	else
	{
		if (dev_signed)
			BlitToS16(mix_buffer, (s16_t *)stream, samples);
		else
			BlitToU16(mix_buffer, (u16_t *)stream, samples);
	}
}


//----------------------------------------------------------------------------

void S_InitChannels(int total)
{
	// NOTE: assumes audio is locked!

	SYS_ASSERT(total >= MIN_CHANNELS);
	SYS_ASSERT(total <= MAX_CHANNELS);

	num_chan = total;

	for (int i = 0; i < num_chan; i++)
		mix_chan[i] = new mix_channel_c();

	// allocate mixer buffer
	mix_buf_len = dev_frag_pairs * (dev_stereo ? 2 : 1);
	mix_buffer = new int[mix_buf_len];
}

void S_FreeChannels(void)
{
	// NOTE: assumes audio is locked!
	
	for (int i = 0; i < num_chan; i++)
	{
		mix_channel_c *chan = mix_chan[i];

		if (chan && chan->data)
		{
			S_CacheRelease(chan->data);
			chan->data = NULL;
		}

		delete chan;
	}

	memset(mix_chan, 0, sizeof(mix_chan));
}

void S_KillChannel(int k)
{
	mix_channel_c *chan = mix_chan[k];

	if (chan->state != CHAN_Empty)
	{
		S_CacheRelease(chan->data);

		chan->data = NULL;
		chan->state = CHAN_Empty;
	}
}

void S_ReallocChannels(int total)
{
	// NOTE: assumes audio is locked!

	SYS_ASSERT(total >= MIN_CHANNELS);
	SYS_ASSERT(total <= MAX_CHANNELS);

	if (total > num_chan)
	{
		for (int i = num_chan; i < total; i++)
			mix_chan[i] = new mix_channel_c();
	}

	if (total < num_chan)
	{
		// kill all non-UI sounds, pack the UI sounds into the
		// remaining slots (normally there will be enough), and
		// delete the unused channels
		int i, j;

		for (i = 0; i < num_chan; i++)
		{
			mix_channel_c *chan = mix_chan[i];

			if (chan->state == CHAN_Playing)
			{
				if (chan->category != SNCAT_UI)
					S_KillChannel(i);
			}
		}

		for (i = j = 0; i < num_chan; i++)
		{
			if (mix_chan[i])
			{
				/* SWAP ! */
				mix_channel_c *tmp = mix_chan[j];

				mix_chan[j] = mix_chan[i];
				mix_chan[i] = tmp;
			}
		}

		for (i = total; i < num_chan; i++)
		{
			if (mix_chan[i]->state == CHAN_Playing)
				S_KillChannel(i);

			delete mix_chan[i];
			mix_chan[i] = NULL;
		}
	}

	num_chan = total;
}
	
void S_UpdateSounds(position_c *listener, angle_t angle)
{
	// NOTE: assume SDL_LockAudio has been called

	listen_x = listener ? listener->x : 0;
	listen_y = listener ? listener->y : 0;
	listen_z = listener ? listener->z : 0;

	listen_angle = angle;

	for (int i = 0; i < num_chan; i++)
	{
		mix_channel_c *chan = mix_chan[i];

		if (chan->state == CHAN_Playing)
			chan->ComputeVolume();

		else if (chan->state == CHAN_Finished)
			S_KillChannel(i);
	}

	if (queue_chan)
		queue_chan->ComputeMusicVolume();
}


void S_ChangeSoundVolume(void)
{
	/* nothing to do */
}

void S_PauseSound(void)
{
	sfxpaused = true;
}

void S_ResumeSound(void)
{
	sfxpaused = false;
}


//----------------------------------------------------------------------------

void S_QueueInit(void)
{
	if (nosound) return;

	I_LockAudio();
	{
		if (free_qbufs.empty())
		{
			for (int i=0; i < MAX_QUEUE_BUFS; i++)
			{
				free_qbufs.push_back(new epi::sound_data_c());
			}
		}

		if (! queue_chan)
			queue_chan = new mix_channel_c();

		queue_chan->state = CHAN_Empty;
		queue_chan->data  = NULL;

		queue_chan->ComputeMusicVolume();
	}
	I_UnlockAudio();
}

void S_QueueShutdown(void)
{
	if (nosound) return;

	I_LockAudio();
	{
		if (queue_chan)
		{
			// free all data on the playing / free lists.
			// The sound_data_c destructor takes care of data_L/R.

			for (; ! playing_qbufs.empty(); playing_qbufs.pop_front())
			{
				delete playing_qbufs.front();
			}
			for (; ! free_qbufs.empty(); free_qbufs.pop_front())
			{
				delete free_qbufs.front();
			}

			queue_chan->data = NULL;

			delete queue_chan;
			queue_chan = NULL;
		}
	}
	I_UnlockAudio();
}

void S_QueueStop(void)
{
	if (nosound) return;

	SYS_ASSERT(queue_chan);

	I_LockAudio();
	{
		for (; ! playing_qbufs.empty(); playing_qbufs.pop_front())
		{
			free_qbufs.push_back(playing_qbufs.front());
		}

		queue_chan->state = CHAN_Finished;
		queue_chan->data  = NULL;
	}
	I_UnlockAudio();
}

epi::sound_data_c * S_QueueGetFreeBuffer(int samples, int buf_mode)
{
	if (nosound) return NULL;

	epi::sound_data_c *buf = NULL;

	I_LockAudio();
	{
		if (! free_qbufs.empty())
		{
			buf = free_qbufs.front();
			free_qbufs.pop_front();

			buf->Allocate(samples, buf_mode);
		}
			
	}
	I_UnlockAudio();

	return buf;
}

void S_QueueAddBuffer(epi::sound_data_c *buf, int freq)
{
	SYS_ASSERT(! nosound);
	SYS_ASSERT(buf);

	I_LockAudio();
	{
		buf->freq = freq;

		playing_qbufs.push_back(buf);

		if (queue_chan->state != CHAN_Playing)
		{
			QueueNextBuffer();
		}
	}
	I_UnlockAudio();
}

void S_QueueReturnBuffer(epi::sound_data_c *buf)
{
	SYS_ASSERT(! nosound);
	SYS_ASSERT(buf);

	I_LockAudio();
	{
		free_qbufs.push_back(buf);
	}
	I_UnlockAudio();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
