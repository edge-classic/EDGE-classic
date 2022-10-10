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


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
