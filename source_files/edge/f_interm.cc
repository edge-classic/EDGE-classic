//----------------------------------------------------------------------------
//  EDGE Intermission Code
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
// -KM- 1998/12/16 Nuked half of this for DDF. DOOM 1 works now!
//
// TODO HERE:
//    + have proper styles (for text font and sounds).
//

#include "f_interm.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "f_finale.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "i_system.h"
#include "p_local.h"
#include "r_backend.h"
#include "r_draw.h"
#include "r_misc.h"
#include "r_modes.h"
#include "s_music.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "w_wad.h"

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

// GLOBAL LOCATIONS
static constexpr uint8_t kIntermissionTitleY = 6;

// SINGLE-PLAYER STUFF
static constexpr uint8_t kSinglePlayerStateStatsX = 55;
static constexpr uint8_t kSinglePlayerStateStatsY = 70;
static constexpr uint8_t kSinglePlayerStateTimeX  = 16;
static constexpr uint8_t kSinglePlayerStateTimeY  = (200 - 32);

//
// GENERAL DATA
//

// contains information passed into intermission
IntermissionInfo intermission_stats;

//
// Locally used stuff.
//

static constexpr uint8_t kCycleLocationDelay = 4;

// used to accelerate or skip a stage
static bool accelerate_stage;

// specifies current state
static IntermissionState state;

// used for general timing
static int count;

// used for timing of background animation
static int background_count;

// signals to refresh everything for one frame
static int first_refresh;

static constexpr uint8_t kNumberOfPlayersShown = 10;

static int single_player_state;

static int count_kills[kNumberOfPlayersShown];
static int count_items[kNumberOfPlayersShown];
static int count_secrets[kNumberOfPlayersShown];
static int count_frags[kNumberOfPlayersShown];
static int count_totals[kNumberOfPlayersShown];

static int count_time;
static int count_par;
static int count_pause;

static int deathmatch_state;

static int deathmatch_frags[kNumberOfPlayersShown];
static int deathmatch_totals[kNumberOfPlayersShown];
static int deathmatch_rank[kNumberOfPlayersShown];

static int do_frags;

static int state_ticker_count;

static bool intermission_pointer_on = false;

static Style *single_player_intermission_style;
static Style *multiplayer_intermission_style;

// GRAPHICS

// background
static const Image *background_image;
static const Image *leaving_background_image;
static const Image *entering_background_image;

bool tile_background          = false;
bool tile_leaving_background  = false;
bool tile_entering_background = false;

// You Are Here graphic
static const Image *you_are_here[2] = {nullptr, nullptr};

// splat
static const Image *splat[2] = {nullptr, nullptr};

// %, : graphics
static const Image *percent;
static const Image *colon;

// 0-9 graphic
static const Image *digits[10]; // FIXME: use FONT/STYLE

// minus sign
static const Image *wiminus;

// "Finished!" graphics
static const Image *finished;

// "Entering" graphic
static const Image *entering;

// "secret"
static const Image *single_player_secret;

// "Kills", "Scrt", "Items", "Frags"
static const Image *kills;
static const Image *secret;
static const Image *items;
static const Image *frags;

// Time sucks.
static const Image *time_image; // -ACB- 1999/09/19 Removed Conflict with <time.h>
static const Image *par;
static const Image *sucks;

// "killers", "victims"
static const Image *killers;
static const Image *victims;

// "Total", your face, your dead face
static const Image *total;
static const Image *face;
static const Image *dead_face;

// Name graphics of each level (centered)
static const Image *level_names[2];

//
// -ACB- 2004/06/25 Short-term containers for
//                  the world intermission data
//

struct IntermissionMapPosition
{
    IntermissionMapPositionInfo *info = nullptr;
    bool                         done = false;
};

struct IntermissionFrame
{
    IntermissionFrameInfo *info  = nullptr;
    const Image           *image = nullptr; // cached image
};

class IntermissionAnimation
{
  public:
    IntermissionAnimationInfo *info_;

    // This array doesn't need to built up, so we stick to primitive form
    IntermissionFrame *frames_;
    int                total_frames_;

    int count_;
    int frame_on_;

  public:
    IntermissionAnimation()
    {
        frames_       = nullptr;
        total_frames_ = 0;
    }

    ~IntermissionAnimation()
    {
        Clear();
    }

  private:
    /* ... */

  public:
    void Clear(void)
    {
        if (frames_)
        {
            delete[] frames_;
            frames_ = nullptr;

            total_frames_ = 0;
        }
    }

    void Load(IntermissionAnimationInfo *def)
    {
        int size;

        // Frames...
        size = def->frames_.size();
        if (size > 0)
        {
            int i;

            frames_ = new IntermissionFrame[size];
            for (i = 0; i < size; i++)
                frames_[i].info = def->frames_[i];
        }

        info_         = def;
        total_frames_ = size;
    }

    void Reset(void)
    {
        count_    = 0;
        frame_on_ = -1;
    }
};

class Intermission
{
  public:
    // This array doesn't need to built up, so we stick to primitive form
    IntermissionAnimation *animations_;
    int                    total_animations_;

    // This array doesn't need to built up, so we stick to primitive form
    IntermissionMapPosition *map_positions_;
    int                      total_map_positions_;

  public:
    Intermission()
    {
        game_definition_ = nullptr;

        animations_       = nullptr;
        total_animations_ = 0;

        map_positions_       = nullptr;
        total_map_positions_ = 0;
    }

    ~Intermission()
    {
        Clear();
    }

  private:
    GameDefinition *game_definition_;

    void Clear(void)
    {
        if (animations_)
        {
            delete[] animations_;
            animations_ = nullptr;

            total_animations_ = 0;
        }

        if (map_positions_)
        {
            delete[] map_positions_;
            map_positions_ = nullptr;

            total_map_positions_ = 0;
        }
    }

    void Load(GameDefinition *definition)
    {
        // Animations
        int size = definition->anims_.size();

        if (size > 0)
        {
            animations_ = new IntermissionAnimation[size];

            for (int i = 0; i < size; i++)
                animations_[i].Load(definition->anims_[i]);

            total_animations_ = size;
        }

        // Map positions
        size = definition->mappos_.size();
        if (size > 0)
        {
            map_positions_ = new IntermissionMapPosition[size];

            for (int i = 0; i < size; i++)
                map_positions_[i].info = definition->mappos_[i];

            total_map_positions_ = size;
        }
    }

    void Reset(void)
    {
        for (int i = 0; i < total_animations_; i++)
            animations_[i].Reset();
    }

