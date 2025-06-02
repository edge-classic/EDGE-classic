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
// -KM- 1998/09/27 sounds.ddf stuff: seesound_ -> DDFLookupSound(seesound_)
// -KM- 1998/11/25 Finale generalised.
//

#include "f_finale.h"

#include "am_map.h"
#include "ddf_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "epi.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "i_movie.h"
#include "m_menu.h"
#include "m_random.h"
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
#include "stb_sprintf.h"
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

static const char *finale_text;

static GameAction              new_game_action;
static const FinaleDefinition *finale;

static void CastInitNew(int num);
static void CastTicker(void);
static void CastSkip(void);

static const Image *finale_text_background;
static float        finale_text_background_scale = 1.0f;
static RGBAColor    finale_text_color;

static Style *finale_level_text_style;
static Style *finale_cast_style;

// forward dec
static void DoBumpFinale(void);

static bool HasFinale(const FinaleDefinition *F, FinaleStage cur)
{
    EPI_ASSERT(F);

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
}

// returns kFinaleStageDone if nothing found
static FinaleStage FindValidFinale(const FinaleDefinition *F, FinaleStage cur)
{
    EPI_ASSERT(F);

    for (; cur != kFinaleStageDone; cur = (FinaleStage)(cur + 1))
    {
        if (HasFinale(F, cur))
            return cur;
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
        ChangeMusic(finale->music_, true);
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
            ChangeMusic(current_map->episode_->special_music_, true);
        break;

    case kFinaleStageCast:
        CastInitNew(2);
        if (current_map->episode_)
            ChangeMusic(current_map->episode_->special_music_, true);
        break;

    default:
        FatalError("DoStartFinale: bad stage #%d\n", (int)finale_stage);
    }

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
        if (players[pnum])
            players[pnum]->command_.buttons = 0;
}

static void DoBumpFinale(void)
{
    // find next valid Finale stage
    FinaleStage stage = finale_stage;
    stage             = (FinaleStage)(stage + 1);
    stage             = FindValidFinale(finale, stage);

    if (stage != kFinaleStageDone)
    {
        if (game_state != kGameStateIntermission)
            ForceWipe();

        finale_stage = stage;

        DoStartFinale();
        return;
    }

    // capture the screen _before_ changing any global state
    if (new_game_action != kGameActionNothing)
    {
        ForceWipe();
        game_action = new_game_action;
    }

    game_state = kGameStateNothing; // hack ???  (cannot leave as GS_FINALE)
}

static void LookupFinaleStuff(void)
{
    // here is where we lookup the required images

    if (finale->text_flat_ != "")
        finale_text_background = ImageLookup(finale->text_flat_.c_str(), kImageNamespaceFlat);
    else if (finale->text_back_ != "")
        finale_text_background = ImageLookup(finale->text_back_.c_str(), kImageNamespaceGraphic);
    else
        finale_text_background = nullptr;

    finale_text_color = GetFontColor(finale->text_colmap_);

    if (!finale_level_text_style)
    {
        StyleDefinition *def = styledefs.Lookup("INTERLEVEL TEXT");
        if (!def)
            def = default_style;
        finale_level_text_style = hud_styles.Lookup(def);
    }
    if (!finale_cast_style)
    {
        StyleDefinition *def = styledefs.Lookup("CAST_SCREEN");
        if (!def)
            def = default_style;
        finale_cast_style = hud_styles.Lookup(def);
    }
}

void FinaleStart(const FinaleDefinition *F, GameAction newaction)
{
    EPI_ASSERT(F);

    new_game_action = newaction;
    automap_active  = false;

    FinaleStage stage = FindValidFinale(F, kFinaleStageText);

    if (stage == kFinaleStageDone)
    {
        if (new_game_action != kGameActionNothing)
            game_action = new_game_action;

        return /* false */;
    }

    // capture the screen _before_ changing any global state
    //--- E_ForceWipe();   // CRASH with IDCLEV

    finale       = F;
    finale_stage = stage;

    LookupFinaleStuff();

    game_state = kGameStateFinale;

    DoStartFinale();
}

