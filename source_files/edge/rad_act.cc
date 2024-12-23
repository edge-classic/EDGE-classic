//----------------------------------------------------------------------------
//  EDGE Radius Trigger Actions
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
// -AJA- 1999/10/24: Split these off from the rad_trig.c file.
//

#include "rad_act.h"

#include <limits.h>

#include "AlmostEquals.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "rad_trig.h"
#include "s_music.h"
#include "s_sound.h"
#include "sv_chunk.h"
#include "w_texture.h"
#include "w_wad.h"

static Style *rts_tip_style;

// current tip slots
ScriptDrawTip tip_slots[kMaximumTipSlots];

// properties for fixed slots
static constexpr uint8_t kFixedSlots = 15;

static ScriptTipProperties fixed_props[kFixedSlots] = {
    {1, 0.50f, 0.50f, 0, "#FFFFFF", 1.0f, 0},  {2, 0.20f, 0.25f, 1, "#FFFFFF", 1.0f, 0},
    {3, 0.20f, 0.75f, 1, "#FFFFFF", 1.0f, 0},  {4, 0.50f, 0.50f, 0, "#3333FF", 1.0f, 0},
    {5, 0.20f, 0.25f, 1, "#3333FF", 1.0f, 0},  {6, 0.20f, 0.75f, 1, "#3333FF", 1.0f, 0},
    {7, 0.50f, 0.50f, 0, "#FFFF00", 1.0f, 0},  {8, 0.20f, 0.25f, 1, "#FFFF00", 1.0f, 0},
    {9, 0.20f, 0.75f, 1, "#FFFF00", 1.0f, 0},  {10, 0.50f, 0.50f, 0, "", 1.0f, 0},
    {11, 0.20f, 0.25f, 1, "", 1.0f, 0},        {12, 0.20f, 0.75f, 1, "", 1.0f, 0},
    {13, 0.50f, 0.50f, 0, "#33FF33", 1.0f, 0}, {14, 0.20f, 0.25f, 1, "#33FF33", 1.0f, 0},
    {15, 0.20f, 0.75f, 1, "#33FF33", 1.0f, 0}};

//
// Once-only initialisation.
//
void InitializeScriptTips(void)
{
    for (int i = 0; i < kMaximumTipSlots; i++)
    {
        ScriptDrawTip *current = tip_slots + i;

        // initial properties
        EPI_CLEAR_MEMORY(current, ScriptDrawTip, 1);

        current->p = fixed_props[i % kFixedSlots];

        current->delay = -1;
        current->color = kRGBANoValue;

        current->p.slot_num = i;
    }
}

//
// Used when changing levels to clear any tips.
//
void ResetScriptTips(void)
{
    // free any text strings
    for (int i = 0; i < kMaximumTipSlots; i++)
    {
        ScriptDrawTip *current = tip_slots + i;

        SaveChunkFreeString(current->tip_text);
    }

    InitializeScriptTips();
}

static void SetupTip(ScriptDrawTip *cur)
{
    if (cur->tip_graphic)
        return;

    if (cur->color == kRGBANoValue)
        cur->color = ParseFontColor(cur->p.color_name);
}

static void SendTip(RADScriptTrigger *R, ScriptTip *tip, int slot)
{
    ScriptDrawTip *current;

    EPI_ASSERT(0 <= slot && slot < kMaximumTipSlots);

    current = tip_slots + slot;

    current->delay = tip->display_time;

    SaveChunkFreeString(current->tip_text);

    if (tip->tip_ldf)
        current->tip_text = SaveChunkCopyString(language[tip->tip_ldf]);
    else if (tip->tip_text)
        current->tip_text = SaveChunkCopyString(tip->tip_text);
    else
        current->tip_text = nullptr;

    // send message to the console (unless it would clog it up)
    if (current->tip_text && current->tip_text != R->last_con_message)
    {
        ConsolePrint("%s\n", current->tip_text);
        R->last_con_message = current->tip_text;
    }

    current->tip_graphic = tip->tip_graphic ? ImageLookup(tip->tip_graphic) : nullptr;
    current->playsound   = tip->playsound ? true : false;
    // current->scale       = tip->tip_graphic ? tip->gfx_scale : 1.0f;
    current->scale     = tip->gfx_scale;
    current->fade_time = 0;

    // mark it as "set me up please"
    current->dirty = true;
}

