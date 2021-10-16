/*
    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    Suddenly, you realize that this program is free software; you get
    an overwhelming urge to redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received another copy of the GNU General Public
    License along with this program; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    I bet they'll be amazed.

    mix.c (+ output.c)
*/

#include "config.h"

#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "ctrlmode.h"
#include "tables.h"
#include "resample.h"
#include "mix.h"

/* Returns 1 if envelope runs out */
int recompute_envelope(int v)
{
	int stage = voice[v].envelope_stage;

	if (stage > 5)
	{
		/* Envelope ran out. */
		int tmp=(voice[v].status == VOICE_DIE); /* Already displayed as dead */
		voice[v].status = VOICE_FREE;
		if(!tmp)
			return 1;
	}

	if (voice[v].sample->modes & MODES_ENVELOPE)
	{
		if (voice[v].status==VOICE_ON || voice[v].status==VOICE_SUSTAINED)
		{
			if (stage > 2)
			{
				/* Freeze envelope until note turns off. Trumpets want this. */
				voice[v].envelope_increment=0;
				return 0;
			}
		}
	}
	voice[v].envelope_stage=stage+1;

	if (voice[v].envelope_volume==voice[v].sample->envelope_offset[stage])
		return recompute_envelope(v);

	voice[v].envelope_target    = voice[v].sample->envelope_offset[stage];
	voice[v].envelope_increment = voice[v].sample->envelope_rate[stage];

	if (voice[v].envelope_target < voice[v].envelope_volume)
		voice[v].envelope_increment = -voice[v].envelope_increment;

	return 0;
}

void apply_envelope_to_amp(int v)
{
	double lamp = voice[v].left_amp;

	if (voice[v].panned == PANNED_MYSTERY)
	{
		double lramp=voice[v].lr_amp;
		double ramp=voice[v].right_amp;
		double ceamp=voice[v].ce_amp;
		double rramp=voice[v].rr_amp;
		double lfeamp=voice[v].lfe_amp;

		if (voice[v].tremolo_phase_increment)
		{
			double tv = voice[v].tremolo_volume;

			lramp  *= tv;
			lamp   *= tv;
			ceamp  *= tv;
			ramp   *= tv;
			rramp  *= tv;
			lfeamp *= tv;
		}

		if (voice[v].sample->modes & MODES_ENVELOPE)
		{
			double ev = (double)vol_table[voice[v].envelope_volume>>23];

			lramp  *= ev;
			lamp   *= ev;
			ceamp  *= ev;
			ramp   *= ev;
			rramp  *= ev;
			lfeamp *= ev;
		}

		s32_t la   = (s32_t)FSCALE(lamp,AMP_BITS);
		s32_t ra   = (s32_t)FSCALE(ramp,AMP_BITS);
		s32_t lra  = (s32_t)FSCALE(lramp,AMP_BITS);
		s32_t rra  = (s32_t)FSCALE(rramp,AMP_BITS);
		s32_t cea  = (s32_t)FSCALE(ceamp,AMP_BITS);
		s32_t lfea = (s32_t)FSCALE(lfeamp,AMP_BITS);

		if (la>MAX_AMP_VALUE) la=MAX_AMP_VALUE;
		if (ra>MAX_AMP_VALUE) ra=MAX_AMP_VALUE;
		if (lra>MAX_AMP_VALUE) lra=MAX_AMP_VALUE;
		if (rra>MAX_AMP_VALUE) rra=MAX_AMP_VALUE;
		if (cea>MAX_AMP_VALUE) cea=MAX_AMP_VALUE;
		if (lfea>MAX_AMP_VALUE) lfea=MAX_AMP_VALUE;

		voice[v].lr_mix=FINAL_VOLUME(lra);
		voice[v].left_mix=FINAL_VOLUME(la);
		voice[v].ce_mix=FINAL_VOLUME(cea);
		voice[v].right_mix=FINAL_VOLUME(ra);
		voice[v].rr_mix=FINAL_VOLUME(rra);
		voice[v].lfe_mix=FINAL_VOLUME(lfea);
	}
	else
	{
		if (voice[v].tremolo_phase_increment)
			lamp *= voice[v].tremolo_volume;

		if (voice[v].sample->modes & MODES_ENVELOPE)
			lamp *= (double)vol_table[voice[v].envelope_volume>>23];

		s32_t la = (s32_t)FSCALE(lamp,AMP_BITS);

		if (la > MAX_AMP_VALUE)
			la = MAX_AMP_VALUE;

		voice[v].left_mix=FINAL_VOLUME(la);
	}
}

