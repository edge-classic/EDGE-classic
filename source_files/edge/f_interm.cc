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
#include "f_finale.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "i_system.h"
#include "p_local.h"
#include "r_draw.h"
#include "r_misc.h"
#include "r_modes.h"
#include "s_music.h"
#include "s_sound.h"
#include "str_compare.h"
#include "str_util.h"
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

static style_c *single_player_intermission_style;
static style_c *multiplayer_intermission_style;

// GRAPHICS

// background
static const image_c *background_image;
static const image_c *leaving_background_image;
static const image_c *entering_background_image;

bool tile_background          = false;
bool tile_leaving_background  = false;
bool tile_entering_background = false;

// You Are Here graphic
static const image_c *you_are_here[2] = {nullptr, nullptr};

// splat
static const image_c *splat[2] = {nullptr, nullptr};

// %, : graphics
static const image_c *percent;
static const image_c *colon;

// 0-9 graphic
static const image_c *digits[10];  // FIXME: use FONT/STYLE

// minus sign
static const image_c *wiminus;

// "Finished!" graphics
static const image_c *finished;

// "Entering" graphic
static const image_c *entering;

// "secret"
static const image_c *single_player_secret;

// "Kills", "Scrt", "Items", "Frags"
static const image_c *kills;
static const image_c *secret;
static const image_c *items;
static const image_c *frags;

// Time sucks.
static const image_c
    *time_image;  // -ACB- 1999/09/19 Removed Conflict with <time.h>
static const image_c *par;
static const image_c *sucks;

// "killers", "victims"
static const image_c *killers;
static const image_c *victims;

// "Total", your face, your dead face
static const image_c *total;
static const image_c *face;
static const image_c *dead_face;

// Name graphics of each level (centered)
static const image_c *level_names[2];

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
    const image_c         *image = nullptr;  // cached image
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

    ~IntermissionAnimation() { Clear(); }

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
            for (i = 0; i < size; i++) frames_[i].info = def->frames_[i];
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

    ~Intermission() { Clear(); }

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
        for (int i = 0; i < total_animations_; i++) animations_[i].Reset();
    }

   public:
    void Init(GameDefinition *definition)
    {
        if (definition != game_definition_)
        {
            // Clear
            Clear();

            if (definition) Load(definition);
        }

        if (definition) Reset();

        game_definition_ = definition;
        return;
    }
};

static Intermission world_intermission;

//
// CODE
//

void IntermissionClear(void) { world_intermission.Init(nullptr); }

// Draws "<Levelname> Finished!"
static void DrawLevelFinished(void)
{
    // draw <LevelName>
    // SYS_ASSERT(level_names[0]);

    // Lobo 2022: if we have a per level image defined, use that instead
    if (leaving_background_image)
    {
        if (tile_leaving_background)
            HUD_TileImage(-240, 0, 820, 200, leaving_background_image);
        else
        {
            if (r_titlescaling.d_)  // Fill Border
            {
                if (!leaving_background_image->blurred_version)
                    W_ImageStoreBlurred(leaving_background_image, 0.75f);
                HUD_StretchImage(-320, -200, 960, 600,
                                 leaving_background_image->blurred_version, 0,
                                 0);
            }
            HUD_DrawImageTitleWS(leaving_background_image);
        }
    }

    float y  = kIntermissionTitleY;
    float w1 = 160;
    float h1 = 15;

    // load styles
    style_c *style;
    style      = single_player_intermission_style;
    int t_type = StyleDefinition::kTextSectionText;

    HUD_SetAlignment(0, -1);  // center it

    // If we have a custom mapname graphic e.g.CWILVxx then use that
    if (level_names[0])
    {
        if (W_IsLumpInPwad(level_names[0]->name.c_str()))
        {
            w1 = IM_WIDTH(level_names[0]);
            h1 = IM_HEIGHT(level_names[0]);
            HUD_SetAlignment(-1, -1);  // center it
            if (w1 > 320)              // Too big? Shrink it to fit the screen
                HUD_StretchImage(0, y, 320, h1, level_names[0], 0.0, 0.0);
            else
                HUD_DrawImage(160 - w1 / 2, y, level_names[0]);
        }
        else
        {
            h1 = style->fonts[t_type]->NominalHeight();

            float txtscale = 1.0;
            if (style->def->text_[t_type].scale_)
            {
                txtscale = style->def->text_[t_type].scale_;
            }
            int txtWidth = 0;
            txtWidth     = style->fonts[t_type]->StringWidth(
                           language[intermission_stats.current_level
                                        ->description_.c_str()]) *
                       txtscale;

            if (txtWidth > 320)  // Too big? Shrink it to fit the screen
            {
                float TempScale = 0;
                TempScale       = 310;
                TempScale /= txtWidth;
                HL_WriteText(style, t_type, 160, y,
                             language[intermission_stats.current_level
                                          ->description_.c_str()],
                             TempScale);
            }
            else
                HL_WriteText(style, t_type, 160, y,
                             language[intermission_stats.current_level
                                          ->description_.c_str()]);
        }
    }
    else
    {
        h1 = style->fonts[t_type]->NominalHeight();

        float txtscale = 1.0;
        if (style->def->text_[t_type].scale_)
        {
            txtscale = style->def->text_[t_type].scale_;
        }
        int txtWidth = 0;
        txtWidth     = style->fonts[t_type]->StringWidth(
                       language[intermission_stats.current_level->description_
                                    .c_str()]) *
                   txtscale;

        if (txtWidth > 320)  // Too big? Shrink it to fit the screen
        {
            float TempScale = 0;
            TempScale       = 310;
            TempScale /= txtWidth;
            HL_WriteText(style, t_type, 160, y,
                         language[intermission_stats.current_level->description_
                                      .c_str()],
                         TempScale);
        }
        else
            HL_WriteText(style, t_type, 160, y,
                         language[intermission_stats.current_level->description_
                                      .c_str()]);
    }
    HUD_SetAlignment(-1, -1);  // set it back to usual

    t_type = StyleDefinition::kTextSectionTitle;
    if (!style->fonts[t_type]) t_type = StyleDefinition::kTextSectionText;

    // ttf_ref_yshift is important for TTF fonts.
    float y_shift =
        style->fonts[t_type]->ttf_ref_yshift[current_font_size];  // * txtscale;

    y = y + h1;
    y += y_shift;

    HUD_SetAlignment(0, -1);  // center it
    // If we have a custom Finished graphic e.g.WIF then use that
    if (W_IsLumpInPwad(finished->name.c_str()))
    {
        w1 = IM_WIDTH(finished);
        h1 = IM_HEIGHT(finished);
        HUD_SetAlignment(-1, -1);  // center it
        HUD_DrawImage(160 - w1 / 2, y * 5 / 4, finished);
    }
    else
    {
        h1 = style->fonts[t_type]->NominalHeight();
        HL_WriteText(style, t_type, 160, y * 5 / 4,
                     language["IntermissionFinished"]);
    }
    HUD_SetAlignment(-1, -1);  // set it back to usual
}

