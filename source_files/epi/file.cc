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

#include "epi.h"

#include "file.h"

namespace epi
{

ANSIFile::ANSIFile(FILE *filep) : fp_(filep)
{
}

ANSIFile::~ANSIFile()
{
    if (fp_)
    {
        fclose(fp_);
        fp_ = nullptr;
    }
}

int ANSIFile::GetLength()
{
    SYS_ASSERT(fp_);

    long cur_pos = ftell(fp_); // Get existing position

    fseek(fp_, 0, SEEK_END); // Seek to the end of file
    long len = ftell(fp_);   // Get the position - it our length

    fseek(fp_, cur_pos, SEEK_SET); // Reset existing position
    return (int)len;
}

int ANSIFile::GetPosition()
{
    SYS_ASSERT(fp_);

    return (int)ftell(fp_);
}

unsigned int ANSIFile::Read(void *dest, unsigned int size)
{
    SYS_ASSERT(fp_);
    SYS_ASSERT(dest);

    return fread(dest, 1, size, fp_);
}

unsigned int ANSIFile::Write(const void *src, unsigned int size)
{
    SYS_ASSERT(fp_);
    SYS_ASSERT(src);

    return fwrite(src, 1, size, fp_);
}

bool ANSIFile::Seek(int offset, int seekpoint)
{
    int whence;

    switch (seekpoint)
    {
    case kSeekpointStart: {
        whence = SEEK_SET;
        break;
    }
    case kSeekpointCurrent: {
        whence = SEEK_CUR;
        break;
    }
    case kSeekpointEnd: {
        whence = SEEK_END;
        break;
    }

    default:
        I_Error("ANSIFile::Seek : illegal seekpoint value.\n");
        return false; /* NOT REACHED */
    }

    int result = fseek(fp_, offset, whence);

    return (result == 0);
}

std::string File::ReadText()
{
    std::string textstring;
    Seek(kSeekpointStart, 0);
    uint8_t *buffer = LoadIntoMemory();
    if (buffer)
    {
        textstring.assign((char *)buffer, GetLength());
        delete[] buffer;
    }
    return textstring;
}

uint8_t *File::LoadIntoMemory(int max_size)
{
    SYS_ASSERT(max_size >= 0);

    int cur_pos     = GetPosition();
    int actual_size = GetLength();

    actual_size -= cur_pos;

    if (actual_size < 0)
    {
        I_Warning("File::LoadIntoMemory : position > length.\n");
        actual_size = 0;
    }

    if (actual_size > max_size)
        actual_size = max_size;

    uint8_t *buffer        = new uint8_t[actual_size + 1];
    buffer[actual_size] = 0;

    if ((int)Read(buffer, actual_size) != actual_size)
    {
        delete[] buffer;
        return nullptr;
    }

    return buffer; // success!
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
