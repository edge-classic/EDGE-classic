//------------------------------------------------------------------------
//  SYSTEM : Bridging code
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

#pragma once

namespace dehacked
{

extern bool quiet_mode;

void System_Startup(void);

// error message storage and retrieval
#ifdef __GNUC__
void SetErrorMsg(const char *str, ...) __attribute__((format(printf, 1, 2)));
#else
void SetErrorMsg(const char *str, ...);
#endif
const char *GetErrorMsg(void);

} // namespace dehacked