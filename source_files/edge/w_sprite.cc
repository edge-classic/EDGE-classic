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
// -KM- 1998/09/27 Dynamic colormaps
// -AJA- 1999/07/12: Now uses colmap.ddf.
//

#include "w_sprite.h"

#include <algorithm> // sort

#include "e_main.h"
#include "e_search.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_endian.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "p_local.h" // map_object_list_head
#include "r_image.h"
#include "r_things.h"
#include "w_epk.h"
#include "w_files.h"
#include "w_wad.h"

//
// A sprite definition: a number of animation frames.
//
class SpriteDefinition
{
  public:
    // four letter sprite name (e.g. "TROO").
    std::string name_;

    // total number of frames.  Zero for missing sprites.
    int total_frames_;

    // sprite frames.
    SpriteFrame *frames_;

  public:
    SpriteDefinition(std::string_view name) : name_(name), total_frames_(0), frames_(nullptr)
    {
    }

    ~SpriteDefinition()
    {
        // TODO: free the frames
    }

    bool HasWeapon() const
    {
        for (int i = 0; i < total_frames_; i++)
            if (frames_[i].is_weapon_)
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
static SpriteDefinition **sprites;
static int                sprite_count = 0;

// Sorted map of sprite defs.  Only used during initialisation.
static SpriteDefinition **sprite_map = nullptr;
static int                sprite_map_length;

//
// SPRITE LOADING FUNCTIONS
//

static SpriteFrame *WhatFrame(SpriteDefinition *def, const char *name, int pos)
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
            LogWarning("Sprite lump %s has illegal frame.\n", name);
            return nullptr;
        }

    EPI_ASSERT(index >= 0);

    // ignore frames larger than what is used in DDF
    if (index >= def->total_frames_)
        return nullptr;

    return &def->frames_[index];
}

static void SetExtendedRots(SpriteFrame *frame)
{
    frame->rotations_ = 16;

    for (int i = 7; i >= 1; i--)
    {
        frame->images_[2 * i] = frame->images_[i];
        frame->flip_[2 * i]   = frame->flip_[i];
    }

    for (int k = 1; k <= 15; k += 2)
    {
        frame->images_[k] = nullptr;
        frame->flip_[k]   = 0;
    }
}

static int WhatRot(SpriteFrame *frame, const char *name, int pos)
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
        LogWarning("Sprite lump %s has illegal rotation.\n", name);
        return -1;
    }

    if (frame->rotations_ == 0)
        frame->rotations_ = 1;

    if (rot >= 1 && frame->rotations_ == 1)
        frame->rotations_ = 8;

    if (rot >= 9 && frame->rotations_ != 16)
        SetExtendedRots(frame);

    switch (frame->rotations_)
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
        FatalError("INTERNAL ERROR: frame->rotations_ = %d\n", frame->rotations_);
    }
}

static void InstallSpriteLump(SpriteDefinition *def, int lump, const char *lumpname, int pos, uint8_t flip)
{
    SpriteFrame *frame = WhatFrame(def, lumpname, pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished_)
        return;

    int rot = WhatRot(frame, lumpname, pos + 1);
    if (rot < 0)
        return;

    EPI_ASSERT(0 <= rot && rot < 16);

    if (frame->images_[rot])
        return;

    frame->images_[rot] = CreateSprite(lumpname, lump, frame->is_weapon_);
    frame->flip_[rot]   = flip;

    if (rot == 0 && frame->rotations_ == 1)
        frame->finished_ = true;
}

static void InstallSpritePack(SpriteDefinition *def, PackFile *pack, const std::string &spritebase,
                              const std::string &packname, int pos, uint8_t flip)
{
    SpriteFrame *frame = WhatFrame(def, spritebase.c_str(), pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished_)
        return;

    int rot = WhatRot(frame, spritebase.c_str(), pos + 1);
    if (rot < 0)
        return;

    EPI_ASSERT(0 <= rot && rot < 16);

    if (frame->images_[rot])
        return;

    frame->images_[rot] = CreatePackSprite(packname, pack, frame->is_weapon_);
    frame->flip_[rot]   = flip;

    if (rot == 0 && frame->rotations_ == 1)
        frame->finished_ = true;
}