bool FinaleResponder(InputEvent *event)
{
    EPI_ASSERT(game_state == kGameStateFinale);

    // FIXME: use WI_CheckAccelerate() in netgames
    if (event->type != kInputEventKeyDown)
        return false;

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
    EPI_ASSERT(game_state == kGameStateFinale);

    // advance animation
    finale_count++;

    switch (finale_stage)
    {
    case kFinaleStageText:
        if (skip_finale && finale_count < (int)(strlen(finale_text) * finale->text_speed_))
        {
            finale_count = (int)(strlen(finale_text) * finale->text_speed_);
            skip_finale  = false;
        }
        else if (skip_finale ||
                 finale_count > (int)finale->text_wait_ + (int)(strlen(finale_text) * finale->text_speed_))
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
        if (picture_number >= (int)finale->pics_.size())
        {
            DoBumpFinale();
        }
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
    }

    if (finale_stage == kFinaleStageDone)
    {
        if (new_game_action != kGameActionNothing)
        {
            game_action = new_game_action;

            // don't come here again (for E_ForceWipe)
            new_game_action = kGameActionNothing;

            if (game_state == kGameStateFinale)
                ForceWipe();
        }
    }
}

static void TextWrite(void)
{
    // 98-7-10 KM erase the entire screen to a tiled background
    if (finale_text_background)
    {
        HUDSetScale(finale_text_background_scale);

        if (finale->text_flat_[0])
        {
            // AJA 2022: make the flats be square, not squished
            HUDSetCoordinateSystem(266, 200);

            // Lobo: if it's a flat, tile it
            HUDTileImage(hud_x_left, 0, hud_x_right - hud_x_left, 200,
                         finale_text_background); // Lobo: Widescreen support
        }
        else
        {
            if (title_scaling.d_) // Fill Border
            {
                if (!finale_text_background->blurred_version_)
                    StoreBlurredImage(finale_text_background);
                HUDStretchImage(-320, -200, 960, 600, finale_text_background->blurred_version_, 0, 0);
            }
            HUDDrawImageTitleWS(finale_text_background);
        }

        // reset coordinate system
        HUDReset();
    }

    Style *style;
    style      = finale_level_text_style;
    int t_type = StyleDefinition::kTextSectionText;

    // draw some of the text onto the screen
    int cx = 10;
    // int cy = 10;

    const char *ch = finale_text;

    int count = (int)((float)finale_count / finale->text_speed_);
    if (count < 0)
        count = 0;

    EPI_ASSERT(finale);

    // HUDSetFont();
    // HUDSetScale();
    HUDSetTextColor(finale_text_color); // set a default

    float txtscale = 0.9;               // set a default
    if (style->definition_->text_[t_type].scale_)
    {
        txtscale = style->definition_->text_[t_type].scale_;
        HUDSetScale(txtscale);
    }

    if (style->definition_->text_[t_type].colmap_)
    {
        const Colormap *colmap = style->definition_->text_[t_type].colmap_;
        HUDSetTextColor(GetFontColor(colmap));
    }

    int h = 11; // set a default
    if (style->fonts_[t_type])
    {
        HUDSetFont(style->fonts_[t_type]);
        h = style->fonts_[t_type]->NominalHeight();
        h = h + (3 * txtscale); // bit of spacing
        h = h * txtscale;
    }

    // Autoscale if there are too many lines of text to fit onscreen
    float TempHeight = StringLines(finale_text) * h;
    TempHeight += h;
    if (TempHeight > 200)
    {
        // Too big, need to scale
        float TempScale = 1.0f;

        TempScale = 200.0f / TempHeight;
        txtscale  = TempScale;
        HUDSetScale(txtscale);

        // Need to recalculate this
        h = 11; // set a default
        if (style->fonts_[t_type])
        {
            HUDSetFont(style->fonts_[t_type]);
            h = style->fonts_[t_type]->NominalHeight();
            h = h + (3 * txtscale); // bit of spacing
            h = h * txtscale;
        }
    }

    int cy = h;

    char line[200];
    int  pos = 0;

    line[0] = 0;

    const Colormap *Dropshadow_colmap = style->definition_->text_[t_type].dropshadow_colmap_;
    for (;;)
    {
        if (count == 0 || *ch == 0)
        {
            if (Dropshadow_colmap) // we want a dropshadow
            {
                float Dropshadow_Offset = style->definition_->text_[t_type].dropshadow_offset_;
                Dropshadow_Offset *= style->definition_->text_[t_type].scale_ * txtscale;
                HUDSetTextColor(GetFontColor(Dropshadow_colmap));
                HUDDrawText(cx + Dropshadow_Offset, cy + Dropshadow_Offset, line);
                HUDSetTextColor(finale_text_color); // set to default
                if (style->definition_->text_[t_type].colmap_)
                {
                    const Colormap *colmap = style->definition_->text_[t_type].colmap_;
                    HUDSetTextColor(GetFontColor(colmap));
                }
            }
            HUDDrawText(cx, cy, line);
            break;
        }

        int c = *ch++;
        count--;

        if (c == '\n' || pos > (int)sizeof(line) - 4)
        {
            if (Dropshadow_colmap) // we want a dropshadow
            {
                float Dropshadow_Offset = style->definition_->text_[t_type].dropshadow_offset_;
                Dropshadow_Offset *= style->definition_->text_[t_type].scale_ * txtscale;
                HUDSetTextColor(GetFontColor(Dropshadow_colmap));
                HUDDrawText(cx + Dropshadow_Offset, cy + Dropshadow_Offset, line);
                HUDSetTextColor(finale_text_color); // set to default
                if (style->definition_->text_[t_type].colmap_)
                {
                    const Colormap *colmap = style->definition_->text_[t_type].colmap_;
                    HUDSetTextColor(GetFontColor(colmap));
                }
            }
            HUDDrawText(cx, cy, line);

            pos     = 0;
            line[0] = 0;

            cy += h; // 11;
            continue;
        }

        line[pos++] = c;
        line[pos]   = 0;
    }

    // set back to defaults
    HUDSetFont();
    HUDSetScale();
    HUDSetTextColor();
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
    if (st == 0)
        return;

    cast_state = &states[st];

    cast_tics = cast_state->tics;
    if (cast_tics < 0)
        cast_tics = 15;
}

