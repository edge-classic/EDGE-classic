//----------------------------------------------------------------------------
//  EDGE Console Main
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#pragma once

#include <string>
#include <vector>

#include "math_color.h"

constexpr uint8_t kEndoomLines = 25;

const RGBAColor endoom_colors[16] = {
    0x000000FF, 0x0000AAFF, 0x00AA00FF, 0x00AAAAFF, 0xAA0000FF, 0xAA00AAFF,
    0xAA5500FF, 0xAAAAAAFF, 0x555555FF, 0x5555FFFF, 0x55FF55FF, 0x55FFFFFF,
    0xFF5555FF, 0xFF55FFFF, 0xFFFF55FF, 0xFFFFFFFF};

enum ConsoleVisibility
{
    kConsoleVisibilityNotVisible,  // invisible
    kConsoleVisibilityMaximal,     // fullscreen + a command line
    kConsoleVisibilityToggle
};

class ConsoleLine
{
   public:
    std::string line_;

    RGBAColor color_;

    std::vector<uint8_t> endoom_bytes_;

   public:
    ConsoleLine(const std::string &text, RGBAColor col = SG_LIGHT_GRAY_RGBA32)
        : line_(text), color_(col)
    {
    }

    ConsoleLine(const char *text, RGBAColor col = SG_LIGHT_GRAY_RGBA32)
        : line_(text), color_(col)
    {
    }

    ~ConsoleLine() {}

    void Append(const char *text) { line_.append(text); }

    void AppendEndoom(uint8_t endoom_byte)
    {
        endoom_bytes_.push_back(endoom_byte);
    }

    void Clear()
    {
        line_.clear();
        endoom_bytes_.clear();
    }
};

void ConsoleTryCommand(const char *cmd);

#ifdef __GNUC__
void ConsolePrint(const char *message, ...)
    __attribute__((format(printf, 1, 2)));
void ConsoleMessage(const char *message, ...)
    __attribute__((format(printf, 1, 2)));
void ConsolePlayerMessage(int plyr, const char *message, ...)
    __attribute__((format(printf, 2, 3)));
#else
void ConsolePrint(const char *message, ...);
void ConsoleMessage(const char *message, ...);
void ConsolePlayerMessage(int plyr, const char *message, ...);
#endif

void ConsolePrintEndoom();

void ConsoleCreateQuitScreen();

void ConsoleClearLines();

// Looks up the string in LDF, appends an extra '\n', and then writes it to
// the console. Should be used for most player messages.
void ConsoleMessageLDF(const char *lookup, ...);

void ConsoleImportantMessageLDF(const char *lookup, ...);

// Looks up in LDF.
void ConsolePlayerMessageLDF(int plyr, const char *message, ...);

// this color will apply to the next ConsoleMessage or ConsolePrint call.
void ConsoleMessageColor(RGBAColor col);

// Displays/Hides the console.
void ConsoleSetVisible(ConsoleVisibility v);

int ConsoleMatchAllCmds(std::vector<const char *> &list, const char *pattern);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
