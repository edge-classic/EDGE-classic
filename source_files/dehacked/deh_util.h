//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#ifndef __DEH_UTIL_HDR__
#define __DEH_UTIL_HDR__

#include <stdio.h>

namespace dehacked
{

// string utilities
int         StrCaseCmp(const char *A, const char *B);
int         StrCaseCmpPartial(const char *A, const char *B);
void        StrMaxCopy(char *dest, const char *src, int max);
const char *StrUpper(const char *name);
const char *StrSanitize(const char *name);
char       *StringNew(int length);
char       *StringDup(const char *orig);

} // namespace dehacked

#endif /* __DEH_UTIL_HDR__ */
