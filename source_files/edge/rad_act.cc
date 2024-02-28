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



#include <limits.h>

#include "dm_defs.h"
#include "dm_state.h"
#include "con_main.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"
#include "rad_act.h"
#include "r_defs.h"
#include "r_misc.h"
#include "r_sky.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "r_draw.h"
#include "r_colormap.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_wad.h"
#include "w_texture.h"

#include "str_util.h"

#include "AlmostEquals.h"

static Style *rts_tip_style;

// current tip slots
drawtip_t tip_slots[MAXTIPSLOT];

// properties for fixed slots
#define FIXEDSLOTS 15

static s_tip_prop_t fixed_props[FIXEDSLOTS] = {
    {1, 0.50f, 0.50f, 0, "#FFFFFF", 1.0f},  {2, 0.20f, 0.25f, 1, "#FFFFFF", 1.0f},
    {3, 0.20f, 0.75f, 1, "#FFFFFF", 1.0f},  {4, 0.50f, 0.50f, 0, "#3333FF", 1.0f},
    {5, 0.20f, 0.25f, 1, "#3333FF", 1.0f},  {6, 0.20f, 0.75f, 1, "#3333FF", 1.0f},
    {7, 0.50f, 0.50f, 0, "#FFFF00", 1.0f},  {8, 0.20f, 0.25f, 1, "#FFFF00", 1.0f},
    {9, 0.20f, 0.75f, 1, "#FFFF00", 1.0f},  {10, 0.50f, 0.50f, 0, "", 1.0f},
    {11, 0.20f, 0.25f, 1, "", 1.0f},        {12, 0.20f, 0.75f, 1, "", 1.0f},
    {13, 0.50f, 0.50f, 0, "#33FF33", 1.0f}, {14, 0.20f, 0.25f, 1, "#33FF33", 1.0f},
    {15, 0.20f, 0.75f, 1, "#33FF33", 1.0f}};

//
// Once-only initialisation.
//
void RAD_InitTips(void)
{
    for (int i = 0; i < MAXTIPSLOT; i++)
    {
        drawtip_t *current = tip_slots + i;

        // initial properties
        Z_Clear(current, drawtip_t, 1);

        current->p = fixed_props[i % FIXEDSLOTS];

        current->delay = -1;
        current->color = kRGBANoValue;

        current->p.slot_num = i;
    }
}

//
// Used when changing levels to clear any tips.
//
void RAD_ResetTips(void)
{
    // free any text strings
    for (int i = 0; i < MAXTIPSLOT; i++)
    {
        drawtip_t *current = tip_slots + i;

        SV_FreeString(current->tip_text);
    }

    RAD_InitTips();
}

static void SetupTip(drawtip_t *cur)
{
    if (cur->tip_graphic)
        return;

    if (cur->color == kRGBANoValue)
        cur->color = V_ParseFontColor(cur->p.color_name);
}