  public:
    void Init(GameDefinition *definition)
    {
        if (definition != game_definition_)
        {
            // Clear
            Clear();

            if (definition)
                Load(definition);
        }

        if (definition)
            Reset();

        game_definition_ = definition;
        return;
    }
};

static Intermission world_intermission;

//
// CODE
//

void IntermissionClear(void)
{
    world_intermission.Init(nullptr);
}

// Draws "<Levelname> Finished!"
static void DrawLevelFinished(void)
{
    // draw <LevelName>
    // EPI_ASSERT(level_names[0]);

    // Lobo 2022: if we have a per level image defined, use that instead
    if (leaving_background_image)
    {
        if (tile_leaving_background)
            HUDTileImage(-240, 0, 820, 200, leaving_background_image);
        else
        {
            if (title_scaling.d_) // Fill Border
            {
                if (!leaving_background_image->blurred_version_)
                    StoreBlurredImage(leaving_background_image);
                HUDStretchImage(-320, -200, 960, 600, leaving_background_image->blurred_version_, 0, 0);
            }
            HUDDrawImageTitleWS(leaving_background_image);
        }
    }

    float y  = kIntermissionTitleY;
    float w1 = 160;
    float h1 = 15;

    // load styles
    Style *style;
    style      = single_player_intermission_style;
    int t_type = StyleDefinition::kTextSectionText;

    HUDSetAlignment(0, -1); // center it

    // If we have a custom mapname graphic e.g.CWILVxx then use that
    if (level_names[0])
    {
        if (IsLumpInPwad(level_names[0]->name_.c_str()))
        {
            w1 = level_names[0]->ScaledWidth();
            h1 = level_names[0]->ScaledHeight();
            HUDSetAlignment(-1, -1); // center it
            if (w1 > 320)            // Too big? Shrink it to fit the screen
                HUDStretchImage(0, y, 320, h1, level_names[0], 0.0, 0.0);
            else
                HUDDrawImage(160 - w1 / 2, y, level_names[0]);
        }
        else
        {
            h1 = style->fonts_[t_type]->NominalHeight();

            float txtscale = 1.0;
            if (style->definition_->text_[t_type].scale_)
            {
                txtscale = style->definition_->text_[t_type].scale_;
            }
            int txtWidth = 0;
            txtWidth =
                style->fonts_[t_type]->StringWidth(language[intermission_stats.current_level->description_.c_str()]) *
                txtscale;

            if (txtWidth > 320) // Too big? Shrink it to fit the screen
            {
                float TempScale = 0;
                TempScale       = 310;
                TempScale /= txtWidth;
                HUDWriteText(style, t_type, 160, y, language[intermission_stats.current_level->description_.c_str()],
                             TempScale);
            }
            else
                HUDWriteText(style, t_type, 160, y, language[intermission_stats.current_level->description_.c_str()]);
        }
    }
    else
    {
        h1 = style->fonts_[t_type]->NominalHeight();

        float txtscale = 1.0;
        if (style->definition_->text_[t_type].scale_)
        {
            txtscale = style->definition_->text_[t_type].scale_;
        }
        int txtWidth = 0;
        txtWidth =
            style->fonts_[t_type]->StringWidth(language[intermission_stats.current_level->description_.c_str()]) *
            txtscale;

        if (txtWidth > 320) // Too big? Shrink it to fit the screen
        {
            float TempScale = 0;
            TempScale       = 310;
            TempScale /= txtWidth;
            HUDWriteText(style, t_type, 160, y, language[intermission_stats.current_level->description_.c_str()],
                         TempScale);
        }
        else
            HUDWriteText(style, t_type, 160, y, language[intermission_stats.current_level->description_.c_str()]);
    }
    HUDSetAlignment(-1, -1); // set it back to usual

    t_type = StyleDefinition::kTextSectionTitle;
    if (!style->fonts_[t_type])
        t_type = StyleDefinition::kTextSectionText;

    float y_shift = style->fonts_[t_type]->GetYShift(); // * txtscale;

    y = y + h1;
    y += y_shift;

    HUDSetAlignment(0, -1); // center it
    // If we have a custom Finished graphic e.g.WIF then use that
    if (IsLumpInPwad(finished->name_.c_str()))
    {
        w1 = finished->ScaledWidth();
        h1 = finished->ScaledHeight();
        HUDSetAlignment(-1, -1); // center it
        HUDDrawImage(160 - w1 / 2, y * 5 / 4, finished);
    }
    else
    {
        h1 = style->fonts_[t_type]->NominalHeight();
        HUDWriteText(style, t_type, 160, y * 5 / 4, language["IntermissionFinished"]);
    }
    HUDSetAlignment(-1, -1); // set it back to usual
}

static void DrawOnLnode(IntermissionMapPosition *mappos, const Image *images[2])
{
    int i;

    // -AJA- this is used to select between Left and Right pointing
    // arrows (WIURH0 and WIURH1).  Smells like a massive HACK.

    for (i = 0; i < 2; i++)
    {
        if (images[i] == nullptr)
            continue;

        float left = mappos->info->x_ - images[i]->ScaledOffsetX();
        float top  = mappos->info->y_ - images[i]->ScaledOffsetY();

        float right  = left + images[i]->ScaledWidth();
        float bottom = top + images[i]->ScaledHeight();

        if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
        {
            /* this one fits on the screen */
            break;
        }
    }

    if (i < 2)
    {
        HUDDrawImage(mappos->info->x_, mappos->info->y_, images[i]);
    }
    else
    {
        LogDebug("Could not place patch on level '%s'\n", mappos->info->name_.c_str());
    }
}

