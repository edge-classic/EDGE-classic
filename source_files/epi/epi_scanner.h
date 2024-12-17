//----------------------------------------------------------------------------
//  EPI Scanner (tokenizer)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024 The EDGE Team
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
//
// Adapted from the Declarate reference implementation with the following
// copyright:
//
// Copyright (c) 2010, Braden "Blzut3" Obrzut
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * The names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include <stdint.h>

#include <string>

namespace epi
{

class Scanner
{
	public:
		struct ParserState
		{
			std::string		string;
			int				number;
			double			decimal;
			bool			boolean;
			char			token;
			uint32_t		token_line;
			uint32_t		token_line_position;
			uint32_t		scan_position;
		};

		enum
		{
			kIdentifier,		// Ex: SomeIdentifier
			kStringConst,		// Ex: "Some String"
			kIntConst,			// Ex: 27
			kFloatConst,		// Ex: 1.5
			kBoolConst,			// Ex: true
			kAndAnd,			// &&
			kOrOr,				// ||
			kEqEq,				// ==
			kNotEq,				// !=
			kGtrEq,				// >=
			kLessEq,			// <=
			kShiftLeft,			// <<
			kShiftRight,		// >>
			kIncrement,			// ++
			kDecrement,			// --
			kPointerMember,		// ->
			kScopeResolution,	// ::
			kMacroConcat,		// ##
			kAddEq,				// +=
			kSubEq,				// -=
			kMulEq,				// *=
			kDivEq,				// /=
			kModEq,				// %=
			kShiftLeftEq,		// <<=
			kShiftRightEq,		// >>=
			kAndEq,				// &=
			kOrEq,				// |=
			kXorEq,				// ^=
			kEllipsis,			// ...
			kAnnotateStart,		// Block comment start
			kAnnotateEnd,		// Block comment end

			kTotalSpecialTokens,

			kNoToken = -1
		};

		enum MessageLevel
		{
			kError,
			kWarning,
			kNotice
		};

		Scanner(std::string_view data, size_t length=0);
		~Scanner();

		void			CheckForMeta();
		void			CheckForWhitespace();
		bool			CheckToken(char token);
		void			ExpandState();
		//const char*	GetData() const { return data; }
		int				GetLine() const { return state_.token_line; }
		int				GetLinePosition() const { return state_.token_line_position; }
		int				GetPosition() const { return logical_position_; }
		uint32_t		GetScanPosition() const { return scan_position_; }
		bool			GetNextString();
		bool			GetNextToken(bool expandState=true);
		void			MustGetToken(char token);
		void			Rewind(); /// Only can rewind one step.
		void			ScriptMessage(MessageLevel level, const char* error, ...) const;
		void			SetScriptIdentifier(std::string_view ident) { script_identifier_ = ident; }
		int				SkipLine();
		bool			TokensLeft() const;
		const ParserState &operator*() const { return state_; }
		const ParserState *operator->() const { return &state_; }

		static const std::string	&Escape(std::string &str);
		static const std::string	Escape(const char *str);
		static const std::string	&Unescape(std::string &str);

		ParserState		state_;

	protected:
		void	IncrementLine();

	private:
		ParserState		next_state_, previous_state_;

		std::string_view data_;
		size_t	length_;

		uint32_t	line_;
		uint32_t	line_start_;
		uint32_t	logical_position_;
		uint32_t	scan_position_;

		bool			need_next_; // If checkToken returns false this will be false.

		std::string		script_identifier_;
};

} // namespace epi