static int update_envelope(int v)
{
	voice[v].envelope_volume += voice[v].envelope_increment;

	/* Why is there no ^^ operator?? */
	if (((voice[v].envelope_increment < 0) &&
		 (voice[v].envelope_volume <= voice[v].envelope_target)) ||
		((voice[v].envelope_increment > 0) &&
		 (voice[v].envelope_volume >= voice[v].envelope_target)))
	{
		voice[v].envelope_volume = voice[v].envelope_target;

		if (recompute_envelope(v))
			return 1;
	}

	return 0;
}

static void update_tremolo(int v)
{
	s32_t depth=voice[v].sample->tremolo_depth<<7;

	if (voice[v].tremolo_sweep)
	{
		/* Update sweep position */

		voice[v].tremolo_sweep_position += voice[v].tremolo_sweep;

		if (voice[v].tremolo_sweep_position >= (1<<SWEEP_SHIFT))
			voice[v].tremolo_sweep=0; /* Swept to max amplitude */
		else
		{
			/* Need to adjust depth */
			depth *= voice[v].tremolo_sweep_position;
			depth >>= SWEEP_SHIFT;
		}
	}

	voice[v].tremolo_phase += voice[v].tremolo_phase_increment;

	/* if (voice[v].tremolo_phase >= (SINE_CYCLE_LENGTH<<RATE_SHIFT))
	   voice[v].tremolo_phase -= SINE_CYCLE_LENGTH<<RATE_SHIFT;  */

	double sineval = sine(voice[v].tremolo_phase >> RATE_SHIFT);

	voice[v].tremolo_volume = (double) 
		(1.0 - FSCALENEG((sineval+1.0) * depth * TREMOLO_AMPLITUDE_TUNING, 17));

	/* I'm not sure about the +1.0 there -- it makes tremoloed voices'
	   volumes on average the lower the higher the tremolo amplitude. */
}

/* Returns 1 if the note died */
static int update_signal(int v)
{
	if (voice[v].envelope_increment && update_envelope(v))
		return 1;

	if (voice[v].tremolo_phase_increment)
		update_tremolo(v);

	apply_envelope_to_amp(v);
	return 0;
}


//------------------------------------------------------------------------

#define MIXATION(a)  *lp++ += (a)*s;

#define MIXSKIP lp++
#define MIXMAX(a,b) *lp++ += ((a>b)?a:b) * s
#define MIXCENT(a,b) *lp++ += (a/2+b/2) * s
#define MIXHALF(a)	*lp++ += (a>>1)*s;

static void mix_mystery_signal(const resample_t *sp, s32_t *lp, int v, int count)
{
	Voice *vp = voice + v;

	final_volume_t left_rear = vp->lr_mix; 
	final_volume_t left = vp->left_mix; 
	final_volume_t center = vp->ce_mix; 
	final_volume_t right = vp->right_mix; 
	final_volume_t right_rear = vp->rr_mix; 
	final_volume_t lfe = vp->lfe_mix;

	int cc;

	if (! (cc = vp->control_counter))
	{
		cc = control_ratio;

		if (update_signal(v))
			return;	/* Envelope ran out */

		left_rear = vp->lr_mix;
		left = vp->left_mix;
		center = vp->ce_mix;
		right = vp->right_mix;
		right_rear = vp->rr_mix;
		lfe = vp->lfe_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				resample_t s = *sp++;

				MIXATION(left);
				MIXATION(right);

				if (num_ochannels >= 4)
				{
					MIXATION(left_rear);
					MIXATION(right_rear);
				}
				if (num_ochannels == 6)
				{
					MIXATION(center);
					MIXATION(lfe);
				}
			}
			cc = control_ratio;

			if (update_signal(v))
				return;	/* Envelope ran out */

			left_rear = vp->lr_mix;
			left = vp->left_mix;
			center = vp->ce_mix;
			right = vp->right_mix;
			right_rear = vp->rr_mix;
			lfe = vp->lfe_mix;
		}
		else
		{
			vp->control_counter = cc - count;

			while (count--)
			{
				resample_t s = *sp++;

				MIXATION(left);
				MIXATION(right);

				if (num_ochannels >= 4)
				{
					MIXATION(left_rear);
					MIXATION(right_rear);
				}
				if (num_ochannels == 6)
				{
					MIXATION(center);
					MIXATION(lfe);
				}
			}
			return;
		}
	}
}