static void DrawOnLnode(IntermissionMapPosition *mappos,
                        const image_c           *images[2])
{
    int i;

    // -AJA- this is used to select between Left and Right pointing
    // arrows (WIURH0 and WIURH1).  Smells like a massive HACK.

    for (i = 0; i < 2; i++)
    {
        if (images[i] == nullptr) continue;

        float left = mappos->info->x_ - IM_OFFSETX(images[i]);
        float top  = mappos->info->y_ - IM_OFFSETY(images[i]);

        float right  = left + IM_WIDTH(images[i]);
        float bottom = top + IM_HEIGHT(images[i]);

        if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
        {
            /* this one fits on the screen */
            break;
        }
    }

    if (i < 2) { HUD_DrawImage(mappos->info->x_, mappos->info->y_, images[i]); }
    else
    {
        L_WriteDebug("Could not place patch on level '%s'\n",
                     mappos->info->name_.c_str());
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
    if (!intermission_stats.next_level) return;

    // Lobo 2022: if we have a per level image defined, use that instead
    if (entering_background_image)
    {
        if (tile_entering_background)
            HUD_TileImage(-240, 0, 820, 200, entering_background_image);
        else
        {
            if (r_titlescaling.d_)  // Fill Border
            {
                if (!entering_background_image->blurred_version)
                    W_ImageStoreBlurred(entering_background_image, 0.75f);
                HUD_StretchImage(-320, -200, 960, 600,
                                 entering_background_image->blurred_version, 0,
                                 0);
            }
            HUD_DrawImageTitleWS(entering_background_image);
        }
    }

    float y  = kIntermissionTitleY;
    float w1 = 160;
    float h1 = 15;

    style_c *style;
    style      = single_player_intermission_style;
    int t_type = StyleDefinition::kTextSectionTitle;
    if (!style->fonts[t_type]) t_type = StyleDefinition::kTextSectionText;

    HUD_SetAlignment(0, -1);  // center it

    // If we have a custom Entering graphic e.g.WIENTER then use that
    if (W_IsLumpInPwad(entering->name.c_str()))
    {
        w1 = IM_WIDTH(entering);
        h1 = IM_HEIGHT(entering);
        HUD_SetAlignment(-1, -1);  // center it
        HUD_DrawImage(160 - w1 / 2, y, entering);
    }
    else
    {
        h1 = style->fonts[t_type]->NominalHeight();
        HL_WriteText(style, t_type, 160, y, language["IntermissionEntering"]);
    }
    HUD_SetAlignment(-1, -1);  // set it back to usual

    for (int i = 0; i < world_intermission.total_map_positions_; i++)
    {
        if (world_intermission.map_positions_[i].done)
            DrawOnLnode(&world_intermission.map_positions_[i], splat);

        if (intermission_pointer_on &&
            !epi::StringCompare(
                intermission_stats.next_level->name_.c_str(),
                world_intermission.map_positions_[i].info->name_.c_str()))
            DrawOnLnode(&world_intermission.map_positions_[i], you_are_here);
    }

    // ttf_ref_yshift is important for TTF fonts.
    float y_shift =
        style->fonts[t_type]->ttf_ref_yshift[current_font_size];  // * txtscale;

    y = y + h1;
    y += y_shift;

    t_type = StyleDefinition::kTextSectionText;

    HUD_SetAlignment(0, -1);  // center it

    // If we have a custom mapname graphic e.g.CWILVxx then use that
    if (level_names[1])
    {
        if (W_IsLumpInPwad(level_names[1]->name.c_str()))
        {
            w1 = IM_WIDTH(level_names[1]);
            h1 = IM_HEIGHT(level_names[1]);
            HUD_SetAlignment(-1, -1);  // center it
            if (w1 > 320)              // Too big? Shrink it to fit the screen
                HUD_StretchImage(0, y * 5 / 4, 320, h1, level_names[1], 0.0,
                                 0.0);
            else
                HUD_DrawImage(160 - w1 / 2, y * 5 / 4, level_names[1]);
        }
        else
        {
            float txtscale = 1.0;
            if (style->def->text_[t_type].scale_)
            {
                txtscale = style->def->text_[t_type].scale_;
            }
            int txtWidth = 0;
            txtWidth     = style->fonts[t_type]->StringWidth(
                           language[intermission_stats.next_level->description_
                                        .c_str()]) *
                       txtscale;

            if (txtWidth > 320)  // Too big? Shrink it to fit the screen
            {
                float TempScale = 0;
                TempScale       = 310;
                TempScale /= txtWidth;
                HL_WriteText(style, t_type, 160, y * 5 / 4,
                             language[intermission_stats.next_level
                                          ->description_.c_str()],
                             TempScale);
            }
            else
                HL_WriteText(style, t_type, 160, y * 5 / 4,
                             language[intermission_stats.next_level
                                          ->description_.c_str()]);
        }
    }
    else
    {
        float txtscale = 1.0;
        if (style->def->text_[t_type].scale_)
        {
            txtscale = style->def->text_[t_type].scale_;
        }
        int txtWidth = 0;
        txtWidth =
            style->fonts[t_type]->StringWidth(
                language[intermission_stats.next_level->description_.c_str()]) *
            txtscale;

        if (txtWidth > 320)  // Too big? Shrink it to fit the screen
        {
            float TempScale = 0;
            TempScale       = 310;
            TempScale /= txtWidth;
            HL_WriteText(
                style, t_type, 160, y * 5 / 4,
                language[intermission_stats.next_level->description_.c_str()],
                TempScale);
        }
        else
            HL_WriteText(
                style, t_type, 160, y * 5 / 4,
                language[intermission_stats.next_level->description_.c_str()]);
    }
    HUD_SetAlignment(-1, -1);  // set it back to usual
}

static float PercentWidth(std::string &s)
{
    float perc_width = 0;
    for (auto c : s)
    {
        if (c == '%') { perc_width += IM_WIDTH(percent); }
        else if (epi::IsDigitASCII(c))
        {
            perc_width += IM_WIDTH(digits[c - 48]);
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
            HUD_DrawImage(x, y, percent);
            x += IM_WIDTH(percent);
        }
        else if (epi::IsDigitASCII(c))
        {
            HUD_DrawImage(x, y, digits[c - 48]);
            x += IM_WIDTH(digits[c - 48]);
        }
    }
}

//
// Calculate width of time message
//
static float TimeWidth(int t, bool drawText = false)
{
    if (t < 0) return 0;

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
        float txtscale = single_player_intermission_style->def
                             ->text_[StyleDefinition::kTextSectionAlternate]
                             .scale_;

        if (t > 3599)
        {
            return single_player_intermission_style
                       ->fonts[StyleDefinition::kTextSectionAlternate]
                       ->StringWidth("Sucks") *
                   txtscale;
        }
        else
        {
            return single_player_intermission_style
                       ->fonts[StyleDefinition::kTextSectionAlternate]
                       ->StringWidth(s.c_str()) *
                   txtscale;
        }
    }
    else
    {
        if (t > 3599)
        {
            // "sucks"
            if ((sucks) && (W_IsLumpInPwad(sucks->name.c_str())))
                return IM_WIDTH(sucks);
            else
                return single_player_intermission_style
                    ->fonts[StyleDefinition::kTextSectionAlternate]
                    ->StringWidth("Sucks");
        }
        else
        {
            float time_width = 0;
            for (auto c : s)
            {
                if (c == ':') { time_width += IM_WIDTH(colon); }
                else if (epi::IsDigitASCII(c))
                {
                    time_width += IM_WIDTH(digits[c - 48]);
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
    if (t < 0) return;

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
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionTitle, x, y, "Sucks");
        }
        else
        {
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionAlternate, x, y,
                         s.c_str());
        }
    }
    else
    {
        if (t > 3599)
        {
            // "sucks"
            if ((sucks) && (W_IsLumpInPwad(sucks->name.c_str())))
                HUD_DrawImage(x, y, sucks);
            else
                HL_WriteText(single_player_intermission_style,
                             StyleDefinition::kTextSectionTitle, x, y, "Sucks");
        }
        else
        {
            for (auto c : s)
            {
                if (c == ':')
                {
                    HUD_DrawImage(x, y, colon);
                    x += IM_WIDTH(colon);
                }
                else if (epi::IsDigitASCII(c))
                {
                    HUD_DrawImage(x, y, digits[c - 48]);
                    x += IM_WIDTH(digits[c - 48]);
                }
            }
        }
    }
}

