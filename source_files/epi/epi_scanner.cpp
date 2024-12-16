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

#include "epi_scanner.h"

#include <stdarg.h>
#include <string.h>

#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"

namespace epi
{

static constexpr const char* TokenNames[Scanner::kNumSpecialTokens] =
{
	"Identifier",
	"String Constant",
	"Integer Constant",
	"Float Constant",
	"Boolean Constant",
	"Logical And",
	"Logical Or",
	"Equals",
	"Not Equals",
	"Greater Than or Equals",
	"Less Than or Equals",
	"Left Shift",
	"Right Shift",
	"Increment",
	"Decrement",
	"Pointer Member",
	"Scope Resolution",
	"Macro Concatenation",
	"Assign Sum",
	"Assign Difference",
	"Assign Product",
	"Assign Quotient",
	"Assign Modulus",
	"Assign Left Shift",
	"Assign Right Shift",
	"Assign Bitwise And",
	"Assign Bitwise Or",
	"Assign Exclusive Or",
	"Ellipsis",
	"Annotation Start",
	"Annotation End",
};

Scanner::Scanner(std::string_view data, size_t length) : line(1), lineStart(0), logicalPosition(0), scanPos(0), needNext(true)
{
	if(length == 0 && !data.empty())
		length = data.size();
	this->length = length;
	data_ = data;

	CheckForWhitespace();

	state.scanPos = scanPos;
	state.tokenLine = 0;
	state.tokenLinePosition = 0;
}

Scanner::~Scanner()
{

}

// Here's my answer to the preprocessor screwing up line numbers. What we do is
// after a new line in CheckForWhitespace, look for a comment in the form of
// "/*meta:filename:line*/"
void Scanner::CheckForMeta()
{
	if(scanPos+10 < length)
	{
		if(epi::StringCompare(data_.substr(scanPos, 7), "/*meta:") == 0)
		{
			scanPos += 7;
			int metaStart = scanPos;
			int fileLength = 0;
			int lineLength = 0;
			while(scanPos < length)
			{
				char thisChar = data_.at(scanPos);
				char nextChar = scanPos+1 < length ? data_.at(scanPos+1) : 0;
				if(thisChar == '*' && nextChar == '/')
				{
					lineLength = scanPos-metaStart-1-fileLength;
					scanPos += 2;
					break;
				}
				if(thisChar == ':' && fileLength == 0)
					fileLength = scanPos-metaStart;
				scanPos++;
			}
			if(fileLength > 0 && lineLength > 0)
			{
				SetScriptIdentifier(data_.substr(metaStart, fileLength));
				std::string lineNumber(data_.data()+metaStart+fileLength+1, lineLength);
				line = atoi(lineNumber.c_str());
				lineStart = scanPos;
			}
		}
	}
}

void Scanner::CheckForWhitespace()
{
	int comment = 0; // 1 = till next new line, 2 = till end block
	while(scanPos < length)
	{
		char cur = data_.at(scanPos);
		char next = scanPos+1 < length ? data_.at(scanPos+1) : 0;
		if(comment == 2)
		{
			if(cur != '*' || next != '/')
			{
				if(cur == '\n' || cur == '\r')
				{
					scanPos++;
					if(comment == 1)
						comment = 0;

					// Do a quick check for Windows style new line
					if(cur == '\r' && next == '\n')
						scanPos++;
					IncrementLine();
				}
				else
					scanPos++;
			}
			else
			{
				comment = 0;
				scanPos += 2;
			}
			continue;
		}

		if(cur == ' ' || cur == '\t' || cur == 0)
			scanPos++;
		else if(cur == '\n' || cur == '\r')
		{
			scanPos++;
			if(comment == 1)
				comment = 0;

			// Do a quick check for Windows style new line
			if(cur == '\r' && next == '\n')
				scanPos++;
			IncrementLine();
			CheckForMeta();
		}
		else if(cur == '/' && comment == 0)
		{
			switch(next)
			{
				case '/':
					comment = 1;
					break;
				/*case '*':
				{
					char next2 = scanPos+2 < length ? data[scanPos+2] : 0;
					if(next2 != ' ')
						comment = 2;
					break;
				}*/
				default:
					return;
			}
			scanPos += 2;
		}
		else
		{
			if(comment == 0)
				return;
			else
				scanPos++;
		}
	}
}

bool Scanner::CheckToken(char token)
{
	if(needNext)
	{
		if(!GetNextToken(false))
			return false;
	}

	// An int can also be a float.
	if(nextState.token == token || (nextState.token == Scanner::kIntConst && token == Scanner::kFloatConst))
	{
		needNext = true;
		ExpandState();
		return true;
	}
	needNext = false;
	return false;
}

void Scanner::ExpandState()
{
	scanPos = nextState.scanPos;
	logicalPosition = scanPos;
	CheckForWhitespace();

	prevState = state;
	state = nextState;
}

bool Scanner::GetNextString()
{
	if(!needNext)
	{
		int prevLine = line;
		scanPos = state.scanPos;
		CheckForWhitespace();
		line = prevLine;
	}
	else
		CheckForWhitespace();

	nextState.tokenLine = line;
	nextState.tokenLinePosition = scanPos - lineStart;
	nextState.token = Scanner::kNoToken;
	if(scanPos >= length)
		return false;

	unsigned int start = scanPos;
	unsigned int end = scanPos;
	bool quoted = data_.at(scanPos) == '"';
	if(quoted) // String Constant
	{
		end = ++start; // Remove starting quote
		scanPos++;
		while(scanPos < length)
		{
			char cur = data_.at(scanPos);
			if(cur == '"')
				end = scanPos;
			else if(cur == '\\')
			{
				scanPos += 2;
				continue;
			}
			scanPos++;
			if(start != end)
				break;
		}
	}
	else // Unquoted string
	{
		while(scanPos < length)
		{
			char cur = data_.at(scanPos);
			switch(cur)
			{
				default:
					break;
				case ',':
					if(scanPos == start)
						break;
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					end = scanPos;
					break;
			}
			if(start != end)
				break;
			scanPos++;
		}

		if(scanPos == length)
			end = scanPos;
	}
	if(end-start > 0)
	{
		nextState.scanPos = scanPos;
		std::string thisString(data_.data()+start, end-start);
		if(quoted)
			Unescape(thisString);
		nextState.str = thisString;
		nextState.token = Scanner::kStringConst;
		ExpandState();
		needNext = true;
		return true;
	}
	return false;
}

bool Scanner::GetNextToken(bool expandState)
{
	if(!needNext)
	{
		needNext = true;
		if(expandState)
			ExpandState();
		return true;
	}

	nextState.tokenLine = line;
	nextState.tokenLinePosition = scanPos - lineStart;
	nextState.token = Scanner::kNoToken;
	if(scanPos >= length)
	{
		if(expandState)
			ExpandState();
		return false;
	}

	unsigned int start = scanPos;
	unsigned int end = scanPos;
	int integerBase = 10;
	bool floatHasDecimal = false;
	bool floatHasExponent = false;
	bool stringFinished = false; // Strings are the only things that can have 0 length tokens.

	char cur = data_.at(scanPos++);
	// Determine by first character
	if(cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
		nextState.token = Scanner::kIdentifier;
	else if(cur >= '0' && cur <= '9')
	{
		if(cur == '0')
			integerBase = 8;
		nextState.token = Scanner::kIntConst;
	}
	else if(cur == '.' && scanPos < length && data_.at(scanPos) != '.')
	{
		floatHasDecimal = true;
		nextState.token = Scanner::kFloatConst;
	}
	else if(cur == '"')
	{
		end = ++start; // Move the start up one character so we don't have to trim it later.
		nextState.token = Scanner::kStringConst;
	}
	else
	{
		end = scanPos;
		nextState.token = cur;

		// Now check for operator tokens
		if(scanPos < length)
		{
			char next = data_.at(scanPos);
			if(cur == '&' && next == '&')
				nextState.token = Scanner::kAndAnd;
			else if(cur == '|' && next == '|')
				nextState.token = Scanner::kOrOr;
			else if(
				(cur == '<' && next == '<') ||
				(cur == '>' && next == '>')
			)
			{
				// Next for 3 character tokens
				if(scanPos+1 > length && data_.at(scanPos+1) == '=')
				{
					scanPos++;
					nextState.token = cur == '<' ? Scanner::kShiftLeftEq : Scanner::kShiftRightEq;
					
				}
				else
					nextState.token = cur == '<' ? Scanner::kShiftLeft : Scanner::kShiftRight;
			}
			else if(cur == '#' && next == '#')
				nextState.token = Scanner::kMacroConcat;
			else if(cur == ':' && next == ':')
				nextState.token = Scanner::kScopeResolution;
			else if(cur == '+' && next == '+')
				nextState.token = Scanner::kIncrement;
			else if(cur == '/' && next == '*')
				nextState.token = Scanner::kAnnotateStart;
			else if(cur == '*' && next == '/')
				nextState.token = Scanner::kAnnotateEnd;
			else if(cur == '-')
			{
				if(next == '-')
					nextState.token = Scanner::kDecrement;
				else if(next == '>')
					nextState.token = Scanner::kPointerMember;
			}
			else if(cur == '.' && next == '.' &&
				scanPos+1 < length && data_.at(scanPos+1) == '.')
			{
				nextState.token = Scanner::kEllipsis;
				++scanPos;
			}
			else if(next == '=')
			{
				switch(cur)
				{
					case '=':
						nextState.token = Scanner::kEqEq;
						break;
					case '!':
						nextState.token = Scanner::kNotEq;
						break;
					case '>':
						nextState.token = Scanner::kGtrEq;
						break;
					case '<':
						nextState.token = Scanner::kLessEq;
						break;
					case '+':
						nextState.token = Scanner::kAddEq;
						break;
					case '-':
						nextState.token = Scanner::kSubEq;
						break;
					case '*':
						nextState.token = Scanner::kMulEq;
						break;
					case '/':
						nextState.token = Scanner::kDivEq;
						break;
					case '%':
						nextState.token = Scanner::kModEq;
						break;
					case '&':
						nextState.token = Scanner::kAndEq;
						break;
					case '|':
						nextState.token = Scanner::kOrEq;
						break;
					case '^':
						nextState.token = Scanner::kXorEq;
						break;
					default:
						break;
				}
			}

			if(nextState.token != cur)
			{
				scanPos++;
				end = scanPos;
			}
		}
	}

	if(start == end)
	{
		while(scanPos < length)
		{
			cur = data_.at(scanPos);
			switch(nextState.token)
			{
				default:
					break;
				case Scanner::kIdentifier:
					if(cur != '_' && (cur < 'A' || cur > 'Z') && (cur < 'a' || cur > 'z') && (cur < '0' || cur > '9'))
						end = scanPos;
					break;
				case Scanner::kIntConst:
					if(cur == '.' || (scanPos-1 != start && cur == 'e'))
						nextState.token = Scanner::kFloatConst;
					else if((cur == 'x' || cur == 'X') && scanPos-1 == start)
					{
						integerBase = 16;
						break;
					}
					else
					{
						switch(integerBase)
						{
							default:
								if(cur < '0' || cur > '9')
									end = scanPos;
								break;
							case 8:
								if(cur < '0' || cur > '7')
									end = scanPos;
								break;
							case 16:
								if((cur < '0' || cur > '9') && (cur < 'A' || cur > 'F') && (cur < 'a' || cur > 'f'))
									end = scanPos;
								break;
						}
						break;
					}
				case Scanner::kFloatConst:
					if(cur < '0' || cur > '9')
					{
						if(!floatHasDecimal && cur == '.')
						{
							floatHasDecimal = true;
							break;
						}
						else if(!floatHasExponent && cur == 'e')
						{
							floatHasDecimal = true;
							floatHasExponent = true;
							if(scanPos+1 < length)
							{
								char next = data_.at(scanPos+1);
								if((next < '0' || next > '9') && next != '+' && next != '-')
									end = scanPos;
								else
									scanPos++;
							}
							break;
						}
						end = scanPos;
					}
					break;
				case Scanner::kStringConst:
					if(cur == '"')
					{
						stringFinished = true;
						end = scanPos;
						scanPos++;
					}
					else if(cur == '\\')
						scanPos++; // Will add two since the loop automatically adds one
					break;
			}
			if(start == end && !stringFinished)
				scanPos++;
			else
				break;
		}
		// Handle small tokens at the end of a file.
		if(scanPos == length && !stringFinished)
			end = scanPos;
	}

	nextState.scanPos = scanPos;
	if(end-start > 0 || stringFinished)
	{
		nextState.str = std::string(data_.data()+start, end-start);
		if(nextState.token == Scanner::kFloatConst)
		{
			if(floatHasDecimal && nextState.str.size() == 1)
			{
				// Don't treat a lone '.' as a decimal.
				nextState.token = '.';
			}
			else
			{
				nextState.decimal = atof(nextState.str.c_str());
				nextState.number = (int)nextState.decimal;
				nextState.boolean = (nextState.number != 0);
			}
		}
		else if(nextState.token == Scanner::kIntConst)
		{
			nextState.number = strtol(nextState.str.c_str(), NULL, integerBase);
			nextState.decimal = nextState.number;
			nextState.boolean = (nextState.number != 0);
		}
		else if(nextState.token == Scanner::kIdentifier)
		{
			// Check for a boolean constant.
			if(epi::StringCaseCompareASCII(nextState.str, "true") == 0)
			{
				nextState.token = Scanner::kBoolConst;
				nextState.boolean = true;
			}
			else if(epi::StringCaseCompareASCII(nextState.str, "false") == 0)
			{
				nextState.token = Scanner::kBoolConst;
				nextState.boolean = false;
			}
		}
		else if(nextState.token == Scanner::kStringConst)
		{
			Unescape(nextState.str);
		}
		if(expandState)
			ExpandState();
		return true;
	}
	nextState.token = Scanner::kNoToken;
	if(expandState)
		ExpandState();
	return false;
}

void Scanner::IncrementLine()
{
	line++;
	lineStart = scanPos;
}

void Scanner::MustGetToken(char token)
{
	if(!CheckToken(token))
	{
		ExpandState();
		if(state.token == Scanner::kNoToken)
			ScriptMessage(Scanner::kError, "Unexpected end of script.");
		else if(token < Scanner::kNumSpecialTokens && state.token < Scanner::kNumSpecialTokens)
			ScriptMessage(Scanner::kError, "Expected '%s' but got '%s' instead.", TokenNames[(int)token], TokenNames[(int)state.token]);
		else if(token < Scanner::kNumSpecialTokens && state.token >= Scanner::kNumSpecialTokens)
			ScriptMessage(Scanner::kError, "Expected '%s' but got '%c' instead.", TokenNames[(int)token], state.token);
		else if(token >= Scanner::kNumSpecialTokens && state.token < Scanner::kNumSpecialTokens)
			ScriptMessage(Scanner::kError, "Expected '%c' but got '%s' instead.", token, TokenNames[(int)state.token]);
		else
			ScriptMessage(Scanner::kError, "Expected '%c' but got '%c' instead.", token, state.token);
	}
}

void Scanner::Rewind()
{
	needNext = false;
	nextState = state;
	state = prevState;
	scanPos = state.scanPos;

	line = prevState.tokenLine;
	logicalPosition = prevState.tokenLinePosition;
}

void Scanner::ScriptMessage(MessageLevel level, const char* error, ...) const
{
	const char* messageLevel;
	switch(level)
	{
		case kWarning:
			messageLevel = "Warning";
			break;
		case kError:
			messageLevel = "Error";
			break;
		default:
			messageLevel = "Notice";
			break;
	}

	std::string newMessage = epi::StringFormat("%s:%d:%d:%s: %s\n", scriptIdentifier.c_str(), GetLine(), GetLinePos(), messageLevel, error);
	va_list list;
	va_start(list, error);
	switch(level)
	{
		case kWarning:
			LogWarning(newMessage.c_str(), list);
			break;
		case kError:
			FatalError(newMessage.c_str(), list);
			break;
		default:
			LogPrint(newMessage.c_str(), list);
			break;
	}
	va_end(list);
}

int Scanner::SkipLine()
{
	int ret = GetPos();
	while(logicalPosition < length)
	{
		char thisChar = data_.at(logicalPosition);
		char nextChar = logicalPosition+1 < length ? data_.at(logicalPosition+1) : 0;
		if(thisChar == '\n' || thisChar == '\r')
		{
			ret = logicalPosition++; // Return the first newline character we see.
			if(nextChar == '\r')
				logicalPosition++;
			IncrementLine();
			CheckForWhitespace();
			break;
		}
		logicalPosition++;
	}
	if(logicalPosition > scanPos)
	{
		scanPos = logicalPosition;
		CheckForWhitespace();
		needNext = true;
		logicalPosition = scanPos;
	}
	return ret;
}

bool Scanner::TokensLeft() const
{
	return scanPos < length;
}

// NOTE: Be sure that '\\' is the first thing in the array otherwise it will re-escape.
static char escapeCharacters[] = {'\\', '"', 'n', 0};
static char resultCharacters[] = {'\\', '"', '\n', 0};
const std::string &Scanner::Escape(std::string &str)
{
	for(unsigned int i = 0;escapeCharacters[i] != 0;i++)
	{
		// += 2 because we'll be inserting 1 character.
		for(size_t p = 0;p < str.size() && (p = str.find_first_of(resultCharacters[i], p)) != std::string::npos;p += 2)
		{
			str.insert(p, 1, '\\');
		}
	}
	return str;
}
const std::string Scanner::Escape(const char *str)
{
	std::string tmp(str);
	Escape(tmp);
	return tmp;
}
const std::string &Scanner::Unescape(std::string &str)
{
	for(unsigned int i = 0;escapeCharacters[i] != 0;i++)
	{
		std::string sequence("\\");
		sequence.push_back(escapeCharacters[i]);
		for(size_t p = 0;p < str.size() && (p = str.find(sequence, p)) != std::string::npos;p++)
		{
			str.replace(str.find(sequence, p), 2, 1, resultCharacters[i]);
		}
	}
	return str;
}

} // namespace epi