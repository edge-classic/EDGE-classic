//----------------------------------------------------------------------------
//  EDGE WAD Support Code
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
//
// This file contains various levels of support for using sprites and
// flats directly from a PWAD as well as some minor optimisations for
// patches. Because there are some PWADs that do arcane things with
// sprites, it is possible that this feature may not always work (at
// least, not until I become aware of them and support them) and so
// this feature can be turned off from the command line if necessary.
//
// -MH- 1998/03/04
//

#include "i_defs.h"

#include <limits.h>

#include <list>
#include <vector>
#include <algorithm>

// EPI
#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "math_md5.h"
#include "str_util.h"
#include "str_compare.h"
// AJBSP
#include "bsp.h"

// DDF
#include "main.h"
#include "switch.h"
#include "colormap.h"
#include "wadfixes.h"

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dm_structs.h"
#include "dstrings.h"
#include "e_main.h"
#include "e_search.h"
#include "l_deh.h"
#include "m_misc.h"
#include "r_image.h"
#include "vm_coal.h"
#include "w_epk.h"
#include "w_files.h"
#include "w_wad.h"
#include "w_texture.h"

#include "rad_trig.h"
#include "p_umapinfo.h" //Lobo 2022
#include "script/compat/lua_compat.h"

// Combination of unique lumps needed to best identify an IWAD
const std::vector<game_check_t> game_checker = {{
    {"Custom", "custom", {"EDGEGAME", "EDGEGAME"}},
    {"Blasphemer", "blasphemer", {"BLASPHEM", "E1M1"}},
    {"Freedoom 1", "freedoom1", {"FREEDOOM", "E1M1"}},
    {"Freedoom 2", "freedoom2", {"FREEDOOM", "MAP01"}},
    {"REKKR", "rekkr", {"REKCREDS", "E1M1"}},
    {"HacX", "hacx", {"HACX-R", "MAP01"}},
    {"Harmony", "harmony", {"0HAWK01", "DBIGFONT"}}, // Only the original, not Harmony-Compatible, should have DBIGFONT
    {"Chex Quest", "chex", {"ENDOOM", "_DEUTEX_"}},     // Original Chex Quest, NOT CQ3
    {"Heretic", "heretic", {"MUS_E1M1", "E1M1"}},
    {"Plutonia", "plutonia", {"CAMO1", "MAP01"}},
    {"Evilution", "tnt", {"REDTNT2", "MAP01"}},
    {"Doom", "doom", {"BFGGA0", "E2M1"}},
    {"Doom BFG", "doom", {"DMENUPIC", "M_MULTI"}}, // BFG Edition
    {"Doom Demo", "doom1", {"SHOTA0", "E1M1"}},
    {"Doom II", "doom2", {"BFGGA0", "MAP01"}},
    {"Doom II BFG", "doom2", {"DMENUPIC", "MAP33"}}, // BFG Edition
    {"Strife", "strife", {"VELLOGO", "RGELOGO"}} // Dev/internal use - Definitely nowhwere near playable
}};

class wad_file_c
{
  public:
    //??	data_file_c *_parent;

    // lists for sprites, flats, patches (stuff between markers)
    std::vector<int> sprite_lumps;
    std::vector<int> flat_lumps;
    std::vector<int> patch_lumps;
    std::vector<int> colmap_lumps;
    std::vector<int> tx_lumps;
    std::vector<int> hires_lumps;
    std::vector<int> xgl_lumps;

    // level markers and skin markers
    std::vector<int> level_markers;
    std::vector<int> skin_markers;

    // ddf and rts lump list
    int ddf_lumps[kTotalDDFTypes];

    // texture information
    wadtex_resource_c wadtex;

    // DeHackEd support
    int deh_lump;

    // COAL scripts
    int coal_huds;

    // LUA scripts
    int lua_huds;

    // UMAPINFO
    int umapinfo_lump;

    // BOOM stuff
    int animated;
    int switches;

    std::string md5_string;

  public:
    wad_file_c()
        : sprite_lumps(), flat_lumps(), patch_lumps(), colmap_lumps(), tx_lumps(), hires_lumps(), xgl_lumps(),
          level_markers(), skin_markers(), wadtex(), deh_lump(-1), coal_huds(-1), lua_huds(-1), umapinfo_lump(-1), animated(-1), switches(-1),
          md5_string()
    {
        for (int d = 0; d < kTotalDDFTypes; d++)
            ddf_lumps[d] = -1;
    }

    ~wad_file_c()
    {
    }

    bool HasLevel(const char *name) const;
};

typedef enum
{
    LMKIND_Normal = 0,  // fallback value
    LMKIND_Marker = 3,  // X_START, X_END, S_SKIN, level name
    LMKIND_WadTex = 6,  // palette, pnames, texture1/2
    LMKIND_DDFRTS = 10, // DDF, RTS, DEHACKED lump
    LMKIND_TX     = 14,
    LMKIND_Colmap = 15,
    LMKIND_Flat   = 16,
    LMKIND_Sprite = 17,
    LMKIND_Patch  = 18,
    LMKIND_HiRes  = 19,
    LMKIND_XGL    = 20,
} lump_kind_e;

typedef struct
{
    char name[10];

    int position;
    int size;

    // file number (an index into data_files[]).
    int file;

    // one of the LMKIND values.  For sorting, this is the least
    // significant aspect (but still necessary).
    lump_kind_e kind;
} lumpinfo_t;

//
//  GLOBALS
//

// Location of each lump on disk.
static std::vector<lumpinfo_t> lumpinfo;

static std::vector<int> sortedlumps;

#define LUMP_MAP_CMP(a) (strncmp(lumpinfo[sortedlumps[a]].name, buf, 8))

// the first datafile which contains a PLAYPAL lump
static int palette_datafile = -1;

// Sprites & Flats
static bool within_sprite_list;
static bool within_flat_list;
static bool within_patch_list;
static bool within_colmap_list;
static bool within_tex_list;
static bool within_hires_list;
static bool within_xgl_list;

//
// Is the name a sprite list start flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsS_START(char *name)
{
    if (strncmp(name, "SS_START", 8) == 0)
    {
        // fix up flag to standard syntax
        // Note: strncpy will pad will nulls
        strncpy(name, "S_START", 8);
        return 1;
    }

    return (strncmp(name, "S_START", 8) == 0);
}

//
// Is the name a sprite list end flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsS_END(char *name)
{
    if (strncmp(name, "SS_END", 8) == 0)
    {
        // fix up flag to standard syntax
        strncpy(name, "S_END", 8);
        return 1;
    }

    return (strncmp(name, "S_END", 8) == 0);
}

//
// Is the name a flat list start flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsF_START(char *name)
{
    if (strncmp(name, "FF_START", 8) == 0)
    {
        // fix up flag to standard syntax
        strncpy(name, "F_START", 8);
        return 1;
    }

    return (strncmp(name, "F_START", 8) == 0);
}

//
// Is the name a flat list end flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsF_END(char *name)
{
    if (strncmp(name, "FF_END", 8) == 0)
    {
        // fix up flag to standard syntax
        strncpy(name, "F_END", 8);
        return 1;
    }

    return (strncmp(name, "F_END", 8) == 0);
}

//
// Is the name a patch list start flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsP_START(char *name)
{
    if (strncmp(name, "PP_START", 8) == 0)
    {
        // fix up flag to standard syntax
        strncpy(name, "P_START", 8);
        return 1;
    }

    return (strncmp(name, "P_START", 8) == 0);
}

//
// Is the name a patch list end flag?
// If lax syntax match, fix up to standard syntax.
//
static bool IsP_END(char *name)
{
    if (strncmp(name, "PP_END", 8) == 0)
    {
        // fix up flag to standard syntax
        strncpy(name, "P_END", 8);
        return 1;
    }

    return (strncmp(name, "P_END", 8) == 0);
}

//
// Is the name a colourmap list start/end flag?
//
static bool IsC_START(char *name)
{
    return (strncmp(name, "C_START", 8) == 0);
}

static bool IsC_END(char *name)
{
    return (strncmp(name, "C_END", 8) == 0);
}

//
// Is the name a texture list start/end flag?
//
static bool IsTX_START(char *name)
{
    return (strncmp(name, "TX_START", 8) == 0);
}

static bool IsTX_END(char *name)
{
    return (strncmp(name, "TX_END", 8) == 0);
}

//
// Is the name a high-resolution start/end flag?
//
static bool IsHI_START(char *name)
{
    return (strncmp(name, "HI_START", 8) == 0);
}

static bool IsHI_END(char *name)
{
    return (strncmp(name, "HI_END", 8) == 0);
}

//
// Is the name a XGL nodes start/end flag?
//
static bool IsXG_START(char *name)
{
    return (strncmp(name, "XG_START", 8) == 0);
}

static bool IsXG_END(char *name)
{
    return (strncmp(name, "XG_END", 8) == 0);
}

//
// Is the name a dummy sprite/flat/patch marker ?
//
static bool IsDummySF(const char *name)
{
    return (
        strncmp(name, "S1_START", 8) == 0 || strncmp(name, "S2_START", 8) == 0 || strncmp(name, "S3_START", 8) == 0 ||
        strncmp(name, "F1_START", 8) == 0 || strncmp(name, "F2_START", 8) == 0 || strncmp(name, "F3_START", 8) == 0 ||
        strncmp(name, "P1_START", 8) == 0 || strncmp(name, "P2_START", 8) == 0 || strncmp(name, "P3_START", 8) == 0);
}

