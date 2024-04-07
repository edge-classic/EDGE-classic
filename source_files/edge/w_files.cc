//----------------------------------------------------------------------------
//  EDGE file handling
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "w_files.h"

#include <algorithm>
#include <list>
#include <vector>

#include "con_main.h"
#include "ddf_anim.h"
#include "ddf_colormap.h"
#include "ddf_flat.h"
#include "ddf_font.h"
#include "ddf_image.h"
#include "ddf_main.h"
#include "ddf_style.h"
#include "ddf_switch.h"
#include "ddf_wadfixes.h"
#include "deh_edge.h"
#include "dm_state.h"
#include "dstrings.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_system.h"
#include "l_deh.h"
#include "rad_trig.h"
#include "w_epk.h"
#include "w_wad.h"

std::vector<DataFile *> data_files;

DataFile::DataFile(std::string name, FileKind kind)
    : name_(name), kind_(kind), file_(nullptr), wad_(nullptr), pack_(nullptr)
{
}

DataFile::~DataFile()
{
}

int GetTotalFiles()
{
    return (int)data_files.size();
}

size_t AddDataFile(std::string file, FileKind kind)
{
    LogDebug("Added filename: %s\n", file.c_str());

    size_t index = data_files.size();

    DataFile *df = new DataFile(file, kind);
    data_files.push_back(df);

    return index;
}

//----------------------------------------------------------------------------

std::vector<DataFile *> pending_files;

size_t AddPendingFile(std::string file, FileKind kind)
{
    size_t index = pending_files.size();

    DataFile *df = new DataFile(file, kind);
    pending_files.push_back(df);

    return index;
}

// TODO tidy this
extern void ProcessFixersForWad(DataFile *df);
extern void ProcessWad(DataFile *df, size_t file_index);

extern std::string BuildXGLNodesForWad(DataFile *df);

static void DEH_ConvertFile(std::string &filename)
{
    epi::File *F = epi::FileOpen(filename, epi::kFileAccessRead | epi::kFileAccessBinary);
    if (F == nullptr)
    {
        LogPrint("FAILED to open file: %s\n", filename.c_str());
        return;
    }

    int      length = F->GetLength();
    uint8_t *data   = F->LoadIntoMemory();

    if (data == nullptr)
    {
        LogPrint("FAILED to read file: %s\n", filename.c_str());
        delete F;
        return;
    }

    ConvertDehacked(data, length, filename);

    // close file, free that data
    delete F;
    delete[] data;
}

static void W_ExternalDDF(DataFile *df)
{
    DDFType type = DDFFilenameToType(df->name_);

    std::string bare_name = epi::GetFilename(df->name_);

    if (type == kDDFTypeUnknown)
        FatalError("Unknown DDF filename: %s\n", bare_name.c_str());

    LogPrint("Reading DDF file: %s\n", df->name_.c_str());

    epi::File *F = epi::FileOpen(df->name_, epi::kFileAccessRead);
    if (F == nullptr)
        FatalError("Couldn't open file: %s\n", df->name_.c_str());

    // WISH: load directly into a std::string

    char *raw_data = (char *)F->LoadIntoMemory();
    if (raw_data == nullptr)
        FatalError("Couldn't read file: %s\n", df->name_.c_str());

    std::string data(raw_data);
    delete[] raw_data;

    DDFAddFile(type, data, df->name_);
}

static void W_ExternalRTS(DataFile *df)
{
    LogPrint("Reading RTS script: %s\n", df->name_.c_str());

    epi::File *F = epi::FileOpen(df->name_, epi::kFileAccessRead);
    if (F == nullptr)
        FatalError("Couldn't open file: %s\n", df->name_.c_str());

    // WISH: load directly into a std::string

    char *raw_data = (char *)F->LoadIntoMemory();
    if (raw_data == nullptr)
        FatalError("Couldn't read file: %s\n", df->name_.c_str());

    std::string data(raw_data);
    delete[] raw_data;

    DDFAddFile(kDDFTypeRadScript, data, df->name_);
}

void ProcessFile(DataFile *df)
{
    size_t file_index = data_files.size();
    data_files.push_back(df);

    // open a WAD/PK3 file and add contents to directory
    std::string filename = df->name_;

    LogPrint("  Processing: %s\n", filename.c_str());

    if (df->kind_ <= kFileKindXWAD)
    {
        epi::File *file = epi::FileOpen(filename, epi::kFileAccessRead | epi::kFileAccessBinary);
        if (file == nullptr)
        {
            FatalError("Couldn't open file: %s\n", filename.c_str());
            return;
        }

        df->file_ = file;

        ProcessWad(df, file_index);
    }
    else if (df->kind_ == kFileKindPackWAD || df->kind_ == kFileKindIPackWAD)
    {
        EPI_ASSERT(df->file_); // This should already be handled by the pack
                               // processing
        ProcessWad(df, file_index);
    }
    else if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEPK ||
             df->kind_ == kFileKindEEPK || df->kind_ == kFileKindIPK || df->kind_ == kFileKindIFolder)
    {
        ProcessAllInPack(df, file_index);
    }
    else if (df->kind_ == kFileKindDDF)
    {
        // handle external ddf files (from `-file` option)
        W_ExternalDDF(df);
    }
    else if (df->kind_ == kFileKindRTS)
    {
        // handle external rts scripts (from `-file` or `-script` option)
        W_ExternalRTS(df);
    }
    else if (df->kind_ == kFileKindDehacked)
    {
        // handle stand-alone DeHackEd patches
        LogPrint("Converting DEH file: %s\n", df->name_.c_str());

        DEH_ConvertFile(df->name_);
    }

    // handle fixer-uppers   [ TODO support it for EPK files too ]
    if (df->wad_ != nullptr)
        ProcessFixersForWad(df);
}

