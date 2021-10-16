//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Chunks)
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
//
// -AJA- 2000/07/13: Wrote this file.
//

#ifndef __SV_CHUNK_H__
#define __SV_CHUNK_H__

#include "i_defs.h"
#include "p_local.h"

#define DATA_END_MARKER  "ENDE"

void SV_ChunkInit(void);
void SV_ChunkShutdown(void);

int SV_GetError(void);

//
//  READING
//

extern int savegame_version;

bool SV_OpenReadFile(const char *filename);
bool SV_CloseReadFile(void);
bool SV_VerifyHeader(int *version);
bool SV_VerifyContents(void);

bool SV_PushReadChunk(const char *id);
bool SV_PopReadChunk(void);
int SV_RemainingChunkSize(void);
bool SV_SkipReadChunk(const char *id);

unsigned char  SV_GetByte(void);
unsigned short SV_GetShort(void);
unsigned int   SV_GetInt(void);

angle_t SV_GetAngle(void);
float SV_GetFloat(void);

const char *SV_GetString(void);
const char *SV_DupString(const char *old);
void SV_FreeString(const char *str);

bool SV_GetMarker(char id[5]);

//
//  WRITING
//

bool SV_OpenWriteFile(const char *filename, int version);
bool SV_CloseWriteFile(void);

bool SV_PushWriteChunk(const char *id);
bool SV_PopWriteChunk(void);

void SV_PutByte(unsigned char value);
void SV_PutShort(unsigned short value);
void SV_PutInt(unsigned int value);

void SV_PutAngle(angle_t value);
void SV_PutFloat(float value);

void SV_PutString(const char *str);
void SV_PutMarker(const char *id);

#endif /* __SV_CHUNK_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