static void IntermissionEnd(void)
{
    E_ForceWipe();

    background_camera_mo = nullptr;

    FinaleStart(&currmap->f_end_, nextmap ? ga_finale : ga_nothing);
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

    if (count == 0) { IntermissionEnd(); }
}

static void ShowNextLocationInit(void)
{
    int i;

    state            = kIntermissionStateShowNextLocation;
    accelerate_stage = false;

    for (i = 0; i < world_intermission.total_map_positions_; i++)
    {
        if (epi::StringCompare(
                world_intermission.map_positions_[i].info->name_.c_str(),
                intermission_stats.current_level->name_.c_str()) == 0)
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
                !epi::StringCompare(
                    intermission_stats.next_level->name_.c_str(),
                    world_intermission.map_positions_[i].info->name_.c_str()))
                DrawOnLnode(&world_intermission.map_positions_[i],
                            you_are_here);
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

        for (int i = 0; i < MAXPLAYERS - 1; i++)
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
    if (pl >= 0) { return players[pl]->totalfrags * 2 + players[pl]->frags; }

    return -999;
}

static void InitDeathmatchStats(void)
{
    SYS_ASSERT(kNumberOfPlayersShown <= MAXPLAYERS);

    state            = kIntermissionStateStatScreen;
    accelerate_stage = false;
    deathmatch_state = 1;

    count_pause = kTicRate;

    int rank[MAXPLAYERS];
    int score[MAXPLAYERS];

    int i;

    for (i = 0; i < MAXPLAYERS; i++)
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

            if (p < 0) break;

            deathmatch_frags[i]  = players[p]->frags;
            deathmatch_totals[i] = players[p]->totalfrags;
        }

        S_StartFX(gd->done_);
        deathmatch_state = 4;
    }

    switch (deathmatch_state)
    {
        case 2:
            if (!(background_count & 3)) S_StartFX(gd->percent_);

            stillticking = false;
            for (int i = 0; i < kNumberOfPlayersShown; i++)
            {
                int p = deathmatch_rank[i];

                if (p < 0) break;

                if (deathmatch_frags[i] < players[p]->frags)
                {
                    deathmatch_frags[i]++;
                    stillticking = true;
                }
                if (deathmatch_totals[i] < players[p]->totalfrags)
                {
                    deathmatch_totals[i]++;
                    stillticking = true;
                }
            }

            if (!stillticking)
            {
                S_StartFX(gd->done_);
                deathmatch_state++;
            }
            break;

        case 4:
            if (accelerate_stage)
            {
                S_StartFX(gd->accel_snd_);

                // Skip next loc on no map -ACB- 2004/06/27
                if (!world_intermission.total_map_positions_ ||
                    !intermission_stats.next_level)
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
    int y      = kSinglePlayerStateStatsY;  // 40;

    HL_WriteText(multiplayer_intermission_style, t_type, 20, y, "Player");
    HL_WriteText(multiplayer_intermission_style, t_type, 100, y, "Frags");
    HL_WriteText(multiplayer_intermission_style, t_type, 200, y, "Total");

    for (int i = 0; i < kNumberOfPlayersShown; i++)
    {
        int p = deathmatch_rank[i];

        if (p < 0) break;

        y += 12;

        t_type = StyleDefinition::kTextSectionText;

        // hightlight the console player
#if 1
        if (p == consoleplayer) t_type = StyleDefinition::kTextSectionAlternate;
#else
        if (p == consoleplayer && ((background_count & 31) < 16)) continue;
#endif

        char temp[40];

        sprintf(temp, "%s", players[p]->playername);
        HL_WriteText(multiplayer_intermission_style, t_type, 20, y, temp);

        sprintf(temp, "%5d", deathmatch_frags[i]);
        HL_WriteText(multiplayer_intermission_style, t_type, 100, y, temp);

        sprintf(temp, "%11d", deathmatch_totals[i]);
        HL_WriteText(multiplayer_intermission_style, t_type, 200, y, temp);
    }
}

// Calculates value of this player for ranking
static int CoopScore(int pl)
{
    if (pl >= 0)
    {
        int coop_kills =
            players[pl]->killcount * 400 / intermission_stats.kills;
        int coop_items =
            players[pl]->itemcount * 100 / intermission_stats.items;
        int coop_secret =
            players[pl]->secretcount * 200 / intermission_stats.secrets;
        int coop_frags = (players[pl]->frags + players[pl]->totalfrags) * 25;

        return coop_kills + coop_items + coop_secret - coop_frags;
    }

    return -999;
}

static void InitCoopStats(void)
{
    SYS_ASSERT(kNumberOfPlayersShown <= MAXPLAYERS);

    state              = kIntermissionStateStatScreen;
    accelerate_stage   = false;
    state_ticker_count = 1;

    count_pause = kTicRate;

    int rank[MAXPLAYERS];
    int score[MAXPLAYERS];

    int i;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        rank[i]  = players[i] ? i : -1;
        score[i] = CoopScore(rank[i]);
    }

    SortRanks(rank, score);

    do_frags = 0;

    for (i = 0; i < kNumberOfPlayersShown; i++)
    {
        deathmatch_rank[i] = rank[i];

        if (deathmatch_rank[i] < 0) continue;

        count_kills[i] = count_items[i] = count_secrets[i] = count_frags[i] =
            count_totals[i]                                = 0;

        do_frags += players[deathmatch_rank[i]]->frags +
                    players[deathmatch_rank[i]]->totalfrags;
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

            if (p < 0) break;

            count_kills[i] =
                (players[p]->killcount * 100) / intermission_stats.kills;
            count_items[i] =
                (players[p]->itemcount * 100) / intermission_stats.items;
            count_secrets[i] =
                (players[p]->secretcount * 100) / intermission_stats.secrets;

            if (do_frags)
            {
                count_frags[i]  = players[p]->frags;
                count_totals[i] = players[p]->totalfrags;
            }
        }

        S_StartFX(gd->done_);
        state_ticker_count = 10;
    }

    switch (state_ticker_count)
    {
        case 2:
            if (!(background_count & 3)) S_StartFX(gd->percent_);

            stillticking = false;

            for (int i = 0; i < kNumberOfPlayersShown; i++)
            {
                int p = deathmatch_rank[i];

                if (p < 0) break;

                count_kills[i] += 2;

                if (count_kills[i] >=
                    (players[p]->killcount * 100) / intermission_stats.kills)
                    count_kills[i] = (players[p]->killcount * 100) /
                                     intermission_stats.kills;
                else
                    stillticking = true;
            }

            if (!stillticking)
            {
                S_StartFX(gd->done_);
                state_ticker_count++;
            }
            break;

        case 4:
            if (!(background_count & 3)) S_StartFX(gd->percent_);

            stillticking = false;

            for (int i = 0; i < kNumberOfPlayersShown; i++)
            {
                int p = deathmatch_rank[i];

                if (p < 0) break;

                count_items[i] += 2;
                if (count_items[i] >=
                    (players[p]->itemcount * 100) / intermission_stats.items)
                    count_items[i] = (players[p]->itemcount * 100) /
                                     intermission_stats.items;
                else
                    stillticking = true;
            }

            if (!stillticking)
            {
                S_StartFX(gd->done_);
                state_ticker_count++;
            }
            break;

        case 6:
            if (!(background_count & 3)) S_StartFX(gd->percent_);

            stillticking = false;

            for (int i = 0; i < kNumberOfPlayersShown; i++)
            {
                int p = deathmatch_rank[i];

                if (p < 0) break;

                count_secrets[i] += 2;

                if (count_secrets[i] >= (players[p]->secretcount * 100) /
                                            intermission_stats.secrets)
                    count_secrets[i] = (players[p]->secretcount * 100) /
                                       intermission_stats.secrets;
                else
                    stillticking = true;
            }

            if (!stillticking)
            {
                S_StartFX(gd->done_);
                state_ticker_count += 1 + 2 * !do_frags;
            }
            break;

        case 8:
            if (!(background_count & 3)) S_StartFX(gd->percent_);

            stillticking = false;

            for (int i = 0; i < kNumberOfPlayersShown; i++)
            {
                int p = deathmatch_rank[i];

                if (p < 0) break;

                count_frags[i]++;
                count_totals[i]++;

                if (count_frags[i] >= players[p]->frags)
                    count_frags[i] = players[p]->frags;
                else if (count_totals[i] >= players[p]->totalfrags)
                    count_totals[i] = players[p]->totalfrags;
                else
                    stillticking = true;
            }

            if (!stillticking)
            {
                S_StartFX(gd->frag_snd_);
                state_ticker_count++;
            }
            break;

        case 10:
            if (accelerate_stage)
            {
                S_StartFX(gd->nextmap_);

                // Skip next loc on no map -ACB- 2004/06/27
                if (!world_intermission.total_map_positions_ ||
                    !intermission_stats.next_level)
                    NoStateInit();
                else
                    ShowNextLocationInit();
            }

        default:
            if (!--count_pause)
            {
                state_ticker_count++;
                count_pause = kTicRate;
            }
    }
}

