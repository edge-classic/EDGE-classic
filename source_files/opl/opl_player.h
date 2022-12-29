//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C)      2022 Andrew Apted
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef OPL_PLAYER_H
#define OPL_PLAYER_H

#include <inttypes.h>

bool OPLAY_Init(int freq, bool stereo, bool opl3_wanted);

bool OPLAY_StartSong(void);
void OPLAY_FinishSong(void);

void OPLAY_NotesOff(void);
int  OPLAY_Stream(int16_t *buf, int samples, bool stereo);
void OPLAY_WriteReg(uint8_t reg, uint8_t value);

// Event Handling
extern void OPLAY_KeyOff(uint8_t channel, uint8_t note);
void OPLAY_KeyOn(uint8_t channel, uint8_t note, uint8_t volume);
void OPLAY_ProgramChange(uint8_t channel, uint8_t patch);
void OPLAY_ControllerChange(uint8_t channel, uint8_t type, uint8_t value);
void OPLAY_PitchBend(uint8_t channel, uint8_t msb);


#endif /* #ifndef OPL_PLAYER_H */
