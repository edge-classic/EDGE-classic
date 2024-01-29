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

#include "epi.h"
#include "str_util.h"

#include "superfasthash.h"

#include "grapheme.h"

namespace epi
{

#ifdef _WIN32
std::wstring UTF8ToWString(std::string_view instring)
{
    size_t utf8pos = 0;
    const char *utf8ptr = instring.data();
    size_t utf8len = instring.size();
	std::wstring outstring;
    uint32_t u32c;
    while (utf8pos < utf8len)
    {
        u32c = 0;
        utf8pos += grapheme_decode_utf8(utf8ptr+utf8pos, utf8len, &u32c);
        if (u32c < 0x10000)
            outstring.push_back((wchar_t)u32c);
        else
        {
            u32c -= 0x10000;
            outstring.push_back((wchar_t)(u32c >> 10) + 0xD800);
            outstring.push_back((wchar_t)(u32c & 0x3FF) + 0xDC00);
        }
    }
    return outstring;
}
std::string WStringToUTF8(const wchar_t *instring)
{
    std::string outstring;
    char u8c[4];
    while (*instring)
    {
        memset(u8c, 0, 4);
        grapheme_encode_utf8((uint_least32_t)(*instring), &u8c[0], 4);
        if (u8c[0]) outstring.push_back(u8c[0]);
        if (u8c[1]) outstring.push_back(u8c[1]);
        if (u8c[2]) outstring.push_back(u8c[2]);
        if (u8c[3]) outstring.push_back(u8c[3]);
        instring++;
    }
    return outstring;
}
#endif

void StringLowerASCII(std::string &s)
{
    for (char &ch : s)
    {
        if (ch > '@' && ch < '[')
            ch ^= 0x20;
    }
}
void StringUpperASCII(std::string &s)
{
    for (char &ch : s)
    {
        if (ch > '`' && ch < '{')
            ch ^= 0x20;
    }
}

void StringLowerUTF8(std::string &s)
{
    const char *sc = s.c_str();
    size_t ss = s.size()+1;
    size_t needed = grapheme_to_lowercase_utf8(sc, ss, nullptr, 0);
    char *c = (char *)calloc(needed, sizeof(char));
    grapheme_to_lowercase_utf8(sc, ss, c, needed);
    s = c;
}
void StringUpperUTF8(std::string &s)
{
    const char *sc = s.c_str();
    size_t ss = s.size()+1;
    size_t needed = grapheme_to_uppercase_utf8(sc, ss, nullptr, 0);
    char *c = (char *)calloc(needed, sizeof(char));
    grapheme_to_uppercase_utf8(sc, ss, c, needed);
    s = c;
}

void TextureNameFromFilename(std::string &buf, std::string_view stem)
{
    size_t pos = 0;

    buf.clear();

    while (pos < stem.size())
    {
        int ch = (unsigned char)stem[pos++];

        // remap caret --> backslash
        if (ch == '^')
            ch = '\\';
        else if (ch > '`' && ch < '{')
            ch ^= 0x20;

        buf.push_back((char)ch);
    }
}

std::string StringFormat(const char *fmt, ...)
{
    /* Algorithm: keep doubling the allocated buffer size
     * until the output fits. Based on code by Darren Salt.
     */
    int buf_size = 128;

    for (;;)
    {
        char *buf = new char[buf_size];

        va_list args;

        va_start(args, fmt);
        int out_len = vsnprintf(buf, buf_size, fmt, args);
        va_end(args);

        // old versions of vsnprintf() simply return -1 when
        // the output doesn't fit.
        if (out_len >= 0 && out_len < buf_size)
        {
            std::string result(buf);
            delete[] buf;

            return result;
        }

        delete[] buf;

        buf_size *= 2;
    }
}

std::vector<std::string> SeparatedStringVector(std::string_view str, char separator)
{
    std::vector<std::string> vec;
    std::string_view::size_type   oldpos = 0;
    std::string_view::size_type   pos    = 0;
    while (pos != std::string_view::npos)
    {
        pos                    = str.find(separator, oldpos);
        std::string sub_string(str.substr(oldpos, (pos == std::string::npos ? str.size() : pos) - oldpos));
        if (!sub_string.empty())
            vec.push_back(sub_string);
        if (pos != std::string_view::npos)
            oldpos = pos + 1;
    }
    return vec;
}

uint32_t StringHash32(std::string_view str_to_hash)
{
    if (str_to_hash.empty())
    {
        return 0;
    }

    return SFH_MakeKey(str_to_hash.data(), str_to_hash.length());
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
