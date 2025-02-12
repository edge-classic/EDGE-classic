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

#include "epi_str_util.h"

#include <stdarg.h>

#include "epi.h"
#include "stb_sprintf.h"

namespace epi
{

#ifdef _WIN32
static constexpr uint32_t kBadUnicodeValue = 0xFFFFFFFF;

// The following two functions are adapted from PHYSFS' internal Unicode stuff,
// but tweaked to account for string_views which are not null terminated

static int GetNextUTF8Codepoint(const char *_str, size_t length, uint32_t *u32c)
{
    const char *str     = _str;
    uint32_t    retval  = 0;
    uint32_t    octet   = 0;
    uint32_t    octet2  = 0;
    uint32_t    octet3  = 0;
    uint32_t    octet4  = 0;
    int         advance = 0;

    if (length == 0)
    {
        *u32c = 0;
        return 0;
    }
    else
        octet = (uint32_t)((uint8_t)*str);

    if (octet == 0) /* null terminator, end of string. */
    {
        *u32c = 0;
        return 1;
    }
    else if (octet < 128) /* one octet char: 0 to 127 */
    {
        *u32c = octet;
        return 1;
    } /* else if */
    else if ((octet > 127) && (octet < 192)) /* bad (starts with 10xxxxxx). */
    {
        /*
         * Apparently each of these is supposed to be flagged as a bogus
         *  char, instead of just resyncing to the next valid codepoint.
         */
        *u32c = kBadUnicodeValue;
        return 1;
    } /* else if */
    else if (octet < 224) /* two octets */
    {
        advance = 1;
        octet -= (128 + 64);
        if (length > 1)
        {
            octet2 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet2 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        retval = ((octet << 6) | (octet2 - 128));
        if ((retval >= 0x80) && (retval <= 0x7FF))
        {
            *u32c = retval;
            return advance;
        }
        else
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }
    } /* else if */
    else if (octet < 240) /* three octets */
    {
        advance = 1;
        octet -= (128 + 64 + 32);
        if (length > 1)
        {
            octet2 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet2 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        if (length > 2)
        {
            octet3 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet3 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        retval = (((octet << 12)) | ((octet2 - 128) << 6) | ((octet3 - 128)));

        /* There are seven "UTF-16 surrogates" that are illegal in UTF-8. */
        switch (retval)
        {
        case 0xD800:
        case 0xDB7F:
        case 0xDB80:
        case 0xDBFF:
        case 0xDC00:
        case 0xDF80:
        case 0xDFFF: {
            *u32c = kBadUnicodeValue;
            return advance;
        }
        } /* switch */

        /* 0xFFFE and 0xFFFF are illegal, too, so we check them at the edge. */
        if ((retval >= 0x800) && (retval <= 0xFFFD))
        {
            *u32c = retval;
            return advance;
        }
        else
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }
    } /* else if */
    else if (octet < 248) /* four octets */
    {
        advance = 1;
        octet -= (128 + 64 + 32 + 16);
        if (length > 1)
        {
            octet2 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet2 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        if (length > 2)
        {
            octet3 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet3 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        if (length > 3)
        {
            octet4 = (uint32_t)((uint8_t)*(++str));
            advance++;
        }
        if ((octet4 & (128 + 64)) != 128) /* Format isn't 10xxxxxx? */
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }

        retval = (((octet << 18)) | ((octet2 - 128) << 12) | ((octet3 - 128) << 6) | ((octet4 - 128)));
        if ((retval >= 0x10000) && (retval <= 0x10FFFF))
        {
            *u32c = retval;
            return advance;
        }
        else
        {
            *u32c = kBadUnicodeValue;
            return advance;
        }
    } /* else if */

    // Shouldn't get here, but ok
    *u32c = kBadUnicodeValue;
    return 0;
}

static bool CodepointToUTF8(uint32_t cp, char *_dst, uint64_t *_len)
{
    char    *dst = _dst;
    uint64_t len = *_len;

    if (len == 0)
        return true;

    if (cp > 0x10FFFF)
        cp = kBadUnicodeValue;
    else if ((cp == 0xFFFE) || (cp == 0xFFFF)) /* illegal values. */
        cp = kBadUnicodeValue;
    else
    {
        /* There are seven "UTF-16 surrogates" that are illegal in UTF-8. */
        switch (cp)
        {
        case 0xD800:
        case 0xDB7F:
        case 0xDB80:
        case 0xDBFF:
        case 0xDC00:
        case 0xDF80:
        case 0xDFFF:
            cp = kBadUnicodeValue;
        } /* switch */
    } /* else */

    /* Do the encoding... */
    if (cp < 0x80)
    {
        *(dst++) = (char)cp;
        len--;
    } /* if */

    else if (cp < 0x800)
    {
        if (len < 2)
            len = 0;
        else
        {
            *(dst++) = (char)((cp >> 6) | 128 | 64);
            *(dst++) = (char)(cp & 0x3F) | 128;
            len -= 2;
        } /* else */
    } /* else if */

    else if (cp < 0x10000)
    {
        if (len < 3)
            len = 0;
        else
        {
            *(dst++) = (char)((cp >> 12) | 128 | 64 | 32);
            *(dst++) = (char)((cp >> 6) & 0x3F) | 128;
            *(dst++) = (char)(cp & 0x3F) | 128;
            len -= 3;
        } /* else */
    } /* else if */

    else
    {
        if (len < 4)
            len = 0;
        else
        {
            *(dst++) = (char)((cp >> 18) | 128 | 64 | 32 | 16);
            *(dst++) = (char)((cp >> 12) & 0x3F) | 128;
            *(dst++) = (char)((cp >> 6) & 0x3F) | 128;
            *(dst++) = (char)(cp & 0x3F) | 128;
            len -= 4;
        } /* else if */
    } /* else */

    _dst  = dst;
    *_len = len;
    if (cp == kBadUnicodeValue)
        return false;
    else
        return true;
}

std::wstring UTF8ToWString(std::string_view instring)
{
    size_t       utf8pos = 0;
    const char  *utf8ptr = (const char *)instring.data();
    size_t       utf8len = instring.size();
    std::wstring outstring;
    while (utf8pos < utf8len)
    {
        uint32_t u32c = 0;
        int      res  = GetNextUTF8Codepoint(utf8ptr + utf8pos, utf8len - utf8pos, &u32c);
        if (u32c == kBadUnicodeValue)
            FatalError("Failed to convert %s to a wide string!\n", std::string(instring).c_str());
        else
            utf8pos += res;
        if (u32c < 0x10000)
            outstring.push_back((wchar_t)u32c);
        else // Make into surrogate pair if needed
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
    std::string    outstring;
    size_t         inpos = 0;
    size_t         inlen = instring.size();
    const wchar_t *inptr = instring.data();
    uint8_t        u8c[4];
    while (inpos < inlen)
    {
        uint32_t u32c = 0;
        uint64_t len  = 0;
        if ((*(inptr + inpos) & 0xD800) == 0xD800)                              // High surrogate
        {
            if (inpos + 1 < inlen && (*(inptr + inpos + 1) & 0xDC00) == 0xDC00) // Low surrogate
            {
                u32c = ((*(inptr + inpos) - 0xD800) * 0x400) + (*(inptr + inpos + 1) - 0xDC00) + 0x10000;
                inpos += 2;
                len += 4;
            }
            else // Assume an unpaired surrogate is malformed
            {
                // print what was safely converted if present
                if (!outstring.empty())
                    FatalError("Failure to convert %s from a wide string!\n", outstring.c_str());
                else
                    FatalError("Wide string to UTF-8 conversion failure!\n");
            }
        }
        else
        {
            u32c = *(inptr + inpos);
            inpos++;
            len += 2;
        }
        EPI_CLEAR_MEMORY(u8c, uint8_t, 4);
        if (!CodepointToUTF8(u32c, (char *)u8c, &len))
        {
            // print what was safely converted if present
            if (!outstring.empty())
                FatalError("Failure to convert %s from a wide string!\n", outstring.c_str());
            else
                FatalError("Wide string to UTF-8 conversion failure!\n");
        }
        if (u8c[0])
            outstring.push_back(u8c[0]);
        if (u8c[1])
            outstring.push_back(u8c[1]);
        if (u8c[2])
            outstring.push_back(u8c[2]);
        if (u8c[3])
            outstring.push_back(u8c[3]);
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
        int out_len = stbsp_vsnprintf(buf, buf_size, fmt, args);
        va_end(args);

        // old versions of stbsp_vsnprintf() simply return -1 when
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
    std::vector<std::string>    vec;
    std::string_view::size_type oldpos = 0;
    std::string_view::size_type pos    = 0;
    while (pos != std::string_view::npos)
    {
        pos = str.find(separator, oldpos);
        std::string sub_string(str.substr(oldpos, (pos == std::string::npos ? str.size() : pos) - oldpos));
        if (!sub_string.empty())
            vec.push_back(sub_string);
        if (pos != std::string_view::npos)
            oldpos = pos + 1;
    }
    return vec;
}

// Copies up to max characters of src into dest, and then applies a
// terminating zero (so dest must hold at least max+1 characters).
// The terminating zero is always applied (there is no reason not to)
void CStringCopyMax(char *destination, const char *source, int max)
{
    for (; *source && max > 0; max--)
    {
        *destination++ = *source++;
    }

    *destination = 0;
}

char *CStringNew(int length)
{
    // length does not include the trailing NUL.

    char *s = (char *)calloc(length + 1, 1);

    if (!s)
        FatalError("Out of memory (%d bytes for string)\n", length);

    return s;
}

char *CStringDuplicate(const char *original, int limit)
{
    if (!original)
        return nullptr;

    if (limit < 0)
    {
        char *s = strdup(original);

        if (!s)
            FatalError("Out of memory (copy string)\n");

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

    for (char *p = copy; *p; p++)
        *p = epi::ToUpperASCII(*p);

    return copy;
}

void CStringFree(const char *string)
{
    if (string)
    {
        free((void *)string);
    }
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