static void SendTip(rad_trigger_t *R, s_tip_t *tip, int slot)
{
    drawtip_t *current;

    SYS_ASSERT(0 <= slot && slot < MAXTIPSLOT);

    current = tip_slots + slot;

    current->delay = tip->display_time;

    SV_FreeString(current->tip_text);

    if (tip->tip_ldf)
        current->tip_text = SV_DupString(language[tip->tip_ldf]);
    else if (tip->tip_text)
        current->tip_text = SV_DupString(tip->tip_text);
    else
        current->tip_text = nullptr;

    // send message to the console (unless it would clog it up)
    if (current->tip_text && current->tip_text != R->last_con_message)
    {
        ConsolePrint("%s\n", current->tip_text);
        R->last_con_message = current->tip_text;
    }

    current->tip_graphic = tip->tip_graphic ? W_ImageLookup(tip->tip_graphic) : nullptr;
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
void RAD_DisplayTips(void)
{
    HudReset();

    // lookup styles
    StyleDefinition *def;

    def = styledefs.Lookup("RTS_TIP");
    if (!def)
        def = default_style;
    rts_tip_style = hud_styles.Lookup(def);

    for (int slot = 0; slot < MAXTIPSLOT; slot++)
    {
        drawtip_t *current = tip_slots + slot;

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
            S_StartFX(sfxdefs.GetEffect("TINK"));
            current->playsound = false;
        }

        float alpha = current->p.translucency;

        if (alpha < 0.02f)
            continue;

        HudSetScale(current->scale);
        HudSetTextColor(current->color);
        HudSetAlpha(alpha);

        if (current->p.left_just)
            HudSetAlignment(-1, 0);
        else
            HudSetAlignment(0, 0);

        float x = current->p.x_pos * 320.0f;
        float y = current->p.y_pos * 200.0f;

        if (rts_tip_style->fonts_[StyleDefinition::kTextSectionText])
            HudSetFont(rts_tip_style->fonts_[StyleDefinition::kTextSectionText]);

        if (current->tip_graphic)
            HudDrawImage(x, y, current->tip_graphic);
        else
            HudDrawText(x, y, current->tip_text);

        HudSetAlignment();
        HudSetAlpha();
        HudSetScale();
        HudSetTextColor();
    }
}