static void DrawCoopStats(void)
{
    DrawLevelFinished();

    int t_type = StyleDefinition::kTextSectionTitle;
    int y      = kSinglePlayerStateStatsY;  // 40;

    // FIXME: better alignment

    HL_WriteText(multiplayer_intermission_style, t_type, 6, y, "Player");
    HL_WriteText(multiplayer_intermission_style, t_type, 56, y, "Kills");
    HL_WriteText(multiplayer_intermission_style, t_type, 98, y, "Items");
    HL_WriteText(multiplayer_intermission_style, t_type, 142, y, "Secret");

    if (do_frags)
    {
        HL_WriteText(multiplayer_intermission_style, t_type, 190, y, "Frags");
        HL_WriteText(multiplayer_intermission_style, t_type, 232, y, "Total");
    }

    for (int i = 0; i < kNumberOfPlayersShown; i++)
    {
        int p = deathmatch_rank[i];

        if (p < 0) break;

        y += 12;

        t_type = StyleDefinition::kTextSectionText;

        // highlight the console player
#if 1
        if (p == consoleplayer) t_type = StyleDefinition::kTextSectionAlternate;
#else
        if (p == consoleplayer && ((background_count & 31) < 16)) continue;
#endif

        char temp[40];

        sprintf(temp, "%s", players[p]->playername);
        HL_WriteText(multiplayer_intermission_style, t_type, 6, y, temp);

        sprintf(temp, "%3d%%", count_kills[i]);
        HL_WriteText(multiplayer_intermission_style, t_type, 64, y, temp);

        sprintf(temp, "%3d%%", count_items[i]);
        HL_WriteText(multiplayer_intermission_style, t_type, 106, y, temp);

        sprintf(temp, "%3d%%", count_secrets[i]);
        HL_WriteText(multiplayer_intermission_style, t_type, 158, y, temp);

        if (do_frags)
        {
            sprintf(temp, "%5d", count_frags[i]);
            HL_WriteText(multiplayer_intermission_style, t_type, 190, y, temp);

            sprintf(temp, "%11d", count_totals[i]);
            HL_WriteText(multiplayer_intermission_style, t_type, 232, y, temp);
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
    player_t *con_plyr = players[consoleplayer];

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    if (accelerate_stage && single_player_state != kSinglePlayerStateEnd)
    {
        accelerate_stage = false;
        count_kills[0] = (con_plyr->killcount * 100) / intermission_stats.kills;
        count_items[0] = (con_plyr->itemcount * 100) / intermission_stats.items;
        count_secrets[0] =
            (con_plyr->secretcount * 100) / intermission_stats.secrets;
        count_time = con_plyr->leveltime / kTicRate;
        count_par  = intermission_stats.par_time / kTicRate;
        S_StartFX(gd->done_);
        single_player_state = kSinglePlayerStateEnd;
    }

    if (single_player_state == kSinglePlayerStateKills)
    {
        count_kills[0] += 2;

        if (!(background_count & 3)) S_StartFX(gd->percent_);

        if (count_kills[0] >=
            (con_plyr->killcount * 100) / intermission_stats.kills)
        {
            count_kills[0] =
                (con_plyr->killcount * 100) / intermission_stats.kills;
            S_StartFX(gd->done_);
            single_player_state++;
        }
    }
    else if (single_player_state == kSinglePlayerStateItems)
    {
        count_items[0] += 2;

        if (!(background_count & 3)) S_StartFX(gd->percent_);

        if (count_items[0] >=
            (con_plyr->itemcount * 100) / intermission_stats.items)
        {
            count_items[0] =
                (con_plyr->itemcount * 100) / intermission_stats.items;
            S_StartFX(gd->done_);
            single_player_state++;
        }
    }
    else if (single_player_state == kSinglePlayerStateSecrets)
    {
        count_secrets[0] += 2;

        if (!(background_count & 3)) S_StartFX(gd->percent_);

        if (count_secrets[0] >=
            (con_plyr->secretcount * 100) / intermission_stats.secrets)
        {
            count_secrets[0] =
                (con_plyr->secretcount * 100) / intermission_stats.secrets;
            S_StartFX(gd->done_);
            single_player_state++;
        }
    }

    else if (single_player_state == kSinglePlayerStateTime)
    {
        if (!(background_count & 3)) S_StartFX(gd->percent_);

        count_time += 3;

        if (count_time >= con_plyr->leveltime / kTicRate)
            count_time = con_plyr->leveltime / kTicRate;

        count_par += 3;

        if (count_par >= intermission_stats.par_time / kTicRate)
        {
            count_par = intermission_stats.par_time / kTicRate;

            if (count_time >= con_plyr->leveltime / kTicRate)
            {
                S_StartFX(gd->done_);
                single_player_state++;
            }
        }
    }
    else if (single_player_state == kSinglePlayerStateEnd)
    {
        if (accelerate_stage)
        {
            S_StartFX(gd->nextmap_);

            // Skip next loc on no map -ACB- 2004/06/27
            if (!world_intermission.total_map_positions_ ||
                !intermission_stats.next_level)
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
    float lh = IM_HEIGHT(digits[0]) * 3 / 2;

    DrawLevelFinished();

    bool drawTextBased = true;
    if (kills != nullptr)
    {
        if (W_IsLumpInPwad(kills->name.c_str()))
            drawTextBased = false;
        else
            drawTextBased = true;
    }
    else { drawTextBased = true; }

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
        HUD_DrawImage(kSinglePlayerStateStatsX, kSinglePlayerStateStatsY,
                      kills);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s),
                        kSinglePlayerStateStatsY, s);
    }
    else
    {
        HL_WriteText(single_player_intermission_style,
                     StyleDefinition::kTextSectionAlternate,
                     kSinglePlayerStateStatsX, kSinglePlayerStateStatsY,
                     "Kills");
        if (!s.empty())
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionAlternate,
                         320 - kSinglePlayerStateStatsX -
                             single_player_intermission_style
                                 ->fonts[StyleDefinition::kTextSectionAlternate]
                                 ->StringWidth(s.c_str()),
                         kSinglePlayerStateStatsY, s.c_str());
    }

    if (count_items[0] < 0)
        s.clear();
    else
    {
        s = std::to_string(count_items[0]);
        s = s + "%";
    }

    if ((items) && (W_IsLumpInPwad(items->name.c_str())))
    {
        HUD_DrawImage(kSinglePlayerStateStatsX, kSinglePlayerStateStatsY + lh,
                      items);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s),
                        kSinglePlayerStateStatsY + lh, s);
    }
    else
    {
        HL_WriteText(single_player_intermission_style,
                     StyleDefinition::kTextSectionAlternate,
                     kSinglePlayerStateStatsX, kSinglePlayerStateStatsY + lh,
                     "Items");
        if (!s.empty())
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionAlternate,
                         320 - kSinglePlayerStateStatsX -
                             single_player_intermission_style
                                 ->fonts[StyleDefinition::kTextSectionAlternate]
                                 ->StringWidth(s.c_str()),
                         kSinglePlayerStateStatsY + lh, s.c_str());
    }

    if (count_secrets[0] < 0)
        s.clear();
    else
    {
        s = std::to_string(count_secrets[0]);
        s = s + "%";
    }

    if ((single_player_secret) &&
        (W_IsLumpInPwad(single_player_secret->name.c_str())))
    {
        HUD_DrawImage(kSinglePlayerStateStatsX,
                      kSinglePlayerStateStatsY + 2 * lh, single_player_secret);
        if (!s.empty())
            DrawPercent(320 - kSinglePlayerStateStatsX - PercentWidth(s),
                        kSinglePlayerStateStatsY + 2 * lh, s);
    }
    else
    {
        HL_WriteText(single_player_intermission_style,
                     StyleDefinition::kTextSectionAlternate,
                     kSinglePlayerStateStatsX,
                     kSinglePlayerStateStatsY + 2 * lh, "Secrets");
        if (!s.empty())
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionAlternate,
                         320 - kSinglePlayerStateStatsX -
                             single_player_intermission_style
                                 ->fonts[StyleDefinition::kTextSectionAlternate]
                                 ->StringWidth(s.c_str()),
                         kSinglePlayerStateStatsY + 2 * lh, s.c_str());
    }

    if ((time_image) && (W_IsLumpInPwad(time_image->name.c_str())))
    {
        HUD_DrawImage(kSinglePlayerStateTimeX, kSinglePlayerStateTimeY,
                      time_image);
        DrawTime(160 - kSinglePlayerStateTimeX - TimeWidth(count_time),
                 kSinglePlayerStateTimeY, count_time);
    }
    else
    {
        HL_WriteText(single_player_intermission_style,
                     StyleDefinition::kTextSectionAlternate,
                     kSinglePlayerStateTimeX, kSinglePlayerStateTimeY, "Time");
        DrawTime(160 - kSinglePlayerStateTimeX - TimeWidth(count_time, true),
                 kSinglePlayerStateTimeY, count_time, true);
    }

    // -KM- 1998/11/25 Removed episode check. Replaced with partime check
    if (intermission_stats.par_time)
    {
        if ((par) && (W_IsLumpInPwad(par->name.c_str())))
        {
            HUD_DrawImage(170, kSinglePlayerStateTimeY, par);
            DrawTime(320 - kSinglePlayerStateTimeX - TimeWidth(count_par),
                     kSinglePlayerStateTimeY, count_par);
        }
        else
        {
            HL_WriteText(single_player_intermission_style,
                         StyleDefinition::kTextSectionAlternate, 170,
                         kSinglePlayerStateTimeY, "Par");
            DrawTime(320 - kSinglePlayerStateTimeX - TimeWidth(count_par, true),
                     kSinglePlayerStateTimeY, count_par, true);
        }
    }
}

