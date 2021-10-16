//----------------------------------------------------------------------------
//  EDGE LINUX System Specific header
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

#ifndef __UNIX_SYSTEM_INTERNAL_H__
#define __UNIX_SYSTEM_INTERNAL_H__

#include "i_defs.h"

#include <sys/ioctl.h>

#ifndef MACOSX
#include <linux/cdrom.h>
#endif

#include "i_local.h"  // FIXME: remove


#endif // __UNIX_SYSTEM_INTERNAL_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
