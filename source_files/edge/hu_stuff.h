//----------------------------------------------------------------------------
//  EDGE Head Up Display
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

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#include "e_event.h"
#include "r_defs.h"

extern int showMessages;

//
// Globally visible constants.
//

#define HU_BROADCAST	5

#define HU_MSGX		0
#define HU_MSGY		0
#define HU_MSGWIDTH	64  // in characters
#define HU_MSGHEIGHT	1  // in lines

#define HU_MSGTIMEOUT	(4*TICRATE)

#define HU_IS_PRINTABLE(c) ((c) >= 32 && (c) <= 126)

extern bool chat_on;

//
// HEADS UP TEXT
//

void HU_Init(void);
void HU_Start(void);

bool HU_Responder(event_t * ev);

void HU_StartMessage(const char *msg);

void HU_Ticker(void);
void HU_Drawer(void);
void HU_QueueChatChar(char c);
char HU_DequeueChatChar(void);
void HU_Erase(void);

#endif // __HU_STUFF_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
