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

    ctrlmode.h
*/

#ifndef __TIMIDITY_CONTROLS_H__
#define __TIMIDITY_CONTROLS_H__

#define CMSG_INFO	0
#define CMSG_WARNING	1
#define CMSG_ERROR	2
#define CMSG_FATAL	3
#define CMSG_TRACE	4
#define CMSG_TIME	5
#define CMSG_TOTAL	6
#define CMSG_FILE	7
#define CMSG_TEXT	8

#define VERB_NORMAL	0
#define VERB_VERBOSE	1
#define VERB_NOISY	2
#define VERB_DEBUG	3
#define VERB_DEBUG_SILLY	4

extern int ctl_verbosity;

void ctl_msg(int type, int verbosity, const char *fmt, ...);


#endif /* __TIMIDITY_CONTROLS_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
