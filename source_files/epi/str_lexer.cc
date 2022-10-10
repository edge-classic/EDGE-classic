//----------------------------------------------------------------------------
//  EPI Lexer (tokenizer)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  Andrew Apted
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
#include "str_lexer.h"

#include <cstdlib>
#include <cctype>

namespace epi
{

token_kind_e lexer_c::Next(std::string& s)
{
	s.clear();

	SkipToNext();

	if (pos >= data.size())
		return TOK_EOF;

	// TODO : Next
	return TOK_EOF;
}


bool lexer_c::Match(const char *s)
{
	SkipToNext();

	if (pos >= data.size())
		return false;

	// TODO : Match
	return false;
}


int lexer_c::LastLine()
{
	return line;
}


int lexer_c::ToInt(const std::string& s)
{
	// strtol handles all the integer sequences of the UDMF spec
	return (int)std::strtol(s.c_str(), NULL, 0);
}


double lexer_c::ToDouble(const std::string& s)
{
	// strtod handles all the floating-point sequences of the UDMF spec
	return std::strtod(s.c_str(), NULL);
}


void lexer_c::SkipToNext()
{
	while (pos < data.size())
	{
		unsigned char ch = (unsigned char) data[pos++];

		// bump line number at end of a line
		if (ch == '\n')
			line += 1;

		// whitespace?
		if (ch <= ' ')
			continue;

		if (ch == '/' && pos < data.size())
		{
			// single line comment?
			if (data[pos] == '/')
			{
				pos++;

				while (pos < data.size() && data[pos] != '\n')
					pos++;

				continue;
			}

			// multi-line comment?
			if (data[pos] == '*')
			{
				pos++;

				while (pos < data.size())
				{
					if (pos+1 < data.size() && data[pos] == '*' && data[pos+1] == '/')
						break;

					if (data[pos] == '\n')
						line += 1;

					pos++;
				}

				continue;
			}
		}

		// reached a token!
		return;
	}
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
