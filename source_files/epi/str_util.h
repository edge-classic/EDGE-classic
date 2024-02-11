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
#ifdef _WIN32
// Technically these are to and from UTF-16, but since these are only for
// Windows "wide" APIs I think we'll be ok - Dasho
std::string  WStringToUTF8(std::wstring_view instring);
std::wstring UTF8ToWString(std::string_view instring);
#endif

inline bool IsUpperASCII(int character)
{
    return (character > '@' && character < '[');
}
inline bool IsLowerASCII(int character)
{
    return (character > '`' && character < '{');
}
inline bool IsAlphaASCII(int character)
{
    return ((character > '@' && character < '[') ||
            (character > '`' && character < '{'));
}
inline bool IsAlphanumericASCII(int character)
{
    return ((character > '@' && character < '[') ||
            (character > '`' && character < '{') ||
            (character > '/' && character < ':'));
}
inline bool IsDigitASCII(int character)
{
    return (character > '/' && character < ':');
}
inline bool IsXDigitASCII(int character)
{
    return ((character > '@' && character < 'G') ||
            (character > '`' && character < 'g') ||
            (character > '/' && character < ':'));
}
inline bool IsPrintASCII(int character)
{
    return (character > 0x1F && character < 0x7F);
}
inline bool IsSpaceASCII(int character)
{
    return ((character > 0x8 && character < 0xE) || character == 0x20);
}
inline int ToLowerASCII(int character)
{
    if (character > '@' && character < '[')
        return character ^ 0x20;
    else
        return character;
}
inline int ToUpperASCII(int character)
{
    if (character > '`' && character < '{')
        return character ^ 0x20;
    else
        return character;
}

void StringLowerASCII(std::string &s);
void StringUpperASCII(std::string &s);

// These are for AJBSP/Dehacked, just de-duplicated
// CStringCopyMax also replaces the old Z_StrNCpy macro
void  CStringCopyMax(char *destination, const char *source, int max);
char *CStringNew(int length);
char *CStringDuplicate(const char *original, int limit = -1);
char *CStringUpper(const char *name);
void  CStringFree(const char *string);

void TextureNameFromFilename(std::string &buf, std::string_view stem);
#ifdef __GNUC__
std::string StringFormat(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#else
std::string StringFormat(const char *fmt, ...);
#endif
std::vector<std::string> SeparatedStringVector(std::string_view str,
                                               char             separator);
uint32_t                 StringHash32(std::string_view str_to_hash);
}  // namespace epi

#endif /* __EPI_STR_UTIL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
