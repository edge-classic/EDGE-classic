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

   instrum.c 
   
   Code to load and unload GUS-compatible instrument patches.
*/

#include "config.h"

#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "mix.h"
#include "ctrlmode.h"
#include "resample.h"
#include "tables.h"

/* Some functions get aggravated if not even the standard banks are 
   available. */
static ToneBank standard_tonebank, standard_drumset;

ToneBank *tonebank[MAXBANK]={&standard_tonebank};
ToneBank *drumset[MAXBANK]={&standard_drumset};

/* This is a special instrument, used for all melodic programs */
InstrumentLayer *default_instrument=0;

/* This is only used for tracks that don't specify a program */
int default_program=DEFAULT_PROGRAM;

#ifdef FAST_DECAY
int fast_decay=1;
#else
int fast_decay=0;
#endif


int current_tune_number = 0;
int last_tune_purged = 0;
int current_patch_memory = 0;

static void free_instrument(Instrument *ip)
{
	Sample *sp;
	int i;
	if (!ip) return;

	if (!ip->contents)
		for (i=0; i<ip->samples; i++)
		{
			sp=&(ip->sample[i]);
			if (sp->data) free(sp->data);
		}
	free(ip->sample);

	if (!ip->contents)
		for (i=0; i<ip->right_samples; i++)
		{
			sp=&(ip->right_sample[i]);
			if (sp->data) free(sp->data);
		}
	if (ip->right_sample)
		free(ip->right_sample);
	free(ip);
}


static void free_layer(InstrumentLayer *lp)
{
	InstrumentLayer *next;

	current_patch_memory -= lp->size;

	for (; lp; lp = next)
	{
		next = lp->next;
		free_instrument(lp->instrument);
		free(lp);
	}
}

static void free_bank(int dr, int b)
{
	ToneBank *bank=((dr) ? drumset[b] : tonebank[b]);

	for (int i=0; i<MAXPROG; i++)
	{
		if (bank->tone[i].layer)
		{
			/* Not that this could ever happen, of course */
			if (bank->tone[i].layer != MAGIC_LOAD_INSTRUMENT)
			{
				if (! bank->tone[i].layer_copy)
					free_layer(bank->tone[i].layer);
				bank->tone[i].layer = NULL;
				bank->tone[i].last_used=-1;
			}
		}
		if (bank->tone[i].name)
		{
			free(bank->tone[i].name);
			bank->tone[i].name = NULL;
		}
	}
}


int convert_envelope_rate_attack(byte rate, byte fastness)
{
	int r = 3 - ((rate>>6) & 0x3);

	r = (int)(rate & 0x3f) << (r * 3); /* 6.9 fixed point */

	/* 15.15 fixed point. */
	return (((r * 44100) / play_mode_rate) * control_ratio) << 10;
}

int convert_envelope_rate(byte rate)
{
	int r = 3 - ((rate>>6) & 0x3);

	r = (int)(rate & 0x3f) << (r * 3); /* 6.9 fixed point */

	/* 15.15 fixed point. */
	return (((r * 44100) / play_mode_rate) * control_ratio) << ((fast_decay) ? 10 : 9);
}

int convert_envelope_offset(byte offset)
{
	/* This is not too good... Can anyone tell me what these values mean?
	   Are they GUS-style "exponential" volumes? And what does that mean? */

	/* 15.15 fixed point */
	return (int)offset << (7+15);
}

int convert_tremolo_sweep(byte sweep)
{
	if (!sweep)
		return 0;

	return
		((control_ratio * SWEEP_TUNING) << SWEEP_SHIFT) /
		(play_mode_rate * sweep);
}

int convert_vibrato_sweep(byte sweep, int vib_control_ratio)
{
	if (!sweep)
		return 0;

	return
		(int) (FSCALE((double) (vib_control_ratio) * SWEEP_TUNING, SWEEP_SHIFT)
				 / (double)(play_mode_rate * sweep));

	/* this was overflowing with seashore.pat

	   ((vib_control_ratio * SWEEP_TUNING) << SWEEP_SHIFT) /
	   (play_mode_rate * sweep); */
}

int convert_tremolo_rate(byte rate)
{
	return
		((SINE_CYCLE_LENGTH * control_ratio * rate) << RATE_SHIFT) /
		(TREMOLO_RATE_TUNING * play_mode_rate);
}

