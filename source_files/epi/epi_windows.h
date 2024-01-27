//----------------------------------------------------------------------------
//  EDGE Platform Interface Header - Windows
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024 The EDGE Team.
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

#ifndef __EPI_WIN_H__
#define __EPI_WIN_H__

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#ifndef _WINDOWS
#define _WINDOWS
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef WIN32
#define WIN32
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define UNICODE
#endif
#include <windows.h>
#endif

#endif