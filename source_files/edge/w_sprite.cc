//----------------------------------------------------------------------------
//  EDGE Sprite Management
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
// -KM- 1998/07/26 Replaced #ifdef RANGECHECK with #ifdef DEVELOPERS
// -KM- 1998/09/27 Dynamic colourmaps
// -AJA- 1999/07/12: Now uses colmap.ddf.
//

#include "i_defs.h"

// EPI
#include "endianess.h"
#include "filesystem.h"
#include "str_util.h"

#include "e_main.h"
#include "e_search.h"
#include "r_image.h"
#include "r_things.h"
#include "w_sprite.h"
#include "w_files.h"
#include "w_epk.h"
#include "w_wad.h"

#include "p_local.h" // mobjlisthead

#include "dm_data.h" // patch_t

#include <algorithm> // sort

//
// A sprite definition: a number of animation frames.
//
class spritedef_c
{
  public:
    // four letter sprite name (e.g. "TROO").
    std::string name;

    // total number of frames.  Zero for missing sprites.
    int numframes;

    // sprite frames.
    spriteframe_c *frames;

  public:
    spritedef_c(std::string _name) : numframes(0), frames(NULL)
    {
        name = _name;
    }

    ~spritedef_c()
    {
        // TODO: free the frames
    }

    bool HasWeapon() const
    {
        for (int i = 0; i < numframes; i++)
            if (frames[i].is_weapon)
                return true;

        return false;
    }
};

//----------------------------------------------------------------------------

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

// Sprite definitions
static spritedef_c **sprites;
static int           numsprites = 0;

// Sorted map of sprite defs.  Only used during initialisation.
static spritedef_c **sprite_map = NULL;
static int           sprite_map_len;

//
// SPRITE LOADING FUNCTIONS
//

static spriteframe_c *WhatFrame(spritedef_c *def, const char *name, int pos)
{
    char frame_ch = name[pos];

    int index;

    if ('A' <= frame_ch && frame_ch <= 'Z')
    {
        index = (frame_ch - 'A');
    }
    else
        switch (frame_ch)
        {
        case '[':
            index = 26;
            break;
        case '\\':
            index = 27;
            break;
        case ']':
            index = 28;
            break;
        case '^':
            index = 29;
            break;
        case '_':
            index = 30;
            break;

        default:
            I_Warning("Sprite lump %s has illegal frame.\n", name);
            return NULL;
        }

    SYS_ASSERT(index >= 0);

    // ignore frames larger than what is used in DDF
    if (index >= def->numframes)
        return NULL;

    return &def->frames[index];
}

static void SetExtendedRots(spriteframe_c *frame)
{
    frame->rots = 16;

    for (int i = 7; i >= 1; i--)
    {
        frame->images[2 * i] = frame->images[i];
        frame->flip[2 * i]   = frame->flip[i];
    }

    for (int k = 1; k <= 15; k += 2)
    {
        frame->images[k] = NULL;
        frame->flip[k]   = 0;
    }
}

static int WhatRot(spriteframe_c *frame, const char *name, int pos)
{
    char rot_ch = name[pos];

    int rot;

    // NOTE: rotations 9 and A-G are EDGE specific.

    if ('0' <= rot_ch && rot_ch <= '9')
        rot = (rot_ch - '0');
    else if ('A' <= rot_ch && rot_ch <= 'G')
        rot = (rot_ch - 'A') + 10;
    else
    {
        I_Warning("Sprite lump %s has illegal rotation.\n", name);
        return -1;
    }

    if (frame->rots == 0)
        frame->rots = 1;

    if (rot >= 1 && frame->rots == 1)
        frame->rots = 8;

    if (rot >= 9 && frame->rots != 16)
        SetExtendedRots(frame);

    switch (frame->rots)
    {
    case 1:
        return 0;

    case 8:
        return rot - 1;

    case 16:
        if (rot >= 9)
            return 1 + (rot - 9) * 2;
        else
            return 0 + (rot - 1) * 2;

    default:
        I_Error("INTERNAL ERROR: frame->rots = %d\n", frame->rots);
        return -1; /* NOT REACHED */
    }
}