//
// -AJA- 1999/09/07: Reworked to handle tips with multiple lines.
//
void DisplayScriptTips(void)
{
    HUDReset();

    // lookup styles
    StyleDefinition *def;

    def = styledefs.Lookup("RTS_TIP");
    if (!def)
        def = default_style;
    rts_tip_style = hud_styles.Lookup(def);

    for (int slot = 0; slot < kMaximumTipSlots; slot++)
    {
        ScriptDrawTip *current = tip_slots + slot;

        // Is there actually a tip to display ?
        if (current->delay < 0)
            continue;

        if (current->dirty)
        {
            SetupTip(current);
            current->dirty = false;
        }

        // If the display time is up reset the tip and erase it.
        if (current->delay == 0)
        {
            current->delay = -1;
            continue;
        }

        // Make a noise when the tip is first displayed.
        // Note: This happens only once.

        if (current->playsound)
        {
            // SFX_FIXME: Use new form
            StartSoundEffect(sfxdefs.GetEffect("TINK"));
            current->playsound = false;
        }

        float alpha = current->p.translucency;

        if (alpha < 0.02f)
            continue;

        HUDSetScale(current->scale);
        HUDSetTextColor(current->color);
        HUDSetAlpha(alpha);

        if (current->p.left_just)
            HUDSetAlignment(-1, 0);
        else
            HUDSetAlignment(0, 0);

        float x = current->p.x_pos * 320.0f;
        float y = current->p.y_pos * 200.0f;

        if (rts_tip_style->fonts_[StyleDefinition::kTextSectionText])
            HUDSetFont(rts_tip_style->fonts_[StyleDefinition::kTextSectionText]);

        if (current->tip_graphic)
            HUDDrawImage(x, y, current->tip_graphic);
        else
        {
            const Colormap *Dropshadow_colmap =
                rts_tip_style->definition_->text_[StyleDefinition::kTextSectionText].dropshadow_colmap_;
            if (Dropshadow_colmap) // we want a dropshadow
            {
                float Dropshadow_Offset =
                    rts_tip_style->definition_->text_[StyleDefinition::kTextSectionText].dropshadow_offset_;
                Dropshadow_Offset *=
                    rts_tip_style->definition_->text_[StyleDefinition::kTextSectionText].scale_ * current->scale;
                HUDSetTextColor(GetFontColor(Dropshadow_colmap));
                HUDDrawText(x + Dropshadow_Offset, y + Dropshadow_Offset, current->tip_text);
                HUDSetTextColor(current->color);
            }
            HUDDrawText(x, y, current->tip_text);
        }

        HUDSetAlignment();
        HUDSetAlpha();
        HUDSetScale();
        HUDSetTextColor();
    }
}

//
// Does any tic-related RTS stuff.  For now, just update the tips.
//
void ScriptTicker(void)
{
    for (int i = 0; i < kMaximumTipSlots; i++)
    {
        ScriptDrawTip *current = tip_slots + i;

        if (current->delay < 0)
            continue;

        if (current->delay > 0)
            current->delay--;

        // handle fading
        if (current->fade_time > 0)
        {
            float diff = current->fade_target - current->p.translucency;

            current->fade_time--;

            if (current->fade_time == 0)
                current->p.translucency = current->fade_target;
            else
                current->p.translucency += diff / (current->fade_time + 1);
        }
    }
}

// --- Radius Trigger Actions -----------------------------------------------

static Player *GetWhoDunnit(RADScriptTrigger *R)
{
    EPI_UNUSED(R);
    return players[console_player];

    /*
    // this IS NOT CORRECT, but matches old behavior
    if (numplayers == 1)
        return players[consoleplayer];

    if (R->acti_players == 0)
        return nullptr;

    // does the activator list have only one player?
    // if so, return that one.
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
        if (R->acti_players == (1 << pnum))
            return players[pnum];

    // there are multiple players who triggered the script.
    // one option: select one of them (round robin style).
    // However the following is probably more correct.
    //return nullptr;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
        if (R->acti_players & (1 << pnum))
            return players[pnum];
    */
}

void ScriptNoOperation(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    EPI_UNUSED(param);
    // No Operation
}

void ScriptShowTip(RADScriptTrigger *R, void *param)
{
    ScriptTip *tip = (ScriptTip *)param;

    // Only display the tip to the player that stepped into the radius
    // trigger.

    if (total_players > 1 && (R->acti_players & (1 << console_player)) == 0)
        return;

    SendTip(R, tip, R->tip_slot);
}

void ScriptUpdateTipProperties(RADScriptTrigger *R, void *param)
{
    ScriptTipProperties *tp = (ScriptTipProperties *)param;
    ScriptDrawTip       *current;

    if (total_players > 1 && (R->acti_players & (1 << console_player)) == 0)
        return;

    if (tp->slot_num >= 0)
        R->tip_slot = tp->slot_num;

    EPI_ASSERT(0 <= R->tip_slot && R->tip_slot < kMaximumTipSlots);

    current = tip_slots + R->tip_slot;

    if (tp->x_pos >= 0)
        current->p.x_pos = tp->x_pos;

    if (tp->y_pos >= 0)
        current->p.y_pos = tp->y_pos;

    if (tp->left_just >= 0)
        current->p.left_just = tp->left_just;

    if (tp->color_name)
        current->color = ParseFontColor(tp->color_name);

    if (tp->translucency >= 0)
    {
        if (tp->time == 0)
            current->p.translucency = tp->translucency;
        else
        {
            current->fade_target = tp->translucency;
            current->fade_time   = tp->time;
        }
    }

    // make tip system recompute some stuff
    current->dirty = true;
}

