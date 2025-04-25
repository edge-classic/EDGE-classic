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

#include "epi_color.h"

extern std::string working_directory;

constexpr uint8_t   kENDOOMLines        = 25;
constexpr uint8_t   kENDOOMBytesPerLine = 160;
constexpr uint16_t  kENDOOMTotalVerts   = kENDOOMLines * 80 * 4; // 80 characters per line * 4 verts per quad
constexpr RGBAColor kENDOOMColors[16]   = {0x000000FF, 0x0000AAFF, 0x00AA00FF, 0x00AAAAFF, 0xAA0000FF, 0xAA00AAFF,
                                           0xAA5500FF, 0xAAAAAAFF, 0x555555FF, 0x5555FFFF, 0x55FF55FF, 0x55FFFFFF,
                                           0xFF5555FF, 0xFF55FFFF, 0xFFFF55FF, 0xFFFFFFFF};

enum ConsoleVisibility
{
    kConsoleNotVisible, // invisible
    kConsoleMaximal,    // fullscreen + a command line
    kConsoleToggle
};

class ConsoleLine
{
  public:
    std::string line_;

    RGBAColor color_;

    std::vector<uint8_t> endoom_bytes_;

  public:
    ConsoleLine() : color_(kRGBANoValue)
    {
    }

    ConsoleLine(const std::string &text, RGBAColor col = kRGBALightGray) : line_(text), color_(col)
    {
    }

    ConsoleLine(const char *text, RGBAColor col = kRGBALightGray) : line_(text), color_(col)
    {
    }

    ~ConsoleLine()
    {
    }

    void Append(const char *text)
    {
        line_.append(text);
    }

    void Clear()
    {
        line_.clear();
        endoom_bytes_.clear();
    }
};

void TryConsoleCommand(const char *cmd);

enum ConsoleMessageTarget
{
    kConsoleOnly,
    kConsoleHUDTop,
    kConsoleHUDCenter
};

#ifdef __GNUC__
void ConsoleMessage(ConsoleMessageTarget target, const char *message, ...) __attribute__((format(printf, 2, 3)));
#else
void ConsoleMessage(ConsoleMessageTarget target, const char *message, ...);
#endif

void ConsoleENDOOM();

void CreateQuitScreen();

void ClearConsole();

// this color will apply to the next ConsoleMessage or ConsolePrint call.
void ConsoleMessageColor(RGBAColor col);

bool ConsoleIsVisible();

// Displays/Hides the console.
void SetConsoleVisibility(ConsoleVisibility v);

int MatchConsoleCommands(std::vector<const char *> &list, const char *pattern);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