// Draws "Entering <LevelName>"
static void DrawEnteringLevel(void)
{
    // -KM- 1998/11/25 If there is no level to enter, don't draw it.
    //      (Stop Map30 from crashing)
    // -Lobo- 2022 Seems we don't need this check anymore
    /*
    if (! level_names[1])
        return;
    */
    if (!intermission_stats.next_level)
        return;

    // Lobo 2022: if we have a per level image defined, use that instead
    if (entering_background_image)
    {
        if (tile_entering_background)
            HUDTileImage(-240, 0, 820, 200, entering_background_image);
        else
        {
            if (title_scaling.d_) // Fill Border
            {
                if (!entering_background_image->blurred_version_)
                    StoreBlurredImage(entering_background_image);
                HUDStretchImage(-320, -200, 960, 600, entering_background_image->blurred_version_, 0, 0);
            }
            HUDDrawImageTitleWS(entering_background_image);
        }
    }

    float y  = kIntermissionTitleY;
    float w1 = 160;
    float h1 = 15;

    Style *style;
    style      = single_player_intermission_style;
    int t_type = StyleDefinition::kTextSectionTitle;
    if (!style->fonts_[t_type])
        t_type = StyleDefinition::kTextSectionText;

    HUDSetAlignment(0, -1); // center it

    // If we have a custom Entering graphic e.g.WIENTER then use that
    if (IsLumpInPwad(entering->name_.c_str()))
    {
        w1 = entering->ScaledWidth();
        h1 = entering->ScaledHeight();
        HUDSetAlignment(-1, -1); // center it
        HUDDrawImage(160 - w1 / 2, y, entering);
    }
    else
    {
        h1 = style->fonts_[t_type]->NominalHeight();
        HUDWriteText(style, t_type, 160, y, language["IntermissionEntering"]);
    }
    HUDSetAlignment(-1, -1); // set it back to usual

    for (int i = 0; i < world_intermission.total_map_positions_; i++)
    {
        if (world_intermission.map_positions_[i].done)
            DrawOnLnode(&world_intermission.map_positions_[i], splat);

        if (intermission_pointer_on &&
            !epi::StringCompare(intermission_stats.next_level->name_, world_intermission.map_positions_[i].info->name_))
            DrawOnLnode(&world_intermission.map_positions_[i], you_are_here);
    }

    float y_shift = style->fonts_[t_type]->GetYShift(); // * txtscale;

    y = y + h1;
    y += y_shift;

    t_type = StyleDefinition::kTextSectionText;

    HUDSetAlignment(0, -1); // center it

    // If we have a custom mapname graphic e.g.CWILVxx then use that
    if (level_names[1])
    {
        if (IsLumpInPwad(level_names[1]->name_.c_str()))
        {
            w1 = level_names[1]->ScaledWidth();
            h1 = level_names[1]->ScaledHeight();
            HUDSetAlignment(-1, -1); // center it
            if (w1 > 320)            // Too big? Shrink it to fit the screen
                HUDStretchImage(0, y * 5 / 4, 320, h1, level_names[1], 0.0, 0.0);
            else
                HUDDrawImage(160 - w1 / 2, y * 5 / 4, level_names[1]);
        }
        else
        {
            float txtscale = 1.0;
            if (style->definition_->text_[t_type].scale_)
            {
                txtscale = style->definition_->text_[t_type].scale_;
            }
            int txtWidth = 0;
            txtWidth =
                style->fonts_[t_type]->StringWidth(language[intermission_stats.next_level->description_.c_str()]) *
                txtscale;

            if (txtWidth > 320) // Too big? Shrink it to fit the screen
            {
                float TempScale = 0;
                TempScale       = 310;
                TempScale /= txtWidth;
                HUDWriteText(style, t_type, 160, y * 5 / 4,
                             language[intermission_stats.next_level->description_.c_str()], TempScale);
            }
            else
                HUDWriteText(style, t_type, 160, y * 5 / 4,
                             language[intermission_stats.next_level->description_.c_str()]);
        }
    }
    else
    {
        float txtscale = 1.0;
        if (style->definition_->text_[t_type].scale_)
        {
            txtscale = style->definition_->text_[t_type].scale_;
        }
        int txtWidth = 0;
        txtWidth = style->fonts_[t_type]->StringWidth(language[intermission_stats.next_level->description_.c_str()]) *
                   txtscale;

        if (txtWidth > 320) // Too big? Shrink it to fit the screen
        {
            float TempScale = 0;
            TempScale       = 310;
            TempScale /= txtWidth;
            HUDWriteText(style, t_type, 160, y * 5 / 4, language[intermission_stats.next_level->description_.c_str()],
                         TempScale);
        }
        else
            HUDWriteText(style, t_type, 160, y * 5 / 4, language[intermission_stats.next_level->description_.c_str()]);
    }
    HUDSetAlignment(-1, -1); // set it back to usual
}

static float PercentWidth(std::string &s)
{
    float perc_width = 0;
    for (auto c : s)
    {
        if (c == '%')
        {
            perc_width += percent->ScaledWidth();
        }
        else if (epi::IsDigitASCII(c))
        {
            perc_width += digits[c - 48]->ScaledWidth();
        }
    }
    return perc_width;
}

static void DrawPercent(float x, float y, std::string &s)
{
    for (auto c : s)
    {
        if (c == '%')
        {
            HUDDrawImage(x, y, percent);
            x += percent->ScaledWidth();
        }
        else if (epi::IsDigitASCII(c))
        {
            HUDDrawImage(x, y, digits[c - 48]);
            x += digits[c - 48]->ScaledWidth();
        }
    }
}

//
// Calculate width of time message
//
static float TimeWidth(int t, bool drawText = false)
{
    if (t < 0)
        return 0;

    std::string s;
    int         seconds, hours, minutes;
    minutes = t / 60;
    seconds = t % 60;
    hours   = minutes / 60;
    minutes = minutes % 60;
    s       = "";
    if (hours > 0)
    {
        if (hours > 9)
            s = s + std::to_string(hours) + ":";
        else
            s = s + "0" + std::to_string(hours) + ":";
    }
    if (minutes > 0)
    {
        if (minutes > 9)
            s = s + std::to_string(minutes);
        else
            s = s + "0" + std::to_string(minutes);
    }
    if (seconds > 0 || minutes > 0)
    {
        if (seconds > 9)
            s = s + ":" + std::to_string(seconds);
        else
            s = s + ":" + "0" + std::to_string(seconds);
    }

    if (drawText == true)
    {
        float txtscale =
            single_player_intermission_style->definition_->text_[StyleDefinition::kTextSectionAlternate].scale_;

        if (t > 3599)
        {
            return single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                       "Sucks") *
                   txtscale;
        }
        else
        {
            return single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                       s.c_str()) *
                   txtscale;
        }
    }
    else
    {
        if (t > 3599)
        {
            // "sucks"
            if ((sucks) && (IsLumpInPwad(sucks->name_.c_str())))
                return sucks->ScaledWidth();
            else
                return single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                    "Sucks");
        }
        else
        {
            float time_width = 0;
            for (auto c : s)
            {
                if (c == ':')
                {
                    time_width += colon->ScaledWidth();
                }
                else if (epi::IsDigitASCII(c))
                {
                    time_width += digits[c - 48]->ScaledWidth();
                }
            }
            return time_width;
        }
    }
}

