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

#include "file.h"

#include <string.h>

#include "HandmadeMath.h"
#include "epi.h"
namespace epi
{

ANSIFile::ANSIFile(FILE *filep) : fp_(filep) {}

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

    long cur_pos = ftell(fp_);  // Get existing position

    fseek(fp_, 0, SEEK_END);  // Seek to the end of file
    long len = ftell(fp_);    // Get the position - it our length

    fseek(fp_, cur_pos, SEEK_SET);  // Reset existing position
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
        case kSeekpointStart:
        {
            whence = SEEK_SET;
            break;
        }
        case kSeekpointCurrent:
        {
            whence = SEEK_CUR;
            break;
        }
        case kSeekpointEnd:
        {
            whence = SEEK_END;
            break;
        }

        default:
            FatalError("ANSIFile::Seek : illegal seekpoint value.\n");
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
        LogWarning("File::LoadIntoMemory : position > length.\n");
        actual_size = 0;
    }

    if (actual_size > max_size) actual_size = max_size;

    uint8_t *buffer     = new uint8_t[actual_size + 1];
    buffer[actual_size] = 0;

    if ((int)Read(buffer, actual_size) != actual_size)
    {
        delete[] buffer;
        return nullptr;
    }

    return buffer;  // success!
}

SubFile::SubFile(File *parent, int start, int len)
    : parent_(parent), start_(start), length_(len), pos_(0)
{
    SYS_ASSERT(parent_ != nullptr);
    SYS_ASSERT(start_ >= 0);
    SYS_ASSERT(length_ >= 0);
}

SubFile::~SubFile() { parent_ = nullptr; }

unsigned int SubFile::Read(void *dest, unsigned int size)
{
    // EOF ?
    if (pos_ >= length_) return 0;

    size = HMM_MIN(size, (unsigned int)(length_ - pos_));

    // we must always seek before a read, because other things may also be
    // reading the parent file.
    parent_->Seek(start_ + pos_, kSeekpointStart);

    unsigned int got = parent_->Read(dest, size);

    pos_ += (int)got;

    return got;
}

bool SubFile::Seek(int offset, int seekpoint)
{
    int new_pos = 0;

    switch (seekpoint)
    {
        case kSeekpointStart:
        {
            new_pos = 0;
            break;
        }
        case kSeekpointCurrent:
        {
            new_pos = pos_;
            break;
        }
        case kSeekpointEnd:
        {
            new_pos = length_;
            break;
        }

        default:
            return false;
    }

    new_pos += offset;

    // NOTE: we allow position at the very end (last byte + 1).
    if (new_pos < 0 || new_pos > length_) return false;

    pos_ = new_pos;

    return true;
}

unsigned int SubFile::Write(const void *src, unsigned int size)
{
    (void)src;
    (void)size;

    FatalError("SubFile::Write called.\n");

    return 0; /* read only, cobber */
}

MemFile::MemFile(const uint8_t *block, int len, bool copy_it)
{
    SYS_ASSERT(block);
    SYS_ASSERT(len >= 0);

    pos_    = 0;
    copied_ = false;

    if (len == 0)
    {
        data_   = nullptr;
        length_ = 0;
        return;
    }

    if (copy_it)
    {
        data_   = new uint8_t[len];
        length_ = len;

        memcpy(data_, block, len);
        copied_ = true;
    }
    else
    {
        data_   = (uint8_t *)block;
        length_ = len;
    }
}

MemFile::~MemFile()
{
    if (data_ && copied_)
    {
        delete[] data_;
        data_ = nullptr;
    }

    length_ = 0;
}

unsigned int MemFile::Read(void *dest, unsigned int size)
{
    SYS_ASSERT(dest);

    unsigned int avail = length_ - pos_;

    if (size > avail) size = avail;

    if (size == 0) return 0;  // EOF

    memcpy(dest, data_ + pos_, size);
    pos_ += size;

    return size;
}

bool MemFile::Seek(int offset, int seekpoint)
{
    int new_pos = 0;

    switch (seekpoint)
    {
        case kSeekpointStart:
        {
            new_pos = 0;
            break;
        }
        case kSeekpointCurrent:
        {
            new_pos = pos_;
            break;
        }
        case kSeekpointEnd:
        {
            new_pos = length_;
            break;
        }

        default:
            return false;
    }

    new_pos += offset;

    // Note: allow position at the very end (last byte + 1).
    if (new_pos < 0 || new_pos > length_) return false;

    pos_ = new_pos;
    return true;
}

unsigned int MemFile::Write(const void *src, unsigned int size)
{
    (void)src;
    (void)size;

    FatalError("MemFile::Write called.\n");

    return 0; /* read only, cobber */
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
