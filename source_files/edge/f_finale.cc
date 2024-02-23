//----------------------------------------------------------------------------
// EDGE Finale Code on Game Completion
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
// -KM- 1998/07/21 Clear the background behind those end pics.
// -KM- 1998/09/27 sounds.ddf stuff: seesound_ -> DDF_LookupSound(seesound_)
// -KM- 1998/11/25 Finale generalised.
//

#include "f_finale.h"

#include "am_map.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "i_movie.h"
#include "m_menu.h"
#include "m_random.h"
#include "main.h"
#include "p_action.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_md2.h"
#include "r_mdl.h"
#include "r_modes.h"
#include "r_state.h"
#include "s_music.h"
#include "s_sound.h"
#include "w_model.h"
#include "w_wad.h"

enum FinaleStage
{
    kFinaleStageText,
    kFinaleStageMovie,
    kFinaleStagePicture,
    kFinaleStageBunny,
    kFinaleStageCast,
    kFinaleStageDone
};

// Stage of animation
static FinaleStage finale_stage;

// -ES- 2000/03/11 skip to the next finale stage
static bool skip_finale;
static int  finale_count;
static int  picture_number;

static constexpr uint8_t kFinaleTextSpeed    = 3;
static constexpr uint8_t kFinaleTextWaitTime = 250;

static const char *finale_text;

static GameAction            new_game_action;
static const FinaleDefinition *finale;

static void CastInitNew(int num);
static void CastTicker(void);
static void CastSkip(void);

static const image_c *finale_text_background;
static float          finale_text_background_scale = 1.0f;
static RGBAColor      finale_text_color;

static Style *finale_level_text_style;
static Style *finale_cast_style;

// forward dec
static void DoBumpFinale(void);

static bool HasFinale(const FinaleDefinition *F, FinaleStage cur)
{
    SYS_ASSERT(F);

    switch (cur)
    {
        case kFinaleStageText:
            return F->text_ != "";

        case kFinaleStageMovie:
            return F->movie_ != "";

        case kFinaleStagePicture:
            return (F->pics_.size() > 0);

        case kFinaleStageBunny:
            return F->dobunny_;

        case kFinaleStageCast:
            return F->docast_;

        default:
            FatalError("Bad parameter passed to HasFinale().\n");
    }

    return false; /* NOT REACHED */
}

// returns kFinaleStageDone if nothing found
static FinaleStage FindValidFinale(const FinaleDefinition *F, FinaleStage cur)
{
    SYS_ASSERT(F);

    for (; cur != kFinaleStageDone; cur = (FinaleStage)(cur + 1))
    {
        if (HasFinale(F, cur)) return cur;
    }

    return kFinaleStageDone;
}

static void DoStartFinale(void)
{
    finale_count = 0;

    switch (finale_stage)
    {
        case kFinaleStageText:
            finale_text = language[finale->text_];
            S_ChangeMusic(finale->music_, true);
            break;

        case kFinaleStageMovie:
            PlayMovie(finale->movie_);
            DoBumpFinale();
            break;

        case kFinaleStagePicture:
            picture_number = 0;
            break;

        case kFinaleStageBunny:
            if (current_map->episode_)
                S_ChangeMusic(current_map->episode_->special_music_, true);
            break;

        case kFinaleStageCast:
            CastInitNew(2);
            if (current_map->episode_)
                S_ChangeMusic(current_map->episode_->special_music_, true);
            break;

        default:
            FatalError("DoStartFinale: bad stage #%d\n", (int)finale_stage);
            break;
    }

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
        if (players[pnum]) players[pnum]->cmd.buttons = 0;
}