//
// Does any tic-related RTS stuff.  For now, just update the tips.
//
void RAD_Ticker(void)
{
    for (int i = 0; i < MAXTIPSLOT; i++)
    {
        drawtip_t *current = tip_slots + i;

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

static player_t *GetWhoDunnit(rad_trigger_t *R)
{
    return players[consoleplayer];

    /*
    // this IS NOT CORRECT, but matches old behavior
    if (numplayers == 1)
        return players[consoleplayer];

    if (R->acti_players == 0)
        return nullptr;

    // does the activator list have only one player?
    // if so, return that one.
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
        if (R->acti_players == (1 << pnum))
            return players[pnum];

    // there are multiple players who triggered the script.
    // one option: select one of them (round robin style).
    // However the following is probably more correct.
    //return nullptr;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
        if (R->acti_players & (1 << pnum))
            return players[pnum];
    */
}

void RAD_ActNOP(rad_trigger_t *R, void *param)
{
    // No Operation
}

void RAD_ActTip(rad_trigger_t *R, void *param)
{
    s_tip_t *tip = (s_tip_t *)param;

    // Only display the tip to the player that stepped into the radius
    // trigger.

    if (numplayers > 1 && (R->acti_players & (1 << consoleplayer)) == 0)
        return;

    SendTip(R, tip, R->tip_slot);
}

void RAD_ActTipProps(rad_trigger_t *R, void *param)
{
    s_tip_prop_t *tp = (s_tip_prop_t *)param;
    drawtip_t    *current;

    if (numplayers > 1 && (R->acti_players & (1 << consoleplayer)) == 0)
        return;

    if (tp->slot_num >= 0)
        R->tip_slot = tp->slot_num;

    SYS_ASSERT(0 <= R->tip_slot && R->tip_slot < MAXTIPSLOT);

    current = tip_slots + R->tip_slot;

    if (tp->x_pos >= 0)
        current->p.x_pos = tp->x_pos;

    if (tp->y_pos >= 0)
        current->p.y_pos = tp->y_pos;

    if (tp->left_just >= 0)
        current->p.left_just = tp->left_just;

    if (tp->color_name)
        current->color = V_ParseFontColor(tp->color_name);

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

void RAD_ActSpawnThing(rad_trigger_t *R, void *param)
{
    s_thing_t *t = (s_thing_t *)param;

    MapObject           *mo;
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
    if (!GameCheckWhenAppear(t->appear))
        return;

    // -AJA- 1999/10/02: -nomonsters check.
    if (level_flags.nomonsters && (minfo->extended_flags_ & kExtendedFlagMonster))
        return;

    // -AJA- 1999/10/07: -noextra check.
    if (!level_flags.have_extra && (minfo->extended_flags_ & kExtendedFlagExtra))
        return;

    // -AJA- 1999/09/11: Support for supplying Z value.

    if (t->spawn_effect)
    {
        mo = P_MobjCreateObject(t->x, t->y, t->z, minfo->respawneffect_);
    }

    mo = P_MobjCreateObject(t->x, t->y, t->z, minfo);

    // -ACB- 1998/07/10 New Check, so that spawned mobj's don't
    //                  spawn somewhere where they should not.
    if (!P_CheckAbsPosition(mo, mo->x, mo->y, mo->z))
    {
        P_RemoveMobj(mo);
        return;
    }

    P_SetMobjDirAndSpeed(mo, t->angle, t->slope, 0);

    mo->tag_ = t->tag;

    mo->spawnpoint_.x         = t->x;
    mo->spawnpoint_.y         = t->y;
    mo->spawnpoint_.z         = t->z;
    mo->spawnpoint_.angle     = t->angle;
    mo->spawnpoint_.vertical_angle = epi::BAMFromATan(t->slope);
    mo->spawnpoint_.info      = minfo;
    mo->spawnpoint_.flags     = t->ambush ? kMapObjectFlagAmbush : 0;
    mo->spawnpoint_.tag       = t->tag;

    if (t->ambush)
        mo->flags_ |= kMapObjectFlagAmbush;

    // -AJA- 1999/09/25: If radius trigger is a path node, then
    //       setup the thing to follow the path.

    if (R->info->next_in_path)
        mo->path_trigger_ = R->info;
}

void RAD_ActDamagePlayers(rad_trigger_t *R, void *param)
{
    s_damagep_t *damage = (s_damagep_t *)param;

    // Make sure these can happen to everyone within the radius.
    // Damage the player(s)
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (!RAD_WithinRadius(p->mo, R->info))
            continue;

        P_DamageMobj(p->mo, nullptr, nullptr, damage->damage_amount, nullptr);
    }
}

void RAD_ActHealPlayers(rad_trigger_t *R, void *param)
{
    s_healp_t *heal = (s_healp_t *)param;

    // Heal the player(s)
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (!RAD_WithinRadius(p->mo, R->info))
            continue;

        if (p->health >= heal->limit)
            continue;

        if (p->health + heal->heal_amount >= heal->limit)
            p->health = heal->limit;
        else
            p->health += heal->heal_amount;

        p->mo->health_ = p->health;
    }
}

void RAD_ActArmourPlayers(rad_trigger_t *R, void *param)
{
    s_armour_t *armour = (s_armour_t *)param;

    // Armour for player(s)
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (!RAD_WithinRadius(p->mo, R->info))
            continue;

        float slack = armour->limit - p->totalarmour;

        if (slack <= 0)
            continue;

        p->armours[armour->type] += armour->armour_amount;

        if (p->armours[armour->type] > slack)
            p->armours[armour->type] = slack;

        P_UpdateTotalArmour(p);
    }
}

void RAD_ActBenefitPlayers(rad_trigger_t *R, void *param)
{
    s_benefit_t *be = (s_benefit_t *)param;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (!RAD_WithinRadius(p->mo, R->info))
            continue;

        P_GiveBenefitList(p, nullptr, be->benefit, be->lose_it);
    }
}

void RAD_ActDamageMonsters(rad_trigger_t *R, void *param)
{
    s_damage_monsters_t *mon = (s_damage_monsters_t *)param;

    const MapObjectDefinition *info = nullptr;
    int               tag  = mon->thing_tag;

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

    player_t *player = GetWhoDunnit(R);

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (info && mo->info_ != info)
            continue;

        if (tag && (mo->tag_ != tag))
            continue;

        if (!(mo->extended_flags_ & kExtendedFlagMonster) || mo->health_ <= 0)
            continue;

        if (!RAD_WithinRadius(mo, R->info))
            continue;

        P_DamageMobj(mo, nullptr, player ? player->mo : nullptr, mon->damage_amount, nullptr);
    }
}

