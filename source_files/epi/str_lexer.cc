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

	unsigned char ch = (unsigned char) data[pos];

	if (ch == '"')
		return ParseString(s);

	if (ch == '-' || ch == '+' || std::isdigit(ch))
		return ParseNumber(s);

	if (std::isalpha(ch) || ch == '_' || ch >= 128)
		return ParseIdentifier(s);

	// anything else is a single-character symbol
	s.push_back(data[pos++]);

	return TOK_Symbol;
}


bool lexer_c::Match(const char *s)
{
	SYS_ASSERT(s);
	SYS_ASSERT(s[0]);

	bool is_keyword = std::isalnum(s[0]);

	SkipToNext();

	size_t ofs = 0;

	for (; *s != 0 ; s++, ofs++)
	{
		if (pos + ofs >= data.size())
			return false;

		unsigned char A = (unsigned char) data[pos + ofs];
		unsigned char B = (unsigned char) s[0];

		// don't change a char when high-bit is set (for UTF-8)
		if (A < 128) A = std::tolower(A);
		if (B < 128) B = std::tolower(B);

		if (A != B)
			return false;
	}

	pos += ofs;

	// for a keyword, require a non-alphanumeric char after it.
	if (is_keyword && pos < data.size())
	{
		unsigned char ch = (unsigned char) data[pos];

		if (std::isalnum(ch) || ch >= 128)
			return false;
	}

	return true;
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

//----------------------------------------------------------------------------

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


token_kind_e lexer_c::ParseIdentifier(std::string& s)
{
	// TODO ParseIdentifier
	return TOK_ERROR;
}


token_kind_e lexer_c::ParseNumber(std::string& s)
{
	if (data[pos] == '-' || data[pos] == '+')
	{
		// no digits after the sign?
		if (pos+1 >= data.size() || ! std::isdigit(data[pos+1]))
		{
			s.push_back(data[pos++]);
			return TOK_Symbol;
		}
	}

	for (;;)
	{
		s.push_back(data[pos++]);

		if (pos >= data.size())
			break;

		unsigned char ch = (unsigned char) data[pos];

		// this is fairly lax, but adequate for our purposes
		if (std::isalnum(ch) || ch == '+' || ch == '-' || ch == '.')
			continue;

		break;
	}

	return TOK_Number;
}


token_kind_e lexer_c::ParseString(std::string& s)
{
	// TODO ParseString
	return TOK_ERROR;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