//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
static void DrawTime(float x, float y, int t, bool drawText = false)
{
    if (t < 0)
        return;

    std::string s;
    int         seconds, hours, minutes;
    minutes = t / 60;
    seconds = t % 60;
    hours   = minutes / 60;
    minutes = minutes % 60;
    s       = "";
    if (hours > 0)
    {
        if (hours > 9)
            s = s + std::to_string(hours) + ":";
        else
            s = s + "0" + std::to_string(hours) + ":";
    }
    if (minutes > 0)
    {
        if (minutes > 9)
            s = s + std::to_string(minutes);
        else
            s = s + "0" + std::to_string(minutes);
    }
    if (seconds > 0 || minutes > 0)
    {
        if (seconds > 9)
            s = s + ":" + std::to_string(seconds);
        else
            s = s + ":" + "0" + std::to_string(seconds);
    }

    if (drawText == true)
    {
        if (t > 3599)
        {
            HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionTitle, x, y, "Sucks");
        }
        else
        {
            HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, x, y, s.c_str());
        }
    }
    else
    {
        if (t > 3599)
        {
            // "sucks"
            if ((sucks) && (IsLumpInPwad(sucks->name_.c_str())))
                HUDDrawImage(x, y, sucks);
            else
                HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionTitle, x, y, "Sucks");
        }
        else
        {
            for (auto c : s)
            {
                if (c == ':')
                {
                    HUDDrawImage(x, y, colon);
                    x += colon->ScaledWidth();
                }
                else if (epi::IsDigitASCII(c))
                {
                    HUDDrawImage(x, y, digits[c - 48]);
                    x += digits[c - 48]->ScaledWidth();
                }
            }
        }
    }
}

static void IntermissionEnd(void)
{
    ForceWipe();

    background_camera_map_object = nullptr;

    FinaleStart(&current_map->f_end_, next_map ? kGameActionFinale : kGameActionNothing);
}

static void NoStateInit(void)
{
    state            = kIntermissionStateNone;
    accelerate_stage = false;
    count            = 10;
}

static void UpdateNoState(void)
{
    count--;

    if (count == 0)
    {
        IntermissionEnd();
    }
}

static void ShowNextLocationInit(void)
{
    int i;

    state            = kIntermissionStateShowNextLocation;
    accelerate_stage = false;

    for (i = 0; i < world_intermission.total_map_positions_; i++)
    {
        if (epi::StringCompare(world_intermission.map_positions_[i].info->name_,
                               intermission_stats.current_level->name_) == 0)
            world_intermission.map_positions_[i].done = true;
    }

    count = kCycleLocationDelay * kTicRate;
}

static void UpdateShowNextLocation(void)
{
    if (!--count || accelerate_stage)
        NoStateInit();
    else
        intermission_pointer_on = (count & 31) < 20;
}

static void DrawShowNextLocation(void)
{
    if (intermission_stats.next_level)
        DrawEnteringLevel();
    else
    {
        for (int i = 0; i < world_intermission.total_map_positions_; i++)
        {
            if (world_intermission.map_positions_[i].done)
                DrawOnLnode(&world_intermission.map_positions_[i], splat);

            if (intermission_pointer_on && intermission_stats.next_level &&
                !epi::StringCompare(intermission_stats.next_level->name_,
                                    world_intermission.map_positions_[i].info->name_))
                DrawOnLnode(&world_intermission.map_positions_[i], you_are_here);
        }
    }
}

static void DrawNoState(void)
{
    intermission_pointer_on = true;
    DrawShowNextLocation();
}

static void SortRanks(int *rank, int *score)
{
    // bubble sort the rank list
    bool done = false;

    while (!done)
    {
        done = true;

        for (int i = 0; i < kMaximumPlayers - 1; i++)
        {
            if (score[i] < score[i + 1])
            {
                int tmp      = score[i];
                score[i]     = score[i + 1];
                score[i + 1] = tmp;

                tmp         = rank[i];
                rank[i]     = rank[i + 1];
                rank[i + 1] = tmp;

                done = false;
            }
        }
    }
}

static int DeathmatchScore(int pl)
{
    if (pl >= 0)
    {
        return players[pl]->total_frags_ * 2 + players[pl]->frags_;
    }

    return -999;
}

static void InitDeathmatchStats(void)
{
    EPI_ASSERT(kNumberOfPlayersShown <= kMaximumPlayers);

    state            = kIntermissionStateStatScreen;
    accelerate_stage = false;
    deathmatch_state = 1;

    count_pause = kTicRate;

    int rank[kMaximumPlayers];
    int score[kMaximumPlayers];

    int i;

    for (i = 0; i < kMaximumPlayers; i++)
    {
        rank[i]  = players[i] ? i : -1;
        score[i] = DeathmatchScore(rank[i]);
    }

    SortRanks(rank, score);

    for (i = 0; i < kNumberOfPlayersShown; i++)
    {
        deathmatch_frags[i] = deathmatch_totals[i] = 0;
        deathmatch_rank[i]                         = rank[i];
    }
}

static void UpdateDeathmatchStats(void)
{
    bool stillticking;

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    if (accelerate_stage && deathmatch_state != 4)
    {
        accelerate_stage = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            deathmatch_frags[i]  = players[p]->frags_;
            deathmatch_totals[i] = players[p]->total_frags_;
        }

        StartSoundEffect(gd->done_);
        deathmatch_state = 4;
    }

    switch (deathmatch_state)
    {
    case 2:
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        stillticking = false;
        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            if (deathmatch_frags[i] < players[p]->frags_)
            {
                deathmatch_frags[i]++;
                stillticking = true;
            }
            if (deathmatch_totals[i] < players[p]->total_frags_)
            {
                deathmatch_totals[i]++;
                stillticking = true;
            }
        }

        if (!stillticking)
        {
            StartSoundEffect(gd->done_);
            deathmatch_state++;
        }
        break;

    case 4:
        if (accelerate_stage)
        {
            StartSoundEffect(gd->accel_snd_);

            // Skip next loc on no map -ACB- 2004/06/27
            if (!world_intermission.total_map_positions_ || !intermission_stats.next_level)
                NoStateInit();
            else
                ShowNextLocationInit();
        }
        break;

    default:
        if (!--count_pause)
        {
            deathmatch_state++;
            count_pause = kTicRate;
        }
        break;
    }
}