void RAD_ActThingEvent(rad_trigger_t *R, void *param)
{
    s_thing_event_t *tev = (s_thing_event_t *)param;

    const MapObjectDefinition *info = nullptr;
    int               tag  = tev->thing_tag;

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

        if (!RAD_WithinRadius(mo, R->info))
            continue;

        int state = P_MobjFindLabel(mo, tev->label);

        if (state)
            P_SetMobjStateDeferred(mo, state + tev->offset, 0);
    }
}

void RAD_ActGotoMap(rad_trigger_t *R, void *param)
{
    s_gotomap_t *go = (s_gotomap_t *)param;

    // Warp to level n
    if (go->is_hub)
        GameExitToHub(go->map_name, go->tag);
    else
        GameExitToLevel(go->map_name, 5, go->skip_all);
}

void RAD_ActExitLevel(rad_trigger_t *R, void *param)
{
    s_exit_t *exit = (s_exit_t *)param;

    if (exit->is_secret)
        GameSecretExitLevel(exit->exit_time);
    else
        GameExitLevel(exit->exit_time);
}

// Lobo November 2021
void RAD_ActExitGame(rad_trigger_t *R, void *param)
{
    GameDeferredEndGame();
}

void RAD_ActPlaySound(rad_trigger_t *R, void *param)
{
    s_sound_t *ambient = (s_sound_t *)param;

    int flags = 0;

    if (ambient->kind == PSOUND_BossMan)
        flags |= FX_Boss;

    // Ambient sound
    R->sfx_origin.x = ambient->x;
    R->sfx_origin.y = ambient->y;

    if (AlmostEquals(ambient->z, ONFLOORZ))
        R->sfx_origin.z = R_PointInSubsector(ambient->x, ambient->y)->sector->f_h;
    else
        R->sfx_origin.z = ambient->z;

    if (ambient->kind == PSOUND_BossMan)
    { // Lobo: want BOSSMAN to sound from the player
        player_t *player = GetWhoDunnit(R);
        S_StartFX(ambient->sfx, SNCAT_Player, player->mo);
    }
    else
    {
        S_StartFX(ambient->sfx, SNCAT_Level, &R->sfx_origin, flags);
    }
}

void RAD_ActKillSound(rad_trigger_t *R, void *param)
{
    S_StopFX(&R->sfx_origin);
}

void RAD_ActChangeMusic(rad_trigger_t *R, void *param)
{
    s_music_t *music = (s_music_t *)param;

    S_ChangeMusic(music->playnum, music->looping);
}

void RAD_ActPlayMovie(rad_trigger_t *R, void *param)
{
    s_movie_t *mov = (s_movie_t *)param;

    PlayMovie(mov->movie);
}