static void CAST_RangeAttack(const AttackDefinition *range)
{
    SoundEffect *sfx = nullptr;

    EPI_ASSERT(range);

    if (range->attackstyle_ == kAttackStyleShot)
    {
        sfx = range->sound_;
    }
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
    else if (range->atk_mobj_)
    {
        sfx = range->atk_mobj_->seesound_;
    }

    StartSoundEffect(sfx);
}

static void CastPerformAction(void)
{
    SoundEffect *sfx = nullptr;

    // Yuk, handles sounds

    if (cast_state->action == A_MakeCloseAttemptSound)
    {
        if (cast_order->closecombat_)
            sfx = cast_order->closecombat_->initsound_;
    }
    else if (cast_state->action == A_MeleeAttack)
    {
        if (cast_order->closecombat_)
            sfx = cast_order->closecombat_->sound_;
    }
    else if (cast_state->action == A_MakeRangeAttemptSound)
    {
        if (cast_order->rangeattack_)
            sfx = cast_order->rangeattack_->initsound_;
    }
    else if (cast_state->action == A_RangeAttack)
    {
        if (cast_order->rangeattack_)
            CAST_RangeAttack(cast_order->rangeattack_);
    }
    else if (cast_state->action == A_ComboAttack)
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
    else if (cast_order->activesound_ && (RandomByte() < 2) && !cast_death)
    {
        sfx = cast_order->activesound_;
    }
    else if (cast_state->action == A_WalkSoundChase)
    {
        sfx = cast_order->walksound_;
    }

    StartSoundEffect(sfx);
}