void ScriptSpawnThing(RADScriptTrigger *R, void *param)
{
    ScriptThingParameter *t = (ScriptThingParameter *)param;

    MapObject                 *mo;
    const MapObjectDefinition *minfo;

    // Spawn a new map object.

    if (t->thing_name)
        minfo = mobjtypes.Lookup(t->thing_name);
    else
        minfo = mobjtypes.Lookup(t->thing_type);

    if (minfo == nullptr)
    {
        if (t->thing_name)
            LogWarning("Unknown thing type: %s in RTS trigger.\n", t->thing_name);
        else
            LogWarning("Unknown thing type: %d in RTS trigger.\n", t->thing_type);

        return;
    }

    // -AJA- 2007/09/04: allow individual when_appear flags
    if (!CheckWhenAppear(t->appear))
        return;

    // -AJA- 1999/10/02: -nomonsters check.
    if (level_flags.no_monsters && (minfo->extended_flags_ & kExtendedFlagMonster))
        return;

    // -AJA- 1999/10/07: -noextra check.
    if (!level_flags.have_extra && (minfo->extended_flags_ & kExtendedFlagExtra))
        return;

    // -AJA- 1999/09/11: Support for supplying Z value.

    if (t->spawn_effect)
    {
        mo = CreateMapObject(t->x, t->y, t->z, minfo->respawneffect_);
    }

    mo = CreateMapObject(t->x, t->y, t->z, minfo);

    // -ACB- 1998/07/10 New Check, so that spawned mobj's don't
    //                  spawn somewhere where they should not.
    if (!CheckAbsolutePosition(mo, mo->x, mo->y, mo->z))
    {
        RemoveMapObject(mo);
        return;
    }

    MapObjectSetDirectionAndSpeed(mo, t->angle, t->slope, 0);

    mo->tag_ = t->tag;

    mo->spawnpoint_.x              = t->x;
    mo->spawnpoint_.y              = t->y;
    mo->spawnpoint_.z              = t->z;
    mo->spawnpoint_.angle          = t->angle;
    mo->spawnpoint_.vertical_angle = epi::BAMFromATan(t->slope);
    mo->spawnpoint_.info           = minfo;
    mo->spawnpoint_.flags          = t->ambush ? kMapObjectFlagAmbush : 0;
    mo->spawnpoint_.tag            = t->tag;

    if (t->ambush)
        mo->flags_ |= kMapObjectFlagAmbush;

    // -AJA- 1999/09/25: If radius trigger is a path node, then
    //       setup the thing to follow the path.

    if (R->info->next_in_path)
        mo->path_trigger_ = R->info;
}

void ScriptDamagePlayers(RADScriptTrigger *R, void *param)
{
    ScriptDamagePlayerParameter *damage = (ScriptDamagePlayerParameter *)param;

    // Make sure these can happen to everyone within the radius.
    // Damage the player(s)
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (!ScriptRadiusCheck(p->map_object_, R->info))
            continue;

        DamageMapObject(p->map_object_, nullptr, nullptr, damage->damage_amount, nullptr);
    }
}

void ScriptHealPlayers(RADScriptTrigger *R, void *param)
{
    ScriptHealParameter *heal = (ScriptHealParameter *)param;

    // Heal the player(s)
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (!ScriptRadiusCheck(p->map_object_, R->info))
            continue;

        if (p->health_ >= heal->limit)
            continue;

        if (p->health_ + heal->heal_amount >= heal->limit)
            p->health_ = heal->limit;
        else
            p->health_ += heal->heal_amount;

        p->map_object_->health_ = p->health_;
    }
}

void ScriptArmourPlayers(RADScriptTrigger *R, void *param)
{
    ScriptArmourParameter *armour = (ScriptArmourParameter *)param;

    // Armour for player(s)
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (!ScriptRadiusCheck(p->map_object_, R->info))
            continue;

        float slack = armour->limit - p->total_armour_;

        if (slack <= 0)
            continue;

        p->armours_[armour->type] += armour->armour_amount;

        if (p->armours_[armour->type] > slack)
            p->armours_[armour->type] = slack;

        UpdateTotalArmour(p);
    }
}

void ScriptBenefitPlayers(RADScriptTrigger *R, void *param)
{
    ScriptBenefitParameter *be = (ScriptBenefitParameter *)param;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (!ScriptRadiusCheck(p->map_object_, R->info))
            continue;

        GiveBenefitList(p, nullptr, be->benefit, be->lose_it);
    }
}

void ScriptDamageMonsters(RADScriptTrigger *R, void *param)
{
    ScriptDamangeMonstersParameter *mon = (ScriptDamangeMonstersParameter *)param;

    const MapObjectDefinition *info = nullptr;
    int                        tag  = mon->thing_tag;

    if (mon->thing_name)
    {
        info = mobjtypes.Lookup(mon->thing_name);
    }
    else if (mon->thing_type > 0)
    {
        info = mobjtypes.Lookup(mon->thing_type);

        if (info == nullptr)
            FatalError("RTS DAMAGE_MONSTERS: Unknown thing type %d.\n", mon->thing_type);
    }

    // scan the mobj list
    // FIXME: optimise for fixed-sized triggers

    MapObject *mo;
    MapObject *next;

    Player *player = GetWhoDunnit(R);

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (info && mo->info_ != info)
            continue;

        if (tag && (mo->tag_ != tag))
            continue;

        if (!(mo->extended_flags_ & kExtendedFlagMonster) || mo->health_ <= 0)
            continue;

        if (!ScriptRadiusCheck(mo, R->info))
            continue;

        DamageMapObject(mo, nullptr, player ? player->map_object_ : nullptr, mon->damage_amount, nullptr);
    }
}