//
// Is the name a skin specifier ?
//
static bool IsSkin(const char *name)
{
    return (strncmp(name, "S_SKIN", 6) == 0);
}

bool wad_file_c::HasLevel(const char *name) const
{
    for (size_t i = 0; i < level_markers.size(); i++)
        if (strcmp(lumpinfo[level_markers[i]].name, name) == 0)
            return true;

    return false;
}

void W_GetTextureLumps(int file, wadtex_resource_c *res)
{
    SYS_ASSERT(0 <= file && file < (int)data_files.size());
    SYS_ASSERT(res);

    data_file_c *df  = data_files[file];
    wad_file_c  *wad = df->wad;

    if (wad == nullptr)
    {
        // leave the wadtex_resource_c in initial state
        return;
    }

    res->palette  = wad->wadtex.palette;
    res->pnames   = wad->wadtex.pnames;
    res->texture1 = wad->wadtex.texture1;
    res->texture2 = wad->wadtex.texture2;

    // find an earlier PNAMES lump when missing.
    // Ditto for palette.

    if (res->texture1 >= 0 || res->texture2 >= 0)
    {
        int cur;

        for (cur = file; res->pnames == -1 && cur > 0; cur--)
        {
            if (data_files[cur]->wad != nullptr)
                res->pnames = data_files[cur]->wad->wadtex.pnames;
        }

        for (cur = file; res->palette == -1 && cur > 0; cur--)
        {
            if (data_files[cur]->wad != nullptr)
                res->palette = data_files[cur]->wad->wadtex.palette;
        }
    }
}

//
// SortLumps
//
// Create the sortedlumps array, which is sorted by name for fast
// searching.  When two names are the same, we prefer lumps in later
// WADs over those in earlier ones.
//
// -AJA- 2000/10/14: simplified.
//
struct Compare_lump_pred
{
    inline bool operator()(const int &A, const int &B) const
    {
        const lumpinfo_t &C = lumpinfo[A];
        const lumpinfo_t &D = lumpinfo[B];

        // increasing name
        int cmp = strcmp(C.name, D.name);
        if (cmp != 0)
            return (cmp < 0);

        // decreasing file number
        cmp = C.file - D.file;
        if (cmp != 0)
            return (cmp > 0);

        // lump type
        if (C.kind != D.kind)
            return C.kind > D.kind;

        // tie breaker
        return C.position > D.position;
    }
};

static void SortLumps(void)
{
    int i;

    sortedlumps.resize(lumpinfo.size());

    for (i = 0; i < (int)lumpinfo.size(); i++)
        sortedlumps[i] = i;

    // sort it, primarily by increasing name, secondly by decreasing
    // file number, thirdly by the lump type.

    std::sort(sortedlumps.begin(), sortedlumps.end(), Compare_lump_pred());
}

//
// SortSpriteLumps
//
// Put the sprite list in sorted order (of name), required by
// R_InitSprites (speed optimisation).
//
static void SortSpriteLumps(wad_file_c *wad)
{
    if (wad->sprite_lumps.size() < 2)
        return;

    std::sort(wad->sprite_lumps.begin(), wad->sprite_lumps.end(), Compare_lump_pred());

#if 0 // DEBUGGING
	{
		int i, lump;
    
		for (i=0; i < wad->sprite_num; i++)
		{
			lump = wad->sprite_lumps[i];

			I_Debugf("Sorted sprite %d = lump %d [%s]\n", i, lump, lumpinfo[lump].name);
		}
	}
#endif
}

//
// LUMP BASED ROUTINES.
//

//
// AddLump
//
static void AddLump(data_file_c *df, const char *raw_name, int pos, int size, int file_index, bool allow_ddf)
{
    int lump = (int)lumpinfo.size();

    lumpinfo_t info;

    info.position = pos;
    info.size     = size;
    info.file     = file_index;
    info.kind     = LMKIND_Normal;

    // copy name, make it uppercase
    strncpy(info.name, raw_name, 8);
    info.name[8] = 0;

    for (size_t i = 0; i < strlen(info.name); i++)
    {
        info.name[i] = epi::ToUpperASCII(info.name[i]);
    }

    lumpinfo.push_back(info);

    lumpinfo_t *lump_p = &lumpinfo.back();

    // -- handle special names --

    wad_file_c *wad = df->wad;

    if (strcmp(info.name, "PLAYPAL") == 0)
    {
        lump_p->kind = LMKIND_WadTex;
        if (wad != nullptr)
            wad->wadtex.palette = lump;
        if (palette_datafile < 0)
            palette_datafile = file_index;
        return;
    }
    else if (strcmp(info.name, "PNAMES") == 0)
    {
        lump_p->kind = LMKIND_WadTex;
        if (wad != nullptr)
            wad->wadtex.pnames = lump;
        return;
    }
    else if (strcmp(info.name, "TEXTURE1") == 0)
    {
        lump_p->kind = LMKIND_WadTex;
        if (wad != nullptr)
            wad->wadtex.texture1 = lump;
        return;
    }
    else if (strcmp(info.name, "TEXTURE2") == 0)
    {
        lump_p->kind = LMKIND_WadTex;
        if (wad != nullptr)
            wad->wadtex.texture2 = lump;
        return;
    }
    else if (strcmp(info.name, "DEHACKED") == 0)
    {
        lump_p->kind = LMKIND_DDFRTS;
        if (wad != nullptr && info.size > 0)
            wad->deh_lump = lump;
        return;
    }
    else if (strcmp(info.name, "COALHUDS") == 0)
    {
        lump_p->kind = LMKIND_DDFRTS;
        if (wad != nullptr)
            wad->coal_huds = lump;
        return;
    }
    else if (strcmp(info.name, "LUAHUDS") == 0)
    {
        lump_p->kind = LMKIND_DDFRTS;
        if (wad != nullptr)
            wad->lua_huds = lump;
        return;
    }
    else if (strcmp(info.name, "UMAPINFO") == 0)
    {
        lump_p->kind = LMKIND_Normal;
        if (wad != nullptr)
            wad->umapinfo_lump = lump;
        return;
    }
    else if (strcmp(info.name, "ANIMATED") == 0)
    {
        lump_p->kind = LMKIND_DDFRTS;
        if (wad != nullptr)
            wad->animated = lump;
        return;
    }
    else if (strcmp(info.name, "SWITCHES") == 0)
    {
        lump_p->kind = LMKIND_DDFRTS;
        if (wad != nullptr)
            wad->switches = lump;
        return;
    }

    // -KM- 1998/12/16 Load DDF/RSCRIPT file from wad.
    if (allow_ddf && wad != nullptr)
    {
        DDFType type = DDF_LumpToType(info.name);

        if (type != kDDFTypeUNKNOWN)
        {
            lump_p->kind         = LMKIND_DDFRTS;
            wad->ddf_lumps[type] = lump;
            return;
        }
    }

    if (IsSkin(info.name))
    {
        lump_p->kind = LMKIND_Marker;
        if (wad != nullptr)
            wad->skin_markers.push_back(lump);
        return;
    }

    // -- handle sprite, flat & patch lists --

    if (IsS_START(lump_p->name))
    {
        lump_p->kind       = LMKIND_Marker;
        within_sprite_list = true;
        return;
    }
    else if (IsS_END(lump_p->name))
    {
        if (!within_sprite_list)
            I_Warning("Unexpected S_END marker in wad.\n");

        lump_p->kind       = LMKIND_Marker;
        within_sprite_list = false;
        return;
    }
    else if (IsF_START(lump_p->name))
    {
        lump_p->kind     = LMKIND_Marker;
        within_flat_list = true;
        return;
    }
    else if (IsF_END(lump_p->name))
    {
        if (!within_flat_list)
            I_Warning("Unexpected F_END marker in wad.\n");

        lump_p->kind     = LMKIND_Marker;
        within_flat_list = false;
        return;
    }
    else if (IsP_START(lump_p->name))
    {
        lump_p->kind      = LMKIND_Marker;
        within_patch_list = true;
        return;
    }
    else if (IsP_END(lump_p->name))
    {
        if (!within_patch_list)
            I_Warning("Unexpected P_END marker in wad.\n");

        lump_p->kind      = LMKIND_Marker;
        within_patch_list = false;
        return;
    }
    else if (IsC_START(lump_p->name))
    {
        lump_p->kind       = LMKIND_Marker;
        within_colmap_list = true;
        return;
    }
    else if (IsC_END(lump_p->name))
    {
        if (!within_colmap_list)
            I_Warning("Unexpected C_END marker in wad.\n");

        lump_p->kind       = LMKIND_Marker;
        within_colmap_list = false;
        return;
    }
    else if (IsTX_START(lump_p->name))
    {
        lump_p->kind    = LMKIND_Marker;
        within_tex_list = true;
        return;
    }
    else if (IsTX_END(lump_p->name))
    {
        if (!within_tex_list)
            I_Warning("Unexpected TX_END marker in wad.\n");

        lump_p->kind    = LMKIND_Marker;
        within_tex_list = false;
        return;
    }
    else if (IsHI_START(lump_p->name))
    {
        lump_p->kind      = LMKIND_Marker;
        within_hires_list = true;
        return;
    }
    else if (IsHI_END(lump_p->name))
    {
        if (!within_hires_list)
            I_Warning("Unexpected HI_END marker in wad.\n");

        lump_p->kind      = LMKIND_Marker;
        within_hires_list = false;
        return;
    }
    else if (IsXG_START(lump_p->name))
    {
        lump_p->kind    = LMKIND_Marker;
        within_xgl_list = true;
        return;
    }
    else if (IsXG_END(lump_p->name))
    {
        if (!within_xgl_list)
            I_Warning("Unexpected XG_END marker in wad.\n");

        lump_p->kind    = LMKIND_Marker;
        within_xgl_list = false;
        return;
    }

    // ignore zero size lumps or dummy markers
    if (lump_p->size == 0 || IsDummySF(lump_p->name))
        return;

    if (wad == nullptr)
        return;

    if (within_sprite_list)
    {
        lump_p->kind = LMKIND_Sprite;
        wad->sprite_lumps.push_back(lump);
    }

    if (within_flat_list)
    {
        lump_p->kind = LMKIND_Flat;
        wad->flat_lumps.push_back(lump);
    }

    if (within_patch_list)
    {
        lump_p->kind = LMKIND_Patch;
        wad->patch_lumps.push_back(lump);
    }

    if (within_colmap_list)
    {
        lump_p->kind = LMKIND_Colmap;
        wad->colmap_lumps.push_back(lump);
    }

    if (within_tex_list)
    {
        lump_p->kind = LMKIND_TX;
        wad->tx_lumps.push_back(lump);
    }

    if (within_hires_list)
    {
        lump_p->kind = LMKIND_HiRes;
        wad->hires_lumps.push_back(lump);
    }

    if (within_xgl_list)
    {
        lump_p->kind = LMKIND_XGL;
        wad->xgl_lumps.push_back(lump);
    }
}