static void mix_center_signal(const resample_t *sp, s32_t *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc;

	if (!(cc = vp->control_counter))
	{
		cc = control_ratio;
		if (update_signal(v))
			return;	/* Envelope ran out */
		left = vp->left_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXATION(left);
					MIXATION(left);
				}
				else if (num_ochannels == 4)
				{
					MIXATION(left);
					MIXSKIP;
					MIXATION(left);
					MIXSKIP;
				}
				else if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXSKIP;
					MIXSKIP;
					MIXSKIP;
					MIXATION(left);
					MIXATION(left);
				}
			}
			cc = control_ratio;
			
			if (update_signal(v))
				return;	/* Envelope ran out */
			
			left = vp->left_mix;
		}
		else
		{
			vp->control_counter = cc - count;

			while (count--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXATION(left);
					MIXATION(left);
				}
				else if (num_ochannels == 4)
				{
					MIXATION(left);
					MIXSKIP;
					MIXATION(left);
					MIXSKIP;
				}
				else if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXSKIP;
					MIXSKIP;
					MIXSKIP;
					MIXATION(left);
					MIXATION(left);
				}
			}
			return;
		}
	}
}

static void mix_single_left_signal(const resample_t *sp, s32_t *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc;

	if (!(cc = vp->control_counter))
	{
		cc = control_ratio;
		if (update_signal(v))
			return;	/* Envelope ran out */
		left = vp->left_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXATION(left);
					MIXSKIP;
				}
				if (num_ochannels >= 4)
				{
					MIXHALF(left);
					MIXSKIP;
					MIXATION(left);
					MIXSKIP;
				}
				if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXATION(left);
				}
			}
			cc = control_ratio;

			if (update_signal(v))
				return;	/* Envelope ran out */

			left = vp->left_mix;
		}
		else
		{
			vp->control_counter = cc - count;

			while (count--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXATION(left);
					MIXSKIP;
				}
				if (num_ochannels >= 4)
				{
					MIXHALF(left);
					MIXSKIP;
					MIXATION(left);
					MIXSKIP;
				}
				if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXATION(left);
				}
			}
			return;
		}
	}
}

static void mix_single_right_signal(const resample_t *sp, s32_t *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc;

	if (! (cc = vp->control_counter))
	{
		cc = control_ratio;
		
		if (update_signal(v))
			return;	/* Envelope ran out */
		
		left = vp->left_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXSKIP;
					MIXATION(left);
				}
				if (num_ochannels >= 4)
				{
					MIXSKIP;
					MIXHALF(left);
					MIXSKIP;
					MIXATION(left);
				}
				if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXATION(left);
				}
			}
			cc = control_ratio;

			if (update_signal(v))
				return;	/* Envelope ran out */

			left = vp->left_mix;
		}
		else
		{
			vp->control_counter = cc - count;

			while (count--)
			{
				resample_t s = *sp++;

				if (num_ochannels == 2)
				{
					MIXSKIP;
					MIXATION(left);
				}
				if (num_ochannels >= 4)
				{
					MIXSKIP;
					MIXHALF(left);
					MIXSKIP;
					MIXATION(left);
				}
				if (num_ochannels == 6)
				{
					MIXSKIP;
					MIXATION(left);
				}
			}
			return;
		}
	}
}

static void mix_mono_signal(const resample_t *sp, s32_t *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc;

	if (! (cc = vp->control_counter))
	{
		cc = control_ratio;

		if (update_signal(v))
			return;	/* Envelope ran out */

		left = vp->left_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				resample_t s = *sp++;

				MIXATION(left);
			}
			cc = control_ratio;

			if (update_signal(v))
				return;	/* Envelope ran out */

			left = vp->left_mix;
		}
		else
		{
			vp->control_counter = cc - count;

			while (count--)
			{
				resample_t s = *sp++;

				MIXATION(left);
			}
			return;
		}
	}
}

static void mix_mystery(const resample_t *sp, s32_t *lp, int v, int count)
{
	final_volume_t left_rear=voice[v].lr_mix; 
	final_volume_t left=voice[v].left_mix; 
	final_volume_t center=voice[v].ce_mix; 
	final_volume_t right=voice[v].right_mix; 
	final_volume_t right_rear=voice[v].rr_mix; 
	final_volume_t lfe=voice[v].lfe_mix;

	while (count--)
	{
		resample_t s = *sp++;

		MIXATION(left);
		MIXATION(right);

		if (num_ochannels >= 4)
		{
			MIXATION(left_rear);
			MIXATION(right_rear);
		}
		if (num_ochannels == 6)
		{
			MIXATION(center);
			MIXATION(lfe);
		}
	}
}

