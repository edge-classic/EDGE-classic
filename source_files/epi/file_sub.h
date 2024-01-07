//----------------------------------------------------------------------------
//  Subset of an existing File
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#ifndef __EPI_FILE_SUB_H__
#define __EPI_FILE_SUB_H__

#include "file.h"

namespace epi
{

class sub_file_c : public file_c
{
  private:
    file_c *parent;

    int start;
    int length;
    int pos;

  public:
    sub_file_c(file_c *_parent, int _start, int _len);
    ~sub_file_c();

    int GetLength()
    {
        return length;
    }
    int GetPosition()
    {
        return pos;
    }

    unsigned int Read(void *dest, unsigned int size);
    unsigned int Write(const void *src, unsigned int size);

    bool Seek(int offset, int seekpoint);
};

} // namespace epi

#endif /*__EPI_FILE_SUB_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