void ScriptThingEvent(RADScriptTrigger *R, void *param)
{
    ScriptThingEventParameter *tev = (ScriptThingEventParameter *)param;

    const MapObjectDefinition *info = nullptr;
    int                        tag  = tev->thing_tag;

    if (tev->thing_name)
    {
        info = mobjtypes.Lookup(tev->thing_name);

        if (info == nullptr)
            FatalError("RTS THING_EVENT: Unknown thing name '%s'.\n", tev->thing_name);
    }
    else if (tev->thing_type > 0)
    {
        info = mobjtypes.Lookup(tev->thing_type);

        if (info == nullptr)
            FatalError("RTS THING_EVENT: Unknown thing type %d.\n", tev->thing_type);
    }

    // scan the mobj list
    // FIXME: optimise for fixed-sized triggers

    MapObject *mo;
    MapObject *next;

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (info && (mo->info_ != info))
            continue;

        if (tag && (mo->tag_ != tag))
            continue;

        // ignore certain things (e.g. corpses)
        if (mo->health_ <= 0)
            continue;

        if (!ScriptRadiusCheck(mo, R->info))
            continue;

        int state = MapObjectFindLabel(mo, tev->label);

        if (state)
            MapObjectSetStateDeferred(mo, state + tev->offset, 0);
    }
}

void ScriptGotoMap(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptGoToMapParameter *go = (ScriptGoToMapParameter *)param;

    // Warp to level n
    if (go->is_hub)
        ExitToHub(go->map_name, go->tag);
    else
        ExitToLevel(go->map_name, 5, go->skip_all);
}

void ScriptExitLevel(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptExitParameter *exit = (ScriptExitParameter *)param;

    if (exit->is_secret)
        ExitLevelSecret(exit->exit_time);
    else
        ExitLevel(exit->exit_time);
}

// Lobo November 2021
void ScriptExitGame(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    EPI_UNUSED(param);
    DeferredEndGame();
}

void ScriptPlaySound(RADScriptTrigger *R, void *param)
{
    ScriptSoundParameter *ambient = (ScriptSoundParameter *)param;

    int flags = 0;

    if (ambient->kind == kScriptSoundBossMan)
        flags |= kSoundEffectBoss;

    // Ambient sound
    R->sound_effects_origin.x = ambient->x;
    R->sound_effects_origin.y = ambient->y;

    if (AlmostEquals(ambient->z, kOnFloorZ))
        R->sound_effects_origin.z = PointInSubsector(ambient->x, ambient->y)->sector->floor_height;
    else
        R->sound_effects_origin.z = ambient->z;

    if (ambient->kind == kScriptSoundBossMan)
    { // Lobo: want BOSSMAN to sound from the player
        Player *player = GetWhoDunnit(R);
        StartSoundEffect(ambient->sfx, kCategoryPlayer, player->map_object_);
    }
    else
    {
        StartSoundEffect(ambient->sfx, kCategoryLevel, &R->sound_effects_origin, flags);
    }
}

void ScriptKillSound(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(param);
    StopSoundEffect(&R->sound_effects_origin);
}

void ScriptChangeMusic(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptMusicParameter *music = (ScriptMusicParameter *)param;

    ChangeMusic(music->playnum, music->looping);
}

void ScriptPlayMovie(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptMovieParameter *mov = (ScriptMovieParameter *)param;

    PlayMovie(mov->movie);
}

void ScriptChangeTexture(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptChangeTexturetureParameter *ctex = (ScriptChangeTexturetureParameter *)param;

    const Image *image = nullptr;

    EPI_ASSERT(param);

    // find texture or flat
    if (ctex->what >= kChangeTextureFloor)
        image = ImageLookup(ctex->texname, kImageNamespaceFlat);
    else
        image = ImageLookup(ctex->texname, kImageNamespaceTexture);

    if (ctex->what == kChangeTextureSky)
    {
        if (image)
        {
            sky_image = image;
            UpdateSkyboxTextures();
        }
        return;
    }

    // handle the floor/ceiling case
    if (ctex->what >= kChangeTextureFloor)
    {
        bool must_recompute_sky = false;

        Sector *tsec;

        for (tsec = FindSectorFromTag(ctex->tag); tsec != nullptr; tsec = tsec->tag_next)
        {
            if (ctex->subtag)
            {
                bool valid = false;

                for (int i = 0; i < tsec->line_count; i++)
                {
                    if (tsec->lines[i]->tag == ctex->subtag)
                    {
                        valid = true;
                        break;
                    }
                }

                if (!valid)
                    continue;
            }

            if (ctex->what == kChangeTextureFloor)
            {
                tsec->floor.image = image;
                // update sink/bob depth
                if (image)
                {
                    FlatDefinition *current_flatdef = flatdefs.Find(image->name_.c_str());
                    if (current_flatdef)
                    {
                        tsec->bob_depth  = current_flatdef->bob_depth_;
                        tsec->sink_depth = current_flatdef->sink_depth_;
                    }
                    else
                        tsec->bob_depth = 0;
                    tsec->sink_depth = 0;
                }
                else
                {
                    tsec->bob_depth  = 0;
                    tsec->sink_depth = 0;
                }
            }
            else
                tsec->ceiling.image = image;

            if (image == sky_flat_image)
                must_recompute_sky = true;
        }

        if (must_recompute_sky)
            ComputeSkyHeights();

        return;
    }

    // handle the line changers
    EPI_ASSERT(ctex->what < kChangeTextureSky);

    for (int i = 0; i < total_level_lines; i++)
    {
        Side *side = (ctex->what <= kChangeTextureRightLower) ? level_lines[i].side[0] : level_lines[i].side[1];

        if (level_lines[i].tag != ctex->tag || !side)
            continue;

        if (ctex->subtag && side->sector->tag != ctex->subtag)
            continue;

        switch (ctex->what)
        {
        case kChangeTextureRightUpper:
        case kChangeTextureLeftUpper:
            side->top.image = image;
            break;

        case kChangeTextureRightMiddle:
        case kChangeTextureLeftMiddle:
            side->middle.image = image;
            break;

        case kChangeTextureRightLower:
        case kChangeTextureLeftLower:
            side->bottom.image = image;

        default:
            break;
        }
    }
}