static void DoBumpFinale(void)
{
    // find next valid Finale stage
    FinaleStage stage = finale_stage;
    stage             = (FinaleStage)(stage + 1);
    stage             = FindValidFinale(finale, stage);

    if (stage != kFinaleStageDone)
    {
        if (game_state != GS_INTERMISSION) E_ForceWipe();

        finale_stage = stage;

        DoStartFinale();
        return;
    }

    // capture the screen _before_ changing any global state
    if (new_game_action != kGameActionNothing)
    {
        E_ForceWipe();
        game_action = new_game_action;
    }

    game_state = GS_NOTHING;  // hack ???  (cannot leave as GS_FINALE)
}

static void LookupFinaleStuff(void)
{
    // here is where we lookup the required images

    if (finale->text_flat_ != "")
        finale_text_background =
            W_ImageLookup(finale->text_flat_.c_str(), kImageNamespaceFlat);
    else if (finale->text_back_ != "")
        finale_text_background =
            W_ImageLookup(finale->text_back_.c_str(), kImageNamespaceGraphic);
    else
        finale_text_background = nullptr;

    finale_text_color = V_GetFontColor(finale->text_colmap_);

    if (!finale_level_text_style)
    {
        StyleDefinition *def = styledefs.Lookup("INTERLEVEL TEXT");
        if (!def) def = default_style;
        finale_level_text_style = hud_styles.Lookup(def);
    }
    if (!finale_cast_style)
    {
        StyleDefinition *def = styledefs.Lookup("CAST_SCREEN");
        if (!def) def = default_style;
        finale_cast_style = hud_styles.Lookup(def);
    }
}

void FinaleStart(const FinaleDefinition *F, GameAction newaction)
{
    SYS_ASSERT(F);

    new_game_action = newaction;
    automap_active  = false;

    FinaleStage stage = FindValidFinale(F, kFinaleStageText);

    if (stage == kFinaleStageDone)
    {
        if (new_game_action != kGameActionNothing) game_action = new_game_action;

        return /* false */;
    }

    // capture the screen _before_ changing any global state
    //--- E_ForceWipe();   // CRASH with IDCLEV

    finale       = F;
    finale_stage = stage;

    LookupFinaleStuff();

    game_state = GS_FINALE;

    DoStartFinale();
}

bool FinaleResponder(InputEvent *event)
{
    SYS_ASSERT(game_state == GS_FINALE);

    // FIXME: use WI_CheckAccelerate() in netgames
    if (event->type != kInputEventKeyDown) return false;

    if (finale_count > kTicRate)
    {
        if (finale_stage == kFinaleStagePicture && finale->picwait_ == INT_MAX)
            return false;

        skip_finale = true;
        return true;
    }

    return false;
}

void FinaleTicker(void)
{
    SYS_ASSERT(game_state == GS_FINALE);

    // advance animation
    finale_count++;

    switch (finale_stage)
    {
        case kFinaleStageText:
            if (skip_finale &&
                finale_count < (int)strlen(finale_text) * kFinaleTextSpeed)
            {
                finale_count = kFinaleTextSpeed * strlen(finale_text);
                skip_finale  = false;
            }
            else if (skip_finale ||
                     finale_count >
                         kFinaleTextWaitTime +
                             (int)strlen(finale_text) * kFinaleTextSpeed)
            {
                DoBumpFinale();
                skip_finale = false;
            }
            break;

        case kFinaleStagePicture:
            if (skip_finale || finale_count > (int)finale->picwait_)
            {
                picture_number++;
                finale_count = 0;
                skip_finale  = false;
            }
            if (picture_number >= (int)finale->pics_.size()) { DoBumpFinale(); }
            break;

        case kFinaleStageBunny:
            if (skip_finale && finale_count < 1100)
            {
                finale_count = 1100;
                skip_finale  = false;
            }
            break;

        case kFinaleStageCast:
            if (skip_finale)
            {
                CastSkip();
                skip_finale = false;
            }
            else
                CastTicker();

            break;

        default:
            FatalError("FinaleTicker: bad finale_stage #%d\n", (int)finale_stage);
            break;
    }

    if (finale_stage == kFinaleStageDone)
    {
        if (new_game_action != kGameActionNothing)
        {
            game_action = new_game_action;

            // don't come here again (for E_ForceWipe)
            new_game_action = kGameActionNothing;

            if (game_state == GS_FINALE) E_ForceWipe();
        }
    }
}

