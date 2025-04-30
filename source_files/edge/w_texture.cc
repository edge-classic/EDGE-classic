//----------------------------------------------------------------------------
//  Texture Conversion and Caching code
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
// This module converts image lumps on disk to usable structures, and also
// provides a caching system for these.
//
// -ES- 2000/02/12 Written.

#include "w_texture.h"

#include "e_main.h"
#include "e_search.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_endian.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "r_image.h"
#include "w_files.h"
#include "w_wad.h"

extern std::string game_base;

class TextureSet
{
  public:
    TextureSet(int num) : total_textures_(num)
    {
        textures_ = new TextureDefinition *[total_textures_];
    }

    ~TextureSet()
    {
        for (int i = 0; i < total_textures_; ++i)
        {
            if (textures_[i])
                free(textures_[i]);
        }
        delete[] textures_;
    }

    TextureDefinition **textures_;
    int                 total_textures_;
};

static std::vector<TextureSet *> texture_sets;

void ShutdownTextureSets()
{
    for (TextureSet *set : texture_sets)
    {
        delete set;
    }
}

//
// InstallTextureLumps
//
// -ACB- 1998/09/09 Removed the Doom II SkyName change: unnecessary and not DDF.
//                  Reformatted and cleaned up.
//
static void InstallTextureLumps(int file, const WadTextureResource *WT)
{
    int i;
    int maxoff;
    int maxoff2;
    int numtextures1;
    int numtextures2;

    const int *maptex;
    const int *maptex1;
    const int *maptex2;
    const int *directory;

    // Load the patch names from pnames.lmp.
    const char *names         = (const char *)LoadLumpIntoMemory(WT->pnames);
    int         nummappatches = AlignedLittleEndianS32(*((const int *)names)); // Eww...

    const char *name_p = names + 4;

    int *patchlookup = new int[nummappatches + 1];

    std::vector<std::string> patch_names;

    patch_names.resize(nummappatches);

    for (i = 0; i < nummappatches; i++)
    {
        patch_names[i].resize(9);

        epi::CStringCopyMax(patch_names[i].data(), (const char *)(name_p + i * 8), 8);

        patchlookup[i] = CheckPatchLumpNumberForName(patch_names[i].c_str());
    }

    delete[] names;

    //
    // Load the map texture definitions from textures.lmp.
    //
    // The data is contained in one or two lumps:
    //   TEXTURE1 for shareware
    //   TEXTURE2 for commercial.
    //
    maptex = maptex1 = (const int *)LoadLumpIntoMemory(WT->texture1);
    numtextures1     = AlignedLittleEndianS32(*maptex);
    maxoff           = GetLumpLength(WT->texture1);
    directory        = maptex + 1;

    if (WT->texture2 != -1)
    {
        maptex2      = (const int *)LoadLumpIntoMemory(WT->texture2);
        numtextures2 = AlignedLittleEndianS32(*maptex2);
        maxoff2      = GetLumpLength(WT->texture2);
    }
    else
    {
        maptex2      = nullptr;
        numtextures2 = 0;
        maxoff2      = 0;
    }

    TextureSet *cur_set = new TextureSet(numtextures1 + numtextures2);

    texture_sets.push_back(cur_set);

    for (i = 0; i < cur_set->total_textures_; i++, directory++)
    {
        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex    = maptex2;
            maxoff    = maxoff2;
            directory = maptex + 1;
        }

        int offset = AlignedLittleEndianS32(*directory);
        if (offset < 0 || offset > maxoff)
            FatalError("InitializeTextures: bad texture directory");

        const RawTexture *mtexture = (const RawTexture *)((const uint8_t *)maptex + offset);

        // -ES- 2000/02/10 Texture must have patches.
        int patchcount = AlignedLittleEndianS16(mtexture->patch_count);

        // Lobo 2021: Changed this to a warning. Allows us to run several DBPs
        //  which have this issue
        if (!patchcount)
        {
            LogWarning("InitializeTextures: Texture '%.8s' has no patches\n", mtexture->name);
            // FatalError("InitializeTextures: Texture '%.8s' has no patches",
            // mtexture->name);
            patchcount = 0; // mark it as a dud
        }

        int width = AlignedLittleEndianS16(mtexture->width);
        if (width == 0)
            FatalError("InitializeTextures: Texture '%.8s' has zero width", mtexture->name);

        // -ES- Allocate texture, patches and columnlump/ofs in one big chunk
        int base_size = sizeof(TextureDefinition) + sizeof(TexturePatch) * (patchcount - 1);

        TextureDefinition *texture = (TextureDefinition *)malloc(base_size + width * (sizeof(uint8_t) + sizeof(short)));
        cur_set->textures_[i]      = texture;

        uint8_t *base = (uint8_t *)texture + base_size;

        texture->column_offset = (unsigned short *)base;

        texture->width        = width;
        texture->height       = AlignedLittleEndianS16(mtexture->height);
        texture->scale_x      = mtexture->scale_x;
        texture->scale_y      = mtexture->scale_y;
        texture->file         = file;
        texture->palette_lump = GetPaletteForLump(WT->texture1);
        texture->patch_count  = patchcount;

        epi::CStringCopyMax(texture->name, mtexture->name, 8);
        for (size_t j = 0; j < strlen(texture->name); j++)
        {
            texture->name[j] = epi::ToUpperASCII(texture->name[j]);
        }

        const RawPatchDefinition *mpatch = &mtexture->patches[0];
        TexturePatch             *patch  = &texture->patches[0];

        bool is_sky = (epi::StringPrefixCaseCompareASCII(texture->name, "SKY") == 0);

        for (int k = 0; k < texture->patch_count; k++, mpatch++, patch++)
        {
            int pname = AlignedLittleEndianS16(mpatch->pname);

            patch->origin_x = AlignedLittleEndianS16(mpatch->x_origin);
            patch->origin_y = AlignedLittleEndianS16(mpatch->y_origin);
            patch->patch    = patchlookup[pname];

            // work-around for strange Y offset in SKY1 of DOOM 1
            if (is_sky && patch->origin_y < 0)
                patch->origin_y = 0;

            if (patch->patch == -1)
            {
                LogWarning("Missing patch '%.8s' in texture \'%.8s\'\n", patch_names[pname].c_str(), texture->name);

                // mark texture as a dud
                texture->patch_count = 0;
                break;
            }
        }
    }

    // free stuff
    patch_names.clear();

    delete[] maptex1;

    if (maptex2)
        delete[] maptex2;

    delete[] patchlookup;
}