void ScriptSkill(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptSkillParameter *skill = (ScriptSkillParameter *)param;

    // Skill selection trigger function
    // -ACB- 1998/07/30 replaced respawnmonsters with respawnsetting.
    // -ACB- 1998/08/27 removed fast_monsters temporaryly.

    game_skill = skill->skill;

    level_flags.fast_monsters   = skill->fastmonsters;
    level_flags.enemies_respawn = skill->respawn;
}

static void MoveOneSector(Sector *sec, ScriptMoveSectorParameter *t)
{
    float dh;

    if (t->relative)
        dh = t->value;
    else if (t->is_ceiling)
        dh = t->value - sec->ceiling_height;
    else
        dh = t->value - sec->floor_height;

    if (!CheckSolidSectorMove(sec, t->is_ceiling, dh))
        return;

    SolidSectorMove(sec, t->is_ceiling, dh);

    if (t->is_ceiling)
        sec->old_ceiling_height = sec->ceiling_height;
    else
        sec->old_floor_height = sec->floor_height;
}

void ScriptMoveSector(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptMoveSectorParameter *t = (ScriptMoveSectorParameter *)param;
    int                        i;

    // SectorV compatibility
    if (t->tag == 0)
    {
        if (t->secnum < 0 || t->secnum >= total_level_sectors)
            FatalError("RTS SECTORV: no such sector %d.\n", t->secnum);

        MoveOneSector(level_sectors + t->secnum, t);
        return;
    }

    // OPTIMISE !
    for (i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag == t->tag)
            MoveOneSector(level_sectors + i, t);
    }
}

static void LightOneSector(Sector *sec, ScriptSectorLightParameter *t)
{
    if (t->relative)
        sec->properties.light_level += RoundToInteger(t->value);
    else
        sec->properties.light_level = RoundToInteger(t->value);
}

void ScriptLightSector(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptSectorLightParameter *t = (ScriptSectorLightParameter *)param;
    int                         i;

    // SectorL compatibility
    if (t->tag == 0)
    {
        if (t->secnum < 0 || t->secnum >= total_level_sectors)
            FatalError("RTS SECTORL: no such sector %d.\n", t->secnum);

        LightOneSector(level_sectors + t->secnum, t);
        return;
    }

    // OPTIMISE !
    for (i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag == t->tag)
            LightOneSector(level_sectors + i, t);
    }
}

void ScriptFogSector(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptFogSectorParameter *t = (ScriptFogSectorParameter *)param;
    int                       i;

    for (i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag == t->tag)
        {
            if (!t->leave_color)
            {
                if (t->colmap_color)
                    level_sectors[i].properties.fog_color = ParseFontColor(t->colmap_color);
                else // should only happen with a CLEAR directive
                    level_sectors[i].properties.fog_color = kRGBANoValue;
            }
            if (!t->leave_density)
            {
                if (t->relative)
                {
                    level_sectors[i].properties.fog_density += (0.01f * t->density);
                    if (level_sectors[i].properties.fog_density < 0.0001f)
                        level_sectors[i].properties.fog_density = 0;
                    if (level_sectors[i].properties.fog_density > 0.01f)
                        level_sectors[i].properties.fog_density = 0.01f;
                }
                else
                    level_sectors[i].properties.fog_density = 0.01f * t->density;
            }
            for (int j = 0; j < level_sectors[i].line_count; j++)
            {
                for (int k = 0; k < 2; k++)
                {
                    Side *side_check = level_sectors[i].lines[j]->side[k];
                    if (side_check && side_check->middle.fog_wall)
                    {
                        side_check->middle.image = nullptr; // will be rebuilt with proper color later
                                                            // don't delete the image in case other
                                                            // fogwalls use the same color
                    }
                }
            }
        }
    }
}

