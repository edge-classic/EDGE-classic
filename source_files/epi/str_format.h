//----------------------------------------------------------------------------
//  EPI String Formatting
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2008  The EDGE Team.
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

#ifndef __EPI_STR_FORMAT_H__
#define __EPI_STR_FORMAT_H__

#include <string>

namespace epi
{

std::string STR_Format(const char *fmt, ...);

char * STR_FormatCStr(const char *fmt, ...);

} // namespace epi

#endif /* __EPI_STR_FORMAT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
