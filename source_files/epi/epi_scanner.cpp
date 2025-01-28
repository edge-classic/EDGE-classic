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

#include "epi_scanner.h"

#include <stdarg.h>
#include <string.h>

#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"

namespace epi
{

static constexpr const char *TokenNames[Scanner::kTotalSpecialTokens] = {
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

Scanner::Scanner(std::string_view data, size_t length)
    : line_(1), line_start_(0), logical_position_(0), scan_position_(0), need_next_(true)
{
    if (length == 0 && !data.empty())
        length = data.size();
    this->length_ = length;
    data_         = data;

    CheckForWhitespace();

    state_.scan_position       = scan_position_;
    state_.token_line          = 0;
    state_.token_line_position = 0;
}

Scanner::~Scanner()
{
}

// Here's my answer to the preprocessor screwing up line numbers. What we do is
// after a new line in CheckForWhitespace, look for a comment in the form of
// "/*meta:filename:line*/"
void Scanner::CheckForMeta()
{
    if (scan_position_ + 10 < length_)
    {
        if (epi::StringCompare(data_.substr(scan_position_, 7), "/*meta:") == 0)
        {
            scan_position_ += 7;
            int metaStart  = scan_position_;
            int fileLength = 0;
            int lineLength = 0;
            while (scan_position_ < length_)
            {
                char thisChar = data_.at(scan_position_);
                char nextChar = scan_position_ + 1 < length_ ? data_.at(scan_position_ + 1) : 0;
                if (thisChar == '*' && nextChar == '/')
                {
                    lineLength = scan_position_ - metaStart - 1 - fileLength;
                    scan_position_ += 2;
                    break;
                }
                if (thisChar == ':' && fileLength == 0)
                    fileLength = scan_position_ - metaStart;
                scan_position_++;
            }
            if (fileLength > 0 && lineLength > 0)
            {
                SetScriptIdentifier(data_.substr(metaStart, fileLength));
                std::string lineNumber(data_.data() + metaStart + fileLength + 1, lineLength);
                line_       = atoi(lineNumber.c_str());
                line_start_ = scan_position_;
            }
        }
    }
}

void Scanner::CheckForWhitespace()
{
    int comment = 0; // 1 = till next new line, 2 = till end block
    while (scan_position_ < length_)
    {
        char cur  = data_.at(scan_position_);
        char next = scan_position_ + 1 < length_ ? data_.at(scan_position_ + 1) : 0;
        if (comment == 2)
        {
            if (cur != '*' || next != '/')
            {
                if (cur == '\n' || cur == '\r')
                {
                    scan_position_++;
                    if (comment == 1)
                        comment = 0;

                    // Do a quick check for Windows style new line
                    if (cur == '\r' && next == '\n')
                        scan_position_++;
                    IncrementLine();
                }
                else
                    scan_position_++;
            }
            else
            {
                comment = 0;
                scan_position_ += 2;
            }
            continue;
        }

        if (cur == ' ' || cur == '\t' || cur == 0)
            scan_position_++;
        else if (cur == '\n' || cur == '\r')
        {
            scan_position_++;
            if (comment == 1)
                comment = 0;

            // Do a quick check for Windows style new line
            if (cur == '\r' && next == '\n')
                scan_position_++;
            IncrementLine();
            CheckForMeta();
        }
        else if (cur == '/' && comment == 0)
        {
            switch (next)
            {
            case '/':
                comment = 1;
                break;
            /*case '*':
            {
                char next2 = scan_position_+2 < length ? data[scan_position_+2] : 0;
                if(next2 != ' ')
                    comment = 2;
                break;
            }*/
            default:
                return;
            }
            scan_position_ += 2;
        }
        else
        {
            if (comment == 0)
                return;
            else
                scan_position_++;
        }
    }
}

bool Scanner::CheckToken(char token)
{
    if (need_next_)
    {
        if (!GetNextToken(false))
            return false;
    }

    // An int can also be a float.
    if (next_state_.token == token || (next_state_.token == Scanner::kIntConst && token == Scanner::kFloatConst))
    {
        need_next_ = true;
        ExpandState();
        return true;
    }
    need_next_ = false;
    return false;
}

void Scanner::ExpandState()
{
    scan_position_    = next_state_.scan_position;
    logical_position_ = scan_position_;
    CheckForWhitespace();

    previous_state_ = state_;
    state_          = next_state_;
}

bool Scanner::GetNextString()
{
    if (!need_next_)
    {
        int prevLine   = line_;
        scan_position_ = state_.scan_position;
        CheckForWhitespace();
        line_ = prevLine;
    }
    else
        CheckForWhitespace();

    next_state_.token_line          = line_;
    next_state_.token_line_position = scan_position_ - line_start_;
    next_state_.token               = Scanner::kNoToken;
    if (scan_position_ >= length_)
        return false;

    unsigned int start  = scan_position_;
    unsigned int end    = scan_position_;
    bool         quoted = data_.at(scan_position_) == '"';
    if (quoted)        // String Constant
    {
        end = ++start; // Remove starting quote
        scan_position_++;
        while (scan_position_ < length_)
        {
            char cur = data_.at(scan_position_);
            if (cur == '"')
                end = scan_position_;
            else if (cur == '\\')
            {
                scan_position_ += 2;
                continue;
            }
            scan_position_++;
            if (start != end)
                break;
        }
    }
    else // Unquoted string
    {
        while (scan_position_ < length_)
        {
            char cur = data_.at(scan_position_);
            switch (cur)
            {
            default:
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                end = scan_position_;
                break;
            case ',':
                if (scan_position_ == start)
                    break;
                end = scan_position_;
                break;
            }
            if (start != end)
                break;
            scan_position_++;
        }

        if (scan_position_ == length_)
            end = scan_position_;
    }
    if (end - start > 0)
    {
        next_state_.scan_position = scan_position_;
        std::string thisString(data_.data() + start, end - start);
        if (quoted)
            Unescape(thisString);
        next_state_.string = thisString;
        next_state_.token  = Scanner::kStringConst;
        ExpandState();
        need_next_ = true;
        return true;
    }
    return false;
}

bool Scanner::GetNextToken(bool expandState)
{
    if (!need_next_)
    {
        need_next_ = true;
        if (expandState)
            ExpandState();
        return true;
    }

    next_state_.token_line          = line_;
    next_state_.token_line_position = scan_position_ - line_start_;
    next_state_.token               = Scanner::kNoToken;
    if (scan_position_ >= length_)
    {
        if (expandState)
            ExpandState();
        return false;
    }

    unsigned int start            = scan_position_;
    unsigned int end              = scan_position_;
    int          integerBase      = 10;
    bool         floatHasDecimal  = false;
    bool         stringFinished   = false; // Strings are the only things that can have 0 length tokens.

    char cur = data_.at(scan_position_++);
    // Determine by first character
    if (cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
        next_state_.token = Scanner::kIdentifier;
    else if ((cur >= '0' && cur <= '9') || (cur == '-' && scan_position_ < length_ &&
                                            (data_.at(scan_position_) >= '0' && data_.at(scan_position_) <= '9')))
    {
        if (cur == '0')
            integerBase = 8;
        next_state_.token = Scanner::kIntConst;
    }
    else if ((cur == '.' && scan_position_ < length_ && data_.at(scan_position_) != '.') ||
             (cur == '-' && scan_position_ < length_ && data_.at(scan_position_) == '.'))
    {
        floatHasDecimal   = true;
        next_state_.token = Scanner::kFloatConst;
    }
    else if (cur == '"')
    {
        end               = ++start; // Move the start up one character so we don't have to trim it later.
        next_state_.token = Scanner::kStringConst;
    }
    else
    {
        end               = scan_position_;
        next_state_.token = cur;

        // Now check for operator tokens
        if (scan_position_ < length_)
        {
            char next = data_.at(scan_position_);
            if (cur == '&' && next == '&')
                next_state_.token = Scanner::kAndAnd;
            else if (cur == '|' && next == '|')
                next_state_.token = Scanner::kOrOr;
            else if ((cur == '<' && next == '<') || (cur == '>' && next == '>'))
            {
                // Next for 3 character tokens
                if (scan_position_ + 1 > length_ && data_.at(scan_position_ + 1) == '=')
                {
                    scan_position_++;
                    next_state_.token = cur == '<' ? Scanner::kShiftLeftEq : Scanner::kShiftRightEq;
                }
                else
                    next_state_.token = cur == '<' ? Scanner::kShiftLeft : Scanner::kShiftRight;
            }
            else if (cur == '#' && next == '#')
                next_state_.token = Scanner::kMacroConcat;
            else if (cur == ':' && next == ':')
                next_state_.token = Scanner::kScopeResolution;
            else if (cur == '+' && next == '+')
                next_state_.token = Scanner::kIncrement;
            else if (cur == '/' && next == '*')
                next_state_.token = Scanner::kAnnotateStart;
            else if (cur == '*' && next == '/')
                next_state_.token = Scanner::kAnnotateEnd;
            else if (cur == '-')
            {
                if (next == '-')
                    next_state_.token = Scanner::kDecrement;
                else if (next == '>')
                    next_state_.token = Scanner::kPointerMember;
            }
            else if (cur == '.' && next == '.' && scan_position_ + 1 < length_ && data_.at(scan_position_ + 1) == '.')
            {
                next_state_.token = Scanner::kEllipsis;
                ++scan_position_;
            }
            else if (next == '=')
            {
                switch (cur)
                {
                case '=':
                    next_state_.token = Scanner::kEqEq;
                    break;
                case '!':
                    next_state_.token = Scanner::kNotEq;
                    break;
                case '>':
                    next_state_.token = Scanner::kGtrEq;
                    break;
                case '<':
                    next_state_.token = Scanner::kLessEq;
                    break;
                case '+':
                    next_state_.token = Scanner::kAddEq;
                    break;
                case '-':
                    next_state_.token = Scanner::kSubEq;
                    break;
                case '*':
                    next_state_.token = Scanner::kMulEq;
                    break;
                case '/':
                    next_state_.token = Scanner::kDivEq;
                    break;
                case '%':
                    next_state_.token = Scanner::kModEq;
                    break;
                case '&':
                    next_state_.token = Scanner::kAndEq;
                    break;
                case '|':
                    next_state_.token = Scanner::kOrEq;
                    break;
                case '^':
                    next_state_.token = Scanner::kXorEq;
                    break;
                default:
                    break;
                }
            }

            if (next_state_.token != cur)
            {
                scan_position_++;
                end = scan_position_;
            }
        }
    }

    if (start == end)
    {
        bool floatHasExponent = false;
        while (scan_position_ < length_)
        {
            cur = data_.at(scan_position_);
            switch (next_state_.token)
            {
            default:
                break;
            case Scanner::kIdentifier:
                if (cur != '_' && (cur < 'A' || cur > 'Z') && (cur < 'a' || cur > 'z') && (cur < '0' || cur > '9'))
                    end = scan_position_;
                break;
            case Scanner::kIntConst:
                if (cur == '.' || (scan_position_ - 1 != start && cur == 'e'))
                    next_state_.token = Scanner::kFloatConst;
                else if ((cur == 'x' || cur == 'X') && scan_position_ - 1 == start)
                {
                    integerBase = 16;
                    break;
                }
                else
                {
                    switch (integerBase)
                    {
                    default:
                        if (cur < '0' || cur > '9')
                            end = scan_position_;
                        break;
                    case 8:
                        if (cur < '0' || cur > '7')
                            end = scan_position_;
                        break;
                    case 16:
                        if ((cur < '0' || cur > '9') && (cur < 'A' || cur > 'F') && (cur < 'a' || cur > 'f'))
                            end = scan_position_;
                        break;
                    }
                    break;
                }
            case Scanner::kFloatConst:
                if (cur < '0' || cur > '9')
                {
                    if (!floatHasDecimal && cur == '.')
                    {
                        floatHasDecimal = true;
                        break;
                    }
                    else if (!floatHasExponent && cur == 'e')
                    {
                        floatHasDecimal  = true;
                        floatHasExponent = true;
                        if (scan_position_ + 1 < length_)
                        {
                            char next = data_.at(scan_position_ + 1);
                            if ((next < '0' || next > '9') && next != '+' && next != '-')
                                end = scan_position_;
                            else
                                scan_position_++;
                        }
                        break;
                    }
                    end = scan_position_;
                }
                break;
            case Scanner::kStringConst:
                if (cur == '"')
                {
                    stringFinished = true;
                    end            = scan_position_;
                    scan_position_++;
                }
                else if (cur == '\\')
                    scan_position_++; // Will add two since the loop automatically adds one
                break;
            }
            if (start == end && !stringFinished)
                scan_position_++;
            else
                break;
        }
        // Handle small tokens at the end of a file.
        if (scan_position_ == length_ && !stringFinished)
            end = scan_position_;
    }

    next_state_.scan_position = scan_position_;
    if (end - start > 0 || stringFinished)
    {
        next_state_.string = std::string(data_.data() + start, end - start);
        if (next_state_.token == Scanner::kFloatConst)
        {
            if (floatHasDecimal && next_state_.string.size() == 1)
            {
                // Don't treat a lone '.' as a decimal.
                next_state_.token = '.';
            }
            else
            {
                next_state_.decimal = atof(next_state_.string.c_str());
                next_state_.number  = (int)next_state_.decimal;
                next_state_.boolean = (next_state_.number != 0);
            }
        }
        else if (next_state_.token == Scanner::kIntConst)
        {
            next_state_.number  = strtol(next_state_.string.c_str(), NULL, integerBase);
            next_state_.decimal = next_state_.number;
            next_state_.boolean = (next_state_.number != 0);
        }
        else if (next_state_.token == Scanner::kIdentifier)
        {
            // Check for a boolean constant.
            if (epi::StringCaseCompareASCII(next_state_.string, "true") == 0)
            {
                next_state_.token   = Scanner::kBoolConst;
                next_state_.boolean = true;
            }
            else if (epi::StringCaseCompareASCII(next_state_.string, "false") == 0)
            {
                next_state_.token   = Scanner::kBoolConst;
                next_state_.boolean = false;
            }
        }
        else if (next_state_.token == Scanner::kStringConst)
        {
            Unescape(next_state_.string);
        }
        if (expandState)
            ExpandState();
        return true;
    }
    next_state_.token = Scanner::kNoToken;
    if (expandState)
        ExpandState();
    return false;
}

void Scanner::IncrementLine()
{
    line_++;
    line_start_ = scan_position_;
}

void Scanner::MustGetToken(char token)
{
    if (!CheckToken(token))
    {
        ExpandState();
        if (state_.token == Scanner::kNoToken)
            ScriptMessage(Scanner::kError, "Unexpected end of script.");
        else if (token < Scanner::kTotalSpecialTokens && state_.token < Scanner::kTotalSpecialTokens)
            ScriptMessage(Scanner::kError, "Expected '%s' but got '%s' instead.", TokenNames[(int)token],
                          TokenNames[(int)state_.token]);
        else if (token < Scanner::kTotalSpecialTokens && state_.token >= Scanner::kTotalSpecialTokens)
            ScriptMessage(Scanner::kError, "Expected '%s' but got '%c' instead.", TokenNames[(int)token], state_.token);
        else if (token >= Scanner::kTotalSpecialTokens && state_.token < Scanner::kTotalSpecialTokens)
            ScriptMessage(Scanner::kError, "Expected '%c' but got '%s' instead.", token, TokenNames[(int)state_.token]);
        else
            ScriptMessage(Scanner::kError, "Expected '%c' but got '%c' instead.", token, state_.token);
    }
}

void Scanner::Rewind()
{
    need_next_     = false;
    next_state_    = state_;
    state_         = previous_state_;
    scan_position_ = state_.scan_position;

    line_             = previous_state_.token_line;
    logical_position_ = previous_state_.token_line_position;
}

void Scanner::ScriptMessage(MessageLevel level, const char *error, ...) const
{
    const char *messageLevel;
    switch (level)
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

    std::string newMessage = epi::StringFormat("%s:%d:%d:%s: %s\n", script_identifier_.c_str(), GetLine(),
                                               GetLinePosition(), messageLevel, error);
    va_list     list;
    va_start(list, error);
    switch (level)
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
    int ret = GetPosition();
    while (logical_position_ < length_)
    {
        char thisChar = data_.at(logical_position_);
        char nextChar = logical_position_ + 1 < length_ ? data_.at(logical_position_ + 1) : 0;
        if (thisChar == '\n' || thisChar == '\r')
        {
            ret = logical_position_++; // Return the first newline character we see.
            if (nextChar == '\r')
                logical_position_++;
            IncrementLine();
            CheckForWhitespace();
            break;
        }
        logical_position_++;
    }
    if (logical_position_ > scan_position_)
    {
        scan_position_ = logical_position_;
        CheckForWhitespace();
        need_next_        = true;
        logical_position_ = scan_position_;
    }
    return ret;
}

bool Scanner::TokensLeft() const
{
    return scan_position_ < length_;
}

// NOTE: Be sure that '\\' is the first thing in the array otherwise it will re-escape.
static char        escapeCharacters[] = {'\\', '"', 'n', 0};
static char        resultCharacters[] = {'\\', '"', '\n', 0};
const std::string &Scanner::Escape(std::string &str)
{
    for (unsigned int i = 0; escapeCharacters[i] != 0; i++)
    {
        // += 2 because we'll be inserting 1 character.
        for (size_t p = 0; p < str.size() && (p = str.find_first_of(resultCharacters[i], p)) != std::string::npos;
             p += 2)
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
    for (unsigned int i = 0; escapeCharacters[i] != 0; i++)
    {
        std::string sequence("\\");
        sequence.push_back(escapeCharacters[i]);
        for (size_t p = 0; p < str.size() && (p = str.find(sequence, p)) != std::string::npos; p++)
        {
            str.replace(str.find(sequence, p), 2, 1, resultCharacters[i]);
        }
    }
    return str;
}

} // namespace epi