int convert_vibrato_rate(byte rate)
{
	/* Return a suitable vibrato_control_ratio value */
	return
		(VIBRATO_RATE_TUNING * play_mode_rate) / 
		(rate * 2 * VIBRATO_SAMPLE_INCREMENTS);
}

static void reverse_data(s16_t *sp, int ls, int le)
{
	s16_t s, *ep=sp+le;
	sp+=ls;
	le-=ls;
	le/=2;
	while (le--)
	{
		s=*sp;
		*sp++=*ep;
		*ep--=s;
	}
}

/* 
   If panning or note_to_use != -1, it will be used for all samples,
   instead of the sample-specific values in the instrument file. 

   For note_to_use, any value <0 or >127 will be forced to 0.

   For other parameters, 1 means yes, 0 means no, other values are
   undefined.

TODO: do reverse loops right */
static InstrumentLayer *load_instrument(char *name, int font_type, int percussion,
		int panning, int amp, int cfg_tuning, int note_to_use,
		int strip_loop, int strip_envelope,
		int strip_tail, int bank, int gm_num, int sf_ix)
{
	InstrumentLayer *lp, *lastlp;
	InstrumentLayer *headlp = NULL;
	Instrument *ip;

	byte tmp[1024];
	int i,j,noluck=0;

#ifdef PATCH_EXT_LIST
	static const char *patch_ext[] = PATCH_EXT_LIST;
#endif
	int sf2flag = 0;
	int right_samples = 0;
	int stereo_channels = 1, stereo_layer;
	int vlayer_list[19][4], vlayer;
	int vlayer_count = 0;

	if (!name) return 0;

	/* Open patch file */
	FILE *fp = open_via_paths_NOCASE(name, OF_NORMAL);

	if (fp == NULL)
	{
		noluck=1;

#ifdef PATCH_EXT_LIST
		/* Try with various extensions */
		for (i=0; patch_ext[i]; i++)
		{
			if (strlen(name)+strlen(patch_ext[i])<1024)
			{
				char path[1024];
				strcpy(path, name);
				strcat(path, patch_ext[i]);

				fp = open_via_paths_NOCASE(path, OF_NORMAL);

				if (fp)
				{
					noluck=0;
					break;
				}
			}
		}
#endif
	}

	if (noluck)
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, 
				"Instrument `%s' can't be found.", name);
		return 0;
	}

	ctl_msg(CMSG_INFO, VERB_DEBUG, "Loading instrument %s", name);

	/* Read some headers and do cursory sanity checks. There are loads
	   of magic offsets. This could be rewritten... */

	if ((239 != fread(tmp, 1, 239, fp)) ||
			(memcmp(tmp, "GF1PATCH110\0ID#000002", 22) &&
			 memcmp(tmp, "GF1PATCH100\0ID#000002", 22))) /* don't know what the
															differences are */
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: not an instrument", name);
		return 0;
	}

	/* patch layout:
	 * bytes:  info:		starts at offset:
	 * 22	id (see above)		0
	 * 60	copyright		22
	 *  1	instruments		82
	 *  1	voices			83
	 *  1	channels		84
	 *  2	number of waveforms	85
	 *  2	master volume		87
	 *  4	datasize		89
	 * 36   reserved, but now:	93
	 * 	7 "SF2EXT\0" id			93
	 * 	1 right samples		       100
	 *     28 reserved		       101
	 *  2	instrument number	129
	 * 16	instrument name		131
	 *  4	instrument size		147
	 *  1	number of layers	151
	 * 40	reserved		152
	 *  1	layer duplicate		192
	 *  1	layer number		193
	 *  4	layer size		194
	 *  1	number of samples	198
	 * 40	reserved		199
	 * 				239
	 * THEN, for each sample, see below
	 */

	if (0 == memcmp(tmp + 93, "SF2EXT", 6))
	{
		sf2flag = 1;
		vlayer_count = tmp[152];
	}

	if (tmp[82] != 1 && tmp[82] != 0) /* instruments. To some patch makers, 
										 0 means 1 */
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, 
				"Can't handle patches with %d instruments", tmp[82]);
		return 0;
	}

	if (tmp[151] != 1 && tmp[151] != 0) /* layers. What's a layer? */
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, 
				"Can't handle instruments with %d layers", tmp[151]);
		return 0;
	}


	if (sf2flag && vlayer_count > 0)
	{
		for (i = 0; i < 9; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = tmp[153+i*4+j];
		for (i = 9; i < 19; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = tmp[199+(i-9)*4+j];
	}
	else
	{
		for (i = 0; i < 19; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = 0;
		vlayer_list[0][0] = 0;
		vlayer_list[0][1] = 127;
		vlayer_list[0][2] = tmp[198];
		vlayer_list[0][3] = 0;
		vlayer_count = 1;
	}

	lastlp = 0;

	for (vlayer = 0; vlayer < vlayer_count; vlayer++)
	{

		lp=(InstrumentLayer *)safe_malloc(sizeof(InstrumentLayer));
		lp->size = sizeof(InstrumentLayer);
		lp->lo = vlayer_list[vlayer][0];
		lp->hi = vlayer_list[vlayer][1];
		ip=(Instrument *)safe_malloc(sizeof(Instrument));
		lp->size += sizeof(Instrument);
		lp->instrument = ip;
		lp->next = 0;

		if (lastlp) lastlp->next = lp;
		else headlp = lp;

		lastlp = lp;

		if (sf2flag) ip->type = INST_SF2;
		else ip->type = INST_GUS;
		ip->samples = vlayer_list[vlayer][2];
		ip->sample = (Sample *)safe_malloc(sizeof(Sample) * ip->samples);
		lp->size += sizeof(Sample) * ip->samples;
		ip->left_samples = ip->samples;
		ip->left_sample = ip->sample;
		right_samples = vlayer_list[vlayer][3];
		ip->right_samples = right_samples;
		if (right_samples)
		{
			ip->right_sample = (Sample *)safe_malloc(sizeof(Sample) * right_samples);
			lp->size += sizeof(Sample) * right_samples;
			stereo_channels = 2;
		}
		else ip->right_sample = 0;
		ip->contents = 0;

		ctl_msg(CMSG_INFO, VERB_DEBUG_SILLY,
				"%s%s[%d,%d] %s(%d-%d layer %d of %d)",
				(percussion)? "   ":"", name,
				(percussion)? note_to_use : gm_num, bank,
				(right_samples)? "(2) " : "",
				lp->lo, lp->hi, vlayer+1, vlayer_count);

		for (stereo_layer = 0; stereo_layer < stereo_channels; stereo_layer++)
		{
			int sample_count = ip->left_samples;

			if (stereo_layer == 1)
				sample_count = ip->right_samples;

			for (i=0; i < sample_count; i++)
			{
				s32_t tmplong;
				u16_t tmpshort;
				u16_t sample_volume = 0;
				byte tmpchar;

				byte fractions;
				byte sf2delay = 1;

#define READ_CHAR(thing) \
				if (1 != fread(&tmpchar, 1, 1, fp)) goto fail; \
				thing = tmpchar;
#define READ_SHORT(thing) \
				if (1 != fread(&tmpshort, 2, 1, fp)) goto fail; \
				thing = EPI_LE_U16(tmpshort);
#define READ_LONG(thing) \
				if (1 != fread(&tmplong, 4, 1, fp)) goto fail; \
				thing = EPI_LE_U32(tmplong);

				/*
				 *  7	sample name
				 *  1	fractions
				 *  4	length
				 *  4	loop start
				 *  4	loop end
				 *  2	sample rate
				 *  4	low frequency
				 *  4	high frequency
				 *  2	finetune
				 *  1	panning
				 *  6	envelope rates			|
				 *  6	envelope offsets		|  18 bytes
				 *  3	tremolo sweep, rate, depth	|
				 *  3	vibrato sweep, rate, depth	|
				 *  1	sample mode
				 *  2	scale frequency
				 *  2	scale factor
				 *  2	sample volume (??)
				 * 34	reserved
				 * Now: 1	delay
				 * 	33	reserved
				 */
				skip(fp, 7); /* Skip the wave name */

				if (1 != fread(&fractions, 1, 1, fp))
				{
fail:
					ctl_msg(CMSG_ERROR, VERB_NORMAL, "Error reading sample %d", i);

					if (stereo_layer == 1)
					{
						for (j=0; j<i; j++)
							free(ip->right_sample[j].data);
						free(ip->right_sample);
						i = ip->left_samples;
					}
					for (j=0; j<i; j++)
						free(ip->left_sample[j].data);
					free(ip->left_sample);
					free(ip);
					free(lp);
					return 0;
				}

				Sample *sp;

				if (stereo_layer == 0)
					sp=&(ip->left_sample[i]);
				else if (stereo_layer == 1)
					sp=&(ip->right_sample[i]);
				else
					continue; // -AJA- added

				READ_LONG(sp->data_length);
				READ_LONG(sp->loop_start);
				READ_LONG(sp->loop_end);
				READ_SHORT(sp->sample_rate);
				READ_LONG(sp->low_freq);
				READ_LONG(sp->high_freq);
				READ_LONG(sp->root_freq);
				skip(fp, 2); /* Why have a "root frequency" and then "tuning"?? */

				READ_CHAR(tmp[0]);

				if (panning==-1)
					sp->panning = (tmp[0] * 8 + 4) & 0x7f;
				else
					sp->panning=(byte)(panning & 0x7F);

				sp->resonance=0;
				sp->cutoff_freq=0;
				sp->reverberation=0;
				sp->chorusdepth=0;
				sp->exclusiveClass=0;
				sp->keyToModEnvHold=0;
				sp->keyToModEnvDecay=0;
				sp->keyToVolEnvHold=0;
				sp->keyToVolEnvDecay=0;

				if (cfg_tuning)
				{
					double tune_factor = (double)(cfg_tuning)/1200.0;
					tune_factor = pow(2.0, tune_factor);
					sp->root_freq = (u32_t)( tune_factor * (double)sp->root_freq );
				}

				/* envelope, tremolo, and vibrato */
				if (18 != fread(tmp, 1, 18, fp)) goto fail; 

				if (!tmp[13] || !tmp[14])
				{
					sp->tremolo_sweep_increment=
						sp->tremolo_phase_increment=sp->tremolo_depth=0;
					ctl_msg(CMSG_INFO, VERB_DEBUG, " * no tremolo");
				}
				else
				{
					sp->tremolo_sweep_increment=convert_tremolo_sweep(tmp[12]);
					sp->tremolo_phase_increment=convert_tremolo_rate(tmp[13]);
					sp->tremolo_depth=tmp[14];

					ctl_msg(CMSG_INFO, VERB_DEBUG,
							" * tremolo: sweep %d, phase %d, depth %d",
							sp->tremolo_sweep_increment, sp->tremolo_phase_increment,
							sp->tremolo_depth);
				}

				if (!tmp[16] || !tmp[17])
				{
					sp->vibrato_sweep_increment=
						sp->vibrato_control_ratio=sp->vibrato_depth=0;
					ctl_msg(CMSG_INFO, VERB_DEBUG, " * no vibrato");
				}
				else
				{
					sp->vibrato_control_ratio=convert_vibrato_rate(tmp[16]);
					sp->vibrato_sweep_increment=
						convert_vibrato_sweep(tmp[15], sp->vibrato_control_ratio);
					sp->vibrato_depth=tmp[17];

					ctl_msg(CMSG_INFO, VERB_DEBUG,
							" * vibrato: sweep %d, ctl %d, depth %d",
							sp->vibrato_sweep_increment, sp->vibrato_control_ratio,
							sp->vibrato_depth);

				}

				READ_CHAR(sp->modes);
				READ_SHORT(sp->freq_center);
				READ_SHORT(sp->freq_scale);

				if (sf2flag)
				{
					READ_SHORT(sample_volume);
					READ_CHAR(sf2delay);
					READ_CHAR(sp->exclusiveClass);
					skip(fp, 32);
				}
				else
				{
					skip(fp, 36);
				}

				/* Mark this as a fixed-pitch instrument if such a deed is desired. */
				if (note_to_use!=-1)
					sp->note_to_use=(byte)(note_to_use);
				else
					sp->note_to_use=0;

				/* seashore.pat in the Midia patch set has no Sustain. I don't
				   understand why, and fixing it by adding the Sustain flag to
				   all looped patches probably breaks something else. We do it
				   anyway. */

				if (sp->modes & MODES_LOOPING) 
					sp->modes |= MODES_SUSTAIN;

				/* Strip any loops and envelopes we're permitted to */
				if ((strip_loop==1) && 
						(sp->modes & (MODES_SUSTAIN | MODES_LOOPING | 
									  MODES_PINGPONG | MODES_REVERSE)))
				{
					ctl_msg(CMSG_INFO, VERB_DEBUG, " - Removing loop and/or sustain");

					sp->modes &=~(MODES_SUSTAIN | MODES_LOOPING | 
							MODES_PINGPONG | MODES_REVERSE);
				}

				if (strip_envelope==1)
				{
					if (sp->modes & MODES_ENVELOPE)
					{
						ctl_msg(CMSG_INFO, VERB_DEBUG, " - Removing envelope");

						sp->modes &= ~MODES_ENVELOPE;
					}
				}
				else if (strip_envelope != 0)
				{
					/* Have to make a guess. */
					if (!(sp->modes & (MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
					{
						/* No loop? Then what's there to sustain? No envelope needed
						   either... */
						sp->modes &= ~(MODES_SUSTAIN|MODES_ENVELOPE);

						ctl_msg(CMSG_INFO, VERB_DEBUG, 
								" - No loop, removing sustain and envelope");
					}
					else if (!memcmp(tmp, "??????", 6) || tmp[11] >= 100) 
					{
						/* Envelope rates all maxed out? Envelope end at a high "offset"?
						   That's a weird envelope. Take it out. */
						sp->modes &= ~MODES_ENVELOPE;

						ctl_msg(CMSG_INFO, VERB_DEBUG, 
								" - Weirdness, removing envelope");
					}
					else if (!(sp->modes & MODES_SUSTAIN))
					{
						/* No sustain? Then no envelope.  I don't know if this is
						   justified, but patches without sustain usually don't need the
						   envelope either... at least the Gravis ones. They're mostly
						   drums.  I think. */
						sp->modes &= ~MODES_ENVELOPE;

						ctl_msg(CMSG_INFO, VERB_DEBUG, 
								" - No sustain, removing envelope");
					}
				}

				sp->attenuation = 0;

				for (j=ATTACK; j<DELAY; j++)
				{
					sp->envelope_rate[j]=
						(j<3)? convert_envelope_rate_attack(tmp[j], 11) : convert_envelope_rate(tmp[j]);
					sp->envelope_offset[j]= 
						convert_envelope_offset(tmp[6+j]);
				}
				if (sf2flag)
				{
					if (sf2delay > 5) sf2delay = 5;
					sp->envelope_rate[DELAY] = (int)( (sf2delay*play_mode_rate) / 1000 );
				}
				else
				{
					sp->envelope_rate[DELAY]=0;
				}
				sp->envelope_offset[DELAY]=0;

				for (j=ATTACK; j<DELAY; j++)
				{
					sp->modulation_rate[j]=sp->envelope_rate[j];
					sp->modulation_offset[j]=sp->envelope_offset[j];
				}
				sp->modulation_rate[DELAY] = sp->modulation_offset[DELAY] = 0;
				sp->modEnvToFilterFc=0;
				sp->modEnvToPitch=0;
				sp->lfo_sweep_increment = 0;
				sp->lfo_phase_increment = 0;
				sp->modLfoToFilterFc = 0;
				sp->vibrato_delay = 0;

				/* Then read the sample data */
				if (sp->data_length/2 > MAX_SAMPLE_SIZE)
				{
					goto fail;
				}
				sp->data = (sample_t*)safe_malloc(sp->data_length + 1);
				lp->size += sp->data_length + 1;

				if (1 != fread(sp->data, sp->data_length, 1, fp))
					goto fail;

				if (!(sp->modes & MODES_16BIT)) /* convert to 16-bit data */
				{
					int i=sp->data_length;
					byte *cp=(byte *)(sp->data);
					u16_t *tmp,*newdta;
					tmp = newdta = (u16_t*) safe_malloc(sp->data_length*2 + 2);
					while (i--)
						*tmp++ = (u16_t)(*cp++) << 8;
					cp=(byte *)(sp->data);
					sp->data = (sample_t *)newdta;
					free(cp);
					sp->data_length *= 2;
					sp->loop_start *= 2;
					sp->loop_end *= 2;
				}
#if EPI_BYTEORDER == EPI_BIG_ENDIAN
				else
					/* convert to machine byte order */
				{
					int i=sp->data_length/2;
					s16_t *tmp=(s16_t *)sp->data,s;
					while (i--)
					{ 
						s=EPI_LE_U16(*tmp);
						*tmp++=s;
					}
				}
#endif

				if (sp->modes & MODES_UNSIGNED) /* convert to signed data */
				{
					int i=sp->data_length/2;
					s16_t *tmp=(s16_t *)sp->data;
					while (i--)
						*tmp++ ^= 0x8000;
				}

				/* Reverse reverse loops and pass them off as normal loops */
				if (sp->modes & MODES_REVERSE)
				{
					/* The GUS apparently plays reverse loops by reversing the
					   whole sample. We do the same because the GUS does not SUCK. */

					ctl_msg(CMSG_WARNING, VERB_NORMAL, "Reverse loop in %s", name);

					reverse_data((s16_t *)sp->data, 0, sp->data_length/2);

					int t = sp->loop_start;
					sp->loop_start=sp->data_length - sp->loop_end;
					sp->loop_end=sp->data_length - t;

					sp->modes &= ~MODES_REVERSE;
					sp->modes |= MODES_LOOPING; /* just in case */
				}

#ifdef ADJUST_SAMPLE_VOLUMES
				if (amp!=-1)
					sp->volume=(double)((amp) / 100.0);
				else if (sf2flag)
					sp->volume=(double)((sample_volume) / 255.0);
				else
				{
					/* Try to determine a volume scaling factor for the sample.
					   This is a very crude adjustment, but things sound more
					   balanced with it. Still, this should be a runtime option. */
					u32_t i, numsamps=sp->data_length/2;
					u32_t higher=0, highcount=0;
					s16_t maxamp=0,a;
					s16_t *tmp=(s16_t *)sp->data;
					i = numsamps;
					while (i--)
					{
						a=*tmp++;
						if (a<0) a=-a;
						if (a>maxamp)
							maxamp=a;
					}
					tmp=(s16_t *)sp->data;
					i = numsamps;
					while (i--)
					{
						a=*tmp++;
						if (a<0) a=-a;
						if (a > 3*maxamp/4)
						{
							higher += a;
							highcount++;
						}
					}
					if (highcount)
						higher /= highcount;
					else
						higher = 10000;
					
					sp->volume = (32768.0 * 0.875) /  (double)higher ;

					ctl_msg(CMSG_INFO, VERB_DEBUG, " * volume comp: %f", sp->volume);
				}
#else
				if (amp!=-1)
					sp->volume=(double)(amp) / 100.0;
				else
					sp->volume=1.0;
#endif

				sp->data_length /= 2; /* These are in bytes. Convert into samples. */

				sp->loop_start /= 2;
				sp->loop_end /= 2;
				sp->data[sp->data_length] = sp->data[sp->data_length-1];

				/* Then fractional samples */
				sp->data_length <<= FRACTION_BITS;
				sp->loop_start <<= FRACTION_BITS;
				sp->loop_end <<= FRACTION_BITS;

				/* trim off zero data at end */
				{
					int ls = sp->loop_start>>FRACTION_BITS;
					int le = sp->loop_end>>FRACTION_BITS;
					int se = sp->data_length>>FRACTION_BITS;
					while (se > 1 && !sp->data[se-1]) se--;
					if (le > se) le = se;
					if (ls >= le) sp->modes &= ~MODES_LOOPING;
					sp->loop_end = le<<FRACTION_BITS;
					sp->data_length = se<<FRACTION_BITS;
				}

				/* Adjust for fractional loop points. This is a guess. Does anyone
				   know what "fractions" really stands for? */
				sp->loop_start |=
					(fractions & 0x0F) << (FRACTION_BITS-4);
				sp->loop_end |=
					((fractions>>4) & 0x0F) << (FRACTION_BITS-4);

				/* If this instrument will always be played on the same note,
				   and it's not looped, we can resample it now. */
				if (sp->note_to_use && !(sp->modes & MODES_LOOPING))
					pre_resample(sp);

				if (strip_tail==1)
				{
					/* Let's not really, just say we did. */
					ctl_msg(CMSG_INFO, VERB_DEBUG, " - Stripping tail");
					sp->data_length = sp->loop_end;
				}
			} /* end of sample loop */
		} /* end of stereo layer loop */
	} /* end of vlayer loop */


	fclose(fp);
	return headlp;
}

/* AJA: new code */
static const int aja_drum_remap[47][4] =
{
	{ 36, 40, 38,  0 },  // Acoustic Bass Drum
	{ 35, 38, 40,  0 },  // Bass Drum
	{ 75, 77,  0,  0 },  // Slide Stick
	{ 40, 35, 36,  0 },  // Acoustic Snare
	{ 58, 70,  0,  0 },  // Hand Clap
	{ 38, 36, 35,  0 },  // Electric Snare
	{ 45, 47,  0,  0 },  // Low Floor Tom
	{ 44, 46,  0,  0 },  // Closed High-Hat
	{ 50, 48,  0,  0 },  // High Floor Tom
	{ 42, 46,  0,  0 },  // Pedal High Hat
	{ 47, 41,  0,  0 },  // Low Tom
	{ 44, 42,  0,  0 },  // Open High Hat
	{ 45, 41,  0,  0 },  // Low-Mid Tom
	{ 50, 43,  0,  0 },  // High-Mid Tom
	{ 57, 51,  0,  0 },  // Crash Cymbal 1
	{ 48, 43,  0,  0 },  // High Tom
	{ 59,  0,  0,  0 },  // Ride Cymbal 1
	{ 55,  0,  0,  0 },  // Chinses Cymbal
	{ 56,  0,  0,  0 },  // Ride Bell
	{ 69,  0,  0,  0 },  // Tambourine
	{ 52,  0,  0,  0 },  // Splash Cymbal
	{ 53,  0,  0,  0 },  // Cowbell
	{ 49, 59,  0,  0 },  // Crash Cymbal 2
	{ 39,  0,  0,  0 },  // Vibraslap
	{ 51,  0,  0,  0 },  // Ride Cymbal 2
	{ 61,  0,  0,  0 },  // High Bongo
	{ 60,  0,  0,  0 },  // Low Bongo
	{ 63, 64,  0,  0 },  // Mute High Conga
	{ 62, 64,  0,  0 },  // Open High Conga
	{ 63, 62,  0,  0 },  // Low Conga
	{ 66, 67,  0,  0 },  // High Timbale
	{ 65, 68,  0,  0 },  // Low Timbale
	{ 68, 65,  0,  0 },  // High Agogo
	{ 67, 66,  0,  0 },  // Low Agogo
	{ 70,  0,  0,  0 },  // Cabasa
	{ 69,  0,  0,  0 },  // Maracas
	{ 72, 73,  0,  0 },  // Short Whistle
	{ 71, 74,  0,  0 },  // Long Whistle
	{ 74, 71,  0,  0 },  // Short Guiro
	{ 73, 72,  0,  0 },  // Long Guiro
	{ 37, 76,  0,  0 },  // Claves
	{ 77, 75,  0,  0 },  // High Wood Block
	{ 76, 75,  0,  0 },  // Low Wood Block
	{ 79, 80,  0,  0 },  // Mute Cuica
	{ 78, 81,  0,  0 },  // Open Cuica
	{ 81, 78,  0,  0 },  // Mute Triangle
	{ 80, 79,  0,  0 }   // Open Triangle
};                     

int aja_fallback_drum(int i, int dist)
{
	SYS_ASSERT(dist >= 1);

	if (dist > 3)
		return -1;

	for (int k = 0; k < 47; k++)
	{
		const int *map = &aja_drum_remap[k][0];

		if (map[0] == i && map[dist] != 0)
			return map[dist];
	}

	return -1;
}


int aja_fallback_program(int i, int dist)
{
	SYS_ASSERT(dist >= 1);

	if (dist > 7)
		return -1;

	int base = i & ~7;

	// don't remap SYNTH / SOUND EFFECTS / PERCUSSIVE
	if (base == 96 || base == 120 || base == 112)
		return -1;

	// try to use a lower instrument number (on the assumption
	// that it is more "normal" than a higher one).

	if (i - dist >= base)
		return i - dist;

	dist -= (i - base);

	return i + dist;
}


static bool try_load_instr(ToneBank *bank, int b, int dr, int i)
{
	SYS_ASSERT(i >= 0);

	if (! bank->tone[i].name)
		return false;

	bank->tone[i].layer_copy = 0;
	bank->tone[i].layer = load_instrument(bank->tone[i].name, 
			bank->tone[i].font_type, (dr) ? 1 : 0,
			bank->tone[i].pan, bank->tone[i].amp, bank->tone[i].tuning,
			(bank->tone[i].note!=-1) ?  bank->tone[i].note : ((dr) ? i : -1),
			(bank->tone[i].strip_loop!=-1) ?  bank->tone[i].strip_loop : ((dr) ? 1 : -1),
			(bank->tone[i].strip_envelope != -1) ?  bank->tone[i].strip_envelope : ((dr) ? 1 : -1),
			bank->tone[i].strip_tail, b /* bank */,
			((dr) ? i + 128 : i), bank->tone[i].sf_ix);

	if (bank->tone[i].layer)
	{
		bank->tone[i].last_used = current_tune_number;
		current_patch_memory += bank->tone[i].layer->size;

		return true;
	}

	ctl_msg(CMSG_ERROR, VERB_NORMAL, 
			"Couldn't load instrument %s (%s %d, program %d)",
			bank->tone[i].name,
			(dr)? "drum set" : "tone bank", b, i);

	return false;
}


static int fill_bank(int dr, int b)
{
	int errors=0;

	ToneBank *bank = (dr) ? drumset[b] : tonebank[b];

	if (!bank)
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, 
				"Huh. Tried to load instruments in non-existent %s %d",
				(dr) ? "drumset" : "tone bank", b);
		return 0;
	}

	/* Primary Pass : try to load instruments normally */

	for (int i=0; i < MAXPROG; i++)
	{
		if (bank->tone[i].layer != MAGIC_LOAD_INSTRUMENT)
			continue;

		if (try_load_instr(bank, b, dr, i))
		{
			// OK!!
			continue;
		}

		errors++;

		if (b == 0)
		{
			// standard bank : try again in second pass
			bank->tone[i].layer = MAGIC_LOAD_INSTRUMENT;
			continue;
		}

		// force usage of the instrument in bank #0
		bank->tone[i].layer = NULL;

		// mark the instrument in bank #0 to be loaded
		ToneBank *bank_zero = (dr) ? drumset[0] : tonebank[0];

		if (! bank_zero->tone[i].layer)
			bank_zero->tone[i].layer = MAGIC_LOAD_INSTRUMENT;
	}

	if (b != 0)
		return errors;

	/* Secondary Pass for Bank #0 : fallback remaps */

	int passes = dr ? 4 : 8;

	for (int dist = 1; dist <= passes; dist++)
	{
		for (int i=0; i < MAXPROG; i++)
		{
			if (bank->tone[i].layer != MAGIC_LOAD_INSTRUMENT)
				continue;

			int new_i;
			if (dr)
				new_i = aja_fallback_drum(i, dist);
			else
				new_i = aja_fallback_program(i, dist);

			if (new_i < 0)
				continue;

			if (! bank->tone[new_i].name)
				continue;

			if (! bank->tone[new_i].layer)
			{
				try_load_instr(bank, b, dr, new_i);
			}

			if (bank->tone[new_i].layer &&
				bank->tone[new_i].layer != MAGIC_LOAD_INSTRUMENT &&
				! bank->tone[new_i].layer_copy)
			{
				bank->tone[i].layer = bank->tone[new_i].layer;
				bank->tone[i].layer_copy = 1;

				ctl_msg(CMSG_WARNING, VERB_NORMAL,
						"No instrument mapped to %s %d, program %d"
						" - remapped to program %d",
						(dr)? "drum set" : "tone bank", b, i, new_i);
				continue;
			}

		} // instrument
	} // dist

	/* Final Pass : find the failures */

	for (int i=0; i < MAXPROG; i++)
	{
		if (bank->tone[i].layer == MAGIC_LOAD_INSTRUMENT)
		{
			bank->tone[i].layer = NULL;

			ctl_msg(CMSG_WARNING, VERB_NORMAL,
					"No instrument mapped to %s %d, program %d%s",
					(dr)? "drum set" : "tone bank", b, i, 
					(b != 0) ? "" : " - this instrument will not be heard");

			errors++;
		}
	}

	return errors;
}


int load_missing_instruments(void)
{
	int errors=0;

	for (int b=MAXBANK-1; b >= 0; b--)
	{
		if (tonebank[b])
			errors += fill_bank(0,b);

		if (drumset[b])
			errors += fill_bank(1,b);
	}
	current_tune_number++;

	return errors;
}

void free_instruments(void)
{
	for (int b=MAXBANK-1; b >= 0; b--)
	{
		if (tonebank[b])
			free_bank(0,b);

		if (drumset[b])
			free_bank(1,b);
	}
}

int set_default_instrument(char *name)
{
	InstrumentLayer *lp;
	/*  if (!(lp=load_instrument(name, 0, -1, -1, -1, 0, 0, 0))) */
	if (!(lp=load_instrument(name, FONT_NORMAL, 0, -1, -1, 0, -1, -1, -1, -1, 0, -1, -1)))
		return -1;

	if (default_instrument)
		free_layer(default_instrument);

	default_instrument=lp;
	default_program=SPECIAL_PROGRAM;

	return 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
