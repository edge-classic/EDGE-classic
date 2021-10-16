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

    tables.h
*/

#ifndef __TIMIDITY_TABLES_H__
#define __TIMIDITY_TABLES_H__

#define sine(x)  sin((x)*2*M_PI/1024.0)

#define SINE_CYCLE_LENGTH 1024
extern int freq_table[];
extern double vol_table[];
extern double expr_table[];
extern double bend_fine[];
extern double bend_coarse[];
extern byte *_l2u; /* 13-bit PCM to 8-bit u-law */

#define XMAPMAX 800
extern int xmap[XMAPMAX][5];

#endif /* __TIMIDITY_TABLES_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
