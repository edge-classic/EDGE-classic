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

#ifndef __DEH_SYSTEM_HDR__
#define __DEH_SYSTEM_HDR__

#include "epi.h"

#define F_FIXED(n) ((float)(n) / 65536.0)

namespace Deh_Edge
{

extern bool quiet_mode;
extern bool all_mode;

void System_Startup(void);

// error message storage and retrieval
void        SetErrorMsg(const char *str, ...) GCCATTR((format(printf, 1, 2)));
const char *GetErrorMsg(void);

} // namespace Deh_Edge

#endif /* __DEH_SYSTEM_HDR__ */
