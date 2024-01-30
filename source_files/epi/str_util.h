//----------------------------------------------------------------------------
//  EPI String Utilities
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#ifndef __EPI_STR_UTIL_H__
#define __EPI_STR_UTIL_H__

#include <string>
#include <vector>

namespace epi
{

void StringLowerASCII(std::string &s);
void StringUpperASCII(std::string &s);

void TextureNameFromFilename(std::string &buf, std::string_view stem);

std::string StringFormat(const char *fmt, ...) GCCATTR((format(printf, 1, 2)));

std::vector<std::string> SeparatedStringVector(std::string_view str, char separator);

uint32_t StringHash32(std::string_view str_to_hash);

#ifdef _WIN32
// Technically these are to and from UTF-16, but since these are only for 
// Windows "wide" APIs I think we'll be ok - Dasho
std::string WStringToUTF8(std::wstring_view instring);
std::wstring UTF8ToWString(std::string_view instring);
#endif

} // namespace epi

#endif /* __EPI_STR_UTIL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