static void DrawDeathmatchStats(void)
{
    DrawLevelFinished();

    int t_type = StyleDefinition::kTextSectionTitle;
    int y      = kSinglePlayerStateStatsY; // 40;

    HUDWriteText(multiplayer_intermission_style, t_type, 20, y, "Player");
    HUDWriteText(multiplayer_intermission_style, t_type, 100, y, "Frags");
    HUDWriteText(multiplayer_intermission_style, t_type, 200, y, "Total");

    for (int i = 0; i < kNumberOfPlayersShown; i++)
    {
        int p = deathmatch_rank[i];

        if (p < 0)
            break;

        y += 12;

        t_type = StyleDefinition::kTextSectionText;

        // hightlight the console player
#if 1
        if (p == console_player)
            t_type = StyleDefinition::kTextSectionAlternate;
#else
        if (p == consoleplayer && ((background_count & 31) < 16))
            continue;
#endif

        char temp[40];

        stbsp_sprintf(temp, "%s", players[p]->player_name_);
        HUDWriteText(multiplayer_intermission_style, t_type, 20, y, temp);

        stbsp_sprintf(temp, "%5d", deathmatch_frags[i]);
        HUDWriteText(multiplayer_intermission_style, t_type, 100, y, temp);

        stbsp_sprintf(temp, "%11d", deathmatch_totals[i]);
        HUDWriteText(multiplayer_intermission_style, t_type, 200, y, temp);
    }
}

// Calculates value of this player for ranking
static int CoopScore(int pl)
{
    if (pl >= 0)
    {
        int coop_kills  = players[pl]->kill_count_ * 400 / intermission_stats.kills;
        int coop_items  = players[pl]->item_count_ * 100 / intermission_stats.items;
        int coop_secret = players[pl]->secret_count_ * 200 / intermission_stats.secrets;
        int coop_frags  = (players[pl]->frags_ + players[pl]->total_frags_) * 25;

        return coop_kills + coop_items + coop_secret - coop_frags;
    }

    return -999;
}

static void InitCoopStats(void)
{
    EPI_ASSERT(kNumberOfPlayersShown <= kMaximumPlayers);

    state              = kIntermissionStateStatScreen;
    accelerate_stage   = false;
    state_ticker_count = 1;

    count_pause = kTicRate;

    int rank[kMaximumPlayers];
    int score[kMaximumPlayers];

    int i;

    for (i = 0; i < kMaximumPlayers; i++)
    {
        rank[i]  = players[i] ? i : -1;
        score[i] = CoopScore(rank[i]);
    }

    SortRanks(rank, score);

    do_frags = 0;

    for (i = 0; i < kNumberOfPlayersShown; i++)
    {
        deathmatch_rank[i] = rank[i];

        if (deathmatch_rank[i] < 0)
            continue;

        count_kills[i] = count_items[i] = count_secrets[i] = count_frags[i] = count_totals[i] = 0;

        do_frags += players[deathmatch_rank[i]]->frags_ + players[deathmatch_rank[i]]->total_frags_;
    }
}

static void UpdateCoopStats(void)
{
    bool stillticking;

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    if (accelerate_stage && state_ticker_count != 10)
    {
        accelerate_stage = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            count_kills[i]   = (players[p]->kill_count_ * 100) / intermission_stats.kills;
            count_items[i]   = (players[p]->item_count_ * 100) / intermission_stats.items;
            count_secrets[i] = (players[p]->secret_count_ * 100) / intermission_stats.secrets;

            if (do_frags)
            {
                count_frags[i]  = players[p]->frags_;
                count_totals[i] = players[p]->total_frags_;
            }
        }

        StartSoundEffect(gd->done_);
        state_ticker_count = 10;
    }

    switch (state_ticker_count)
    {
    case 2:
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        stillticking = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            count_kills[i] += 2;

            if (count_kills[i] >= (players[p]->kill_count_ * 100) / intermission_stats.kills)
                count_kills[i] = (players[p]->kill_count_ * 100) / intermission_stats.kills;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            StartSoundEffect(gd->done_);
            state_ticker_count++;
        }
        break;

    case 4:
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        stillticking = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            count_items[i] += 2;
            if (count_items[i] >= (players[p]->item_count_ * 100) / intermission_stats.items)
                count_items[i] = (players[p]->item_count_ * 100) / intermission_stats.items;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            StartSoundEffect(gd->done_);
            state_ticker_count++;
        }
        break;

    case 6:
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        stillticking = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            count_secrets[i] += 2;

            if (count_secrets[i] >= (players[p]->secret_count_ * 100) / intermission_stats.secrets)
                count_secrets[i] = (players[p]->secret_count_ * 100) / intermission_stats.secrets;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            StartSoundEffect(gd->done_);
            state_ticker_count += 1 + 2 * !do_frags;
        }
        break;

    case 8:
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        stillticking = false;

        for (int i = 0; i < kNumberOfPlayersShown; i++)
        {
            int p = deathmatch_rank[i];

            if (p < 0)
                break;

            count_frags[i]++;
            count_totals[i]++;

            if (count_frags[i] >= players[p]->frags_)
                count_frags[i] = players[p]->frags_;
            else if (count_totals[i] >= players[p]->total_frags_)
                count_totals[i] = players[p]->total_frags_;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            StartSoundEffect(gd->frag_snd_);
            state_ticker_count++;
        }
        break;

    case 10:
        if (accelerate_stage)
        {
            StartSoundEffect(gd->next_map_);

            // Skip next loc on no map -ACB- 2004/06/27
            if (!world_intermission.total_map_positions_ || !intermission_stats.next_level)
                NoStateInit();
            else
                ShowNextLocationInit();
        }
        if (!--count_pause)
        {
            state_ticker_count++;
            count_pause = kTicRate;
        }
        break;

    default:
        if (!--count_pause)
        {
            state_ticker_count++;
            count_pause = kTicRate;
        }
        break;
    }
}

static void DrawCoopStats(void)
{
    DrawLevelFinished();

    int t_type = StyleDefinition::kTextSectionTitle;
    int y      = kSinglePlayerStateStatsY; // 40;

    // FIXME: better alignment

    HUDWriteText(multiplayer_intermission_style, t_type, 6, y, "Player");
    HUDWriteText(multiplayer_intermission_style, t_type, 56, y, language["IntermissionKills"]);
    HUDWriteText(multiplayer_intermission_style, t_type, 98, y, language["IntermissionItems"]);
    HUDWriteText(multiplayer_intermission_style, t_type, 142, y, language["IntermissionSecrets"]);

    if (do_frags)
    {
        HUDWriteText(multiplayer_intermission_style, t_type, 190, y, "Frags");
        HUDWriteText(multiplayer_intermission_style, t_type, 232, y, "Total");
    }

    for (int i = 0; i < kNumberOfPlayersShown; i++)
    {
        int p = deathmatch_rank[i];

        if (p < 0)
            break;

        y += 12;

        t_type = StyleDefinition::kTextSectionText;

        // highlight the console player
#if 1
        if (p == console_player)
            t_type = StyleDefinition::kTextSectionAlternate;
#else
        if (p == consoleplayer && ((background_count & 31) < 16))
            continue;
#endif

        char temp[40];

        stbsp_sprintf(temp, "%s", players[p]->player_name_);
        HUDWriteText(multiplayer_intermission_style, t_type, 6, y, temp);

        stbsp_sprintf(temp, "%3d%%", count_kills[i]);
        HUDWriteText(multiplayer_intermission_style, t_type, 64, y, temp);

        stbsp_sprintf(temp, "%3d%%", count_items[i]);
        HUDWriteText(multiplayer_intermission_style, t_type, 106, y, temp);

        stbsp_sprintf(temp, "%3d%%", count_secrets[i]);
        HUDWriteText(multiplayer_intermission_style, t_type, 158, y, temp);

        if (do_frags)
        {
            stbsp_sprintf(temp, "%5d", count_frags[i]);
            HUDWriteText(multiplayer_intermission_style, t_type, 190, y, temp);

            stbsp_sprintf(temp, "%11d", count_totals[i]);
            HUDWriteText(multiplayer_intermission_style, t_type, 232, y, temp);
        }
    }
}