static void CastInitNew(int num)
{
    cast_order = mobjtypes.LookupCastMember(num);

    // FIXME!!! Better handling of the finale
    if (!cast_order)
        cast_order = mobjtypes.Lookup(0);

    cast_title = cast_order->cast_title_ != "" ? language[cast_order->cast_title_] : cast_order->name_.c_str();

    cast_death     = false;
    cast_frames    = 0;
    cast_on_melee  = 0;
    cast_attacking = false;

    EPI_ASSERT(cast_order->chase_state_); // checked in ddf_mobj.c
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
    if (cast_tics > 0)
        return;

    // switch from deathstate to next monster
    if (cast_state->tics == -1 || cast_state->nextstate == 0 || (cast_death && cast_frames >= 30))
    {
        CastInitNew(cast_order->castorder_ + 1);

        if (cast_order->seesound_)
            StartSoundEffect(cast_order->seesound_);

        return;
    }

    CastPerformAction();

    // advance to next state in animation
    // -AJA- if there's a jumpstate, enter it occasionally

    if (cast_state->action == A_Jump && cast_state->jumpstate && (RandomByte() < 64))
        st = cast_state->jumpstate;
    else
        st = cast_state->nextstate;

    CastSetState(st);
    cast_frames++;

    // go into attack frame
    if (cast_frames == 24 && !cast_death)
    {
        cast_on_melee ^= 1;
        st = cast_on_melee ? cast_order->melee_state_ : cast_order->missile_state_;

        if (st == 0)
        {
            cast_on_melee ^= 1;
            st = cast_on_melee ? cast_order->melee_state_ : cast_order->missile_state_;
        }

        // check if missing both melee and missile states
        if (st != 0)
        {
            cast_attacking = true;
            CastSetState(st);

            if (cast_order->attacksound_)
                StartSoundEffect(cast_order->attacksound_);
        }
    }

    // leave attack frames after a certain time
    if (cast_attacking && (cast_frames == 48 || cast_state == &states[cast_order->chase_state_]))
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
    if (cast_death)
        return; // already in dying frames

    // go into death frame
    cast_death = true;

    if (cast_order->overkill_state_ && (RandomByte() < 32))
        cast_state = &states[cast_order->overkill_state_];
    else
    {
        EPI_ASSERT(cast_order->death_state_); // checked in ddf_mobj.c
        cast_state = &states[cast_order->death_state_];
    }

    cast_tics      = cast_state->tics;
    cast_frames    = 0;
    cast_attacking = false;

    if (cast_order->deathsound_)
        StartSoundEffect(cast_order->deathsound_);
}

//
// CastDrawer
//
static void CastDrawer(void)
{
    float TempScale = 1.0;

    const Image *image;

    if (finale_cast_style->background_image_)
    {
        finale_cast_style->DrawBackground();
    }
    else
    {
        image = ImageLookup("BOSSBACK");
        if (title_scaling.d_) // Fill Border
        {
            if (!image->blurred_version_)
                StoreBlurredImage(image);
            HUDStretchImage(-320, -200, 960, 600, image->blurred_version_, 0, 0);
        }
        HUDDrawImageTitleWS(image);
    }

    HUDSetAlignment(0, -1);

    if (finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText].colmap_)
    {
        HUDSetTextColor(GetFontColor(finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText].colmap_));
    }
    else
    {
        HUDSetTextColor(kRGBAYellow);
    }

    TempScale = finale_cast_style->definition_->text_[StyleDefinition::kTextSectionText].scale_;
    HUDSetScale(TempScale);

    if (finale_cast_style->fonts_[StyleDefinition::kTextSectionText])
    {
        HUDSetFont(finale_cast_style->fonts_[StyleDefinition::kTextSectionText]);
    }

    HUDDrawText(160, 180, cast_title);

    HUDReset();

    bool flip;

    float pos_x, pos_y;
    float scale_x, scale_y;

    TempScale = finale_cast_style->definition_->text_[StyleDefinition::kTextSectionHeader].scale_;
    if (TempScale < 1.0 || TempScale > 1.0)
    {
        scale_y = finale_cast_style->definition_->text_[StyleDefinition::kTextSectionHeader].scale_;
    }
    else
        scale_y = 3;

    HUDGetCastPosition(&pos_x, &pos_y, &scale_x, &scale_y);

    if (cast_state->flags & kStateFrameFlagModel)
    {
        ModelDefinition *md = GetModel(cast_state->sprite);

        const Image *skin_img = md->skins_[cast_order->model_skin_];

        if (!skin_img)
            skin_img = ImageForDummySkin();

        render_state->Clear(GL_DEPTH_BUFFER_BIT);
        render_state->Enable(GL_DEPTH_TEST);

        if (md->md2_model_)
            MD2RenderModel2D(md->md2_model_, skin_img, cast_state->frame, pos_x, pos_y, scale_x, scale_y, cast_order);
        else if (md->mdl_model_)
            MDLRenderModel2D(md->mdl_model_, cast_state->frame, pos_x, pos_y, scale_x, scale_y, cast_order);

        render_state->Disable(GL_DEPTH_TEST);
        return;
    }

    // draw the current frame in the middle of the screen
    image = GetOtherSprite(cast_state->sprite, cast_state->frame, &flip);

    if (!image)
        return;

    scale_x *= cast_order->scale_ * cast_order->aspect_;
    scale_y *= cast_order->scale_;

    float width  = image->ScaledWidthActual();
    float height = image->ScaledHeightActual();

    float offset_x = image->ScaledOffsetX();
    float offset_y = image->ScaledOffsetY();

    if (flip)
        offset_x = -offset_x;

    offset_x = (width / 2.0f + offset_x) * scale_x;
    offset_y *= scale_y;

    width *= scale_x;
    height *= scale_y;

    HUDRawImage(pos_x - offset_x, pos_y + offset_y, pos_x - offset_x + width, pos_y + offset_y + height, image,
                flip ? image->Right() : 0, 0, flip ? 0 : image->Right(), image->Top(), 1.0f, kRGBANoValue);
}

