/*
    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    In case you haven't heard, this program is free software; 
    you can redistribute it and/or modify it under the terms of the
    GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    mix.h
*/

#ifndef __TIMIDITY_MIX_H__
#define __TIMIDITY_MIX_H__

extern void mix_voice(s32_t *buf, int v, int count);
extern int recompute_envelope(int v);
extern void apply_envelope_to_amp(int v);

/* Data format encoding bits */

#define PE_MONO     0x01  /* versus stereo */
#define PE_SIGNED   0x02  /* versus unsigned */
#define PE_16BIT    0x04  /* versus 8-bit */
#define PE_ULAW     0x08  /* versus linear */
#define PE_BYTESWAP 0x10  /* versus the other way */

extern int play_mode_rate;
extern int play_mode_encoding;

/* Conversion functions -- These convert the s32_t data in *lp
   to data in another format */

/* Actual copy function */
extern void (*s32tobuf)(void *dp, const s32_t *lp, int count);

/* 8-bit signed and unsigned*/
extern void s32tos8(void *dp, const s32_t *lp, int count);
extern void s32tou8(void *dp, const s32_t *lp, int count);

/* 16-bit */
extern void s32tos16(void *dp, const s32_t *lp, int count);
extern void s32tou16(void *dp, const s32_t *lp, int count);

/* byte-exchanged 16-bit */
extern void s32tos16x(void *dp, const s32_t *lp, int count);
extern void s32tou16x(void *dp, const s32_t *lp, int count);

/* uLaw (8 bits) */
extern void s32toulaw(void *dp, const s32_t *lp, int count);

#endif /* __TIMIDITY_MIX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