static void InstallSpriteLump(spritedef_c *def, int lump, const char *lumpname, int pos, uint8_t flip)
{
    spriteframe_c *frame = WhatFrame(def, lumpname, pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished)
        return;

    int rot = WhatRot(frame, lumpname, pos + 1);
    if (rot < 0)
        return;

    SYS_ASSERT(0 <= rot && rot < 16);

    if (frame->images[rot])
    {
        // I_Warning("Sprite %s has two lumps mapped to it (frame %c).\n",
        // lumpname, lumpname[pos]);
        return;
    }

    frame->images[rot] = W_ImageCreateSprite(lumpname, lump, frame->is_weapon);

    frame->flip[rot] = flip;
}

static void InstallSpritePack(spritedef_c *def, pack_file_c *pack, std::string spritebase, std::string packname,
                              int pos, uint8_t flip)
{
    spriteframe_c *frame = WhatFrame(def, spritebase.c_str(), pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished)
        return;

    int rot = WhatRot(frame, spritebase.c_str(), pos + 1);
    if (rot < 0)
        return;

    SYS_ASSERT(0 <= rot && rot < 16);

    if (frame->images[rot])
    {
        // I_Warning("Sprite %s has two lumps mapped to it (frame %c).\n",
        // lumpname, lumpname[pos]);
        return;
    }

    frame->images[rot] = W_ImageCreatePackSprite(packname, pack, frame->is_weapon);

    frame->flip[rot] = flip;
}

static void InstallSpriteImage(spritedef_c *def, const image_c *img, const char *img_name, int pos, uint8_t flip)
{
    spriteframe_c *frame = WhatFrame(def, img_name, pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished)
        return;

    int rot = WhatRot(frame, img_name, pos + 1);
    if (rot < 0)
        return;

    if (frame->images[rot])
    {
        // I_Warning("Sprite %s has two images mapped to it (frame %c)\n",
        // img_name, img_name[pos]);
        return;
    }

    frame->images[rot] = img;
    frame->flip[rot]   = flip;
}

//
// FillSpriteFrames
//
static void FillSpriteFrames(int file)
{
    if (data_files[file]->wad)
    {
        std::vector<int> *lumps = W_GetSpriteList(file);
        if (lumps == NULL)
            return;

        int lumpnum = (int)lumps->size();
        if (lumpnum == 0)
            return;

        // check all lumps for prefixes matching the ones in the sprite
        // list.  Both lists have already been sorted to make this as fast
        // as possible.

        int S = 0, L = 0;

        for (; S < sprite_map_len; S++)
        {
            std::string sprname = sprite_map[S]->name;
            size_t      spr_len = sprname.size();
            for (; L < lumpnum; L++)
            {
                const char *lumpname = W_GetLumpName((*lumps)[L]);

                if (strlen(lumpname) != spr_len + 2 && strlen(lumpname) != spr_len + 4)
                {
                    continue;
                }

                // ignore model skins
                if (strlen(lumpname) == spr_len + 4 && lumpname[spr_len] == 'S' && lumpname[spr_len + 1] == 'K' &&
                    lumpname[spr_len + 2] == 'N')
                {
                    continue;
                }

                if (epi::StringCompareMax(sprname, lumpname, spr_len) != 0)
                    continue;

                // we have a match
                InstallSpriteLump(sprite_map[S], (*lumps)[L], lumpname, spr_len, 0);

                if (strlen(lumpname) == spr_len + 4)
                    InstallSpriteLump(sprite_map[S], (*lumps)[L], lumpname, spr_len + 2, 1);
            }
            L = 0;
        }
    }
    else if (data_files[file]->pack)
    {
        std::vector<std::string> packsprites = Pack_GetSpriteList(data_files[file]->pack);
        if (!packsprites.empty())
        {
            std::sort(packsprites.begin(), packsprites.end());

            size_t S = 0, L = 0;

            for (; S < (size_t)sprite_map_len; S++)
            {
                std::string sprname = sprite_map[S]->name;
                size_t      spr_len = sprname.size();
                for (; L < packsprites.size(); L++)
                {
                    std::string spritebase;
                    epi::TextureNameFromFilename(spritebase, epi::GetStem(packsprites[L]));

                    if (spritebase.size() != spr_len + 2 && spritebase.size() != spr_len + 4)
                    {
                        continue;
                    }

                    // ignore model skins
                    if (spritebase.size() == spr_len + 4 && spritebase[spr_len] == 'S' &&
                        spritebase[spr_len + 1] == 'K' && spritebase[spr_len + 2] == 'N')
                    {
                        continue;
                    }

                    if (epi::StringCompareMax(sprname, spritebase, spr_len) != 0)
                        continue;

                    // we have a match
                    InstallSpritePack(sprite_map[S], data_files[file]->pack, spritebase, packsprites[L], spr_len, 0);

                    if (spritebase.size() == spr_len + 4)
                        InstallSpritePack(sprite_map[S], data_files[file]->pack, spritebase, packsprites[L],
                                          spr_len + 2, 1);
                }
                L = 0;
            }
        }
    }
}