//
// CheckForLevel
//
// Tests whether the current lump is a level marker (MAP03, E1M7, etc).
// Because EDGE supports arbitrary names (via DDF), we look at the
// sequence of lumps _after_ this one, which works well since their
// order is fixed (e.g. THINGS is always first).
//
static void CheckForLevel(wad_file_c *wad, int lump, const char *name, const raw_wad_entry_t *raw, int remaining)
{
    // we only test four lumps (it is enough), but fewer definitely
    // means this is not a level marker.
    if (remaining < 2)
        return;

    if (strncmp(raw[1].name, "THINGS", 8) == 0 && strncmp(raw[2].name, "LINEDEFS", 8) == 0 &&
        strncmp(raw[3].name, "SIDEDEFS", 8) == 0 && strncmp(raw[4].name, "VERTEXES", 8) == 0)
    {
        if (strlen(name) > 5)
        {
            I_Warning("Level name '%s' is too long !!\n", name);
            return;
        }

        // check for duplicates (Slige sometimes does this)
        if (wad->HasLevel(name))
        {
            I_Warning("Duplicate level '%s' ignored.\n", name);
            return;
        }

        wad->level_markers.push_back(lump);
        return;
    }

    // handle GL nodes here too

    if (strncmp(raw[1].name, "GL_VERT", 8) == 0 && strncmp(raw[2].name, "GL_SEGS", 8) == 0 &&
        strncmp(raw[3].name, "GL_SSECT", 8) == 0 && strncmp(raw[4].name, "GL_NODES", 8) == 0)
    {
        wad->level_markers.push_back(lump);
        return;
    }

    // UDMF
    // 1.1 Doom/Heretic namespaces supported at the moment

    if (strncmp(raw[1].name, "TEXTMAP", 8) == 0)
    {
        wad->level_markers.push_back(lump);
        return;
    }
}

int W_CheckForUniqueLumps(epi::File *file)
{
    int              length;
    raw_wad_header_t header;

    if (!file)
    {
        I_Warning("W_CheckForUniqueLumps: Received null file_c pointer!\n");
        return -1;
    }

    // WAD file
    // TODO: handle Read failure
    file->Read(&header, sizeof(raw_wad_header_t));
    header.num_entries        = AlignedLittleEndianS32(header.num_entries);
    header.dir_start          = AlignedLittleEndianS32(header.dir_start);
    length                    = header.num_entries * sizeof(raw_wad_entry_t);
    raw_wad_entry_t *raw_info = new raw_wad_entry_t[header.num_entries];
    file->Seek(header.dir_start, epi::File::kSeekpointStart);
    file->Read(raw_info, length);

    for (size_t check = 0; check < game_checker.size(); check++)
    {
        game_check_t gamecheck = game_checker[check];

        // Do not require IWAD header if loading Harmony, REKKR, BFG Edition WADs, Chex Quest or a custom standalone
        // IWAD
        if (epi::StringPrefixCompare(header.identification, "IWAD") != 0 &&
            epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "DMENUPIC") != 0 &&
            epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "REKCREDS") != 0 &&
            epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "0HAWK01") != 0 &&
            epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "EDGEGAME") != 0 &&
            epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "ENDOOM") != 0)
        {
            continue;
        }

        bool lump1_found = false;
        bool lump2_found = false;

        for (size_t i = 0; i < header.num_entries; i++)
        {
            raw_wad_entry_t &entry = raw_info[i];

            if (epi::StringCompareMax(gamecheck.unique_lumps[0], entry.name,
                             gamecheck.unique_lumps[0].size() < 8 ? gamecheck.unique_lumps[0].size() : 8) == 0)
            {
                // EDGEGAME is the only lump needed for custom standalones
                if (epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "EDGEGAME") == 0)
                {
                    delete[] raw_info;
                    file->Seek(0, epi::File::kSeekpointStart);
                    return check;
                }
                // Either really smart or really dumb Chex Quest detection method
                else if (epi::StringCaseCompareASCII(gamecheck.unique_lumps[0], "ENDOOM") == 0)
                {
                    SYS_ASSERT(entry.size == 4000);
                    file->Seek(entry.pos, epi::File::kSeekpointStart);
                    uint8_t *endoom = new uint8_t[entry.size];
                    file->Read(endoom, entry.size);
                    if (endoom[1026] == 'c' && endoom[1028] == 'h' && endoom[1030] == 'e' && endoom[1032] == 'x' &&
                        endoom[1034] == 'q' && endoom[1036] == 'u' && endoom[1038] == 'e' && endoom[1040] == 's' &&
                        endoom[1042] == 't')
                    {
                        lump1_found = true;
                    }
                    delete[] endoom;
                }
                else
                    lump1_found = true;
            }
            if (epi::StringCompareMax(gamecheck.unique_lumps[1], entry.name,
                             gamecheck.unique_lumps[1].size() < 8 ? gamecheck.unique_lumps[1].size() : 8) == 0)
                lump2_found = true;
        }

        if (lump1_found && lump2_found)
        {
            delete[] raw_info;
            file->Seek(0, epi::File::kSeekpointStart);
            return check;
        }
    }

    delete[] raw_info;
    file->Seek(0, epi::File::kSeekpointStart);
    return -1;
}

void ProcessFixersForWad(data_file_c *df)
{
    // Special handling for Doom 2 BFG Edition
    if (df->kind == FLKIND_IWad || df->kind == FLKIND_IPackWAD)
    {
        if (W_CheckNumForName("MAP33") > -1 && W_CheckNumForName("DMENUPIC") > -1)
        {
            std::string fix_path = epi::PathAppend(game_dir, "edge_fixes/doom2_bfg.epk");
            if (epi::TestFileAccess(fix_path))
            {
                W_AddPending(fix_path, FLKIND_EPK);

                I_Printf("WADFIXES: Applying fixes for Doom 2 BFG Edition\n");
            }
            else
                I_Warning("WADFIXES: Doom 2 BFG Edition detected, but fix not found in edge_fixes directory!\n");
            return;
        }
    }

    std::string fix_checker;

    fix_checker = df->wad->md5_string;

    if (fix_checker.empty())
        return;

    for (size_t i = 0; i < fixdefs.size(); i++)
    {
        if (epi::StringCaseCompareASCII(fix_checker, fixdefs[i]->md5_string) == 0)
        {
            std::string fix_path = epi::PathAppend(game_dir, "edge_fixes");
            fix_path = epi::PathAppend(fix_path, fix_checker.append(".epk"));
            if (epi::TestFileAccess(fix_path))
            {
                W_AddPending(fix_path, FLKIND_EPK);

                I_Printf("WADFIXES: Applying fixes for %s\n", fixdefs[i]->name.c_str());
            }
            else
            {
                I_Warning("WADFIXES: %s defined, but no fix WAD located in edge_fixes!\n", fixdefs[i]->name.c_str());
                return;
            }
        }
    }
}

void ProcessDehackedInWad(data_file_c *df)
{
    int deh_lump = df->wad->deh_lump;
    if (deh_lump < 0)
        return;

    const char *lump_name = lumpinfo[deh_lump].name;

    I_Printf("Converting [%s] lump in: %s\n", lump_name, df->name.c_str());

    int         length = -1;
    const uint8_t *data   = (const uint8_t *)W_LoadLump(deh_lump, &length);

    std::string bare_name = epi::GetFilename(df->name);

    std::string source = lump_name;
    source += " in ";
    source += bare_name;

    ConvertDehacked(data, length, source);

    delete[] data;
}