void RAD_ActChangeTex(rad_trigger_t *R, void *param)
{
    s_changetex_t *ctex = (s_changetex_t *)param;

    const image_c *image = nullptr;

    SYS_ASSERT(param);

    // find texture or flat
    if (ctex->what >= CHTEX_Floor)
        image = W_ImageLookup(ctex->texname, kImageNamespaceFlat);
    else
        image = W_ImageLookup(ctex->texname, kImageNamespaceTexture);

    if (ctex->what == CHTEX_Sky)
    {
        if (image)
        {
            sky_image = image;
            RGL_UpdateSkyBoxTextures();
        }
        return;
    }

    // handle the floor/ceiling case
    if (ctex->what >= CHTEX_Floor)
    {
        bool must_recompute_sky = false;

        sector_t *tsec;

        for (tsec = P_FindSectorFromTag(ctex->tag); tsec != nullptr; tsec = tsec->tag_next)
        {
            if (ctex->subtag)
            {
                bool valid = false;

                for (int i = 0; i < tsec->linecount; i++)
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

            if (ctex->what == CHTEX_Floor)
            {
                tsec->floor.image = image;
                // update sink/bob depth
                if (image)
                {
                    FlatDefinition *current_flatdef = flatdefs.Find(image->name.c_str());
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
                tsec->ceil.image = image;

            if (image == skyflatimage)
                must_recompute_sky = true;
        }

        if (must_recompute_sky)
            R_ComputeSkyHeights();

        return;
    }

    // handle the line changers
    SYS_ASSERT(ctex->what < CHTEX_Sky);

    for (int i = 0; i < total_level_lines; i++)
    {
        side_t *side = (ctex->what <= CHTEX_RightLower) ? level_lines[i].side[0] : level_lines[i].side[1];

        if (level_lines[i].tag != ctex->tag || !side)
            continue;

        if (ctex->subtag && side->sector->tag != ctex->subtag)
            continue;

        switch (ctex->what)
        {
        case CHTEX_RightUpper:
        case CHTEX_LeftUpper:
            side->top.image = image;
            break;

        case CHTEX_RightMiddle:
        case CHTEX_LeftMiddle:
            side->middle.image = image;
            break;

        case CHTEX_RightLower:
        case CHTEX_LeftLower:
            side->bottom.image = image;

        default:
            break;
        }
    }
}

void RAD_ActSkill(rad_trigger_t *R, void *param)
{
    s_skill_t *skill = (s_skill_t *)param;

    // Skill selection trigger function
    // -ACB- 1998/07/30 replaced respawnmonsters with respawnsetting.
    // -ACB- 1998/08/27 removed fastparm temporaryly.

    game_skill = skill->skill;

    level_flags.fastparm = skill->fastmonsters;
    level_flags.respawn  = skill->respawn;
}

static void MoveOneSector(sector_t *sec, s_movesector_t *t)
{
    float dh;

    if (t->relative)
        dh = t->value;
    else if (t->is_ceiling)
        dh = t->value - sec->c_h;
    else
        dh = t->value - sec->f_h;

    if (!P_CheckSolidSectorMove(sec, t->is_ceiling, dh))
        return;

    P_SolidSectorMove(sec, t->is_ceiling, dh);
}

void RAD_ActMoveSector(rad_trigger_t *R, void *param)
{
    s_movesector_t *t = (s_movesector_t *)param;
    int             i;

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

static void LightOneSector(sector_t *sec, s_lightsector_t *t)
{
    if (t->relative)
        sec->props.lightlevel += RoundToInteger(t->value);
    else
        sec->props.lightlevel = RoundToInteger(t->value);
}

void RAD_ActLightSector(rad_trigger_t *R, void *param)
{
    s_lightsector_t *t = (s_lightsector_t *)param;
    int              i;

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

void RAD_ActFogSector(rad_trigger_t *R, void *param)
{
    s_fogsector_t *t = (s_fogsector_t *)param;
    int            i;

    for (i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag == t->tag)
        {
            if (!t->leave_color)
            {
                if (t->colmap_color)
                    level_sectors[i].props.fog_color = V_ParseFontColor(t->colmap_color);
                else // should only happen with a CLEAR directive
                    level_sectors[i].props.fog_color = kRGBANoValue;
            }
            if (!t->leave_density)
            {
                if (t->relative)
                {
                    level_sectors[i].props.fog_density += (0.01f * t->density);
                    if (level_sectors[i].props.fog_density < 0.0001f)
                        level_sectors[i].props.fog_density = 0;
                    if (level_sectors[i].props.fog_density > 0.01f)
                        level_sectors[i].props.fog_density = 0.01f;
                }
                else
                    level_sectors[i].props.fog_density = 0.01f * t->density;
            }
            for (int j = 0; j < level_sectors[i].linecount; j++)
            {
                for (int k = 0; k < 2; k++)
                {
                    side_t *side_check = level_sectors[i].lines[j]->side[k];
                    if (side_check && side_check->middle.fogwall)
                    {
                        side_check->middle.image =
                            nullptr; // will be rebuilt with proper color later
                                     // don't delete the image in case other fogwalls use the same color
                    }
                }
            }
        }
    }
}

void RAD_ActEnableScript(rad_trigger_t *R, void *param)
{
    s_enabler_t   *t = (s_enabler_t *)param;
    rad_trigger_t *other;

    // Enable/Disable Scripts
    if (t->script_name)
    {
        other = RAD_FindTriggerByName(t->script_name);

        if (!other)
            return;

        other->disabled = t->new_disabled;
    }
    else
    {
        if (t->tag[0] != 0)
            RAD_EnableByTag(nullptr, t->tag[0], t->new_disabled, RTS_TAG_NUMBER);
        else
            RAD_EnableByTag(nullptr, t->tag[1], t->new_disabled, RTS_TAG_HASH);
    }
}

void RAD_ActActivateLinetype(rad_trigger_t *R, void *param)
{
    s_lineactivator_t *t = (s_lineactivator_t *)param;

    player_t *player = GetWhoDunnit(R);

    P_RemoteActivation(player ? player->mo : nullptr, t->typenum, t->tag, 0, kLineTriggerAny);
}

void RAD_ActUnblockLines(rad_trigger_t *R, void *param)
{
    s_lineunblocker_t *ub = (s_lineunblocker_t *)param;

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        if (ld->tag != ub->tag)
            continue;

        if (!ld->side[0] || !ld->side[1])
            continue;

        // clear standard flags
        ld->flags &= ~(MLF_Blocking | MLF_BlockMonsters | MLF_BlockGrounded | MLF_BlockPlayers);

        // clear EDGE's extended lineflags too
        ld->flags &= ~(MLF_SightBlock | MLF_ShootBlock);
    }
}

void RAD_ActBlockLines(rad_trigger_t *R, void *param)
{
    s_lineunblocker_t *ub = (s_lineunblocker_t *)param;

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        if (ld->tag != ub->tag)
            continue;

        // set standard flags
        ld->flags |= (MLF_Blocking | MLF_BlockMonsters);
    }
}

void RAD_ActJump(rad_trigger_t *R, void *param)
{
    s_jump_t *t = (s_jump_t *)param;

    if (!RandomByteTestDeterministic(t->random_chance))
        return;

    if (!t->cache_state)
    {
        // FIXME: do this in a post-parsing analysis
        t->cache_state = RAD_FindStateByLabel(R->info, t->label);

        if (!t->cache_state)
            FatalError("RTS: No such label `%s' for JUMP primitive.\n", t->label);
    }

    R->state = t->cache_state;

    // Jumps have a one tic surcharge, to prevent accidental infinite
    // loops within radius scripts.
    R->wait_tics += 1;
}

void RAD_ActSleep(rad_trigger_t *R, void *param)
{
    R->disabled = true;
}

void RAD_ActRetrigger(rad_trigger_t *R, void *param)
{
    R->activated    = false;
    R->acti_players = 0;
}

void RAD_ActShowMenu(rad_trigger_t *R, void *param)
{
    s_show_menu_t *menu = (s_show_menu_t *)param;

    if (numplayers > 1 && (R->acti_players & (1 << consoleplayer)) == 0)
        return;

    if (rts_menu_active)
    {
        // this is very unlikely, since RTS triggers do not run while
        // an RTS menu is active.  This menu simply fails.
        R->menu_result = 0;
        return;
    }

    RAD_StartMenu(R, menu);
}

void RAD_ActMenuStyle(rad_trigger_t *R, void *param)
{
    s_menu_style_t *mm = (s_menu_style_t *)param;

    SV_FreeString(R->menu_style_name);

    R->menu_style_name = SV_DupString(mm->style);
}

void RAD_ActJumpOn(rad_trigger_t *R, void *param)
{
    s_jump_on_t *jm = (s_jump_on_t *)param;

    int count = 0;

    while ((count < 9) && jm->labels[count])
        count++;

    if (R->menu_result < 0 || R->menu_result > count)
        return;

    rts_state_t *cache_state;
    char        *label = nullptr;

    if (R->menu_result > 0)
    {
        label = jm->labels[R->menu_result - 1];

        // FIXME: do this in a post-parsing analysis
        cache_state = RAD_FindStateByLabel(R->info, label);
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

static bool WUD_Match(s_wait_until_dead_s *wud, const char *name)
{
    for (int i = 0; i < 10; i++)
    {
        if (!wud->mon_names[i])
            continue;

        if (DDF_CompareName(name, wud->mon_names[i]) == 0)
            return true;
    }

    return false;
}

void RAD_ActWaitUntilDead(rad_trigger_t *R, void *param)
{
    s_wait_until_dead_s *wud = (s_wait_until_dead_s *)param;

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

        if (!RAD_WithinRadius(mo, R->info))
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

void RAD_ActSwitchWeapon(rad_trigger_t *R, void *param)
{
    s_weapon_t *weaparg = (s_weapon_t *)param;

    player_t    *player = GetWhoDunnit(R);
    WeaponDefinition *weap   = weapondefs.Lookup(weaparg->name);

    if (weap)
    {
        P_PlayerSwitchWeapon(player, weap);
    }
}

void RAD_ActTeleportToStart(rad_trigger_t *R, void *param)
{
    player_t *p = GetWhoDunnit(R);

    SpawnPoint *point = GameFindCoopPlayer(1); // start 1

    if (!point)
        return; // should never happen but who knows...

    // 1. Stop the player movement and turn him
    p->mo->momentum_.X = p->mo->momentum_.Y = p->mo->momentum_.Z = 0;
    p->actual_speed                            = 0;
    p->mo->angle_                               = point->angle;

    // 2. Don't move for a bit
    int waitAbit = 30;

    p->mo->reaction_time_ = waitAbit;

    // 3. Do our teleport fog effect
    float x = point->x;
    float y = point->y;
    float z = point->z;

    // spawn teleport fog
    MapObject *fog;
    x += 20 * epi::BAMCos(point->angle);
    y += 20 * epi::BAMSin(point->angle);
    fog = P_MobjCreateObject(x, y, z, mobjtypes.Lookup("TELEPORT_FLASH"));
    // never use this object as a teleport destination
    fog->extended_flags_ |= kExtendedFlagNeverTarget;

    if (fog->info_->chase_state_)
        P_SetMobjStateDeferred(fog, fog->info_->chase_state_, 0);

    // 4. Teleport him
    //  Don't get stuck spawned in things: telefrag them.
    P_TeleportMove(p->mo, point->x, point->y, point->z);
}

static void RAD_SetPsprite(player_t *p, int position, int stnum, WeaponDefinition *info = nullptr)
{
    pspdef_t *psp = &p->psprites[position];

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
            int new_state = DDF_StateFindLabel(info->state_grp_, st->label, true /* quiet */);
            if (new_state != 0)
                stnum = new_state;
        }
    }

    State *st = &states[stnum];

    // model interpolation stuff
    if (psp->state && (st->flags & kStateFrameFlagModel) && (psp->state->flags & kStateFrameFlagModel) &&
        (st->sprite == psp->state->sprite) && st->tics > 1)
    {
        p->weapon_last_frame = psp->state->frame;
    }
    else
        p->weapon_last_frame = -1;

    psp->state      = st;
    psp->tics       = st->tics;
    psp->next_state = (st->nextstate == 0) ? nullptr : (states + st->nextstate);

    // call action routine

    p->action_psp = position;

    if (st->action)
        (*st->action)(p->mo);
}

//
// P_SetPspriteDeferred
//
// -AJA- 2004/11/05: This is preferred method, doesn't run any actions,
//       which (ideally) should only happen during P_MovePsprites().
//
void RAD_SetPspriteDeferred(player_t *p, int position, int stnum)
{
    pspdef_t *psp = &p->psprites[position];

    if (stnum == 0 || psp->state == nullptr)
    {
        RAD_SetPsprite(p, position, stnum);
        return;
    }

    psp->tics       = 0;
    psp->next_state = (states + stnum);
}

// Replace one weapon with another instantly (no up/down states run)
// It doesnt matter if we have the old one currently selected or not.
void RAD_ActReplaceWeapon(rad_trigger_t *R, void *param)
{
    s_weapon_replace_t *weaparg = (s_weapon_replace_t *)param;

    player_t    *p      = GetWhoDunnit(R);
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
    for (i = 0; i < MAXWEAPONS; i++)
    {
        if (p->weapons[i].info == oldWep)
        {
            p->weapons[i].info = newWep;
        }
    }

    // refresh the sprite
    if (p->weapons[p->ready_wp].info == newWep)
    {
        RAD_SetPspriteDeferred(p, ps_weapon, p->weapons[p->ready_wp].info->ready_state_);

        P_FixWeaponClip(p, p->ready_wp); // handle the potential clip_size difference
        P_UpdateAvailWeapons(p);
    }
}

// If we have the weapon we insta-switch to it and
// go to the STATE we indicated.
void RAD_ActWeaponEvent(rad_trigger_t *R, void *param)
{
    s_weapon_event_t *tev = (s_weapon_event_t *)param;

    player_t    *p      = GetWhoDunnit(R);
    WeaponDefinition *oldWep = weapondefs.Lookup(tev->weapon_name);

    if (!oldWep)
    {
        FatalError("RTS WEAPON_EVENT: Unknown weapon name '%s'.\n", tev->weapon_name);
    }

    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < MAXWEAPONS; pw_index++)
    {
        if (!p->weapons[pw_index].owned)
            continue;

        if (p->weapons[pw_index].info == oldWep)
            break;
    }

    if (pw_index == MAXWEAPONS) // we dont have the weapon
        return;

    p->ready_wp = (weapon_selection_e)pw_index; // insta-switch to it

    int state = DDF_StateFindLabel(oldWep->state_grp_, tev->label, true /* quiet */);
    if (state == 0)
        FatalError("RTS WEAPON_EVENT: frame '%s' in [%s] not found!\n", tev->label, tev->weapon_name);
    state += tev->offset;

    RAD_SetPspriteDeferred(p, ps_weapon, state); // refresh the sprite
}

void P_ActReplace(MapObject *mo, const MapObjectDefinition *newThing)
{

    // DO THE DEED !!

    // UnsetThingPosition(mo);
    {
        mo->info_ = newThing;

        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fastparm)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        mo->health_ = mo->spawn_health_; // always top up health to full

        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_       = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_       = mo->info_->model_skin_;
        mo->model_last_frame_ = -1;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_[0];

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

    int state = P_MobjFindLabel(mo, "IDLE"); // nothing fancy, always default to idle
    if (state == 0)
        FatalError("RTS REPLACE_THING: frame '%s' in [%s] not found!\n", "IDLE", mo->info_->name_.c_str());

    P_SetMobjStateDeferred(mo, state, 0);
}

// Replace one thing with another.
void RAD_ActReplaceThing(rad_trigger_t *R, void *param)
{
    s_thing_replace_t *thingarg = (s_thing_replace_t *)param;

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

    // Will only get this far if the previous lookups were for numbers and failed
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

        if (!RAD_WithinRadius(mo, R->info))
            continue;

        P_ActReplace(mo, newThing);
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