//
// FillSpriteFramesUser
//
// Like the above, but made especially for IMAGES.DDF.
//
static void FillSpriteFramesUser()
{
    int             img_num;
    const image_c **images = W_ImageGetUserSprites(&img_num);

    if (img_num == 0)
        return;

    SYS_ASSERT(images);

    int S = 0, L = 0;

    for (; S < sprite_map_len; S++)
    {
        std::string sprname = sprite_map[S]->name;
        size_t      spr_len = sprname.size();
        for (; L < img_num; L++)
        {
            const char *img_name = W_ImageGetName(images[L]);

            if (strlen(img_name) != spr_len + 2 && strlen(img_name) != spr_len + 4)
            {
                continue;
            }

            // ignore model skins
            if (strlen(img_name) == spr_len + 4 && img_name[spr_len] == 'S' && img_name[spr_len + 1] == 'K' &&
                img_name[spr_len + 2] == 'N')
            {
                continue;
            }

            if (epi::StringCompareMax(sprname, img_name, spr_len) != 0)
                continue;

            // Fix offsets if Doom formatted
            // Not sure if this is the 'proper' place to do this yet - Dasho
            if (images[L]->source.graphic.is_patch)
            {
                // const override
                image_c     *change_img   = (image_c *)images[L];
                epi::File *offset_check = nullptr;
                if (images[L]->source.graphic.packfile_name)
                    offset_check = W_OpenPackFile(images[L]->source.graphic.packfile_name);
                else
                    offset_check = W_OpenLump(images[L]->source.graphic.lump);

                if (!offset_check)
                    I_Error("FillSpriteFramesUser: Error loading %s!\n", images[L]->name.c_str());

                uint8_t header[32];
                memset(header, 255, sizeof(header));
                offset_check->Read(header, sizeof(header));
                delete offset_check;

                const patch_t *pat   = (patch_t *)header;
                change_img->offset_x = EPI_LE_S16(pat->leftoffset);
                change_img->offset_y = EPI_LE_S16(pat->topoffset);
                // adjust sprite offsets so that (0,0) is normal
                if (sprite_map[S]->HasWeapon())
                {
                    change_img->offset_x += (320.0f / 2.0f - change_img->actual_w / 2.0f); // loss of accuracy
                    change_img->offset_y += (200.0f - 32.0f - change_img->actual_h);
                }
                else
                {
                    // rim->offset_x -= rim->actual_w / 2;   // loss of accuracy
                    change_img->offset_x -= ((float)change_img->actual_w) / 2.0f; // Lobo 2023: dancing eye fix
                    change_img->offset_y -= change_img->actual_h;
                }
            }

            // we have a match
            InstallSpriteImage(sprite_map[S], images[L], img_name, spr_len, 0);

            if (strlen(img_name) == spr_len + 4)
                InstallSpriteImage(sprite_map[S], images[L], img_name, spr_len + 2, 1);
        }
        L = 0;
    }

    delete[] images;
}

