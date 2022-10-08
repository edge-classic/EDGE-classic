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

#include <locale>

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

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