static void InstallTextureLumpsStrife(int file, const WadTextureResource *WT)
{
    int i;
    int maxoff;
    int maxoff2;
    int numtextures1;
    int numtextures2;

    const int *maptex;
    const int *maptex1;
    const int *maptex2;
    const int *directory;

    // Load the patch names from pnames.lmp.
    const char *names         = (const char *)LoadLumpIntoMemory(WT->pnames);
    int         nummappatches = AlignedLittleEndianS32(*((const int *)names)); // Eww...

    const char *name_p = names + 4;

    int *patchlookup = new int[nummappatches + 1];

    std::vector<std::string> patch_names;

    patch_names.resize(nummappatches);

    for (i = 0; i < nummappatches; i++)
    {
        patch_names[i].resize(9);

        epi::CStringCopyMax(patch_names[i].data(), (const char *)(name_p + i * 8), 8);

        patchlookup[i] = CheckPatchLumpNumberForName(patch_names[i].c_str());
    }

    delete[] names;

    //
    // Load the map texture definitions from textures.lmp.
    //
    // The data is contained in one or two lumps:
    //   TEXTURE1 for shareware
    //   TEXTURE2 for commercial.
    //
    maptex = maptex1 = (const int *)LoadLumpIntoMemory(WT->texture1);
    numtextures1     = AlignedLittleEndianS32(*maptex);
    maxoff           = GetLumpLength(WT->texture1);
    directory        = maptex + 1;

    if (WT->texture2 != -1)
    {
        maptex2      = (const int *)LoadLumpIntoMemory(WT->texture2);
        numtextures2 = AlignedLittleEndianS32(*maptex2);
        maxoff2      = GetLumpLength(WT->texture2);
    }
    else
    {
        maptex2      = nullptr;
        numtextures2 = 0;
        maxoff2      = 0;
    }

    TextureSet *cur_set = new TextureSet(numtextures1 + numtextures2);

    texture_sets.push_back(cur_set);

    for (i = 0; i < cur_set->total_textures_; i++, directory++)
    {
        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex    = maptex2;
            maxoff    = maxoff2;
            directory = maptex + 1;
        }

        int offset = AlignedLittleEndianS32(*directory);
        if (offset < 0 || offset > maxoff)
            FatalError("InitializeTextures: bad texture directory");

        const RawStrifeTexture *mtexture = (const RawStrifeTexture *)((const uint8_t *)maptex + offset);

        // -ES- 2000/02/10 Texture must have patches.
        int patchcount = AlignedLittleEndianS16(mtexture->patch_count);

        // Lobo 2021: Changed this to a warning. Allows us to run several DBPs
        //  which have this issue
        if (!patchcount)
        {
            LogWarning("InitializeTextures: Texture '%.8s' has no patches\n", mtexture->name);
            // FatalError("InitializeTextures: Texture '%.8s' has no patches",
            // mtexture->name);
            patchcount = 0; // mark it as a dud
        }

        int width = AlignedLittleEndianS16(mtexture->width);
        if (width == 0)
            FatalError("InitializeTextures: Texture '%.8s' has zero width", mtexture->name);

        // -ES- Allocate texture, patches and columnlump/ofs in one big chunk
        int base_size = sizeof(TextureDefinition) + sizeof(TexturePatch) * (patchcount - 1);

        TextureDefinition *texture = (TextureDefinition *)malloc(base_size + width * (sizeof(uint8_t) + sizeof(short)));
        cur_set->textures_[i]      = texture;

        uint8_t *base = (uint8_t *)texture + base_size;

        texture->column_offset = (unsigned short *)base;

        texture->width        = width;
        texture->height       = AlignedLittleEndianS16(mtexture->height);
        texture->scale_x      = mtexture->scale_x;
        texture->scale_y      = mtexture->scale_y;
        texture->file         = file;
        texture->palette_lump = GetPaletteForLump(WT->texture1);
        texture->patch_count  = patchcount;

        epi::CStringCopyMax(texture->name, mtexture->name, 8);
        for (size_t j = 0; j < strlen(texture->name); j++)
        {
            texture->name[j] = epi::ToUpperASCII(texture->name[j]);
        }

        const RawStrifePatchDefinition *mpatch = &mtexture->patches[0];
        TexturePatch                   *patch  = &texture->patches[0];

        for (int k = 0; k < texture->patch_count; k++, mpatch++, patch++)
        {
            int pname = AlignedLittleEndianS16(mpatch->pname);

            patch->origin_x = AlignedLittleEndianS16(mpatch->x_origin);
            patch->origin_y = AlignedLittleEndianS16(mpatch->y_origin);
            patch->patch    = patchlookup[pname];

            if (patch->patch == -1)
            {
                LogWarning("Missing patch '%.8s' in texture \'%.8s\'\n", patch_names[pname].c_str(), texture->name);

                // mark texture as a dud
                texture->patch_count = 0;
                break;
            }
        }
    }

    // free stuff
    patch_names.clear();

    delete[] maptex1;

    if (maptex2)
        delete[] maptex2;

    delete[] patchlookup;
}