static void MarkCompletedFrames(void)
{
    int src, dst;

    for (src = dst = 0; src < sprite_map_len; src++)
    {
        spritedef_c *def = sprite_map[src];

        int finish_num = 0;

        for (int f = 0; f < def->numframes; f++)
        {
            char           frame_ch = 'A' + f;
            spriteframe_c *frame    = def->frames + f;

            if (frame->finished)
            {
                finish_num++;
                continue;
            }

            int rot_count = 0;

            // check if all image pointers are NULL
            for (int i = 0; i < frame->rots; i++)
                rot_count += frame->images[i] ? 1 : 0;

            if (rot_count == 0)
                continue;

            frame->finished = 1;
            finish_num++;

            if (rot_count < frame->rots)
            {
                I_Warning("Sprite %s:%c is missing rotations (%d of %d).\n", def->name.c_str(), frame_ch,
                          frame->rots - rot_count, frame->rots);

                // try to fix cases where some dumbass used A1 instead of A0
                if (rot_count == 1 && !frame->is_weapon)
                    frame->rots = 1;
            }
        }

        // remove complete sprites from sprite_map
        if (finish_num == def->numframes)
            continue;

        sprite_map[dst++] = def;
    }

    sprite_map_len = dst;
}

// show warnings for missing patches
static void CheckSpriteFrames(spritedef_c *def)
{
    int missing = 0;

    for (int i = 0; i < def->numframes; i++)
        if (!def->frames[i].finished)
        {
            I_Debugf("Frame %d/%d in sprite %s is not finished\n", 1 + i, def->numframes, def->name.c_str());
            missing++;
        }

    if (missing > 0 && missing < def->numframes)
        I_Warning("Missing %d frames in sprite: %s\n", missing, def->name.c_str());

    // free some memory for completely missing sprites
    if (missing == def->numframes)
    {
        delete[] def->frames;

        def->numframes = 0;
        def->frames    = NULL;
    }
}

//
// W_InitSprites
//
// Use the sprite lists in the WAD (S_START..S_END) to flesh out the
// known sprite definitions (global `sprites' array, created while
// parsing DDF) with images.
//
// Checking for missing frames still done at run time.
//
// -AJA- 2001/02/01: rewrote this stuff.
//
void W_InitSprites(void)
{
    numsprites = (int)ddf_sprite_names.size();

    if (numsprites <= 1)
        I_Error("Missing sprite definitions !!\n");

    E_ProgressMessage("Finding sprite patches...");

    I_Printf("W_InitSprites: Finding sprite patches\n");

    // 1. Allocate sprite definitions (ignore NULL sprite, #0)

    sprites           = new spritedef_c *[numsprites];
    sprites[SPR_NULL] = NULL;

    for (int i = 1; i < numsprites; i++)
    {
        std::string name = ddf_sprite_names[i];

        // SYS_ASSERT(strlen(name) == 4);

        sprites[i] = new spritedef_c(name);
    }

    // 2. Scan the state table, count frames

    for (int stnum = 1; stnum < num_states; stnum++)
    {
        state_t *st = &states[stnum];

        if (st->flags & SFF_Model)
            continue;

        if (st->sprite == SPR_NULL)
            continue;

        spritedef_c *def = sprites[st->sprite];

        if (def->numframes < st->frame + 1)
            def->numframes = st->frame + 1;
    }

    // 3. Allocate frames

    for (int k = 1; k < numsprites; k++)
    {
        spritedef_c *def = sprites[k];

        SYS_ASSERT(def->numframes > 0);

        def->frames = new spriteframe_c[def->numframes];
    }

    // 4. Mark weapon frames

    for (int st_kk = 1; st_kk < num_states; st_kk++)
    {
        state_t *st = &states[st_kk];

        if (st->flags & SFF_Model)
            continue;

        if (st->sprite == SPR_NULL)
            continue;

        spritedef_c *def = sprites[st->sprite];

        if (st->flags & SFF_Weapon)
            def->frames[st->frame].is_weapon = true;
    }

    // 5. Fill in frames using wad lumps + images.ddf

    // create a sorted list (ignore NULL entry, #0)
    sprite_map_len = numsprites - 1;

    sprite_map = new spritedef_c *[sprite_map_len];

    for (int n = 0; n < sprite_map_len; n++)
        sprite_map[n] = sprites[n + 1];

#define CMP(a, b) (epi::StringCompare(a->name, b->name) < 0)
    QSORT(spritedef_c *, sprite_map, sprite_map_len, CUTOFF);
#undef CMP

    // iterate over each file.  Order is important, we must go from
    // newest wad to oldest, so that new sprites override the old ones.
    // Completely finished sprites get removed from the list as we go.
    //
    // NOTE WELL: override granularity is single frames.

    int numfiles = W_GetNumFiles();

    FillSpriteFramesUser();

    for (int file = numfiles - 1; file >= 0; file--)
    {
        FillSpriteFrames(file);
    }

    MarkCompletedFrames();

    // 6. Perform checks and free stuff

    for (int j = 1; j < numsprites; j++)
        CheckSpriteFrames(sprites[j]);

    delete[] sprite_map;
    sprite_map = NULL;
}