void ScriptEnableScript(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptEnablerParameter *t = (ScriptEnablerParameter *)param;
    RADScriptTrigger       *other;

    // Enable/Disable Scripts
    if (t->script_name)
    {
        other = FindScriptTriggerByName(t->script_name);

        if (!other)
            return;

        other->disabled = t->new_disabled;
    }
    else
    {
        if (t->tag[0] != 0)
            ScriptEnableByTag(t->tag[0], t->new_disabled, kTriggerTagNumber);
        else
            ScriptEnableByTag(t->tag[1], t->new_disabled, kTriggerTagHash);
    }
}

void ScriptActivateLinetype(RADScriptTrigger *R, void *param)
{
    ScriptActivateLineParameter *t = (ScriptActivateLineParameter *)param;

    Player *player = GetWhoDunnit(R);

    RemoteActivation(player ? player->map_object_ : nullptr, t->typenum, t->tag, 0, kLineTriggerAny);
}

void ScriptUnblockLines(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptLineBlockParameter *ub = (ScriptLineBlockParameter *)param;

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        if (ld->tag != ub->tag)
            continue;

        if (!ld->side[0] || !ld->side[1])
            continue;

        // clear standard flags
        ld->flags &=
            ~(kLineFlagBlocking | kLineFlagBlockMonsters | kLineFlagBlockGroundedMonsters | kLineFlagBlockPlayers);

        // clear EDGE's extended lineflags too
        ld->flags &= ~(kLineFlagSightBlock | kLineFlagShootBlock);
    }
}

void ScriptBlockLines(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(R);
    ScriptLineBlockParameter *ub = (ScriptLineBlockParameter *)param;

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        if (ld->tag != ub->tag)
            continue;

        // set standard flags
        ld->flags |= (kLineFlagBlocking | kLineFlagBlockMonsters);
    }
}

void ScriptJump(RADScriptTrigger *R, void *param)
{
    ScriptJumpParameter *t = (ScriptJumpParameter *)param;

    if (!RandomByteTestDeterministic(t->random_chance))
        return;

    if (!t->cache_state)
    {
        // FIXME: do this in a post-parsing analysis
        t->cache_state = FindScriptStateByLabel(R->info, t->label);

        if (!t->cache_state)
            FatalError("RTS: No such label `%s' for JUMP primitive.\n", t->label);
    }

    R->state = t->cache_state;

    // Jumps have a one tic surcharge, to prevent accidental infinite
    // loops within radius scripts.
    R->wait_tics += 1;
}

void ScriptSleep(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(param);
    R->disabled = true;
}

void ScriptRetrigger(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(param);
    R->activated    = false;
    R->acti_players = 0;
}

void ScriptShowMenu(RADScriptTrigger *R, void *param)
{
    ScriptShowMenuParameter *menu = (ScriptShowMenuParameter *)param;

    if (total_players > 1 && (R->acti_players & (1 << console_player)) == 0)
        return;

    if (rts_menu_active)
    {
        // this is very unlikely, since RTS triggers do not run while
        // an RTS menu is active.  This menu simply fails.
        R->menu_result = 0;
        return;
    }

    ScriptMenuStart(R, menu);
}

void ScriptUpdateMenuStyle(RADScriptTrigger *R, void *param)
{
    ScriptMenuStyle *mm = (ScriptMenuStyle *)param;

    SaveChunkFreeString(R->menu_style_name);

    R->menu_style_name = SaveChunkCopyString(mm->style);
}

void ScriptJumpOn(RADScriptTrigger *R, void *param)
{
    ScriptJumpOnParameter *jm = (ScriptJumpOnParameter *)param;

    int count = 0;

    while ((count < 9) && jm->labels[count])
        count++;

    if (R->menu_result < 0 || R->menu_result > count)
        return;

    RADScriptState *cache_state;
    char           *label = nullptr;

    if (R->menu_result > 0)
    {
        label = jm->labels[R->menu_result - 1];

        // FIXME: do this in a post-parsing analysis
        cache_state = FindScriptStateByLabel(R->info, label);
        R->state    = cache_state;
    }
    else
    {
        cache_state  = R->info->first_state;
        R->state     = cache_state;
        R->activated = false;
    }

    if (!cache_state && label)
        FatalError("RTS: No such label `%s' for JUMP_ON primitive.\n", label);

    if (!cache_state)
        FatalError("RTS: No state to jump to!\n");

    // Jumps have a one tic surcharge, to prevent accidental infinite
    // loops within radius scripts.
    R->wait_tics += 1;
}

static bool WUD_Match(ScriptWaitUntilDeadParameter *wud, const char *name)
{
    for (int i = 0; i < 10; i++)
    {
        if (!wud->mon_names[i])
            continue;

        if (DDFCompareName(name, wud->mon_names[i]) == 0)
            return true;
    }

    return false;
}