void ProcessMultipleFiles()
{
    // open all the files, add all the lumps.
    // NOTE: we rebuild the list, since new files can get added as we go along,
    //       and they should appear *after* the one which produced it.

    std::vector<DataFile *> copied_files(data_files);
    data_files.clear();

    for (size_t i = 0; i < copied_files.size(); i++)
    {
        ProcessFile(copied_files[i]);

        for (size_t k = 0; k < pending_files.size(); k++)
        {
            ProcessFile(pending_files[k]);
        }

        pending_files.clear();
    }
}

void BuildXGLNodes(void)
{
    for (size_t i = 0; i < data_files.size(); i++)
    {
        DataFile *df = data_files[i];

        if (df->kind_ == kFileKindIWAD || df->kind_ == kFileKindPWAD || df->kind_ == kFileKindPackWAD ||
            df->kind_ == kFileKindIPackWAD)
        {
            std::string xwa_filename = BuildXGLNodesForWad(df);

            if (!xwa_filename.empty())
            {
                DataFile *new_df = new DataFile(xwa_filename, kFileKindXWAD);
                ProcessFile(new_df);
            }
        }
    }
}

//----------------------------------------------------------------------------
int CheckPackFilesForName(const std::string &name)
{
    // search from newest file to oldest
    for (int i = (int)data_files.size() - 1; i >= 0; i--)
    {
        DataFile *df = data_files[i];
        if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEPK ||
            df->kind_ == kFileKindEEPK || df->kind_ == kFileKindIFolder || df->kind_ == kFileKindIPK)
        {
            if (FindPackFile(df->pack_, name))
                return i;
        }
    }
    return -1;
}

//----------------------------------------------------------------------------

epi::File *OpenFileFromPack(const std::string &name)
{
    // search from newest file to oldest
    for (int i = (int)data_files.size() - 1; i >= 0; i--)
    {
        DataFile *df = data_files[i];
        if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEPK ||
            df->kind_ == kFileKindEEPK || df->kind_ == kFileKindIFolder || df->kind_ == kFileKindIPK)
        {
            epi::File *F = OpenPackFile(df->pack_, name);
            if (F != nullptr)
                return F;
        }
    }

    // not found
    return nullptr;
}

//----------------------------------------------------------------------------

uint8_t *OpenPackOrLumpInMemory(const std::string &name, const std::vector<std::string> &extensions, int *length)
{
    int lump_df  = -1;
    int lump_num = CheckLumpNumberForName(name.c_str());
    if (lump_num > -1)
        lump_df = GetDataFileIndexForLump(lump_num);

    for (int i = (int)data_files.size() - 1; i >= 0; i--)
    {
        if (i > lump_df)
        {
            DataFile *df = data_files[i];
            if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEPK ||
                df->kind_ == kFileKindEEPK || df->kind_ == kFileKindIFolder || df->kind_ == kFileKindIPK)
            {
                epi::File *F = OpenPackMatch(df->pack_, name, extensions);
                if (F != nullptr)
                {
                    uint8_t *raw_packfile = F->LoadIntoMemory();
                    *length               = F->GetLength();
                    delete F;
                    return raw_packfile;
                }
            }
        }
    }

    if (lump_num > -1)
        return LoadLumpIntoMemory(lump_num, length);

    // not found
    return nullptr;
}

//----------------------------------------------------------------------------

void DoPackSubstitutions()
{
    for (size_t i = 0; i < data_files.size(); i++)
    {
        if (data_files[i]->pack_)
            ProcessPackSubstitutions(data_files[i]->pack_, i);
    }
}

//----------------------------------------------------------------------------

static const char *FileKindString(FileKind kind)
{
    switch (kind)
    {
    case kFileKindIWAD:
        return "iwad";
    case kFileKindPWAD:
        return "pwad";
    case kFileKindEEPK:
        return "edge";
    case kFileKindXWAD:
        return "xwa";
    case kFileKindPackWAD:
        return "pwad";
    case kFileKindIPackWAD:
        return "iwad";

    case kFileKindFolder:
        return "DIR";
    case kFileKindEFolder:
        return "edge";
    case kFileKindIFolder:
        return "DIR";
    case kFileKindEPK:
        return "epk";
    case kFileKindIPK:
        return "epk";

    case kFileKindDDF:
        return "ddf";
    case kFileKindRTS:
        return "rts";
    case kFileKindDehacked:
        return "deh";

    default:
        return "???";
    }
}

void ShowLoadedFiles()
{
    LogPrint("File list:\n");

    for (int i = 0; i < (int)data_files.size(); i++)
    {
        DataFile *df = data_files[i];

        LogPrint(" %2d: %-4s \"%s\"\n", i + 1, FileKindString(df->kind_), df->name_.c_str());
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