bool W_CheckSpritesExist(const state_group_t &group)
{
    for (int g = 0; g < (int)group.size(); g++)
    {
        const state_range_t &range = group[g];

        for (int i = range.first; i <= range.last; i++)
        {
            if (states[i].sprite == SPR_NULL)
                continue;

            if (sprites[states[i].sprite]->frames)
                return true;

            // -AJA- only check one per group.  It _should_ check them all,
            //       however this maintains compatibility.
            break;
        }
    }

    return false;
}

spriteframe_c *W_GetSpriteFrame(int spr_num, int framenum)
{
    // spr_num comes from the 'sprite' field of state_t, and
    // is also an index into ddf_sprite_names vector.

    SYS_ASSERT(spr_num > 0);
    SYS_ASSERT(spr_num < numsprites);
    SYS_ASSERT(framenum >= 0);

    spritedef_c *def = sprites[spr_num];

    if (framenum >= def->numframes)
        return NULL;

    spriteframe_c *frame = &def->frames[framenum];

    if (!frame || !frame->finished)
        return NULL;

    return frame;
}

void W_PrecacheSprites(void)
{
    SYS_ASSERT(numsprites > 1);

    uint8_t *sprite_present = new uint8_t[numsprites];
    memset(sprite_present, 0, numsprites);

    for (mobj_t *mo = mobjlisthead; mo; mo = mo->next)
    {
        SYS_ASSERT(mo->state);

        if (mo->state->sprite < 1 || mo->state->sprite >= numsprites)
            continue;

        sprite_present[mo->state->sprite] = 1;
    }

    for (int i = 1; i < numsprites; i++) // ignore SPR_NULL
    {
        spritedef_c *def = sprites[i];

        const image_c *cur_image;
        const image_c *last_image = NULL; // an optimisation

        if (def->numframes == 0)
            continue;

        // Note: all weapon sprites are pre-cached

        if (!(sprite_present[i] || def->HasWeapon()))
            continue;

        /* Lobo 2022: info overload. Shut up.
                I_Debugf("Precaching sprite: %s\n", def->name);
        */

        SYS_ASSERT(def->frames);

        for (int fr = 0; fr < def->numframes; fr++)
        {
            if (!def->frames[fr].finished)
                continue;

            for (int rot = 0; rot < 16; rot++)
            {
                cur_image = def->frames[fr].images[rot];

                if (cur_image == NULL || cur_image == last_image)
                    continue;

                W_ImagePreCache(cur_image);

                last_image = cur_image;
            }
        }
    }

    delete[] sprite_present;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