static void mix_center(const resample_t *sp, s32_t *lp, int v, int count)
{
	final_volume_t left=voice[v].left_mix;

	while (count--)
	{
		resample_t s = *sp++;

		if (num_ochannels == 2)
		{
			MIXATION(left);
			MIXATION(left);
		}
		else if (num_ochannels == 4)
		{
			MIXATION(left);
			MIXATION(left);
			MIXSKIP;
			MIXSKIP;
		}
		else if (num_ochannels == 6)
		{
			MIXSKIP;
			MIXSKIP;
			MIXSKIP;
			MIXSKIP;
			MIXATION(left);
			MIXATION(left);
		}
	}
}

static void mix_single_left(const resample_t *sp, s32_t *lp, int v, int count)
{
	final_volume_t left=voice[v].left_mix;

	while (count--)
	{
		resample_t s = *sp++;

		if (num_ochannels == 2)
		{
			MIXATION(left);
			MIXSKIP;
		}
		if (num_ochannels >= 4)
		{
			MIXHALF(left);
			MIXSKIP;
			MIXATION(left);
			MIXSKIP;
		}
		if (num_ochannels == 6)
		{
			MIXSKIP;
			MIXATION(left);
		}
	}
}
static void mix_single_right(const resample_t *sp, s32_t *lp, int v, int count)
{
	final_volume_t left=voice[v].left_mix;

	while (count--)
	{
		resample_t s = *sp++;

		if (num_ochannels == 2)
		{
			MIXSKIP;
			MIXATION(left);
		}
		if (num_ochannels >= 4)
		{
			MIXSKIP;
			MIXHALF(left);
			MIXSKIP;
			MIXATION(left);
		}
		if (num_ochannels == 6)
		{
			MIXSKIP;
			MIXATION(left);
		}
	}
}

static void mix_mono(const resample_t *sp, s32_t *lp, int v, int count)
{
	final_volume_t left=voice[v].left_mix;

	while (count--)
	{
		resample_t s = *sp++;

		MIXATION(left);
	}
}

/* Ramp a note out in c samples */
static void ramp_out(const resample_t *sp, s32_t *lp, int v, int count)
{
	/* should be final_volume_t, but uint8 gives trouble. */
	s32_t left_rear, left, center, right, right_rear, lfe, li, ri;

	/* Fix by James Caldwell */
	if (count == 0)
		count = 1;

	left = voice[v].left_mix;

	li = -(left/count);
	if (!li)
		li = -1;

	/* printf("Ramping out: left=%d, c=%d, li=%d\n", left, count, li); */

	if (play_mode_encoding & PE_MONO)
	{
		/* Mono output */
		while (count--)
		{
			left += li;
			if (left<0)
				return;
			
			resample_t s = *sp++;

			MIXATION(left);
		}
		return;
	}

	if (voice[v].panned==PANNED_MYSTERY)
	{
		left_rear = voice[v].lr_mix;
		center=voice[v].ce_mix;
		right=voice[v].right_mix;
		right_rear = voice[v].rr_mix;
		lfe = voice[v].lfe_mix;
		ri = -(right/count);

		while (count--)
		{
			left_rear += li; if (left_rear<0) left_rear=0;
			left += li; if (left<0) left=0;
			center += li; if (center<0) center=0;
			right += ri; if (right<0) right=0;
			right_rear += ri; if (right_rear<0) right_rear=0;
			lfe += li; if (lfe<0) lfe=0;

			resample_t s = *sp++;

			MIXATION(left);
			MIXATION(right);

			if (num_ochannels >= 4)
			{
				MIXATION(left_rear);
				MIXATION(right_rear);
			}
			if (num_ochannels == 6)
			{
				MIXATION(center);
				MIXATION(lfe);
			}
		}
	}
	else if (voice[v].panned==PANNED_CENTER)
	{
		while (count--)
		{
			left += li;
			if (left<0)
				return;

			resample_t s = *sp++;	

			if (num_ochannels == 2)
			{
				MIXATION(left);
				MIXATION(left);
			}
			else if (num_ochannels == 4)
			{
				MIXATION(left);
				MIXATION(left);
				MIXSKIP;
				MIXSKIP;
			}
			else if (num_ochannels == 6)
			{
				MIXSKIP;
				MIXSKIP;
				MIXSKIP;
				MIXSKIP;
				MIXATION(left);
				MIXATION(left);
			}
		}
	}
	else if (voice[v].panned==PANNED_LEFT)
	{
		while (count--)
		{
			left += li;
			if (left<0)
				return;
			
			resample_t s = *sp++;

			MIXATION(left);
			MIXSKIP;

			if (num_ochannels >= 4)
			{
				MIXATION(left);
				MIXSKIP;
			}
			if (num_ochannels == 6)
			{
				MIXATION(left);
				MIXATION(left);
			}
		}
	}
	else if (voice[v].panned==PANNED_RIGHT)
	{
		while (count--)
		{
			left += li;
			if (left<0)
				return;

			resample_t s = *sp++;

			MIXSKIP;
			MIXATION(left);

			if (num_ochannels >= 4)
			{
				MIXSKIP;
				MIXATION(left);
			}
			if (num_ochannels == 6)
			{
				MIXATION(left);
				MIXATION(left);
			}
		}
	}
}