bool IntermissionCheckForAccelerate(void)
{
    bool do_accel = false;

    // check for button presses to skip delays
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *player = players[pnum];
        if (!player) continue;

        if (player->cmd.buttons & kButtonCodeAttack)
        {
            if (!player->attackdown[0])
            {
                player->attackdown[0] = true;
                do_accel              = true;
            }
        }
        else
            player->attackdown[0] = false;

        if (player->cmd.buttons & kButtonCodeUse)
        {
            if (!player->usedown)
            {
                player->usedown = true;
                do_accel        = true;
            }
        }
        else
            player->usedown = false;
    }

    return do_accel;
}

void IntermissionTicker(void)
{
    // Updates stuff each tick

    SYS_ASSERT(gamestate == GS_INTERMISSION);

    int i;

    // counter for general background animation
    background_count++;

    if (background_count == 1)
    {
        // intermission music
        S_ChangeMusic(intermission_stats.current_level->episode_->music_, true);
    }

    if (IntermissionCheckForAccelerate()) accelerate_stage = true;

    for (i = 0; i < world_intermission.total_animations_; i++)
    {
        if (world_intermission.animations_[i].count_ >= 0)
        {
            if (!world_intermission.animations_[i].count_)
            {
                world_intermission.animations_[i].frame_on_ =
                    (world_intermission.animations_[i].frame_on_ + 1) %
                    world_intermission.animations_[i].total_frames_;
                world_intermission.animations_[i].count_ =
                    world_intermission.animations_[i]
                        .frames_[world_intermission.animations_[i].frame_on_]
                        .info->tics_;
            }
            world_intermission.animations_[i].count_--;
        }
    }

    switch (state)
    {
        case kIntermissionStateStatScreen:
            if (SP_MATCH())
                UpdateSinglePlayerStats();
            else if (DEATHMATCH())
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
    SYS_ASSERT(gamestate == GS_INTERMISSION);

    HUD_Reset();

    if (background_camera_mo)
    {
        HUD_RenderWorld(0, 0, 320, 200, background_camera_mo, 0);
    }
    else
    {
        // HUD_StretchImage(0, 0, 320, 200, background_image);
        if (background_image)
        {
            if (tile_background)
                HUD_TileImage(-240, 0, 820, 200,
                              background_image);  // Lobo: Widescreen support
            else
            {
                if (r_titlescaling.d_)  // Fill Border
                {
                    if (!background_image->blurred_version)
                        W_ImageStoreBlurred(background_image, 0.75f);
                    HUD_StretchImage(-320, -200, 960, 600,
                                     background_image->blurred_version, 0, 0);
                }
                HUD_DrawImageTitleWS(background_image);
            }

            for (int i = 0; i < world_intermission.total_animations_; i++)
            {
                IntermissionAnimation *a = &world_intermission.animations_[i];

                if (a->frame_on_ == -1) continue;

                IntermissionFrame *f = nullptr;

                if (a->info_->type_ ==
                    IntermissionAnimationInfo::kIntermissionAnimationInfoLevel)
                {
                    if (!intermission_stats.next_level)
                        f = nullptr;
                    else if (!epi::StringCompare(
                                 intermission_stats.next_level->name_.c_str(),
                                 a->info_->level_.c_str()))
                        f = &a->frames_[a->frame_on_];
                }
                else
                    f = &a->frames_[a->frame_on_];

                if (f) HUD_DrawImage(f->info->x_, f->info->y_, f->image);
            }
        }
    }

    switch (state)
    {
        case kIntermissionStateStatScreen:
            if (SP_MATCH())
                DrawSinglePlayerStats();
            else if (DEATHMATCH())
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
        if (!def) def = default_style;
        single_player_intermission_style = hu_styles.Lookup(def);
    }

    if (!multiplayer_intermission_style)
    {
        StyleDefinition *def = styledefs.Lookup("NET STATS");
        if (!def) def = default_style;
        multiplayer_intermission_style = hu_styles.Lookup(def);
    }

    const GameDefinition *gd = intermission_stats.current_level->episode_;

    // Lobo 2022: if we have a per level image defined, use that instead
    if (intermission_stats.current_level->leavingbggraphic_ != "")
    {
        leaving_background_image = W_ImageLookup(
            intermission_stats.current_level->leavingbggraphic_.c_str(),
            kImageNamespaceFlat, ILF_Null);
        if (leaving_background_image)
            tile_leaving_background = true;
        else
        {
            leaving_background_image = W_ImageLookup(
                intermission_stats.current_level->leavingbggraphic_.c_str());
            tile_leaving_background = false;
        }
    }

    if (intermission_stats.current_level->enteringbggraphic_ != "")
    {
        entering_background_image = W_ImageLookup(
            intermission_stats.current_level->enteringbggraphic_.c_str(),
            kImageNamespaceFlat, ILF_Null);
        if (entering_background_image)
            tile_entering_background = true;
        else
        {
            entering_background_image = W_ImageLookup(
                intermission_stats.current_level->enteringbggraphic_.c_str());
            tile_entering_background = false;
        }
    }

    background_image =
        W_ImageLookup(gd->background_.c_str(), kImageNamespaceFlat, ILF_Null);

    if (background_image)
        tile_background = true;
    else
    {
        background_image = W_ImageLookup(gd->background_.c_str());
        tile_background  = false;
    }

    level_names[0] =
        W_ImageLookup(intermission_stats.current_level->namegraphic_.c_str());

    if (intermission_stats.next_level)
        level_names[1] =
            W_ImageLookup(intermission_stats.next_level->namegraphic_.c_str());

    if (gd->you_are_here_[0] != "")
        you_are_here[0] = W_ImageLookup(gd->you_are_here_[0].c_str());
    if (gd->you_are_here_[1] != "")
        you_are_here[1] = W_ImageLookup(gd->you_are_here_[1].c_str());
    if (gd->splatpic_ != "") splat[0] = W_ImageLookup(gd->splatpic_.c_str());

    wiminus = W_ImageLookup("WIMINUS");  //!!! FIXME: use the style!
    percent = W_ImageLookup("WIPCOUNT");
    colon   = W_ImageLookup("WICOLON");

    finished = W_ImageLookup("WIF");
    entering = W_ImageLookup("WIENTER");
    kills    = W_ImageLookup("WIOSTK", kImageNamespaceGraphic, ILF_Null);
    // kills = W_ImageLookup("WIOSTK");
    secret = W_ImageLookup("WIOSTS");  // "scrt"

    single_player_secret =
        W_ImageLookup("WISCRT2", kImageNamespaceGraphic, ILF_Null);  // "secret"

    items      = W_ImageLookup("WIOSTI", kImageNamespaceGraphic, ILF_Null);
    frags      = W_ImageLookup("WIFRGS");
    time_image = W_ImageLookup("WITIME", kImageNamespaceGraphic, ILF_Null);
    sucks      = W_ImageLookup("WISUCKS", kImageNamespaceGraphic, ILF_Null);
    par        = W_ImageLookup("WIPAR", kImageNamespaceGraphic, ILF_Null);
    killers    = W_ImageLookup("WIKILRS");  // "killers" (vertical)

    victims = W_ImageLookup("WIVCTMS");  // "victims" (horiz)

    total = W_ImageLookup("WIMSTT");
    face  = W_ImageLookup("STFST01");  // your face

    dead_face = W_ImageLookup("STFDEAD0");  // dead face

    for (i = 0; i < 10; i++)
    {
        // numbers 0-9
        char name[64];
        sprintf(name, "WINUM%d", i);
        digits[i] = W_ImageLookup(name);
    }

    for (i = 0; i < world_intermission.total_animations_; i++)
    {
        for (j = 0; j < world_intermission.animations_[i].total_frames_; j++)
        {
            // FIXME!!! Shorten :)
            L_WriteDebug("IntermissionLoadData: '%s'\n",
                         world_intermission.animations_[i]
                             .frames_[j]
                             .info->pic_.c_str());

            world_intermission.animations_[i].frames_[j].image =
                W_ImageLookup(world_intermission.animations_[i]
                                  .frames_[j]
                                  .info->pic_.c_str());
        }
    }
}

static void InitVariables(void)
{
    intermission_stats.level = intermission_stats.current_level->name_.c_str();
    intermission_stats.par_time = intermission_stats.current_level->partime_;

    accelerate_stage = false;
    count = background_count = 0;
    first_refresh            = 1;

    if (intermission_stats.kills <= 0) intermission_stats.kills = 1;

    if (intermission_stats.items <= 0) intermission_stats.items = 1;

    if (intermission_stats.secrets <= 0) intermission_stats.secrets = 1;

    GameDefinition *def = intermission_stats.current_level->episode_;

    SYS_ASSERT(def);

    world_intermission.Init(def);

    LoadData();
}

void IntermissionStart(void)
{
    InitVariables();

    const GameDefinition *gd = intermission_stats.current_level->episode_;
    SYS_ASSERT(gd);

    if (SP_MATCH())
        InitSinglePlayerStats();
    else if (DEATHMATCH())
        InitDeathmatchStats();
    else
        InitCoopStats();

    // -AJA- 1999/10/22: background cameras.
    background_camera_mo = nullptr;

    if (gd->bg_camera_ != "")
    {
        for (mobj_t *mo = mobjlisthead; mo != nullptr; mo = mo->next)
        {
            if (DDF_CompareName(mo->info->name_.c_str(),
                                gd->bg_camera_.c_str()) != 0)
                continue;

            background_camera_mo = mo;

            // we don't want to see players
            for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
            {
                player_t *p = players[pnum];

                if (p && p->mo)
                    p->mo->visibility = p->mo->vis_target = INVISIBLE;
            }

            break;
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