static void TextWrite(void)
{
    // 98-7-10 KM erase the entire screen to a tiled background
    if (finale_text_background)
    {
        HudSetScale(finale_text_background_scale);

        if (finale->text_flat_[0])
        {
            // AJA 2022: make the flats be square, not squished
            HudSetCoordinateSystem(266, 200);

            // Lobo: if it's a flat, tile it
            HudTileImage(hud_x_left, 0, hud_x_right - hud_x_left, 200,
                          finale_text_background);  // Lobo: Widescreen support
        }
        else
        {
            if (r_titlescaling.d_)  // Fill Border
            {
                if (!finale_text_background->blurred_version)
                    W_ImageStoreBlurred(finale_text_background, 0.75f);
                HudStretchImage(-320, -200, 960, 600,
                                 finale_text_background->blurred_version, 0, 0);
            }
            HudDrawImageTitleWS(finale_text_background);
        }

        // reset coordinate system
        HudReset();
    }

    Style *style;
    style      = finale_level_text_style;
    int t_type = StyleDefinition::kTextSectionText;

    // draw some of the text onto the screen
    int cx = 10;
    // int cy = 10;

    const char *ch = finale_text;

    int count = (int)((finale_count - 10) / finale->text_speed_);
    if (count < 0) count = 0;

    SYS_ASSERT(finale);

    // HudSetFont();
    // HudSetScale();
    HudSetTextColor(finale_text_color);  // set a default

    float txtscale = 0.9;  // set a default
    if (style->definition_->text_[t_type].scale_)
    {
        txtscale = style->definition_->text_[t_type].scale_;
        HudSetScale(txtscale);
    }

    if (style->definition_->text_[t_type].colmap_)
    {
        const Colormap *colmap = style->definition_->text_[t_type].colmap_;
        HudSetTextColor(V_GetFontColor(colmap));
    }

    int h = 11;  // set a default
    if (style->fonts_[t_type])
    {
        HudSetFont(style->fonts_[t_type]);
        h = style->fonts_[t_type]->NominalHeight();
        h = h + (3 * txtscale);  // bit of spacing
        h = h * txtscale;
    }

    int cy = h;

    char line[200];
    int  pos = 0;

    line[0] = 0;

    for (;;)
    {
        if (count == 0 || *ch == 0)
        {
            HudDrawText(cx, cy, line);
            break;
        }

        int c = *ch++;
        count--;

        if (c == '\n' || pos > (int)sizeof(line) - 4)
        {
            HudDrawText(cx, cy, line);

            pos     = 0;
            line[0] = 0;

            cy += h;  // 11;
            continue;
        }

        line[pos++] = c;
        line[pos]   = 0;
    }

    // set back to defaults
    HudSetFont();
    HudSetScale();
    HudSetTextColor();
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//

static const MapObjectDefinition *cast_order;
static const char                *cast_title;
static int                        cast_tics;
static State                     *cast_state;
static bool                       cast_death;
static int                        cast_frames;
static int                        cast_on_melee;
static bool                       cast_attacking;

//
// CastSetState, CastPerformAction
//
// -AJA- 2001/05/28: separated this out from CastTicker
//
static void CastSetState(int st)
{
    if (st == 0) return;

    cast_state = &states[st];

    cast_tics = cast_state->tics;
    if (cast_tics < 0) cast_tics = 15;
}

static void CAST_RangeAttack(const AttackDefinition *range)
{
    SoundEffect *sfx = nullptr;

    SYS_ASSERT(range);

    if (range->attackstyle_ == kAttackStyleShot) { sfx = range->sound_; }
    else if (range->attackstyle_ == kAttackStyleSkullFly)
    {
        sfx = range->initsound_;
    }
    else if (range->attackstyle_ == kAttackStyleSpawner)
    {
        if (range->spawnedobj_ && range->spawnedobj_->rangeattack_)
            sfx = range->spawnedobj_->rangeattack_->initsound_;
    }
    else if (range->attackstyle_ == kAttackStyleTracker)
    {
        sfx = range->initsound_;
    }
    else if (range->atk_mobj_) { sfx = range->atk_mobj_->seesound_; }

    S_StartFX(sfx);
}

static void CastPerformAction(void)
{
    SoundEffect *sfx = nullptr;

    // Yuk, handles sounds

    if (cast_state->action == P_ActMakeCloseAttemptSound)
    {
        if (cast_order->closecombat_)
            sfx = cast_order->closecombat_->initsound_;
    }
    else if (cast_state->action == P_ActMeleeAttack)
    {
        if (cast_order->closecombat_) sfx = cast_order->closecombat_->sound_;
    }
    else if (cast_state->action == P_ActMakeRangeAttemptSound)
    {
        if (cast_order->rangeattack_)
            sfx = cast_order->rangeattack_->initsound_;
    }
    else if (cast_state->action == P_ActRangeAttack)
    {
        if (cast_order->rangeattack_)
            CAST_RangeAttack(cast_order->rangeattack_);
    }
    else if (cast_state->action == P_ActComboAttack)
    {
        if (cast_on_melee && cast_order->closecombat_)
        {
            sfx = cast_order->closecombat_->sound_;
        }
        else if (cast_order->rangeattack_)
        {
            CAST_RangeAttack(cast_order->rangeattack_);
        }
    }
    else if (cast_order->activesound_ && (M_Random() < 2) && !cast_death)
    {
        sfx = cast_order->activesound_;
    }
    else if (cast_state->action == P_ActWalkSoundChase)
    {
        sfx = cast_order->walksound_;
    }

    S_StartFX(sfx);
}

static void CastInitNew(int num)
{
    cast_order = mobjtypes.LookupCastMember(num);

    // FIXME!!! Better handling of the finale
    if (!cast_order) cast_order = mobjtypes.Lookup(0);

    cast_title = cast_order->cast_title_ != ""
                     ? language[cast_order->cast_title_]
                     : cast_order->name_.c_str();

    cast_death     = false;
    cast_frames    = 0;
    cast_on_melee  = 0;
    cast_attacking = false;

    SYS_ASSERT(cast_order->chase_state_);  // checked in ddf_mobj.c
    CastSetState(cast_order->chase_state_);
}

//
// CastTicker
//
// -KM- 1998/10/29 Use sfx_t.
//      Known bug: Chaingun/Spiderdemon's sounds aren't stopped.
//
static void CastTicker(void)
{
    int st;

    // time to change state yet ?
    cast_tics--;
    if (cast_tics > 0) return;

    // switch from deathstate to next monster
    if (cast_state->tics == -1 || cast_state->nextstate == 0 ||
        (cast_death && cast_frames >= 30))
    {
        CastInitNew(cast_order->castorder_ + 1);

        if (cast_order->seesound_) S_StartFX(cast_order->seesound_);

        return;
    }

    CastPerformAction();

    // advance to next state in animation
    // -AJA- if there's a jumpstate, enter it occasionally

    if (cast_state->action == P_ActJump && cast_state->jumpstate &&
        (M_Random() < 64))
        st = cast_state->jumpstate;
    else
        st = cast_state->nextstate;

    CastSetState(st);
    cast_frames++;

    // go into attack frame
    if (cast_frames == 24 && !cast_death)
    {
        cast_on_melee ^= 1;
        st = cast_on_melee ? cast_order->melee_state_
                           : cast_order->missile_state_;

        if (st == 0)
        {
            cast_on_melee ^= 1;
            st = cast_on_melee ? cast_order->melee_state_
                               : cast_order->missile_state_;
        }

        // check if missing both melee and missile states
        if (st != 0)
        {
            cast_attacking = true;
            CastSetState(st);

            if (cast_order->attacksound_) S_StartFX(cast_order->attacksound_);
        }
    }

    // leave attack frames after a certain time
    if (cast_attacking &&
        (cast_frames == 48 || cast_state == &states[cast_order->chase_state_]))
    {
        cast_attacking = false;
        cast_frames    = 0;
        CastSetState(cast_order->chase_state_);
    }
}

//
// CastSkip
//
static void CastSkip(void)
{
    if (cast_death) return;  // already in dying frames

    // go into death frame
    cast_death = true;

    if (cast_order->overkill_state_ && (M_Random() < 32))
        cast_state = &states[cast_order->overkill_state_];
    else
    {
        SYS_ASSERT(cast_order->death_state_);  // checked in ddf_mobj.c
        cast_state = &states[cast_order->death_state_];
    }

    cast_tics      = cast_state->tics;
    cast_frames    = 0;
    cast_attacking = false;

    if (cast_order->deathsound_) S_StartFX(cast_order->deathsound_);
}

//
// CastDrawer
//
static void CastDrawer(void)
{
    float TempScale = 1.0;

    const image_c *image;

    if (finale_cast_style->background_image_) { finale_cast_style->DrawBackground(); }
    else
    {
        image = W_ImageLookup("BOSSBACK");
        if (r_titlescaling.d_)  // Fill Border
        {
            if (!image->blurred_version) W_ImageStoreBlurred(image, 0.75f);
            HudStretchImage(-320, -200, 960, 600, image->blurred_version, 0,
                             0);
        }
        HudDrawImageTitleWS(image);
    }

    HudSetAlignment(0, -1);

    if (finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText]
            .colmap_)
    {
        HudSetTextColor(V_GetFontColor(
            finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText]
                .colmap_));
    }
    else { HudSetTextColor(SG_YELLOW_RGBA32); }

    TempScale =
        finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText].scale_;
    HudSetScale(TempScale);

    if (finale_cast_style->fonts_[StyleDefinition::kTextSectionText])
    {
        HudSetFont(
            finale_cast_style->fonts_[StyleDefinition::kTextSectionText]);
    }

    HudDrawText(160, 180, cast_title);

    HudReset();

    bool flip;

    float pos_x, pos_y;
    float scale_x, scale_y;

    TempScale =
        finale_cast_style->definition_->text_[StyleDefinition::kTextSectionHeader]
            .scale_;
    if (TempScale < 1.0 || TempScale > 1.0)
    {
        scale_y =
            finale_cast_style->definition_->text_[StyleDefinition::kTextSectionHeader]
                .scale_;
    }
    else
        scale_y = 3;

    HudGetCastPosition(&pos_x, &pos_y, &scale_x, &scale_y);

    if (cast_state->flags & kStateFrameFlagModel)
    {
        modeldef_c *md = W_GetModel(cast_state->sprite);

        const image_c *skin_img = md->skins[cast_order->model_skin_];

        if (!skin_img) skin_img = W_ImageForDummySkin();

        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        if (md->md2_model)
            MD2_RenderModel_2D(md->md2_model, skin_img, cast_state->frame,
                               pos_x, pos_y, scale_x, scale_y, cast_order);
        else if (md->mdl_model)
            MDL_RenderModel_2D(md->mdl_model, skin_img, cast_state->frame,
                               pos_x, pos_y, scale_x, scale_y, cast_order);

        glDisable(GL_DEPTH_TEST);
        return;
    }

    // draw the current frame in the middle of the screen
    image = R2_GetOtherSprite(cast_state->sprite, cast_state->frame, &flip);

    if (!image) return;

    scale_x *= cast_order->scale_ * cast_order->aspect_;
    scale_y *= cast_order->scale_;

    float width  = IM_WIDTH(image);
    float height = IM_HEIGHT(image);

    float offset_x = IM_OFFSETX(image);
    float offset_y = IM_OFFSETY(image);

    if (flip) offset_x = -offset_x;

    offset_x = (width / 2.0f + offset_x) * scale_x;
    offset_y *= scale_y;

    width *= scale_x;
    height *= scale_y;

    RGL_DrawImage(pos_x - offset_x, pos_y + offset_y, width, height, image,
                  flip ? IM_RIGHT(image) : 0, 0, flip ? 0 : IM_RIGHT(image),
                  IM_TOP(image), nullptr, 1.0f, cast_order->palremap_);
}