enum SinglePlayerState
{
    kSinglePlayerStatePaused  = 1,
    kSinglePlayerStateKills   = 2,
    kSinglePlayerStateItems   = 4,
    kSinglePlayerStateSecrets = 6,
    kSinglePlayerStateTime    = 8,
    kSinglePlayerStateEnd     = 10
};

static void InitSinglePlayerStats(void)
{
    state               = kIntermissionStateStatScreen;
    accelerate_stage    = false;
    single_player_state = kSinglePlayerStatePaused;
    count_kills[0] = count_items[0] = count_secrets[0] = -1;
    count_time = count_par = -1;
    count_pause            = kTicRate;
}

static void UpdateSinglePlayerStats(void)
{
    Player *con_plyr = players[console_player];

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    if (accelerate_stage && single_player_state != kSinglePlayerStateEnd)
    {
        accelerate_stage = false;
        count_kills[0]   = (con_plyr->kill_count_ * 100) / intermission_stats.kills;
        count_items[0]   = (con_plyr->item_count_ * 100) / intermission_stats.items;
        count_secrets[0] = (con_plyr->secret_count_ * 100) / intermission_stats.secrets;
        count_time       = con_plyr->level_time_ / kTicRate;
        count_par        = intermission_stats.par_time / kTicRate;
        StartSoundEffect(gd->done_);
        single_player_state = kSinglePlayerStateEnd;
    }

    if (single_player_state == kSinglePlayerStateKills)
    {
        count_kills[0] += 2;

        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        if (count_kills[0] >= (con_plyr->kill_count_ * 100) / intermission_stats.kills)
        {
            count_kills[0] = (con_plyr->kill_count_ * 100) / intermission_stats.kills;
            StartSoundEffect(gd->done_);
            single_player_state++;
        }
    }
    else if (single_player_state == kSinglePlayerStateItems)
    {
        count_items[0] += 2;

        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        if (count_items[0] >= (con_plyr->item_count_ * 100) / intermission_stats.items)
        {
            count_items[0] = (con_plyr->item_count_ * 100) / intermission_stats.items;
            StartSoundEffect(gd->done_);
            single_player_state++;
        }
    }
    else if (single_player_state == kSinglePlayerStateSecrets)
    {
        count_secrets[0] += 2;

        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        if (count_secrets[0] >= (con_plyr->secret_count_ * 100) / intermission_stats.secrets)
        {
            count_secrets[0] = (con_plyr->secret_count_ * 100) / intermission_stats.secrets;
            StartSoundEffect(gd->done_);
            single_player_state++;
        }
    }

    else if (single_player_state == kSinglePlayerStateTime)
    {
        if (!(background_count & 3))
            StartSoundEffect(gd->percent_);

        count_time += 3;

        if (count_time >= con_plyr->level_time_ / kTicRate)
            count_time = con_plyr->level_time_ / kTicRate;

        count_par += 3;

        if (count_par >= intermission_stats.par_time / kTicRate)
        {
            count_par = intermission_stats.par_time / kTicRate;

            if (count_time >= con_plyr->level_time_ / kTicRate)
            {
                StartSoundEffect(gd->done_);
                single_player_state++;
            }
        }
    }
    else if (single_player_state == kSinglePlayerStateEnd)
    {
        if (accelerate_stage)
        {
            StartSoundEffect(gd->next_map_);

            // Skip next loc on no map -ACB- 2004/06/27
            if (!world_intermission.total_map_positions_ || !intermission_stats.next_level)
                NoStateInit();
            else
                ShowNextLocationInit();
        }
    }
    else if (single_player_state & kSinglePlayerStatePaused)
    {
        if (!--count_pause)
        {
            single_player_state++;
            count_pause = kTicRate;
        }
    }
}

