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

#include "str_util.h"

#include <stdarg.h>

#include "epi.h"
#include "superfasthash.h"

#define UTF8PROC_STATIC
#include "utf8proc.h"

namespace epi
{

#ifdef _WIN32
std::wstring UTF8ToWString(std::string_view instring)
{
    size_t                  utf8pos = 0;
    const utf8proc_uint8_t *utf8ptr = (const utf8proc_uint8_t *)instring.data();
    size_t                  utf8len = instring.size();
    std::wstring            outstring;
    utf8proc_int32_t        u32c;
    while (utf8pos < utf8len)
    {
        u32c = 0;
        size_t res =
            utf8proc_iterate(utf8ptr + utf8pos, utf8len - utf8pos, &u32c);
        if (res < 0)
            FatalError("Failed to convert %s to a wide string!\n",
                    std::string(instring).c_str());
        else
            utf8pos += res;
        if (u32c < 0x10000)
            outstring.push_back((wchar_t)u32c);
        else  // Make into surrogate pair if needed
        {
            u32c -= 0x10000;
            outstring.push_back((wchar_t)(u32c >> 10) + 0xD800);
            outstring.push_back((wchar_t)(u32c & 0x3FF) + 0xDC00);
        }
    }
    return outstring;
}
std::string WStringToUTF8(std::wstring_view instring)
{
    std::string      outstring;
    size_t           inpos = 0;
    size_t           inlen = instring.size();
    const wchar_t   *inptr = instring.data();
    utf8proc_uint8_t u8c[4];
    while (inpos < inlen)
    {
        utf8proc_int32_t u32c = 0;
        if ((*(inptr + inpos) & 0xD800) == 0xD800)  // High surrogate
        {
            if (inpos + 1 < inlen &&
                (*(inptr + inpos + 1) & 0xDC00) == 0xDC00)  // Low surrogate
            {
                u32c = ((*(inptr + inpos) - 0xD800) * 0x400) +
                       (*(inptr + inpos + 1) - 0xDC00) + 0x10000;
                inpos += 2;
            }
            else  // Assume an unpaired surrogate is malformed
            {
                // print what was safely converted if present
                if (!outstring.empty())
                    FatalError("Failure to convert %s from a wide string!\n",
                            outstring.c_str());
                else
                    FatalError("Wide string to UTF-8 conversion failure!\n");
            }
        }
        else
        {
            u32c = *(inptr + inpos);
            inpos++;
        }
        memset(u8c, 0, 4);
        if (utf8proc_encode_char(u32c, u8c) == 0)
        {
            // print what was safely converted if present
            if (!outstring.empty())
                FatalError("Failure to convert %s from a wide string!\n",
                        outstring.c_str());
            else
                FatalError("Wide string to UTF-8 conversion failure!\n");
        }
        if (u8c[0]) outstring.push_back(u8c[0]);
        if (u8c[1]) outstring.push_back(u8c[1]);
        if (u8c[2]) outstring.push_back(u8c[2]);
        if (u8c[3]) outstring.push_back(u8c[3]);
    }
    return outstring;
}
#endif

void StringLowerASCII(std::string &s)
{
    for (char &ch : s)
    {
        if (ch > '@' && ch < '[') ch ^= 0x20;
    }
}
void StringUpperASCII(std::string &s)
{
    for (char &ch : s)
    {
        if (ch > '`' && ch < '{') ch ^= 0x20;
    }
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

std::vector<std::string> SeparatedStringVector(std::string_view str,
                                               char             separator)
{
    std::vector<std::string>    vec;
    std::string_view::size_type oldpos = 0;
    std::string_view::size_type pos    = 0;
    while (pos != std::string_view::npos)
    {
        pos = str.find(separator, oldpos);
        std::string sub_string(str.substr(
            oldpos, (pos == std::string::npos ? str.size() : pos) - oldpos));
        if (!sub_string.empty()) vec.push_back(sub_string);
        if (pos != std::string_view::npos) oldpos = pos + 1;
    }
    return vec;
}

uint32_t StringHash32(std::string_view str_to_hash)
{
    if (str_to_hash.empty()) { return 0; }

    return SFH_MakeKey(str_to_hash.data(), str_to_hash.length());
}

// Copies up to max characters of src into dest, and then applies a
// terminating zero (so dest must hold at least max+1 characters).
// The terminating zero is always applied (there is no reason not to)
void CStringCopyMax(char *destination, const char *source, int max)
{
    for (; *source && max > 0; max--) { *destination++ = *source++; }

    *destination = 0;
}

char *CStringNew(int length)
{
    // length does not include the trailing NUL.

    char *s = (char *)calloc(length + 1, 1);

    if (!s) FatalError("Out of memory (%d bytes for string)\n", length);

    return s;
}

char *CStringDuplicate(const char *original, int limit)
{
    if (!original) return nullptr;

    if (limit < 0)
    {
        char *s = strdup(original);

        if (!s) FatalError("Out of memory (copy string)\n");

        return s;
    }

    char *s = CStringNew(limit + 1);
    strncpy(s, original, limit);
    s[limit] = 0;

    return s;
}

char *CStringUpper(const char *name)
{
    char *copy = CStringDuplicate(name);

    for (char *p = copy; *p; p++) *p = epi::ToUpperASCII(*p);

    return copy;
}

void CStringFree(const char *string)
{
    if (string) { free((void *)string); }
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
