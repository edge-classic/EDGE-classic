//----------------------------------------------------------------------------
//  Block of memory with File interface
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

#ifndef __EPI_FILE_MEMORY_H__
#define __EPI_FILE_MEMORY_H__

#include "file.h"

namespace epi
{

class MemFile : public File
{
  private:
    uint8_t *data_;

    int  length_;
    int  pos_;
    bool copied_;

  public:
    MemFile(const uint8_t *block, int len, bool copy_it = true);
    ~MemFile();

    int GetLength()
    {
        return length_;
    }
    int GetPosition()
    {
        return pos_;
    }

    unsigned int Read(void *dest, unsigned int size);
    unsigned int Write(const void *src, unsigned int size);

    bool Seek(int offset, int seekpoint);
};

} // namespace epi

#endif /*__EPI_FILE_MEMORY_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