static void DrawSinglePlayerStats(void)
{
    // line height
    float lh = digits[0]->ScaledHeight() * 3 / 2;

    DrawLevelFinished();

    bool drawTextBased = true;
    if (kills != nullptr)
    {
        if (IsLumpInPwad(kills->name_.c_str()))
            drawTextBased = false;
        else
            drawTextBased = true;
    }
    else
    {
        drawTextBased = true;
    }

    std::string s;
    if (count_kills[0] < 0)
        s.clear();
    else
    {
        s = std::to_string(count_kills[0]);
        s = s + "%";
    }

    if (drawTextBased == false)
    {
        HUDDrawImage(kSinglePlayerStateStatsX, kSinglePlayerStateStatsY, kills);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s), kSinglePlayerStateStatsY, s);
    }
    else
    {
        HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, kSinglePlayerStateStatsX,
                     kSinglePlayerStateStatsY, language["IntermissionKills"]);
        if (!s.empty())
            HUDWriteText(
                single_player_intermission_style, StyleDefinition::kTextSectionAlternate,
                320 - kSinglePlayerStateStatsX -
                    single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                        s.c_str()),
                kSinglePlayerStateStatsY, s.c_str());
    }

    if (count_items[0] < 0)
        s.clear();
    else
    {
        s = std::to_string(count_items[0]);
        s = s + "%";
    }

    if ((items) && (IsLumpInPwad(items->name_.c_str())))
    {
        HUDDrawImage(kSinglePlayerStateStatsX, kSinglePlayerStateStatsY + lh, items);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s), kSinglePlayerStateStatsY + lh, s);
    }
    else
    {
        HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, kSinglePlayerStateStatsX,
                     kSinglePlayerStateStatsY + lh, language["IntermissionItems"]);
        if (!s.empty())
            HUDWriteText(
                single_player_intermission_style, StyleDefinition::kTextSectionAlternate,
                320 - kSinglePlayerStateStatsX -
                    single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                        s.c_str()),
                kSinglePlayerStateStatsY + lh, s.c_str());
    }

    if (count_secrets[0] < 0)
        s.clear();
    else
    {
        s = std::to_string(count_secrets[0]);
        s = s + "%";
    }

    if ((single_player_secret) && (IsLumpInPwad(single_player_secret->name_.c_str())))
    {
        HUDDrawImage(kSinglePlayerStateStatsX, kSinglePlayerStateStatsY + 2 * lh, single_player_secret);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s), kSinglePlayerStateStatsY + 2 * lh, s);
    }
    else
    {
        HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, kSinglePlayerStateStatsX,
                     kSinglePlayerStateStatsY + 2 * lh, language["IntermissionSecrets"]);
        if (!s.empty())
            HUDWriteText(
                single_player_intermission_style, StyleDefinition::kTextSectionAlternate,
                320 - kSinglePlayerStateStatsX -
                    single_player_intermission_style->fonts_[StyleDefinition::kTextSectionAlternate]->StringWidth(
                        s.c_str()),
                kSinglePlayerStateStatsY + 2 * lh, s.c_str());
    }

    if ((time_image) && (IsLumpInPwad(time_image->name_.c_str())))
    {
        HUDDrawImage(kSinglePlayerStateTimeX, kSinglePlayerStateTimeY, time_image);
        DrawTime(160 - kSinglePlayerStateTimeX - TimeWidth(count_time), kSinglePlayerStateTimeY, count_time);
    }
    else
    {
        HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, kSinglePlayerStateTimeX,
                     kSinglePlayerStateTimeY, language["IntermissionTime"]);
        DrawTime(160 - kSinglePlayerStateTimeX - TimeWidth(count_time, true), kSinglePlayerStateTimeY, count_time,
                 true);
    }

    // -KM- 1998/11/25 Removed episode check. Replaced with partime check
    if (intermission_stats.par_time)
    {
        if ((par) && (IsLumpInPwad(par->name_.c_str())))
        {
            HUDDrawImage(170, kSinglePlayerStateTimeY, par);
            DrawTime(320 - kSinglePlayerStateTimeX - TimeWidth(count_par), kSinglePlayerStateTimeY, count_par);
        }
        else
        {
            HUDWriteText(single_player_intermission_style, StyleDefinition::kTextSectionAlternate, 170,
                         kSinglePlayerStateTimeY, "Par");
            DrawTime(320 - kSinglePlayerStateTimeX - TimeWidth(count_par, true), kSinglePlayerStateTimeY, count_par,
                     true);
        }
    }
}

bool IntermissionCheckForAccelerate(void)
{
    bool do_accel = false;

    // check for button presses to skip delays
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *player = players[pnum];
        if (!player)
            continue;

        if (player->command_.buttons & kButtonCodeAttack)
        {
            if (!player->attack_button_down_[0])
            {
                player->attack_button_down_[0] = true;
                do_accel                       = true;
            }
        }
        else
            player->attack_button_down_[0] = false;

        if (player->command_.buttons & kButtonCodeUse)
        {
            if (!player->use_button_down_)
            {
                player->use_button_down_ = true;
                do_accel                 = true;
            }
        }
        else
            player->use_button_down_ = false;
    }

    return do_accel;
}

void IntermissionTicker(void)
{
    // Updates stuff each tick

    EPI_ASSERT(game_state == kGameStateIntermission);

    int i;

    // counter for general background animation
    background_count++;

    if (background_count == 1)
    {
        // intermission music
        ChangeMusic(intermission_stats.current_level->episode_->music_, true);
    }

    if (IntermissionCheckForAccelerate())
        accelerate_stage = true;

    for (i = 0; i < world_intermission.total_animations_; i++)
    {
        if (world_intermission.animations_[i].count_ >= 0)
        {
            if (!world_intermission.animations_[i].count_)
            {
                world_intermission.animations_[i].frame_on_ =
                    (world_intermission.animations_[i].frame_on_ + 1) % world_intermission.animations_[i].total_frames_;
                world_intermission.animations_[i].count_ =
                    world_intermission.animations_[i].frames_[world_intermission.animations_[i].frame_on_].info->tics_;
            }
            world_intermission.animations_[i].count_--;
        }
    }

    switch (state)
    {
    case kIntermissionStateStatScreen:
        if (InSinglePlayerMatch())
            UpdateSinglePlayerStats();
        else if (InDeathmatch())
            UpdateDeathmatchStats();
        else
            UpdateCoopStats();
        break;

    case kIntermissionStateShowNextLocation:
        UpdateShowNextLocation();
        break;

    case kIntermissionStateNone:
        UpdateNoState();
        break;
    }
}

void IntermissionDrawer(void)
{
    EPI_ASSERT(game_state == kGameStateIntermission);

    HUDReset();

    if (background_camera_map_object)
    {
        HUDRenderWorld(0, 0, 320, 200, background_camera_map_object, 0);
#ifndef EDGE_SOKOL
        // Dasho - Need to setup the 2D matrics for legacy GL else the intermission
        // stats won't be drawn right
        render_backend->SetRenderLayer(kRenderLayerHUD);
#endif
    }
    else
    {
        // HUDStretchImage(0, 0, 320, 200, background_image);
        if (background_image)
        {
            if (tile_background)
                HUDTileImage(-240, 0, 820, 200,
                             background_image); // Lobo: Widescreen support
            else
            {
                if (title_scaling.d_)           // Fill Border
                {
                    if (!background_image->blurred_version_)
                        StoreBlurredImage(background_image);
                    HUDStretchImage(-320, -200, 960, 600, background_image->blurred_version_, 0, 0);
                }
                HUDDrawImageTitleWS(background_image);
            }

            for (int i = 0; i < world_intermission.total_animations_; i++)
            {
                IntermissionAnimation *a = &world_intermission.animations_[i];

                if (a->frame_on_ == -1)
                    continue;

                IntermissionFrame *f = nullptr;

                if (a->info_->type_ == IntermissionAnimationInfo::kIntermissionAnimationInfoLevel)
                {
                    if (!intermission_stats.next_level)
                        f = nullptr;
                    else if (!epi::StringCompare(intermission_stats.next_level->name_, a->info_->level_))
                        f = &a->frames_[a->frame_on_];
                }
                else
                    f = &a->frames_[a->frame_on_];

                if (f)
                    HUDDrawImage(f->info->x_, f->info->y_, f->image);
            }
        }
    }

    switch (state)
    {
    case kIntermissionStateStatScreen:
        if (InSinglePlayerMatch())
            DrawSinglePlayerStats();
        else if (InDeathmatch())
            DrawDeathmatchStats();
        else
            DrawCoopStats();
        break;

    case kIntermissionStateShowNextLocation:
        DrawShowNextLocation();
        break;

    case kIntermissionStateNone:
        DrawNoState();
        break;
    }
}

