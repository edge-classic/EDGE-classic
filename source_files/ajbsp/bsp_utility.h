//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  Copyright (C) 2001-2023 Andrew Apted
//  Copyright (C) 1997-2003 André Majorel et al
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
//------------------------------------------------------------------------

#ifndef __AJBSP_UTILITY_H__
#define __AJBSP_UTILITY_H__

namespace ajbsp
{

// memory allocation, guaranteed to not return NULL.
void *UtilCalloc(int size);
void *UtilRealloc(void *old, int size);
void  UtilFree(void *data);

// math stuff
int    RoundPOW2(int x);
double ComputeAngle(double dx, double dy);

// string utilities
int StringCaseCmp(const char *s1, const char *s2);
int StringCaseCmpMax(const char *s1, const char *s2, size_t len);

char *StringNew(int length);
char *StringDup(const char *orig, int limit = -1);
char *StringUpper(const char *name);
char *StringPrintf(const char *str, ...);
void  StringFree(const char *str);

// checksum functions
void Adler32_Begin(u32_t *crc);
void Adler32_AddBlock(u32_t *crc, const u8_t *data, int length);

} // namespace ajbsp

#endif /* __AJBSP_UTILITY_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
