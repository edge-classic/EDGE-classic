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

#include "epi.h"
#include "str_util.h"

#include "superfasthash.h"

namespace epi
{

void str_lower(std::string& s)
{
	for (size_t i = 0 ; i < s.size() ; i++)
		s[i] = std::tolower(s[i]);
}
void str_upper(std::string& s)
{
	for (size_t i = 0 ; i < s.size() ; i++)
		s[i] = std::toupper(s[i]);
}

void str_lower(std::u32string& s)
{
	std::string temp = epi::to_u8string(s);
	epi::str_lower(temp);
	s = epi::to_u32string(temp);
}
void str_upper(std::u32string& s)
{
	std::string temp = epi::to_u8string(s);
	epi::str_upper(temp);
	s = epi::to_u32string(temp);
}

void STR_TextureNameFromFilename(std::string& buf, const std::string& stem)
{
	size_t pos = 0;

	buf.clear();

	while (pos < stem.size())
	{
		int ch = (unsigned char) stem[pos++];

		// remap caret --> backslash
		if (ch == '^')
			ch = '\\';

		buf.push_back((char) ch);
	}

	epi::str_upper(buf);
}

std::string STR_Format(const char *fmt, ...)
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

std::vector<std::string> STR_SepStringVector(std::string str, char separator)
{
	std::vector<std::string> vec;
	std::string::size_type oldpos = 0;
	std::string::size_type pos = 0;
	while (pos != std::string::npos) {
		pos = str.find(separator, oldpos);
		std::string sub_string = str.substr(oldpos, (pos == std::string::npos ? str.size() : pos) - oldpos);
		if (!sub_string.empty())
			vec.push_back(sub_string);
		if (pos != std::string::npos)
			oldpos = pos + 1;
	}
	return vec;
}

#ifdef _WIN32
std::vector<std::u32string> STR_SepStringVector(std::u32string str, char32_t separator)
{
	std::vector<std::u32string> vec;
	std::u32string::size_type oldpos = 0;
	std::u32string::size_type pos = 0;
	while (pos != std::u32string::npos) {
		pos = str.find(separator, oldpos);
		std::u32string sub_string = str.substr(oldpos, (pos == std::string::npos ? str.size() : pos) - oldpos);
		if (!sub_string.empty())
			vec.push_back(sub_string);
		if (pos != std::string::npos)
			oldpos = pos + 1;
	}
	return vec;
}
#endif

uint32_t STR_Hash32(std::string str_to_hash)
{
	if (!str_to_hash.length())
	{
		return 0;		
	}

	return SFH_MakeKey(str_to_hash.c_str(), str_to_hash.length());
}

// The following string conversion classes/code are adapted from public domain
// code by Andrew Choi originally found at https://web.archive.org/web/20151209032329/http://members.shaw.ca/akochoi/articles/unicode-processing-c++0x/

template<>
int storageMultiplier<UTF8, UTF32>() { return 4; }

template<>
int storageMultiplier<UTF8, UTF16>() { return 3; }

template<>
int storageMultiplier<UTF16, UTF8>() { return 1; }

template<>
int storageMultiplier<UTF32, UTF8>() { return 1; }

std::string to_u8string(const std::string& s)
{
  return s;
}

std::string to_u8string(const std::u16string& s)
{
  static str_converter<UTF8, UTF16, UTF16> converter;

  return converter.out(s);
}

std::string to_u8string(const std::u32string& s)
{
  static str_converter<UTF8, UTF32, UTF32> converter;

  return converter.out(s);
}

std::u16string to_u16string(const std::string& s)
{
  static str_converter<UTF16, UTF8, UTF16> converter;

  return converter.in(s);
}

std::u32string to_u32string(const std::string& s)
{
  static str_converter<UTF32, UTF8, UTF32> converter;

  return converter.in(s);
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