static void LoadData(void)
{
    int i, j;

    // find styles
    if (!single_player_intermission_style)
    {
        StyleDefinition *def = styledefs.Lookup("STATS");
        if (!def)
            def = default_style;
        single_player_intermission_style = hud_styles.Lookup(def);
    }

    if (!multiplayer_intermission_style)
    {
        StyleDefinition *def = styledefs.Lookup("NET STATS");
        if (!def)
            def = default_style;
        multiplayer_intermission_style = hud_styles.Lookup(def);
    }

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    // Lobo 2022: if we have a per level image defined, use that instead
    if (intermission_stats.current_level->leavingbggraphic_ != "")
    {
        leaving_background_image = ImageLookup(intermission_stats.current_level->leavingbggraphic_.c_str(),
                                               kImageNamespaceFlat, kImageLookupNull);
        if (leaving_background_image)
            tile_leaving_background = true;
        else
        {
            leaving_background_image = ImageLookup(intermission_stats.current_level->leavingbggraphic_.c_str());
            tile_leaving_background  = false;
        }
    }

    if (intermission_stats.current_level->enteringbggraphic_ != "")
    {
        entering_background_image = ImageLookup(intermission_stats.current_level->enteringbggraphic_.c_str(),
                                                kImageNamespaceFlat, kImageLookupNull);
        if (entering_background_image)
            tile_entering_background = true;
        else
        {
            entering_background_image = ImageLookup(intermission_stats.current_level->enteringbggraphic_.c_str());
            tile_entering_background  = false;
        }
    }

    background_image = ImageLookup(gd->background_.c_str(), kImageNamespaceFlat, kImageLookupNull);

    if (background_image)
        tile_background = true;
    else
    {
        background_image = ImageLookup(gd->background_.c_str());
        tile_background  = false;
    }

    level_names[0] = ImageLookup(intermission_stats.current_level->namegraphic_.c_str());

    if (intermission_stats.next_level)
        level_names[1] = ImageLookup(intermission_stats.next_level->namegraphic_.c_str());

    if (gd->you_are_here_[0] != "")
        you_are_here[0] = ImageLookup(gd->you_are_here_[0].c_str());
    if (gd->you_are_here_[1] != "")
        you_are_here[1] = ImageLookup(gd->you_are_here_[1].c_str());
    if (gd->splatpic_ != "")
        splat[0] = ImageLookup(gd->splatpic_.c_str());

    wiminus = ImageLookup("WIMINUS"); //!!! FIXME: use the style!
    percent = ImageLookup("WIPCNT");
    colon   = ImageLookup("WICOLON");

    finished = ImageLookup("WIF");
    entering = ImageLookup("WIENTER");
    kills    = ImageLookup("WIOSTK", kImageNamespaceGraphic, kImageLookupNull);
    // kills = ImageLookup("WIOSTK");
    secret = ImageLookup("WIOSTS");                       // "scrt"

    single_player_secret = ImageLookup("WISCRT2", kImageNamespaceGraphic,
                                       kImageLookupNull); // "secret"

    items      = ImageLookup("WIOSTI", kImageNamespaceGraphic, kImageLookupNull);
    frags      = ImageLookup("WIFRGS");
    time_image = ImageLookup("WITIME", kImageNamespaceGraphic, kImageLookupNull);
    sucks      = ImageLookup("WISUCKS", kImageNamespaceGraphic, kImageLookupNull);
    par        = ImageLookup("WIPAR", kImageNamespaceGraphic, kImageLookupNull);
    killers    = ImageLookup("WIKILRS"); // "killers" (vertical)

    victims = ImageLookup("WIVCTMS");    // "victims" (horiz)

    total = ImageLookup("WIMSTT");
    face  = ImageLookup("STFST01");      // your face

    dead_face = ImageLookup("STFDEAD0"); // dead face

    for (i = 0; i < 10; i++)
    {
        // numbers 0-9
        char name[64];
        stbsp_sprintf(name, "WINUM%d", i);
        digits[i] = ImageLookup(name);
    }

    for (i = 0; i < world_intermission.total_animations_; i++)
    {
        for (j = 0; j < world_intermission.animations_[i].total_frames_; j++)
        {
            // FIXME!!! Shorten :)
            LogDebug("IntermissionLoadData: '%s'\n", world_intermission.animations_[i].frames_[j].info->pic_.c_str());

            world_intermission.animations_[i].frames_[j].image =
                ImageLookup(world_intermission.animations_[i].frames_[j].info->pic_.c_str());
        }
    }
}

static void InitVariables(void)
{
    intermission_stats.level    = intermission_stats.current_level->name_.c_str();
    intermission_stats.par_time = intermission_stats.current_level->partime_;

    accelerate_stage = false;
    count = background_count = 0;
    first_refresh            = 1;

    if (intermission_stats.kills <= 0)
        intermission_stats.kills = 1;

    if (intermission_stats.items <= 0)
        intermission_stats.items = 1;

    if (intermission_stats.secrets <= 0)
        intermission_stats.secrets = 1;

    GameDefinition *def = intermission_stats.current_level->episode_;

    EPI_ASSERT(def);

    world_intermission.Init(def);

    LoadData();
}

void IntermissionStart(void)
{
    InitVariables();

    const GameDefinition *gd = intermission_stats.current_level->episode_;
    EPI_ASSERT(gd);

    if (InSinglePlayerMatch())
        InitSinglePlayerStats();
    else if (InDeathmatch())
        InitDeathmatchStats();
    else
        InitCoopStats();

    // -AJA- 1999/10/22: background cameras.
    background_camera_map_object = nullptr;

    if (gd->bg_camera_ != "")
    {
        for (MapObject *mo = map_object_list_head; mo != nullptr; mo = mo->next_)
        {
            if (DDFCompareName(mo->info_->name_.c_str(), gd->bg_camera_.c_str()) != 0)
                continue;

            background_camera_map_object = mo;

            // we don't want to see players
            for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
            {
                Player *p = players[pnum];

                if (p && p->map_object_)
                    p->map_object_->visibility_ = p->map_object_->target_visibility_ = 0.0f;
            }

            break;
        }
    }

    // Lobo 2025: if we have a camera set up we probably don't mind still hearing level sfx, otherwise nuke 'em ;)
    if (!background_camera_map_object)
    {
        StopAllSoundEffects();
        DestroyAllAmbientSounds();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