void ScriptWaitUntilDead(RADScriptTrigger *R, void *param)
{
    ScriptWaitUntilDeadParameter *wud = (ScriptWaitUntilDeadParameter *)param;

    R->wud_tag   = wud->tag;
    R->wud_count = 0;

    // find all matching monsters
    MapObject *mo;
    MapObject *next;

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (!mo->info_)
            continue;

        if (mo->health_ <= 0)
            continue;

        if (!WUD_Match(wud, mo->info_->name_.c_str()))
            continue;

        if (!ScriptRadiusCheck(mo, R->info))
            continue;

        // mark the monster
        mo->hyper_flags_ |= kHyperFlagWaitUntilDead;
        if (mo->wait_until_dead_tags_.empty())
            mo->wait_until_dead_tags_ = epi::StringFormat("%d", wud->tag);
        else
            mo->wait_until_dead_tags_ = epi::StringFormat("%s,%d", mo->wait_until_dead_tags_.c_str(), wud->tag);

        R->wud_count++;
    }

    if (R->wud_count == 0)
    {
        LogDebug("RTS: waiting forever, no %s found\n", wud->mon_names[0]);
        R->wud_count = 1;
    }
}

void ScriptSwitchWeapon(RADScriptTrigger *R, void *param)
{
    ScriptWeaponParameter *weaparg = (ScriptWeaponParameter *)param;

    Player           *player = GetWhoDunnit(R);
    WeaponDefinition *weap   = weapondefs.Lookup(weaparg->name);

    if (weap)
    {
        PlayerSwitchWeapon(player, weap);
    }
}

void ScriptTeleportToStart(RADScriptTrigger *R, void *param)
{
    EPI_UNUSED(param);
    Player *p = GetWhoDunnit(R);

    SpawnPoint *point = FindCoopPlayer(1); // start 1

    if (!point)
        return;                            // should never happen but who knows...

    // 1. Stop the player movement and turn him
    p->map_object_->momentum_.X = p->map_object_->momentum_.Y = p->map_object_->momentum_.Z = 0;
    p->actual_speed_                                                                        = 0;
    p->map_object_->angle_                                                                  = point->angle;

    // 2. Don't move for a bit
    int waitAbit = 30;

    p->map_object_->reaction_time_ = waitAbit;

    // 3. Do our teleport fog effect
    float x = point->x;
    float y = point->y;
    float z = point->z;

    // spawn teleport fog
    MapObject *fog;
    x += 20 * epi::BAMCos(point->angle);
    y += 20 * epi::BAMSin(point->angle);
    fog = CreateMapObject(x, y, z, mobjtypes.Lookup("TELEPORT_FLASH"));
    // never use this object as a teleport destination
    fog->extended_flags_ |= kExtendedFlagNeverTarget;

    if (fog->info_->chase_state_)
        MapObjectSetStateDeferred(fog, fog->info_->chase_state_, 0);

    // 4. Teleport him
    //  Don't get stuck spawned in things: telefrag them.
    TeleportMove(p->map_object_, point->x, point->y, point->z);
}

static void ScriptSetPlayerSprite(Player *p, int position, int stnum, WeaponDefinition *info = nullptr)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0)
    {
        // object removed itself
        psp->state = psp->next_state = nullptr;
        return;
    }

    // state is old? -- Mundo hack for DDF inheritance
    if (info && stnum < info->state_grp_.back().first)
    {
        State *st = &states[stnum];

        if (st->label)
        {
            int new_state = DDFStateFindLabel(info->state_grp_, st->label, true /* quiet */);
            if (new_state != 0)
                stnum = new_state;
        }
    }

    State *st = &states[stnum];

    // model interpolation stuff
    if (psp->state && (st->flags & kStateFrameFlagModel) && (psp->state->flags & kStateFrameFlagModel) &&
        (st->sprite == psp->state->sprite) && st->tics > 1)
    {
        p->weapon_last_frame_ = psp->state->frame;
    }
    else
        p->weapon_last_frame_ = -1;

    psp->state      = st;
    psp->tics       = st->tics;
    psp->next_state = (st->nextstate == 0) ? nullptr : (states + st->nextstate);

    // call action routine

    p->action_player_sprite_ = position;

    if (st->action)
        (*st->action)(p->map_object_);
}

//
// SetPlayerSpriteDeferred
//
// -AJA- 2004/11/05: This is preferred method, doesn't run any actions,
//       which (ideally) should only happen during MovePlayerSprites().
//
static void ScriptSetPlayerSpriteDeferred(Player *p, int position, int stnum)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0 || psp->state == nullptr)
    {
        ScriptSetPlayerSprite(p, position, stnum);
        return;
    }

    psp->tics       = 0;
    psp->next_state = (states + stnum);
}

// Replace one weapon with another instantly (no up/down states run)
// It doesnt matter if we have the old one currently selected or not.
void ScriptReplaceWeapon(RADScriptTrigger *R, void *param)
{
    ScriptWeaponReplaceParameter *weaparg = (ScriptWeaponReplaceParameter *)param;

    Player           *p      = GetWhoDunnit(R);
    WeaponDefinition *oldWep = weapondefs.Lookup(weaparg->old_weapon);
    WeaponDefinition *newWep = weapondefs.Lookup(weaparg->new_weapon);

    if (!oldWep)
    {
        FatalError("RTS: No such weapon `%s' for REPLACE_WEAPON.\n", weaparg->old_weapon);
    }
    if (!newWep)
    {
        FatalError("RTS: No such weapon `%s' for REPLACE_WEAPON.\n", weaparg->new_weapon);
    }

    int i;
    for (i = 0; i < kMaximumWeapons; i++)
    {
        if (p->weapons_[i].info == oldWep)
        {
            p->weapons_[i].info = newWep;
        }
    }

    // refresh the sprite
    if (p->weapons_[p->ready_weapon_].info == newWep)
    {
        ScriptSetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, p->weapons_[p->ready_weapon_].info->ready_state_);

        FixWeaponClip(p, p->ready_weapon_); // handle the potential clip_size difference
        UpdateAvailWeapons(p);
    }
}

