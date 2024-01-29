//----------------------------------------------------------------------------
//  EDGE File Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
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

#ifndef __EPI_FILE_CLASS__
#define __EPI_FILE_CLASS__

#include <limits.h>

namespace epi
{

// base class
class File
{
  public:
    // Seek reference points
    enum Seek
    {
        kSeekpointStart,
        kSeekpointCurrent,
        kSeekpointEnd,
        kSeekpointNumTypes
    };

  protected:
  public:
    File()
    {
    }
    virtual ~File()
    {
    }

    virtual int GetLength()   = 0;
    virtual int GetPosition() = 0;

    virtual unsigned int Read(void *dest, unsigned int size)       = 0;
    virtual unsigned int Write(const void *src, unsigned int size) = 0;

    virtual bool Seek(int offset, int seekpoint) = 0;

  public:
    // load the file into memory, reading from the current
    // position, and reading no more than the 'max_size'
    // parameter (in bytes).  An extra NUL byte is appended
    // to the result buffer.  Returns NULL on failure.
    // The returned buffer must be freed with delete[].
    uint8_t *LoadIntoMemory(int max_size = INT_MAX);

    // Reads the file as text
    std::string ReadText();
};

// standard File class using ANSI C functions
class ANSIFile : public File
{
  private:
    FILE *fp_;

  public:
    ANSIFile(FILE *filep);
    ~ANSIFile();

  public:
    int GetLength();
    int GetPosition();

    unsigned int Read(void *dest, unsigned int size);
    unsigned int Write(const void *src, unsigned int size);

    bool Seek(int offset, int seekpoint);
};

} // namespace epi

#endif /* __EPI_FILE_CLASS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