//
// BunnyScroll
//
// -KM- 1998/07/31 Made our bunny friend take up more screen space.
// -KM- 1998/12/16 Removed fading routine.
// -Lobo- 2021/11/02 Widescreen support: both images must be the same size
static void BunnyScroll(void)
{
    int            scrolled;
    const image_c *p1;
    const image_c *p2;
    char           name[10];
    int            stage;
    static int     laststage;

    p1 = W_ImageLookup("PFUB2");
    p2 = W_ImageLookup("PFUB1");

    float TempWidth  = 0;
    float TempHeight = 0;
    float TempScale  = 0;
    float CenterX    = 0;
    // 1. Calculate scaling to apply.
    TempScale = 200;
    TempScale /= p1->actual_h;
    TempWidth  = p1->actual_w * TempScale;
    TempHeight = p1->actual_h * TempScale;
    // 2. Calculate centering on screen.
    CenterX = 160;
    CenterX -= (p1->actual_w * TempScale) / 2;

    scrolled = (TempWidth + CenterX) - (finale_count - 230) / 2;
    if (scrolled > (TempWidth + CenterX)) scrolled = (TempWidth + CenterX);
    if (scrolled < 0) scrolled = 0;

    HudStretchImage(CenterX - scrolled, 0, TempWidth, TempHeight, p1, 0.0,
                     0.0);
    HudStretchImage((CenterX + TempWidth) - (scrolled + 1), 0, TempWidth,
                     TempHeight, p2, 0.0, 0.0);

    if (finale_count < 1130) return;

    if (finale_count < 1180)
    {
        p1 = W_ImageLookup("END0");

        HudDrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
        laststage = 0;
        return;
    }

    stage = (finale_count - 1180) / 5;

    if (stage > 6) stage = 6;

    if (stage > laststage)
    {
        S_StartFX(sfx_pistol);
        laststage = stage;
    }

    sprintf(name, "END%i", stage);

    p1 = W_ImageLookup(name);

    HudDrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
}

void FinaleDrawer(void)
{
    SYS_ASSERT(game_state == GS_FINALE);

    switch (finale_stage)
    {
        case kFinaleStageText:
            TextWrite();
            break;

        // Shouldn't get here, but just in case don't allow to fall through to
        // default (error)
        case kFinaleStageMovie:
            break;

        case kFinaleStagePicture:
        {
            const image_c *image =
                W_ImageLookup(finale
                                  ->pics_[HMM_MIN((size_t)picture_number,
                                                  finale->pics_.size() - 1)]
                                  .c_str());
            if (r_titlescaling.d_)  // Fill Border
            {
                if (!image->blurred_version) W_ImageStoreBlurred(image, 0.75f);
                HudStretchImage(-320, -200, 960, 600, image->blurred_version,
                                 0, 0);
            }
            HudDrawImageTitleWS(image);
            break;
        }

        case kFinaleStageBunny:
            BunnyScroll();
            break;

        case kFinaleStageCast:
            CastDrawer();
            break;

        default:
            FatalError("FinaleDrawer: bad finale_stage #%d\n", (int)finale_stage);
            break;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
