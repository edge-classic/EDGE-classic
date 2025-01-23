//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Chunks)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#pragma once

#include "p_local.h"

constexpr const char *kDataEndMarker = "ENDE";

int SaveGetError(void);

//
//  READING
//

bool SaveFileOpenRead(const std::string &filename);
bool SaveFileCloseRead(void);
bool SaveFileVerifyHeader(int *version);
bool SaveFileVerifyContents(void);

bool SavePushReadChunk(const char *id);
bool SavePopReadChunk(void);
int  SaveRemainingChunkSize(void);
bool SaveSkipReadChunk(const char *id);

uint8_t  SaveChunkGetByte(void);
uint16_t SaveChunkGetShort(void);
uint32_t SaveChunkGetInteger(void);

BAMAngle SaveChunkGetAngle(void);
float    SaveChunkGetFloat(void);

const char *SaveChunkGetString(void);
const char *SaveChunkCopyString(const char *old);
void        SaveChunkFreeString(const char *str);

bool SaveChunkGetMarker(char id[5]);

//
//  WRITING
//

bool SaveFileOpenWrite(const std::string &filename, int version);
bool SaveFileCloseWrite(void);

bool SavePushWriteChunk(const char *id);
bool SavePopWriteChunk(void);

void SaveChunkPutByte(uint8_t value);
void SaveChunkPutShort(uint16_t value);
void SaveChunkPutInteger(uint32_t value);

void SaveChunkPutAngle(BAMAngle value);
void SaveChunkPutFloat(float value);

void SaveChunkPutString(const char *str);
void SaveChunkPutMarker(const char *id);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