//
// InitializeTextures
//
// Initialises the texture list with the textures from the world map.
//
// -ACB- 1998/09/09 Fixed the Display routine from display rubbish.
//
void InitializeTextures(void)
{
    int num_files = GetTotalFiles();
    int file;

    TextureDefinition **textures = nullptr;
    TextureDefinition **cur;
    int                 numtextures = 0;

    LogPrint("Initializing Textures...\n");

    EPI_ASSERT(texture_sets.empty());

    // iterate over each file, creating our sets of textures
    // -ACB- 1998/09/09 Removed the Doom II SkyName change: unnecessary and not
    // DDF.

    for (file = 0; file < num_files; file++)
    {
        WadTextureResource WT;

        GetTextureLumpsForWAD(file, &WT);

        if (WT.pnames < 0)
            continue;

        if (WT.texture1 < 0 && WT.texture2 >= 0)
        {
            WT.texture1 = WT.texture2;
            WT.texture2 = -1;
        }

        if (WT.texture1 < 0)
            continue;

        if (game_base == "strife")
            InstallTextureLumpsStrife(file, &WT);
        else
            InstallTextureLumps(file, &WT);
    }

    if (texture_sets.empty())
    {
        // FatalError("No textures found !  Make sure the chosen IWAD is
        // valid.\n");
        LogWarning("No textures found! Generating fallback texture!\n");
        CreateFallbackTexture();
        return;
    }

    // now clump all of the texturedefs together and sort 'em, primarily
    // by increasing name, secondarily by increasing file number
    // (measure of newness).  We ignore "dud" textures (missing
    // patches).

    for (int k = 0; k < (int)texture_sets.size(); k++)
        numtextures += texture_sets[k]->total_textures_;

    textures = cur = new TextureDefinition *[numtextures];

    for (int k = 0; k < (int)texture_sets.size(); k++)
    {
        TextureSet *set = texture_sets[k];

        for (int m = 0; m < set->total_textures_; m++)
            if (set->textures_[m]->patch_count > 0)
                *cur++ = set->textures_[m];
    }

    numtextures = cur - textures;

#define EDGE_CMP(a, b) (strcmp(a->name, b->name) < 0 || (strcmp(a->name, b->name) == 0 && a->file < b->file))
    EDGE_QSORT(TextureDefinition *, textures, numtextures, 10);
#undef EDGE_CMP

    // remove duplicate names.  Because the QSORT took newness into
    // account, only the last entry in a run of identically named
    // textures needs to be kept.

    for (int k = 1; k < numtextures; k++)
    {
        const TextureDefinition *a = textures[k - 1];
        const TextureDefinition *b = textures[k];

        if (strcmp(a->name, b->name) == 0)
        {
            textures[k - 1] = nullptr;
        }
    }

    CreateTextures(textures, numtextures);

    // free pointer array.  We need to keep the definitions in memory
    // for (a) the image system and (b) texture anims.
    delete[] textures;
}