static void InstallSpriteImage(SpriteDefinition *def, const Image *img, const char *img_name, int pos, uint8_t flip)
{
    SpriteFrame *frame = WhatFrame(def, img_name, pos);
    if (!frame)
        return;

    // don't disturb any frames already loaded
    if (frame->finished_)
        return;

    int rot = WhatRot(frame, img_name, pos + 1);
    if (rot < 0)
        return;

    if (frame->images_[rot])
        return;

    frame->images_[rot] = img;
    frame->flip_[rot]   = flip;

    if (rot == 0 && frame->rotations_ == 1)
        frame->finished_ = true;
}

//
// FillSpriteFrames
//
static void FillSpriteFrames(int file)
{
    if (data_files[file]->wad_)
    {
        const std::vector<int> *lumps = GetSpriteListForWAD(file);
        if (lumps == nullptr)
            return;

        int lumpnum = (int)lumps->size();
        if (lumpnum == 0)
            return;

        // check all lumps for prefixes matching the ones in the sprite
        // list.  Both lists have already been sorted to make this as fast
        // as possible.

        int S = 0, L = 0;

        for (; S < sprite_map_length; S++)
        {
            std::string sprname = sprite_map[S]->name_;
            size_t      spr_len = sprname.size();
            for (; L < lumpnum; L++)
            {
                const char *lumpname = GetLumpNameFromIndex((*lumps)[L]);

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
    else if (data_files[file]->pack_)
    {
        std::vector<std::string> packsprites = GetPackSpriteList(data_files[file]->pack_);
        if (!packsprites.empty())
        {
            std::sort(packsprites.begin(), packsprites.end());

            size_t S = 0, L = 0;

            for (; S < (size_t)sprite_map_length; S++)
            {
                std::string sprname = sprite_map[S]->name_;
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
                    InstallSpritePack(sprite_map[S], data_files[file]->pack_, spritebase, packsprites[L], spr_len, 0);

                    if (spritebase.size() == spr_len + 4)
                        InstallSpritePack(sprite_map[S], data_files[file]->pack_, spritebase, packsprites[L],
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
    int           img_num;
    const Image **images = GetUserSprites(&img_num);

    if (img_num == 0)
        return;

    EPI_ASSERT(images);

    int S = 0, L = 0;

    for (; S < sprite_map_length; S++)
    {
        std::string sprname = sprite_map[S]->name_;
        size_t      spr_len = sprname.size();
        for (; L < img_num; L++)
        {
            const char *img_name = images[L]->name_.c_str();

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
            if (images[L]->source_.graphic.is_patch)
            {
                // const override
                Image     *change_img   = (Image *)images[L];
                epi::File *offset_check = nullptr;
                if (images[L]->source_.graphic.packfile_name)
                    offset_check = OpenFileFromPack(images[L]->source_.graphic.packfile_name);
                else
                    offset_check = LoadLumpAsFile(images[L]->source_.graphic.lump);

                if (!offset_check)
                    FatalError("FillSpriteFramesUser: Error loading %s!\n", images[L]->name_.c_str());

                uint8_t header[32];
                memset(header, 255, sizeof(header));
                offset_check->Read(header, sizeof(header));
                delete offset_check;

                const Patch *pat      = (Patch *)header;
                change_img->offset_x_ = AlignedLittleEndianS16(pat->left_offset);
                change_img->offset_y_ = AlignedLittleEndianS16(pat->top_offset);
                // adjust sprite offsets so that (0,0) is normal
                if (sprite_map[S]->HasWeapon())
                {
                    change_img->offset_x_ += (320.0f / 2.0f - change_img->width_ / 2.0f); // loss of accuracy
                    change_img->offset_y_ += (200.0f - 32.0f - change_img->height_);
                }
                else
                {
                    // rim->offset_x_ -= rim->width_ / 2;   // loss of
                    // accuracy
                    change_img->offset_x_ -= ((float)change_img->width_) / 2.0f; // Lobo 2023: dancing eye fix
                    change_img->offset_y_ -= change_img->height_;
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

    for (src = dst = 0; src < sprite_map_length; src++)
    {
        SpriteDefinition *def = sprite_map[src];

        int finish_num = 0;

        for (int f = 0; f < def->total_frames_; f++)
        {
            SpriteFrame *frame = def->frames_ + f;

            if (frame->finished_)
            {
                finish_num++;
                continue;
            }

            int rot_count = 0;

            // check if all image pointers are nullptr
            for (int i = 0; i < frame->rotations_; i++)
                rot_count += frame->images_[i] ? 1 : 0;

            if (rot_count == 0)
                continue;

            frame->finished_ = 1;
            finish_num++;

            if (rot_count < frame->rotations_)
            {
                LogWarning("Sprite %s:%c is missing rotations (%d of %d).\n", def->name_.c_str(), 'A' + f,
                           frame->rotations_ - rot_count, frame->rotations_);

                // try to fix cases where some dumbass used A1 instead of A0
                if (rot_count == 1 && !frame->is_weapon_)
                    frame->rotations_ = 1;
            }
        }

        // remove complete sprites from sprite_map
        if (finish_num == def->total_frames_)
            continue;

        sprite_map[dst++] = def;
    }

    sprite_map_length = dst;
}

// show warnings for missing patches
static void CheckSpriteFrames(SpriteDefinition *def)
{
    int missing = 0;

    for (int i = 0; i < def->total_frames_; i++)
        if (!def->frames_[i].finished_)
        {
            LogDebug("Frame %d/%d in sprite %s is not finished\n", 1 + i, def->total_frames_, def->name_.c_str());
            missing++;
        }

    if (missing > 0 && missing < def->total_frames_)
        LogWarning("Missing %d frames in sprite: %s\n", missing, def->name_.c_str());

    // free some memory for completely missing sprites
    if (missing == def->total_frames_)
    {
        delete[] def->frames_;

        def->total_frames_ = 0;
        def->frames_       = nullptr;
    }
}

//
// InitializeSprites
//
// Use the sprite lists in the WAD (S_START..S_END) to flesh out the
// known sprite definitions (global `sprites' array, created while
// parsing DDF) with images.
//
// Checking for missing frames still done at run time.
//
// -AJA- 2001/02/01: rewrote this stuff.
//
void InitializeSprites(void)
{
    sprite_count = (int)ddf_sprite_names.size();

    if (sprite_count <= 1)
        FatalError("Missing sprite definitions !!\n");

    StartupProgressMessage("Finding sprite patches...");

    LogPrint("InitializeSprites: Finding sprite patches\n");

    // 1. Allocate sprite definitions (ignore nullptr sprite, #0)

    sprites    = new SpriteDefinition *[sprite_count];
    sprites[0] = nullptr;

    for (int i = 1; i < sprite_count; i++)
    {
        std::string name = ddf_sprite_names[i];

        sprites[i] = new SpriteDefinition(name);
    }

    // 2. Scan the state table, count frames

    for (int stnum = 1; stnum < num_states; stnum++)
    {
        State *st = &states[stnum];

        if (st->flags & kStateFrameFlagModel)
            continue;

        if (st->sprite == 0)
            continue;

        SpriteDefinition *def = sprites[st->sprite];

        if (def->total_frames_ < st->frame + 1)
            def->total_frames_ = st->frame + 1;
    }

    // 3. Allocate frames

    for (int k = 1; k < sprite_count; k++)
    {
        SpriteDefinition *def = sprites[k];

        EPI_ASSERT(def->total_frames_ > 0);

        def->frames_ = new SpriteFrame[def->total_frames_];
    }

    // 4. Mark weapon frames

    for (int st_kk = 1; st_kk < num_states; st_kk++)
    {
        State *st = &states[st_kk];

        if (st->flags & kStateFrameFlagModel)
            continue;

        if (st->sprite == 0)
            continue;

        if (st->flags & kStateFrameFlagWeapon)
            sprites[st->sprite]->frames_[st->frame].is_weapon_ = true;
    }

    // 5. Fill in frames using wad lumps + images.ddf

    // create a sorted list (ignore nullptr entry, #0)
    sprite_map_length = sprite_count - 1;

    sprite_map = new SpriteDefinition *[sprite_map_length];

    for (int n = 0; n < sprite_map_length; n++)
        sprite_map[n] = sprites[n + 1];

#define EDGE_CMP(a, b) (epi::StringCompare(a->name_, b->name_) < 0)
    EDGE_QSORT(SpriteDefinition *, sprite_map, sprite_map_length, 10);
#undef EDGE_CMP

    // iterate over each file.  Order is important, we must go from
    // newest wad to oldest, so that new sprites override the old ones.
    // Completely finished sprites get removed from the list as we go.
    //
    // NOTE WELL: override granularity is single frames.

    int numfiles = GetTotalFiles();

    FillSpriteFramesUser();

    for (int file = numfiles - 1; file >= 0; file--)
    {
        FillSpriteFrames(file);
    }

    MarkCompletedFrames();

    // 6. Perform checks and free stuff

    for (int j = 1; j < sprite_count; j++)
        CheckSpriteFrames(sprites[j]);

    delete[] sprite_map;
    sprite_map = nullptr;
}

bool CheckSpritesExist(const std::vector<StateRange> &group)
{
    for (int g = 0; g < (int)group.size(); g++)
    {
        const StateRange &range = group[g];

        for (int i = range.first; i <= range.last; i++)
        {
            if (states[i].sprite == 0)
                continue;

            if (states[i].flags & kStateFrameFlagModel) // Lobo 2024: check 3d models too?
                return true;

            if (sprites[states[i].sprite]->frames_)
                return true;

            // -AJA- only check one per group.  It _should_ check them all,
            //       however this maintains compatibility.
            break;
        }
    }

    return false;
}

SpriteFrame *GetSpriteFrame(int spr_num, int framenum)
{
    // spr_num comes from the 'sprite' field of State, and
    // is also an index into ddf_sprite_names vector.

    EPI_ASSERT(spr_num > 0);
    EPI_ASSERT(spr_num < sprite_count);
    EPI_ASSERT(framenum >= 0);

    SpriteDefinition *def = sprites[spr_num];

    if (framenum >= def->total_frames_)
        return nullptr;

    SpriteFrame *frame = &def->frames_[framenum];

    if (!frame || !frame->finished_)
        return nullptr;

    return frame;
}

void PrecacheSprites(void)
{
    EPI_ASSERT(sprite_count > 1);

    uint8_t *sprite_present = new uint8_t[sprite_count];
    EPI_CLEAR_MEMORY(sprite_present, uint8_t, sprite_count);

    for (MapObject *mo = map_object_list_head; mo; mo = mo->next_)
    {
        EPI_ASSERT(mo->state_);

        if (mo->state_->sprite < 1 || mo->state_->sprite >= sprite_count)
            continue;

        sprite_present[mo->state_->sprite] = 1;
    }

    for (int i = 1; i < sprite_count; i++) // ignore 0
    {
        SpriteDefinition *def = sprites[i];

        const Image *cur_image;
        const Image *last_image = nullptr; // an optimisation

        if (def->total_frames_ == 0)
            continue;

        // Note: all weapon sprites are pre-cached

        if (!(sprite_present[i] || def->HasWeapon()))
            continue;

        /* Lobo 2022: info overload. Shut up.
                LogDebug("Precaching sprite: %s\n", def->name);
        */

        EPI_ASSERT(def->frames_);

        for (int fr = 0; fr < def->total_frames_; fr++)
        {
            if (!def->frames_[fr].finished_)
                continue;

            for (int rot = 0; rot < 16; rot++)
            {
                cur_image = def->frames_[fr].images_[rot];

                if (cur_image == nullptr || cur_image == last_image)
                    continue;

                ImagePrecache(cur_image);

                last_image = cur_image;
            }
        }
    }

    delete[] sprite_present;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