// If we have the weapon we insta-switch to it and
// go to the STATE we indicated.
void ScriptWeaponEvent(RADScriptTrigger *R, void *param)
{
    ScriptWeaponEventParameter *tev = (ScriptWeaponEventParameter *)param;

    Player           *p      = GetWhoDunnit(R);
    WeaponDefinition *oldWep = weapondefs.Lookup(tev->weapon_name);

    if (!oldWep)
    {
        FatalError("RTS WEAPON_EVENT: Unknown weapon name '%s'.\n", tev->weapon_name);
    }

    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < kMaximumWeapons; pw_index++)
    {
        if (!p->weapons_[pw_index].owned)
            continue;

        if (p->weapons_[pw_index].info == oldWep)
            break;
    }

    if (pw_index == kMaximumWeapons)              // we dont have the weapon
        return;

    p->ready_weapon_ = (WeaponSelection)pw_index; // insta-switch to it

    int state = DDFStateFindLabel(oldWep->state_grp_, tev->label, true /* quiet */);
    if (state == 0)
        FatalError("RTS WEAPON_EVENT: frame '%s' in [%s] not found!\n", tev->label, tev->weapon_name);
    state += tev->offset;

    ScriptSetPlayerSpriteDeferred(p, kPlayerSpriteWeapon,
                                  state); // refresh the sprite
}

void P_ActReplace(MapObject *mo, const MapObjectDefinition *newThing)
{
    // DO THE DEED !!

    // UnsetThingPosition(mo);
    {
        mo->info_ = newThing;

        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fast_monsters)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        mo->health_ = mo->spawn_health_;       // always top up health to full

        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_ = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_        = mo->info_->model_skin_;
        mo->model_last_frame_  = -1;
        mo->model_scale_       = mo->info_->model_scale_;
        mo->model_aspect_      = mo->info_->model_aspect_;
        mo->scale_             = mo->info_->scale_;
        mo->aspect_            = mo->info_->aspect_;

        mo->pain_chance_ = mo->info_->pain_chance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_;

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dynamic_light_.target = dinfo->radius_;
                mo->dynamic_light_.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dynamic_light_.shader)
                {
                    // FIXME: delete mo->dynamic_light_.shader;
                    mo->dynamic_light_.shader = nullptr;
                }
            }
        }
    }
    // SetThingPosition(mo);

    int state = MapObjectFindLabel(mo, "IDLE"); // nothing fancy, always default to idle
    if (state == 0)
        FatalError("RTS REPLACE_THING: frame '%s' in [%s] not found!\n", "IDLE", mo->info_->name_.c_str());

    MapObjectSetStateDeferred(mo, state, 0);
}

// Replace one thing with another.
void ScriptReplaceThing(RADScriptTrigger *R, void *param)
{
    ScriptThingReplaceParameter *thingarg = (ScriptThingReplaceParameter *)param;

    const MapObjectDefinition *oldThing = nullptr;
    const MapObjectDefinition *newThing = nullptr;

    // Prioritize number lookup. It's faster and more permissive
    if (thingarg->old_thing_type > -1)
        oldThing = mobjtypes.Lookup(thingarg->old_thing_type);
    else
        oldThing = mobjtypes.Lookup(thingarg->old_thing_name);

    if (thingarg->new_thing_type > -1)
        newThing = mobjtypes.Lookup(thingarg->new_thing_type);
    else
        newThing = mobjtypes.Lookup(thingarg->new_thing_name);

    // Will only get this far if the previous lookups were for numbers and
    // failed
    if (!oldThing)
    {
        if (thingarg->old_thing_type > -1)
            FatalError("RTS: No such old thing %d for REPLACE_THING.\n", thingarg->old_thing_type);
        else // never get this far
            FatalError("RTS: No such old thing '%s' for REPLACE_THING.\n", thingarg->old_thing_name);
    }
    if (!newThing)
    {
        if (thingarg->new_thing_type > -1)
            FatalError("RTS: No such new thing %d for REPLACE_THING.\n", thingarg->new_thing_type);
        else // never get this far
            FatalError("RTS: No such new thing '%s' for REPLACE_THING.\n", thingarg->new_thing_name);
    }

    // scan the mobj list
    // FIXME: optimise for fixed-sized triggers

    MapObject *mo;
    MapObject *next;

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (oldThing && mo->info_ != oldThing)
            continue;

        if (!ScriptRadiusCheck(mo, R->info))
            continue;

        P_ActReplace(mo, newThing);
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