/**************** interface function ******************/

void mix_voice(s32_t *buf, int v, int count)
{
	Voice *vp = voice+v;

	if (count < 0)
		return;

	if (vp->status==VOICE_DIE)
	{
		if (count > MAX_DIE_TIME)
			count = MAX_DIE_TIME;

		resample_t *sp = resample_voice(v, &count);
		ramp_out(sp, buf, v, count);

		vp->status=VOICE_FREE;
		return;
	}

	resample_t *sp = resample_voice(v, &count);

	if (count < 0)
		return;
	
	if (play_mode_encoding & PE_MONO)
	{
		/* Mono output. */
		if (vp->envelope_increment || vp->tremolo_phase_increment)
			mix_mono_signal(sp, buf, v, count);
		else
			mix_mono(sp, buf, v, count);

		return;
	}

	if (vp->panned == PANNED_MYSTERY)
	{
		if (vp->envelope_increment || vp->tremolo_phase_increment)
			mix_mystery_signal(sp, buf, v, count);
		else
			mix_mystery(sp, buf, v, count);

		return;
	}
	else if (vp->panned == PANNED_CENTER)
	{
		if (vp->envelope_increment || vp->tremolo_phase_increment)
			mix_center_signal(sp, buf, v, count);
		else
			mix_center(sp, buf, v, count);

		return;
	}

	/* It's either full left or full right. In either case,
	   every other sample is 0. Just get the offset right: */

	if (vp->envelope_increment || vp->tremolo_phase_increment)
	{
		if (vp->panned == PANNED_RIGHT)
			mix_single_right_signal(sp, buf, v, count);
		else
			mix_single_left_signal(sp, buf, v, count);
	}
	else 
	{
		if (vp->panned == PANNED_RIGHT)
			mix_single_right(sp, buf, v, count);
		else
			mix_single_left(sp, buf, v, count);
	}
}


/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

#define CLIP_THRESHHOLD  ((1L << (31-GUARD_BITS)) - 1)


void s32tos8(void *dp, const s32_t *lp, int count)
{
	s8_t *dest = (s8_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s8_t) (val >> (32-8-GUARD_BITS));
	}
}

void s32tou8(void *dp, const s32_t *lp, int count)
{
	u8_t *dest = (u8_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u8_t) (0x80 ^ (val >> (32-8-GUARD_BITS)));
	}
}

void s32tos16(void *dp, const s32_t *lp, int count)
{
	s16_t *dest = (s16_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s16_t)(val >> (32-16-GUARD_BITS));
	}
}

void s32tou16(void *dp, const s32_t *lp, int count)
{
	u16_t *dest = (u16_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u16_t) (0x8000 ^ (val >> (32-16-GUARD_BITS)));
	}
}

void s32tos16x(void *dp, const s32_t *lp, int count)
{
	s16_t *dest = (s16_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		val >>= (32-16-GUARD_BITS);

		*dest++ = (s16_t)EPI_Swap16((u16_t)val);
	}
}

void s32tou16x(void *dp, const s32_t *lp, int count)
{
	u16_t *dest = (u16_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		val >>= (32-16-GUARD_BITS);

		*dest++ = (u16_t) (0x8000 ^ EPI_Swap16((u16_t)val));
	}
}

void s32toulaw(void *dp, const s32_t *lp, int count)
{
	u8_t *dest = (u8_t *)dp;

	while (count--)
	{
		s32_t val = *lp++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		val >>= (32-13-GUARD_BITS);

		// Note: _l2u has been offsetted by 4096 to allow for
		//       the negative values here.  (-AJA- ick!)
		*dest++ = _l2u[val];
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