static void ProcessDDFInWad(data_file_c *df)
{
    std::string bare_filename = epi::GetFilename(df->name);

    for (size_t d = 0; d < kTotalDDFTypes; d++)
    {
        int lump = df->wad->ddf_lumps[d];

        if (lump >= 0)
        {
            I_Printf("Loading %s lump in %s\n", W_GetLumpName(lump), bare_filename.c_str());

            std::string data   = W_LoadString(lump);
            std::string source = W_GetLumpName(lump);

            source += " in ";
            source += bare_filename;

            DDF_AddFile((DDFType)d, data, source);
        }
    }
}

static void ProcessCoalInWad(data_file_c *df)
{
    std::string bare_filename = epi::GetFilename(df->name);

    wad_file_c *wad = df->wad;

    if (wad->coal_huds >= 0)
    {
        int lump = wad->coal_huds;

        VM_SetCoalDetected(true);

        std::string data   = W_LoadString(lump);
        std::string source = W_GetLumpName(lump);

        source += " in ";
        source += bare_filename;

        VM_AddScript(0, data, source);
    }
}

static void ProcessLuaInWad(data_file_c *df)
{
    std::string bare_filename = epi::GetFilename(df->name);

    wad_file_c *wad = df->wad;

    if (wad->lua_huds >= 0)
    {
        int lump = wad->lua_huds;       

        LUA_SetLuaHudDetected(true);

        std::string data   = W_LoadString(lump);
        std::string source = W_GetLumpName(lump);

        source += " in ";
        source += bare_filename;

        LUA_AddScript(data, source);
    }
}

static void ProcessBoomStuffInWad(data_file_c *df)
{
    // handle Boom's ANIMATED and SWITCHES lumps

    int animated = df->wad->animated;
    int switches = df->wad->switches;

    if (animated >= 0)
    {
        I_Printf("Loading ANIMATED from: %s\n", df->name.c_str());

        int   length = -1;
        uint8_t *data   = W_LoadLump(animated, &length);

        DDF_ConvertANIMATED(data, length);
        delete[] data;
    }

    if (switches >= 0)
    {
        I_Printf("Loading SWITCHES from: %s\n", df->name.c_str());

        int   length = -1;
        uint8_t *data   = W_LoadLump(switches, &length);

        DDF_ConvertSWITCHES(data, length);
        delete[] data;
    }

    // handle BOOM Colourmaps (between C_START and C_END)
    for (int lump : df->wad->colmap_lumps)
    {
        DDF_AddRawColourmap(W_GetLumpName(lump), W_LumpLength(lump), nullptr);
    }
}

void ProcessWad(data_file_c *df, size_t file_index)
{
    wad_file_c *wad = new wad_file_c();
    df->wad         = wad;

    // reset the sprite/flat/patch list stuff
    within_sprite_list = within_flat_list = false;
    within_patch_list = within_colmap_list = false;
    within_tex_list = within_hires_list = false;
    within_xgl_list                     = false;

    raw_wad_header_t header;

    epi::File *file = df->file;

    // TODO: handle Read failure
    file->Read(&header, sizeof(raw_wad_header_t));

    if (strncmp(header.identification, "IWAD", 4) != 0)
    {
        // Homebrew levels?
        if (strncmp(header.identification, "PWAD", 4) != 0)
        {
            I_Error("Wad file %s doesn't have IWAD or PWAD id\n", df->name.c_str());
        }
    }

    header.num_entries = AlignedLittleEndianS32(header.num_entries);
    header.dir_start   = AlignedLittleEndianS32(header.dir_start);

    size_t length = header.num_entries * sizeof(raw_wad_entry_t);

    raw_wad_entry_t *raw_info = new raw_wad_entry_t[header.num_entries];

    file->Seek(header.dir_start, epi::File::kSeekpointStart);
    // TODO: handle Read failure
    file->Read(raw_info, length);

    int startlump = (int)lumpinfo.size();

    for (size_t i = 0; i < header.num_entries; i++)
    {
        raw_wad_entry_t &entry = raw_info[i];

        bool allow_ddf =
            (df->kind == FLKIND_EWad || (df->kind == FLKIND_IWad && epi::StringCompare(game_base, "CUSTOM") == 0) ||
             df->kind == FLKIND_PWad || df->kind == FLKIND_PackWAD || df->kind == FLKIND_IPK ||
             df->kind == FLKIND_IFolder);

        AddLump(df, entry.name, AlignedLittleEndianS32(entry.pos), AlignedLittleEndianS32(entry.size), (int)file_index, allow_ddf);

        // this will be uppercase
        const char *level_name = lumpinfo[startlump + i].name;

        CheckForLevel(wad, startlump + i, level_name, &entry, header.num_entries - 1 - i);
    }

    // check for unclosed sprite/flat/patch lists
    const char *filename = df->name.c_str();
    if (within_sprite_list)
        I_Warning("Missing S_END marker in %s.\n", filename);
    if (within_flat_list)
        I_Warning("Missing F_END marker in %s.\n", filename);
    if (within_patch_list)
        I_Warning("Missing P_END marker in %s.\n", filename);
    if (within_colmap_list)
        I_Warning("Missing C_END marker in %s.\n", filename);
    if (within_tex_list)
        I_Warning("Missing TX_END marker in %s.\n", filename);
    if (within_hires_list)
        I_Warning("Missing HI_END marker in %s.\n", filename);
    if (within_xgl_list)
        I_Warning("Missing XG_END marker in %s.\n", filename);

    SortLumps();

    SortSpriteLumps(wad);

    // compute MD5 hash over wad directory
    epi::MD5Hash dir_md5;
    dir_md5.Compute((const uint8_t *)raw_info, length);

    wad->md5_string = dir_md5.ToString();

    I_Debugf("   md5hash = %s\n", wad->md5_string.c_str());

    delete[] raw_info;

    ProcessDehackedInWad(df);
    ProcessBoomStuffInWad(df);
    ProcessDDFInWad(df);
    ProcessCoalInWad(df);
    ProcessLuaInWad(df);
}

std::string W_BuildNodesForWad(data_file_c *df)
{
    if (df->wad->level_markers.empty())
        return "";

    // determine XWA filename in the cache
    std::string cache_name = epi::GetStem(df->name);
    cache_name += "-";
    cache_name += df->wad->md5_string;
    cache_name += ".xwa";

    std::string xwa_filename = epi::PathAppend(cache_dir, cache_name);

    I_Debugf("XWA filename: %s\n", xwa_filename.c_str());

    // check whether an XWA file for this map exists in the cache
    bool exists = epi::TestFileAccess(xwa_filename);

    if (!exists)
    {
        I_Printf("Building XGL nodes for: %s\n", df->name.c_str());

        I_Debugf("# source: '%s'\n", df->name.c_str());
        I_Debugf("#   dest: '%s'\n", xwa_filename.c_str());

        ajbsp::ResetInfo();

        epi::File *mem_wad    = nullptr;
        uint8_t     *raw_wad    = nullptr;
        int          raw_length = 0;

        if (df->kind == FLKIND_PackWAD)
        {
            mem_wad    = W_OpenPackFile(df->name);
            raw_length = mem_wad->GetLength();
            raw_wad    = mem_wad->LoadIntoMemory();
            ajbsp::OpenMem(df->name, raw_wad, raw_length);
        }
        else
            ajbsp::OpenWad(df->name);

        ajbsp::CreateXWA(xwa_filename);

        for (int i = 0; i < ajbsp::LevelsInWad(); i++)
            ajbsp::BuildLevel(i);

        ajbsp::FinishXWA();
        ajbsp::CloseWad();

        if (df->kind == FLKIND_PackWAD)
        {
            delete[] raw_wad;
            delete mem_wad;
        }

        I_Debugf("AJ_BuildNodes: FINISHED\n");

        epi::SyncFilesystem();
    }

    return xwa_filename;
}

