/*
** Copyright (c) 2010, Braden "Blzut3" Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * The names of its contributors may be used to endorse or promote
**       products derived from this software without specific prior written
**       permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT,
** INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
			std::string		str;
			int				number;
			double			decimal;
			bool			boolean;
			char			token;
			uint32_t		tokenLine;
			uint32_t		tokenLinePosition;
			uint32_t		scanPos;
		};

		enum
		{
			kIdentifier,		// Ex: SomeIdentifier
			kStringConst,		// Ex: "Some String"
			kIntConst,		// Ex: 27
			kFloatConst,		// Ex: 1.5
			kBoolConst,		// Ex: true
			kAndAnd,			// &&
			kOrOr,			// ||
			kEqEq,			// ==
			kNotEq,			// !=
			kGtrEq,			// >=
			kLessEq,			// <=
			kShiftLeft,		// <<
			kShiftRight,		// >>
			kIncrement,		// ++
			kDecrement,		// --
			kPointerMember,	// ->
			kScopeResolution,	// ::
			kMacroConcat,		// ##
			kAddEq,			// +=
			kSubEq,			// -=
			kMulEq,			// *=
			kDivEq,			// /=
			kModEq,			// %=
			kShiftLeftEq,		// <<=
			kShiftRightEq,	// >>=
			kAndEq,			// &=
			kOrEq,			// |=
			kXorEq,			// ^=
			kEllipsis,		// ...
			kAnnotateStart,	// Block comment start
			kAnnotateEnd,		// Block comment end

			kNumSpecialTokens,

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
		int				GetLine() const { return state.tokenLine; }
		int				GetLinePos() const { return state.tokenLinePosition; }
		int				GetPos() const { return logicalPosition; }
		uint32_t		GetScanPos() const { return scanPos; }
		bool			GetNextString();
		bool			GetNextToken(bool expandState=true);
		void			MustGetToken(char token);
		void			Rewind(); /// Only can rewind one step.
		void			ScriptMessage(MessageLevel level, const char* error, ...) const;
		void			SetScriptIdentifier(std::string_view ident) { scriptIdentifier = ident; }
		int				SkipLine();
		bool			TokensLeft() const;
		const ParserState &operator*() const { return state; }
		const ParserState *operator->() const { return &state; }

		static const std::string	&Escape(std::string &str);
		static const std::string	Escape(const char *str);
		static const std::string	&Unescape(std::string &str);

		ParserState		state;

	protected:
		void	IncrementLine();

	private:
		ParserState		nextState, prevState;

		std::string_view data_;
		size_t	length;

		uint32_t	line;
		uint32_t	lineStart;
		uint32_t	logicalPosition;
		uint32_t	scanPos;

		bool			needNext; // If checkToken returns false this will be false.

		std::string		scriptIdentifier;
};

} // namespace epi