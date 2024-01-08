//----------------------------------------------------------------------------
//  EDGE<->AJBSP Bridging code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include "e_main.h"
#include "l_ajbsp.h"

// AJBSP
#include "bsp.h"

#define MSG_BUF_LEN 1024

class ec_buildinfo_t : public buildinfo_t
{
  public:
    void Print(int level, const char *fmt, ...)
    {
        if (level > 1)
            return;

        va_list arg_ptr;

        static char buffer[MSG_BUF_LEN];

        va_start(arg_ptr, fmt);
        vsnprintf(buffer, MSG_BUF_LEN - 1, fmt, arg_ptr);
        va_end(arg_ptr);

        buffer[MSG_BUF_LEN - 1] = 0;

        I_Printf("%s\n", buffer);
    }

    void Debug(const char *fmt, ...)
    {
        va_list arg_ptr;

        static char buffer[MSG_BUF_LEN];

        va_start(arg_ptr, fmt);
        vsnprintf(buffer, MSG_BUF_LEN - 1, fmt, arg_ptr);
        va_end(arg_ptr);

        buffer[MSG_BUF_LEN - 1] = 0;

        I_Debugf("%s\n", buffer);
    }

    void ShowMap(const char *name)
    {
        std::string msg = "Building nodes for ";
        msg += name;
        msg += "...\n";

        E_ProgressMessage(msg.c_str());
    }

    //
    //  show an error message and terminate the program
    //
    void FatalError(const char *fmt, ...)
    {
        va_list arg_ptr;

        static char buffer[MSG_BUF_LEN];

        va_start(arg_ptr, fmt);
        vsnprintf(buffer, MSG_BUF_LEN - 1, fmt, arg_ptr);
        va_end(arg_ptr);

        buffer[MSG_BUF_LEN - 1] = 0;

        ajbsp::CloseWad();

        I_Error("AJBSP: %s", buffer);
    }
};

//
// AJ_BuildNodes
//
// Attempt to build nodes for the WAD file containing the given
// WAD file.  Returns true if successful, otherwise false.
//
bool AJ_BuildNodes(data_file_c *df, std::filesystem::path outname)
{
    I_Debugf("AJ_BuildNodes: STARTED\n");
    I_Debugf("# source: '%s'\n", df->name.u8string().c_str());
    I_Debugf("#   dest: '%s'\n", outname.u8string().c_str());

    ec_buildinfo_t info;

    ajbsp::SetInfo(&info);

    epi::file_c *mem_wad    = nullptr;
    uint8_t        *raw_wad    = nullptr;
    int          raw_length = 0;

    if (df->kind == FLKIND_PackWAD)
    {
        mem_wad    = W_OpenPackFile(df->name.string());
        raw_length = mem_wad->GetLength();
        raw_wad    = mem_wad->LoadIntoMemory();
        ajbsp::OpenMem(df->name, raw_wad, raw_length);
    }
    else
        ajbsp::OpenWad(df->name);
    ajbsp::CreateXWA(outname);

    for (int i = 0; i < ajbsp::LevelsInWad(); i++)
    {
        ajbsp::BuildLevel(i);
    }

    ajbsp::FinishXWA();
    ajbsp::CloseWad();

    if (df->kind == FLKIND_PackWAD)
    {
        delete[] raw_wad;
        delete mem_wad;
    }

    I_Debugf("AJ_BuildNodes: FINISHED\n");
    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
