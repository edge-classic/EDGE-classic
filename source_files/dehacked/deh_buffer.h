//------------------------------------------------------------------------
//  BUFFER for Parsing
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#pragma once

namespace dehacked
{

class InputBuffer
{
  public:
    InputBuffer(const char *data, int length);
    ~InputBuffer();

    bool EndOfFile();
    bool Error();
    int  Read(void *buffer, int count);
    int  GetCharacter();
    void UngetCharacter(int character);
    bool IsBinary() const;

  private:
    const char *data_; // base pointer
    const char *pointer_;  // current read pointer

    int length_;
};

} // namespace dehacked