//----------------------------------------------------------------------------
//  EPI String Utilities
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

#ifndef __EPI_STR_UTIL_H__
#define __EPI_STR_UTIL_H__

#include <string>
#include <vector>

namespace epi
{

void str_lower(std::string& s);
void str_upper(std::string& s);

std::string STR_Format(const char *fmt, ...) GCCATTR((format(printf, 1, 2)));

std::vector<std::string> STR_SepStringVector(std::string str, char separator);

} // namespace epi

#endif /* __EPI_STR_UTIL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