void W_ReadUMAPINFOLumps(void)
{
    for (auto df : data_files)
    {
        if (df->wad)
        {
            if (df->wad->umapinfo_lump < 0)
                continue;
            else
            {
                L_WriteDebug("Parsing UMAPINFO lump in %s\n", df->name.c_str());
                Parse_UMAPINFO(W_LoadString(df->wad->umapinfo_lump));
            }
        }
        else if (df->pack)
        {
            if (!Pack_FindFile(df->pack, "UMAPINFO.txt"))
                continue;
            else
            {
                L_WriteDebug("Parsing UMAPINFO.txt in %s\n", df->name.c_str());
                epi::File *uinfo = Pack_FileOpen(df->pack, "UMAPINFO.txt");
                if (uinfo)
                {
                    Parse_UMAPINFO(uinfo->ReadText());
                    delete uinfo;
                }
                else
                    continue;
            }
        }
        else // ???
            continue;

        unsigned int i;
        for (i = 0; i < Maps.mapcount; i++)
        {
            std::string mapname = Maps.maps[i].mapname;
            epi::StringUpperASCII(mapname);
            // Check that the name adheres to either EXMX or MAPXX format per the standard
            if (epi::StringPrefixCaseCompareASCII(mapname, "MAP") == 0)
            {
                for (auto c : mapname.substr(3))
                {
                    if (!epi::IsDigitASCII(c))
                        I_Error("UMAPINFO: Bad map name: %s!\n", mapname.c_str());
                }
            }
            else if (mapname.size() > 3 && mapname[0] == 'E' && mapname[2] == 'M')
            {
                if (!epi::IsDigitASCII(mapname[1]))
                    I_Error("UMAPINFO: Bad map name: %s!\n", mapname.c_str());
                for (auto c : mapname.substr(3))
                {
                    if (!epi::IsDigitASCII(c))
                        I_Error("UMAPINFO: Bad map name: %s!\n", mapname.c_str());
                }
            }
            else
                I_Error("UMAPINFO: Bad map name: %s!\n", mapname.c_str());

            mapdef_c *temp_level = mapdefs.Lookup(mapname.c_str());
            if (!temp_level)
            {
                temp_level       = new mapdef_c;
                temp_level->name = mapname;
                temp_level->lump = mapname;
                mapdefs.push_back(temp_level);
            }

            if (Maps.maps[i].levelpic[0])
            {
                temp_level->namegraphic = Maps.maps[i].levelpic;
                epi::StringUpperASCII(temp_level->namegraphic);
            }

            if (Maps.maps[i].skytexture[0])
            {
                temp_level->sky = Maps.maps[i].skytexture;
                epi::StringUpperASCII(temp_level->sky);
            }

            if (Maps.maps[i].levelname)
            {
                std::string temp_ref   = epi::StringFormat("%sDesc", Maps.maps[i].mapname);
                std::string temp_value = epi::StringFormat(" %s ", Maps.maps[i].levelname);
                language.AddOrReplace(temp_ref.c_str(), temp_value.c_str());
                temp_level->description = temp_ref;
            }

            if (Maps.maps[i].authorname)
            {
                temp_level->author = Maps.maps[i].authorname;
            }

            if (Maps.maps[i].music[0])
            {
                int val = 0;
                val     = playlist.FindLast(Maps.maps[i].music);
                if (val != -1) // we already have it
                {
                    temp_level->music = val;
                }
                else // we need to add it
                {
                    static pl_entry_c *dynamic_plentry;
                    dynamic_plentry           = new pl_entry_c;
                    dynamic_plentry->number   = playlist.FindFree();
                    dynamic_plentry->info     = Maps.maps[i].music;
                    dynamic_plentry->type     = MUS_UNKNOWN; // MUS_MUS
                    dynamic_plentry->infotype = MUSINF_LUMP;
                    temp_level->music         = dynamic_plentry->number;
                    playlist.push_back(dynamic_plentry);
                }
            }

            if (Maps.maps[i].nextmap[0])
            {
                temp_level->nextmapname = Maps.maps[i].nextmap;
                epi::StringUpperASCII(temp_level->nextmapname);
            }

            if (Maps.maps[i].intertext)
            {
                if (!epi::StringCaseCompareASCII(temp_level->nextmapname, "MAP07"))
                {
                    // Clear out some of our defaults on certain maps
                    mapdef_c *conflict_level = mapdefs.Lookup("MAP07");
                    if (conflict_level)
                    {
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                }
                if (!epi::StringCaseCompareASCII(temp_level->nextmapname, "MAP21"))
                {
                    // Clear out some of our defaults on certain maps
                    mapdef_c *conflict_level = mapdefs.Lookup("MAP21");
                    if (conflict_level)
                    {
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                }
                if (!epi::StringCaseCompareASCII(temp_level->nextmapname, "MAP31"))
                {
                    // Clear out some of our defaults on certain maps
                    mapdef_c *conflict_level = mapdefs.Lookup("MAP31");
                    if (conflict_level)
                    {
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                }
                if (!epi::StringCaseCompareASCII(temp_level->nextmapname, "MAP32"))
                {
                    // Clear out some of our defaults on certain maps
                    mapdef_c *conflict_level = mapdefs.Lookup("MAP32");
                    if (conflict_level)
                    {
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                }

                if (epi::StringCaseCompareASCII(Maps.maps[i].intertext, "clear") == 0)
                {
                    temp_level->f_end.text.clear();
                    temp_level->f_end.text_flat.clear();
                }
                else
                {
                    std::string temp_ref   = epi::StringFormat("%sINTERTEXT", Maps.maps[i].mapname);
                    std::string temp_value = epi::StringFormat(" %s ", Maps.maps[i].intertext);
                    language.AddOrReplace(temp_ref.c_str(), temp_value.c_str());
                    temp_level->f_end.text    = temp_ref;
                    temp_level->f_end.picwait = 350; // 10 seconds
                }

                if (Maps.maps[i].interbackdrop[0])
                {
                    const image_c *rim;

                    std::string ibd_lookup = Maps.maps[i].interbackdrop;
                    epi::StringUpperASCII(ibd_lookup);

                    rim = W_ImageLookup(ibd_lookup.c_str(), INS_Flat, ILF_Null);

                    if (!rim) // no flat
                    {
                        rim = W_ImageLookup(ibd_lookup.c_str(), INS_Graphic, ILF_Null);

                        if (!rim)                                     // no graphic
                            temp_level->f_end.text_flat = "FLOOR4_8"; // should not happen
                        else                                          // background is a graphic
                            temp_level->f_end.text_back = ibd_lookup;
                    }
                    else // background is a flat
                    {
                        temp_level->f_end.text_flat = ibd_lookup;
                    }
                }
            }

            if (Maps.maps[i].intermusic[0])
            {
                int val = 0;
                val     = playlist.FindLast(Maps.maps[i].intermusic);
                if (val != -1) // we already have it
                {
                    temp_level->f_end.music = val;
                }
                else // we need to add it
                {
                    static pl_entry_c *dynamic_plentry;
                    dynamic_plentry           = new pl_entry_c;
                    dynamic_plentry->number   = playlist.FindFree();
                    dynamic_plentry->info     = Maps.maps[i].intermusic;
                    dynamic_plentry->type     = MUS_UNKNOWN; // MUS_MUS
                    dynamic_plentry->infotype = MUSINF_LUMP;
                    temp_level->f_end.music   = dynamic_plentry->number;
                    playlist.push_back(dynamic_plentry);
                }
            }

            if (Maps.maps[i].nextsecret[0])
            {
                temp_level->secretmapname = Maps.maps[i].nextsecret;
                epi::StringUpperASCII(temp_level->secretmapname);
                if (Maps.maps[i].intertextsecret)
                {

                    if (!epi::StringCaseCompareASCII(temp_level->secretmapname, "MAP07"))
                    {
                        // Clear out some of our defaults on certain maps
                        mapdef_c *conflict_level = mapdefs.Lookup("MAP07");
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                    if (!epi::StringCaseCompareASCII(temp_level->secretmapname, "MAP21"))
                    {
                        // Clear out some of our defaults on certain maps
                        mapdef_c *conflict_level = mapdefs.Lookup("MAP21");
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                    if (!epi::StringCaseCompareASCII(temp_level->secretmapname, "MAP31"))
                    {
                        // Clear out some of our defaults on certain maps
                        mapdef_c *conflict_level = mapdefs.Lookup("MAP31");
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }
                    if (!epi::StringCaseCompareASCII(temp_level->secretmapname, "MAP32"))
                    {
                        // Clear out some of our defaults on certain maps
                        mapdef_c *conflict_level = mapdefs.Lookup("MAP32");
                        conflict_level->f_pre.text.clear();
                        conflict_level->f_pre.text_flat.clear();
                    }

                    mapdef_c *secret_level = mapdefs.Lookup(temp_level->secretmapname.c_str());
                    if (!secret_level)
                    {
                        secret_level       = new mapdef_c;
                        secret_level->name = Maps.maps[i].nextsecret;
                        epi::StringUpperASCII(secret_level->name);
                        secret_level->lump = Maps.maps[i].nextsecret;
                        epi::StringUpperASCII(secret_level->lump);
                        mapdefs.push_back(secret_level);
                    }

                    if (epi::StringCaseCompareASCII(Maps.maps[i].intertextsecret, "clear") == 0)
                    {
                        secret_level->f_pre.text.clear();
                        secret_level->f_pre.text_flat.clear();
                    }
                    else
                    {
                        std::string temp_ref   = epi::StringFormat("%sPRETEXT", secret_level->name.c_str());
                        std::string temp_value = epi::StringFormat(" %s ", Maps.maps[i].intertextsecret);
                        language.AddOrReplace(temp_ref.c_str(), temp_value.c_str());

                        // hack for shitty dbp shennanigans :/
                        if (temp_level->nextmapname == temp_level->secretmapname)
                        {
                            temp_level->f_end.text    = temp_ref;
                            temp_level->f_end.picwait = 700; // 20 seconds

                            if (Maps.maps[i].interbackdrop[0])
                            {
                                const image_c *rim;
                                std::string    ibd_lookup = Maps.maps[i].interbackdrop;
                                epi::StringUpperASCII(ibd_lookup);

                                rim = W_ImageLookup(ibd_lookup.c_str(), INS_Flat, ILF_Null);

                                if (!rim) // no flat
                                {
                                    rim = W_ImageLookup(ibd_lookup.c_str(), INS_Graphic, ILF_Null);

                                    if (!rim)                                     // no graphic
                                        temp_level->f_end.text_flat = "FLOOR4_8"; // should not happen
                                    else                                          // background is a graphic
                                        temp_level->f_end.text_back = ibd_lookup;
                                }
                                else // background is a flat
                                {
                                    temp_level->f_end.text_flat = ibd_lookup;
                                }
                            }
                        }
                        else
                        {
                            secret_level->f_pre.text    = temp_ref;
                            secret_level->f_pre.picwait = 700; // 20 seconds
                            if (temp_level->f_end.music)
                                secret_level->f_pre.music = temp_level->f_end.music;

                            if (Maps.maps[i].interbackdrop[0])
                            {
                                const image_c *rim;
                                std::string    ibd_lookup = Maps.maps[i].interbackdrop;
                                epi::StringUpperASCII(ibd_lookup);

                                rim = W_ImageLookup(ibd_lookup.c_str(), INS_Flat, ILF_Null);

                                if (!rim) // no flat
                                {
                                    rim = W_ImageLookup(ibd_lookup.c_str(), INS_Graphic, ILF_Null);

                                    if (!rim)                                       // no graphic
                                        secret_level->f_pre.text_flat = "FLOOR4_8"; // should not happen
                                    else                                            // background is a graphic
                                        secret_level->f_pre.text_back = ibd_lookup;
                                }
                                else // background is a flat
                                {
                                    secret_level->f_pre.text_flat = ibd_lookup;
                                }
                            }
                        }
                    }
                }
            }

            if (Maps.maps[i].exitpic[0])
            {
                temp_level->leavingbggraphic = Maps.maps[i].exitpic;
                epi::StringUpperASCII(temp_level->leavingbggraphic);
            }

            if (Maps.maps[i].enterpic[0])
            {
                temp_level->enteringbggraphic = Maps.maps[i].enterpic;
                epi::StringUpperASCII(temp_level->enteringbggraphic);
            }

            if (Maps.maps[i].endpic[0])
            {
                temp_level->nextmapname.clear();
                temp_level->f_end.pics.clear();
                temp_level->f_end.pics.push_back(Maps.maps[i].endpic);
                epi::StringUpperASCII(temp_level->f_end.pics.back());
                temp_level->f_end.picwait = INT_MAX; // Stay on endpic for now
            }

            if (Maps.maps[i].dobunny)
            {
                temp_level->nextmapname.clear();
                temp_level->f_end.dobunny = true;
            }

            if (Maps.maps[i].docast)
            {
                temp_level->nextmapname.clear();
                temp_level->f_end.docast = true;
            }

            if (Maps.maps[i].endgame)
            {
                temp_level->nextmapname.clear();
            }

            if (Maps.maps[i].partime > 0)
                temp_level->partime = Maps.maps[i].partime;

            if (Maps.maps[i].numbossactions == -1) // "clear" directive
                RAD_ClearWUDsByMap(Maps.maps[i].mapname);
            else if (Maps.maps[i].bossactions && Maps.maps[i].numbossactions >= 1)
            {
                // The UMAPINFO spec seems to suggest that any custom actions should
                // invalidate previous death triggers for the map in question
                RAD_ClearWUDsByMap(Maps.maps[i].mapname);
                std::string ba_rts = "// UMAPINFO SCRIPTS\n\n";
                for (int a = 0; a < Maps.maps[i].numbossactions; a++)
                {
                    for (size_t m = 0; m < mobjtypes.size(); m++)
                    {
                        if (mobjtypes[m]->number == Maps.maps[i].bossactions[a].type)
                        {
                            ba_rts.append(epi::StringFormat("START_MAP %s\n", Maps.maps[i].mapname));
                            ba_rts.append("  RADIUS_TRIGGER 0 0 -1\n");
                            ba_rts.append(epi::StringFormat("    WAIT_UNTIL_DEAD %s\n", mobjtypes[m]->name.c_str()));
                            ba_rts.append(epi::StringFormat("    ACTIVATE_LINETYPE %d %d\n",
                                                        Maps.maps[i].bossactions[a].special,
                                                        Maps.maps[i].bossactions[a].tag));
                            ba_rts.append("  END_RADIUS_TRIGGER\n");
                            ba_rts.append("END_MAP\n\n");
                        }
                    }
                }
                RAD_ReadScript(ba_rts, "UMAPINFO");
            }

            // If a TEMPEPI gamedef had to be created, grab some details from the
            // first valid gamedef iterating through gamedefs in reverse order
            if (temp_level->episode_name == "TEMPEPI")
            {
                for (int g = gamedefs.size() - 1; g >= 0; g--)
                {
                    if (gamedefs[g]->name != "TEMPEPI" && epi::StringCaseCompareMaxASCII(gamedefs[g]->firstmap, temp_level->name, 3) == 0)
                    {
                        if (atoi(gamedefs[g]->firstmap.substr(3).c_str()) > atoi(temp_level->name.substr(3).c_str()))
                            continue;
                        else
                        {
                            temp_level->episode->background = gamedefs[g]->background;
                            temp_level->episode->music      = gamedefs[g]->music;
                            temp_level->episode->titlemusic = gamedefs[g]->titlemusic;
                            temp_level->episode->titlepics  = gamedefs[g]->titlepics;
                            temp_level->episode->titletics  = gamedefs[g]->titletics;
                            temp_level->episode->percent    = gamedefs[g]->percent;
                            temp_level->episode->done       = gamedefs[g]->done;
                            temp_level->episode->accel_snd  = gamedefs[g]->accel_snd;
                            break;
                        }
                    }
                }
            }
            else // Validate episode entry to make sure it wasn't renamed or removed
            {
                bool good_epi = false;
                for (auto g : gamedefs)
                {
                    if (temp_level->episode_name == g->name)
                    {
                        good_epi = true;
                        break;
                    }
                }
                if (!good_epi) // Find a suitable episode
                {
                    for (int g = gamedefs.size() - 1; g >= 0; g--)
                    {
                        if (epi::StringCaseCompareMaxASCII(gamedefs[g]->firstmap, temp_level->name, 3) == 0)
                        {
                            if (atoi(gamedefs[g]->firstmap.substr(3).c_str()) > atoi(temp_level->name.substr(3).c_str()))
                                continue;
                            else
                            {
                                temp_level->episode      = gamedefs[g];
                                temp_level->episode_name = gamedefs[g]->name;
                                good_epi                 = true;
                                break;
                            }
                        }
                    }
                }
                if (!good_epi)
                    I_Error("UMAPINFO: No valid episode found for level %s\n", temp_level->name.c_str());
            }
            // Validate important things
            if (temp_level->sky.empty())
            {
                if (epi::StringPrefixCaseCompareASCII(temp_level->name, "MAP") == 0)
                {
                    int levnum = atoi(temp_level->name.substr(3).c_str());
                    if (levnum < 12)
                        temp_level->sky = "SKY1";
                    else if (levnum < 21)
                        temp_level->sky = "SKY2";
                    else
                        temp_level->sky = "SKY3";
                }
                else
                {
                    int epnum = atoi(temp_level->name.substr(1, 1).c_str());
                    if (epnum == 1)
                        temp_level->sky = "SKY1";
                    else if (epnum == 2)
                        temp_level->sky = "SKY2";
                    else if (epnum == 3)
                        temp_level->sky = "SKY3";
                    else
                        temp_level->sky = "SKY4";
                }
            }
            // Clear pre_text for this map if it is an episode's starting map
            for (int g = gamedefs.size() - 1; g >= 0; g--)
            {
                if (epi::StringCaseCompareASCII(gamedefs[g]->firstmap, temp_level->name) == 0)
                {
                    temp_level->f_pre.text.clear();
                    temp_level->f_pre.text_flat.clear();
                    break;
                }
            }
        }
        FreeMapList();
    }
}

epi::File *W_OpenLump(int lump)
{
    SYS_ASSERT(W_VerifyLump(lump));

    lumpinfo_t *l = &lumpinfo[lump];

    data_file_c *df = data_files[l->file];

    SYS_ASSERT(df->file);

    return new epi::SubFile(df->file, l->position, l->size);
}

epi::File *W_OpenLump(const char *name)
{
    return W_OpenLump(W_GetNumForName(name));
}

//
// W_GetPaletteForLump
//
// Returns the palette lump that should be used for the given lump
// (presumably an image), otherwise -1 (indicating that the global
// palette should be used).
//
// NOTE: when the same WAD as the lump does not contain a palette,
// there are two possibilities: search backwards for the "closest"
// palette, or simply return -1.  Neither one is ideal, though I tend
// to think that searching backwards is more intuitive.
//
// NOTE 2: the palette_datafile stuff is there so we always return -1
// for the "GLOBAL" palette.
//
int W_GetPaletteForLump(int lump)
{
    SYS_ASSERT(W_VerifyLump(lump));

    return W_CheckNumForName("PLAYPAL");
}

static int QuickFindLumpMap(const char *buf)
{
    int low  = 0;
    int high = (int)lumpinfo.size() - 1;

    if (high < 0)
        return -1;

    while (low <= high)
    {
        int i   = (low + high) / 2;
        int cmp = LUMP_MAP_CMP(i);

        if (cmp == 0)
        {
            // jump to first matching name
            while (i > 0 && LUMP_MAP_CMP(i - 1) == 0)
                i--;

            return i;
        }

        if (cmp < 0)
        {
            // mid point < buf, so look in upper half
            low = i + 1;
        }
        else
        {
            // mid point > buf, so look in lower half
            high = i - 1;
        }
    }

    // not found (nothing has that name)
    return -1;
}

//
// W_CheckNumForName
//
// Returns -1 if name not found.
//
// -ACB- 1999/09/18 Added name to error message
//
int W_CheckNumForName(const char *name)
{
    int  i;
    char buf[9];

    if (strlen(name) > 8)
    {
        I_Debugf("W_CheckNumForName: Name '%s' longer than 8 chars!\n", name);
        return -1;
    }

    for (i = 0; name[i]; i++)
    {
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    i = QuickFindLumpMap(buf);

    if (i < 0)
        return -1; // not found

    return sortedlumps[i];
}

//
// W_CheckFileNumForName
//
// Returns data_files index or -1 if name not found.
//
//
int W_CheckFileNumForName(const char *name)
{
    int  i;
    char buf[9];

    if (strlen(name) > 8)
    {
        I_Debugf("W_CheckNumForName: Name '%s' longer than 8 chars!\n", name);
        return -1;
    }

    for (i = 0; name[i]; i++)
    {
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    i = QuickFindLumpMap(buf);

    if (i < 0)
        return -1; // not found

    return lumpinfo[sortedlumps[i]].file;
}

int W_CheckNumForName_GFX(const char *name)
{
    // this looks for a graphic lump, skipping anything which would
    // not be suitable (especially flats and HIRES replacements).

    int  i;
    char buf[9];

    if (strlen(name) > 8)
    {
        I_Debugf("W_CheckNumForName: Name '%s' longer than 8 chars!\n", name);
        return -1;
    }

    for (i = 0; name[i]; i++)
    {
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    // search backwards
    for (i = (int)lumpinfo.size() - 1; i >= 0; i--)
    {
        if (lumpinfo[i].kind == LMKIND_Normal || lumpinfo[i].kind == LMKIND_Sprite || lumpinfo[i].kind == LMKIND_Patch)
        {
            if (strncmp(lumpinfo[i].name, buf, 8) == 0)
                return i;
        }
    }

    return -1; // not found
}

int W_CheckNumForName_XGL(const char *name)
{
    // limit search to stuff between XG_START and XG_END.

    int  i;
    char buf[9];

    if (strlen(name) > 8)
    {
        I_Warning("W_CheckNumForName: Name '%s' longer than 8 chars!\n", name);
        return -1;
    }

    for (i = 0; name[i]; i++)
    {
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    // search backwards
    for (i = (int)lumpinfo.size() - 1; i >= 0; i--)
    {
        if (lumpinfo[i].kind == LMKIND_XGL)
            if (strncmp(lumpinfo[i].name, buf, 8) == 0)
                return i;
    }

    return -1; // not found
}

int W_CheckNumForName_MAP(const char *name)
{
    // avoids anything in XGL namespace

    int  i;
    char buf[9];

    if (strlen(name) > 8)
    {
        I_Warning("W_CheckNumForName: Name '%s' longer than 8 chars!\n", name);
        return -1;
    }

    for (i = 0; name[i]; i++)
    {
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    // search backwards
    for (i = (int)lumpinfo.size() - 1; i >= 0; i--)
    {
        if (lumpinfo[i].kind != LMKIND_XGL)
            if (strncmp(lumpinfo[i].name, buf, 8) == 0)
                return i;
    }

    return -1; // not found
}

//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName(const char *name)
{
    int i;

    if ((i = W_CheckNumForName(name)) == -1)
        I_Error("W_GetNumForName: \'%.8s\' not found!", name);

    return i;
}

//
// W_CheckNumForTexPatch
//
// Returns -1 if name not found.
//
// -AJA- 2004/06/24: Patches should be within the P_START/P_END markers,
//       so we should look there first.  Also we should never return a
//       flat as a tex-patch.
//
int W_CheckNumForTexPatch(const char *name)
{
    int  i;
    char buf[10];

    for (i = 0; name[i]; i++)
    {
#ifdef DEVELOPERS
        if (i > 8)
            I_Error("W_CheckNumForTexPatch: '%s' longer than 8 chars!", name);
#endif
        buf[i] = epi::ToUpperASCII(name[i]);
    }
    buf[i] = 0;

    i = QuickFindLumpMap(buf);

    if (i < 0)
        return -1; // not found

    for (; i < (int)lumpinfo.size() && LUMP_MAP_CMP(i) == 0; i++)
    {
        lumpinfo_t *L = &lumpinfo[sortedlumps[i]];

        if (L->kind == LMKIND_Patch || L->kind == LMKIND_Sprite || L->kind == LMKIND_Normal)
        {
            // allow LMKIND_Normal to support patches outside of the
            // P_START/END markers.  We especially want to disallow
            // flat and colourmap lumps.
            return sortedlumps[i];
        }
    }

    return -1; // nothing suitable
}

//
// W_VerifyLump
//
// Verifies that the given lump number is valid and has the given
// name.
//
// -AJA- 1999/11/26: written.
//
bool W_VerifyLump(int lump)
{
    return (lump >= 0) && (lump < (int)lumpinfo.size());
}

bool W_VerifyLumpName(int lump, const char *name)
{
    if (!W_VerifyLump(lump))
        return false;

    return (strncmp(lumpinfo[lump].name, name, 8) == 0);
}

//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength(int lump)
{
    if (!W_VerifyLump(lump))
        I_Error("W_LumpLength: %i >= numlumps", lump);

    return lumpinfo[lump].size;
}

//
// W_FindFlatSequence
//
// Returns the file number containing the sequence, or -1 if not
// found.  Search is from newest wad file to oldest wad file.
//
int W_FindFlatSequence(const char *start, const char *end, int *s_offset, int *e_offset)
{
    for (int file = (int)data_files.size() - 1; file >= 0; file--)
    {
        data_file_c *df  = data_files[file];
        wad_file_c  *wad = df->wad;

        if (wad == nullptr)
            continue;

        // look for start name
        int i;
        for (i = 0; i < (int)wad->flat_lumps.size(); i++)
        {
            if (strncmp(start, W_GetLumpName(wad->flat_lumps[i]), 8) == 0)
                break;
        }

        if (i >= (int)wad->flat_lumps.size())
            continue;

        (*s_offset) = i;

        // look for end name
        for (i++; i < (int)wad->flat_lumps.size(); i++)
        {
            if (strncmp(end, W_GetLumpName(wad->flat_lumps[i]), 8) == 0)
            {
                (*e_offset) = i;
                return file;
            }
        }
    }

    // not found
    return -1;
}

// returns nullptr for an empty list.
std::vector<int> *W_GetFlatList(int file)
{
    SYS_ASSERT(0 <= file && file < (int)data_files.size());

    data_file_c *df  = data_files[file];
    wad_file_c  *wad = df->wad;

    if (wad != nullptr)
        return &wad->flat_lumps;

    return nullptr;
}

std::vector<int> *W_GetSpriteList(int file)
{
    SYS_ASSERT(0 <= file && file < (int)data_files.size());

    data_file_c *df  = data_files[file];
    wad_file_c  *wad = df->wad;

    if (wad != nullptr)
        return &wad->sprite_lumps;

    return nullptr;
}

std::vector<int> *W_GetPatchList(int file)
{
    SYS_ASSERT(0 <= file && file < (int)data_files.size());

    data_file_c *df  = data_files[file];
    wad_file_c  *wad = df->wad;

    if (wad != nullptr)
        return &wad->patch_lumps;

    return nullptr;
}

int W_GetFileForLump(int lump)
{
    SYS_ASSERT(W_VerifyLump(lump));

    return lumpinfo[lump].file;
}

int W_GetKindForLump(int lump)
{
    SYS_ASSERT(W_VerifyLump(lump));

    return lumpinfo[lump].kind;
}

//
// Loads the lump into the given buffer,
// which must be >= W_LumpLength().
//
static void W_RawReadLump(int lump, void *dest)
{
    if (!W_VerifyLump(lump))
        I_Error("W_ReadLump: %i >= numlumps", lump);

    lumpinfo_t  *L  = &lumpinfo[lump];
    data_file_c *df = data_files[L->file];

    df->file->Seek(L->position, epi::File::kSeekpointStart);

    int c = df->file->Read(dest, L->size);

    if (c < L->size)
        I_Error("W_ReadLump: only read %i of %i on lump %i", c, L->size, lump);
}

//
// W_LoadLump
//
// Returns a copy of the lump (it is your responsibility to free it)
//
uint8_t *W_LoadLump(int lump, int *length)
{
    int w_length = W_LumpLength(lump);

    if (length != nullptr)
        *length = w_length;

    uint8_t *data = new uint8_t[w_length + 1];

    W_RawReadLump(lump, data);

    // zero-terminate, handy for text parsers
    data[w_length] = 0;

    return data;
}

uint8_t *W_LoadLump(const char *name, int *length)
{
    return W_LoadLump(W_GetNumForName(name), length);
}

std::string W_LoadString(int lump)
{
    // WISH: optimise this to remove temporary buffer
    int   length;
    uint8_t *data = W_LoadLump(lump, &length);

    std::string result((char *)data, length);

    delete[] data;

    return result;
}

std::string W_LoadString(const char *name)
{
    return W_LoadString(W_GetNumForName(name));
}

//
// W_GetLumpName
//
const char *W_GetLumpName(int lump)
{
    return lumpinfo[lump].name;
}

void W_ProcessTX_HI(void)
{
    // Add the textures that occur in between TX_START/TX_END markers

    // TODO: collect names, remove duplicates

    E_ProgressMessage("Adding standalone textures...");

    for (int file = 0; file < (int)data_files.size(); file++)
    {
        data_file_c *df  = data_files[file];
        wad_file_c  *wad = df->wad;

        if (wad == nullptr)
            continue;

        for (int i = 0; i < (int)wad->tx_lumps.size(); i++)
        {
            int lump = wad->tx_lumps[i];
            W_ImageAddTX(lump, W_GetLumpName(lump), false);
        }
    }

    E_ProgressMessage("Adding high-resolution textures...");

    // Add the textures that occur in between HI_START/HI_END markers

    for (int file = 0; file < (int)data_files.size(); file++)
    {
        data_file_c *df = data_files[file];

        if (df->wad)
        {
            for (int i = 0; i < (int)df->wad->hires_lumps.size(); i++)
            {
                int lump = df->wad->hires_lumps[i];
                W_ImageAddTX(lump, W_GetLumpName(lump), true);
            }
        }
        else if (df->pack)
        {
            Pack_ProcessHiresSubstitutions(df->pack, file);
        }
    }
}

static const char *LumpKindString(lump_kind_e kind)
{
    switch (kind)
    {
    case LMKIND_Normal:
        return "normal";
    case LMKIND_Marker:
        return "marker";
    case LMKIND_WadTex:
        return "wadtex";
    case LMKIND_DDFRTS:
        return "ddf";

    case LMKIND_TX:
        return "tx";
    case LMKIND_Colmap:
        return "cmap";
    case LMKIND_Flat:
        return "flat";
    case LMKIND_Sprite:
        return "sprite";
    case LMKIND_Patch:
        return "patch";
    case LMKIND_HiRes:
        return "hires";

    default:
        return "???";
    }
}

void W_ShowLumps(int for_file, const char *match)
{
    I_Printf("Lump list:\n");

    int total = 0;

    for (int i = 0; i < (int)lumpinfo.size(); i++)
    {
        lumpinfo_t *L = &lumpinfo[i];

        if (for_file >= 1 && L->file != for_file - 1)
            continue;

        if (match && *match)
            if (!strstr(L->name, match))
                continue;

        I_Printf(" %4d %-9s %2d %-6s %7d @ 0x%08x\n", i + 1, L->name, L->file + 1, LumpKindString(L->kind), L->size,
                 L->position);
        total++;
    }

    I_Printf("Total: %d\n", total);
}

int W_LoboFindSkyImage(int for_file, const char *match)
{
    int total = 0;

    for (int i = 0; i < (int)lumpinfo.size(); i++)
    {
        lumpinfo_t *L = &lumpinfo[i];

        if (for_file >= 1 && L->file != for_file - 1)
            continue;

        if (match && *match)
            if (!strstr(L->name, match))
                continue;

        switch (L->kind)
        {
        case LMKIND_Patch:
            /*I_Printf(" %4d %-9s %2d %-6s %7d @ 0x%08x\n",
             i+1, L->name,
             L->file+1, LumpKind_Strings[L->kind],
             L->size, L->position); */
            total++;
            break;
        case LMKIND_Normal:
            /*I_Printf(" %4d %-9s %2d %-6s %7d @ 0x%08x\n",
             i+1, L->name,
             L->file+1, LumpKind_Strings[L->kind],
             L->size, L->position); */
            total++;
            break;
        default: // Optional
            continue;
        }
    }

    I_Printf("FindSkyPatch: file %i,  match %s, count: %i\n", for_file, match, total);

    // I_Printf("Total: %d\n", total);
    return total;
}

static const char *UserSkyBoxName(const char *base, int face)
{
    static char       buffer[64];
    static const char letters[] = "NESWTB";

    sprintf(buffer, "%s_%c", base, letters[face]);
    return buffer;
}

// W_LoboDisableSkybox
//
// Check if a loaded pwad has a custom sky.
// If so, turn off our EWAD skybox.
//
// Returns true if found
bool W_LoboDisableSkybox(const char *ActualSky)
{
    bool           TurnOffSkyBox = false;
    const image_c *tempImage;
    int            filenum = -1;
    int            lumpnum = -1;

    // First we should try for "SKY1_N" type names but only
    // use it if it's in a pwad i.e. a users skybox
    tempImage = W_ImageLookup(UserSkyBoxName(ActualSky, 0), INS_Texture, ILF_Null);
    if (tempImage)
    {
        if (tempImage->source_type == IMSRC_User) // from images.ddf
        {
            lumpnum = W_CheckNumForName(tempImage->name.c_str());

            if (lumpnum != -1)
            {
                filenum = W_GetFileForLump(lumpnum);
            }

            if (filenum != -1) // make sure we actually have a file
            {
                // we only want pwads
                if (data_files[filenum]->kind == FLKIND_PWad || data_files[filenum]->kind == FLKIND_PackWAD)
                {
                    I_Debugf("SKYBOX: Sky is: %s. Type:%d lumpnum:%d filenum:%d \n", tempImage->name.c_str(),
                             tempImage->source_type, lumpnum, filenum);
                    TurnOffSkyBox = false;
                    return TurnOffSkyBox; // get out of here
                }
            }
        }
    }

    // If we're here then there are no user skyboxes.
    // Lets check for single texture ones instead.
    tempImage = W_ImageLookup(ActualSky, INS_Texture, ILF_Null);

    if (tempImage) // this should always be true but check just in case
    {
        if (tempImage->source_type == IMSRC_Texture) // Normal doom format sky
        {
            filenum = W_GetFileForLump(tempImage->source.texture.tdef->patches->patch);
        }
        else if (tempImage->source_type == IMSRC_User) // texture from images.ddf
        {
            I_Debugf("SKYBOX: Sky is: %s. Type:%d  \n", tempImage->name.c_str(), tempImage->source_type);
            TurnOffSkyBox = true; // turn off or not? hmmm...
            return TurnOffSkyBox;
        }
        else // could be a png or jpg i.e. TX_ or HI_
        {
            lumpnum = W_CheckNumForName(tempImage->name.c_str());
            // lumpnum = tempImage->source.graphic.lump;
            if (lumpnum != -1)
            {
                filenum = W_GetFileForLump(lumpnum);
            }
        }

        if (tempImage->source_type == IMSRC_Dummy) // probably a skybox?
        {
            TurnOffSkyBox = false;
        }

        if (filenum == 0) // it's the IWAD so we're done
        {
            TurnOffSkyBox = false;
        }

        if (filenum != -1) // make sure we actually have a file
        {
            // we only want pwads
            if (data_files[filenum]->kind == FLKIND_PWad || data_files[filenum]->kind == FLKIND_PackWAD)
            {
                TurnOffSkyBox = true;
            }
        }
    }

    I_Debugf("SKYBOX: Sky is: %s. Type:%d lumpnum:%d filenum:%d \n", tempImage->name.c_str(), tempImage->source_type,
             lumpnum, filenum);
    return TurnOffSkyBox;
}

// W_IsLumpInPwad
//
// check if a lump is in a pwad
//
// Returns true if found
bool W_IsLumpInPwad(const char *name)
{
    if (!name)
        return false;

    // first check images.ddf
    const image_c *tempImage;

    tempImage = W_ImageLookup(name);
    if (tempImage)
    {
        if (tempImage->source_type == IMSRC_User) // from images.ddf
        {
            return true;
        }
    }

    // if we're here then check pwad lumps
    int  lumpnum = W_CheckNumForName(name);
    int  filenum = -1;
    bool in_pwad = false;

    if (lumpnum != -1)
    {
        filenum = W_GetFileForLump(lumpnum);

        if (filenum >= 2) // ignore edge_defs and the IWAD itself
        {
            data_file_c *df = data_files[filenum];

            // we only want pwads
            // or ewads ;)
            if (df->kind == FLKIND_PWad || df->kind == FLKIND_EWad || df->kind == FLKIND_PackWAD)
            {
                in_pwad = true;
            }
        }
    }

    if (!in_pwad) // Check EPKs/folders now
    {
        // search from newest file to oldest
        for (int i = (int)data_files.size() - 1; i >= 2; i--) // ignore edge_defs and the IWAD itself
        {
            data_file_c *df = data_files[i];
            if (df->kind == FLKIND_Folder || df->kind == FLKIND_EFolder || df->kind == FLKIND_EPK ||
                df->kind == FLKIND_EEPK)
            {
                if (Pack_FindStem(df->pack, name))
                {
                    in_pwad = true;
                    break;
                }
            }
        }
    }

    return in_pwad;
}

// W_IsLumpInAnyWad
//
// check if a lump is in any wad/epk at all
//
// Returns true if found
bool W_IsLumpInAnyWad(const char *name)
{
    if (!name)
        return false;

    int  lumpnum   = W_CheckNumForName(name);
    bool in_anywad = false;

    if (lumpnum != -1)
        in_anywad = true;

    if (!in_anywad)
    {
        // search from oldest to newest
        for (int i = 0; i < (int)data_files.size() - 1; i++)
        {
            data_file_c *df = data_files[i];
            if (df->kind == FLKIND_Folder || df->kind == FLKIND_EFolder || df->kind == FLKIND_EPK ||
                df->kind == FLKIND_EEPK || df->kind == FLKIND_IFolder || df->kind == FLKIND_IPK)
            {
                if (Pack_FindStem(df->pack, name))
                {
                    in_anywad = true;
                    break;
                }
            }
        }
    }

    return in_anywad;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