//
// FindTextureSequence
//
// Returns the set number containing the texture names (with the
// offset values updated to the indexes), or -1 if none could be
// found.  Used by animation code.
//
// Note: search is from latest set to earliest set.
//
int FindTextureSequence(const char *start, const char *end, int *s_offset, int *e_offset)
{
    int i, j;

    for (i = (int)texture_sets.size() - 1; i >= 0; i--)
    {
        // look for start name
        for (j = 0; j < texture_sets[i]->total_textures_; j++)
            if (epi::StringCaseCompareASCII(start, texture_sets[i]->textures_[j]->name) == 0)
                break;

        if (j >= texture_sets[i]->total_textures_)
            continue;

        (*s_offset) = j;

        // look for end name
        for (j++; j < texture_sets[i]->total_textures_; j++)
        {
            if (epi::StringCaseCompareASCII(end, texture_sets[i]->textures_[j]->name) == 0)
            {
                (*e_offset) = j;
                return i;
            }
        }
    }

    // not found
    return -1;
}

//
// TextureNameInSet
//
const char *TextureNameInSet(int set, int offset)
{
    EPI_ASSERT(0 <= set && set < (int)texture_sets.size());
    EPI_ASSERT(0 <= offset && offset < texture_sets[set]->total_textures_);

    return texture_sets[set]->textures_[offset]->name;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
