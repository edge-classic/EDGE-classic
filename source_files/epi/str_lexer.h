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

#ifndef __EPI_STR_LEXER_H__
#define __EPI_STR_LEXER_H__

namespace epi
{

enum token_kind_e
{
	TOK_EOF = 0,
	TOK_ERROR,

	TOK_Ident,
	TOK_Symbol,
	TOK_Number,
	TOK_String
};


class lexer_c
{
public:
	lexer_c(const std::string& _data) : data(_data), pos(0), line(1)
	{ }

	~lexer_c()
	{ }

	// parse the next token, storing contents into given string.
	// returns TOK_EOF at the end of the data, and TOK_ERROR when a
	// problem is encountered (s will be an error message).
	token_kind_e Next(std::string& s);

	// check if the next token is an identifier or symbol matching the
	// given string.  the match is not case sensitive.  if it matches,
	// the token is consumed and true is returned.  if not, false is
	// returned and the position is unchanged.
	bool Match(const char *s);

	// give the line number for the last token returned by Next() or
	// the token implicitly checked by Match().  can be used to show
	// where in the file an error occurred.
	int LastLine();

	// helpers for converting numeric tokens.
	int    ToInt   (const std::string& s);
	double ToDouble(const std::string& s);

private:
	const std::string& data;

	size_t pos;
	int    line;

	//...
};

} // namespace epi

#endif /* __EPI_STR_LEXER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
