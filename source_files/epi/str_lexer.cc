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

namespace epi
{

token_kind_e lexer_c::Next(std::string& s)
{
	// TODO : Next
	return TOK_EOF;
}


bool lexer_c::Match(const char *s)
{
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

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