//
// BunnyScroll
//
// -KM- 1998/07/31 Made our bunny friend take up more screen space.
// -KM- 1998/12/16 Removed fading routine.
// -Lobo- 2021/11/02 Widescreen support: both images must be the same size
static void BunnyScroll(void)
{
    int          scrolled;
    const Image *p1;
    const Image *p2;
    char         name[10];
    int          stage;
    static int   laststage;

    p1 = ImageLookup("PFUB2");
    p2 = ImageLookup("PFUB1");

    float TempWidth  = 0;
    float TempHeight = 0;
    float TempScale  = 0;
    float CenterX    = 0;
    // 1. Calculate scaling to apply.
    TempScale = 200;
    TempScale /= p1->actual_height_;
    TempWidth  = p1->actual_width_ * TempScale;
    TempHeight = p1->actual_height_ * TempScale;
    // 2. Calculate centering on screen.
    CenterX = 160;
    CenterX -= (p1->actual_width_ * TempScale) / 2;

    scrolled = (TempWidth + CenterX) - (finale_count - 230) / 2;
    if (scrolled > (TempWidth + CenterX))
        scrolled = (TempWidth + CenterX);
    if (scrolled < 0)
        scrolled = 0;

    HUDStretchImage(CenterX - scrolled, 0, TempWidth, TempHeight, p1, 0.0, 0.0);
    HUDStretchImage((CenterX + TempWidth) - (scrolled + 1), 0, TempWidth, TempHeight, p2, 0.0, 0.0);

    if (finale_count < 1130)
        return;

    if (finale_count < 1180)
    {
        p1 = ImageLookup("END0");

        HUDDrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
        laststage = 0;
        return;
    }

    stage = (finale_count - 1180) / 5;

    if (stage > 6)
        stage = 6;

    if (stage > laststage)
    {
        StartSoundEffect(sound_effect_pistol);
        laststage = stage;
    }

    stbsp_sprintf(name, "END%i", stage);

    p1 = ImageLookup(name);

    HUDDrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
}

void FinaleDrawer(void)
{
    EPI_ASSERT(game_state == kGameStateFinale);

    switch (finale_stage)
    {
    case kFinaleStageText:
        TextWrite();
        break;

    // Shouldn't get here, but just in case don't allow to fall through to
    // default (error)
    case kFinaleStageMovie:
        break;

    case kFinaleStagePicture: {
        const Image *image =
            ImageLookup(finale->pics_[HMM_MIN((size_t)picture_number, finale->pics_.size() - 1)].c_str());
        if (title_scaling.d_) // Fill Border
        {
            if (!image->blurred_version_)
                StoreBlurredImage(image);
            HUDStretchImage(-320, -200, 960, 600, image->blurred_version_, 0, 0);
        }
        HUDDrawImageTitleWS(image);
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
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
