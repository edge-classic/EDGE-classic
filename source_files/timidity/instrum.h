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

   instrum.h
*/

#ifndef __TIMIDITY_INSTRUM_H__
#define __TIMIDITY_INSTRUM_H__

typedef struct
{
	int loop_start, loop_end, data_length;
	int sample_rate, low_freq, high_freq, root_freq;

	byte root_tune, fine_tune;

	int envelope_rate[7], envelope_offset[7];
	int modulation_rate[7], modulation_offset[7];

	double volume, resonance;
	double modEnvToFilterFc, modEnvToPitch, modLfoToFilterFc;

	sample_t *data;

	int tremolo_sweep_increment, tremolo_phase_increment; 
	int lfo_sweep_increment, lfo_phase_increment;
	int vibrato_sweep_increment, vibrato_control_ratio;
	int cutoff_freq;

	byte reverberation, chorusdepth;
	byte tremolo_depth, vibrato_depth;
	byte modes;

	byte attenuation, freq_center;

	s8_t panning, note_to_use, exclusiveClass;

	s16_t scale_tuning, keyToModEnvHold, keyToModEnvDecay;
	s16_t keyToVolEnvHold, keyToVolEnvDecay;

	int freq_scale, vibrato_delay;
}
Sample;

/* Bits in modes: */
#define MODES_16BIT (1<<0)
#define MODES_UNSIGNED (1<<1)
#define MODES_LOOPING (1<<2)
#define MODES_PINGPONG (1<<3)
#define MODES_REVERSE (1<<4)
#define MODES_SUSTAIN (1<<5)
#define MODES_ENVELOPE (1<<6)
#define MODES_FAST_RELEASE (1<<7)


#define INST_GUS 0
#define INST_SF2 1

typedef struct
{
	int type;

	int samples;
	Sample *sample;

	int left_samples;
	Sample *left_sample;

	int right_samples;
	Sample *right_sample;

	unsigned char *contents;
}
Instrument;


typedef struct InstrumentLayer_s
{
  byte lo, hi;
  int size;
  Instrument *instrument;
  struct InstrumentLayer_s *next;
}
InstrumentLayer;

#define FONT_NORMAL 0
#define FONT_FFF    1
#define FONT_SBK    2
#define FONT_TONESET 3
#define FONT_DRUMSET 4
#define FONT_PRESET 5


typedef struct
{
  char *name;
  
  InstrumentLayer *layer;
  int layer_copy;

  int font_type, sf_ix, last_used, tuning;
  int note, amp, pan, strip_loop, strip_envelope, strip_tail;
}
ToneBankElement;


/* A hack to delay instrument loading until after reading the
   entire MIDI file. */
#define MAGIC_LOAD_INSTRUMENT ((InstrumentLayer *)(-1))

#define MAXPROG 128
#define MAXBANK 130
#define SFXBANK (MAXBANK-1)
#define SFXDRUM1 (MAXBANK-2)
#define SFXDRUM2 (MAXBANK-1)
#define XGDRUM 1


typedef struct
{
  char *name;
  ToneBankElement tone[MAXPROG];
}
ToneBank;


extern char *sf_file;

extern ToneBank *tonebank[], *drumset[];

extern InstrumentLayer *default_instrument;

extern int default_program;
extern int antialiasing_allowed;
extern int fast_decay;
extern int free_instruments_afterwards;

#define SPECIAL_PROGRAM -1

extern int load_missing_instruments(void);
extern void free_instruments(void);
extern void end_soundfont(void);
extern int set_default_instrument(char *name);


extern int convert_tremolo_sweep(byte sweep);
extern int convert_vibrato_sweep(byte sweep, int vib_control_ratio);
extern int convert_tremolo_rate(byte rate);
extern int convert_vibrato_rate(byte rate);

extern int init_soundfont(char *fname, int oldbank, int newbank, int level);
extern InstrumentLayer *load_sbk_patch(const char *name, int gm_num, int bank, int percussion,
    int panning, int amp, int note_to_use, int sf_ix);

extern int current_tune_number;
extern int current_patch_memory;

#define XMAPMAX 800
extern int xmap[XMAPMAX][5];
extern void pcmap(int *b, int *v, int *p, int *drums);

extern int aja_fallback_drum(int i, int dist);
extern int aja_fallback_program(int i, int dist);

#endif /* __TIMIDITY_INSTRUM_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
