//----------------------------------------------------------------------------
//  EDGE Radius Trigger Actions
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"

#include <limits.h>

#include "dm_defs.h"
#include "dm_state.h"
#include "con_main.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"
#include "rad_act.h"
#include "r_defs.h"
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
#include "z_zone.h"

// current tip slots
drawtip_t tip_slots[MAXTIPSLOT];

// properties for fixed slots
#define FIXEDSLOTS  15

static s_tip_prop_t fixed_props[FIXEDSLOTS] =
{
	{  1, 0.50f, 0.50f, 0, "#FFFFFF", 1.0f }, 
	{  2, 0.20f, 0.25f, 1, "#FFFFFF", 1.0f },
	{  3, 0.20f, 0.75f, 1, "#FFFFFF", 1.0f },
	{  4, 0.50f, 0.50f, 0, "#3333FF", 1.0f },
	{  5, 0.20f, 0.25f, 1, "#3333FF", 1.0f },
	{  6, 0.20f, 0.75f, 1, "#3333FF", 1.0f },
	{  7, 0.50f, 0.50f, 0, "#FFFF00", 1.0f },
	{  8, 0.20f, 0.25f, 1, "#FFFF00", 1.0f },
	{  9, 0.20f, 0.75f, 1, "#FFFF00", 1.0f },
	{ 10, 0.50f, 0.50f, 0, "",        1.0f },
	{ 11, 0.20f, 0.25f, 1, "",        1.0f },
	{ 12, 0.20f, 0.75f, 1, "",        1.0f },
	{ 13, 0.50f, 0.50f, 0, "#33FF33", 1.0f },
	{ 14, 0.20f, 0.25f, 1, "#33FF33", 1.0f },
	{ 15, 0.20f, 0.75f, 1, "#33FF33", 1.0f } 
};


//
// Once-only initialisation.
//
void RAD_InitTips(void)
{
	for (int i=0; i < MAXTIPSLOT; i++)
	{
		drawtip_t *current = tip_slots + i;
		s_tip_prop_t *src = fixed_props + (i % FIXEDSLOTS);

		// initial properties
		Z_Clear(current, drawtip_t, 1);
		Z_MoveData(&current->p, src, s_tip_prop_t, 1);

		current->delay = -1;
		current->color = RGB_NO_VALUE;

		current->p.slot_num  = i;
	}
}

//
// Used when changing levels to clear any tips.
//
void RAD_ResetTips(void)
{
	// free any text strings
	for (int i=0; i < MAXTIPSLOT; i++)
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

	if (cur->color == RGB_NO_VALUE)
		cur->color = V_ParseFontColor(cur->p.color_name);
}


static void SendTip(rad_trigger_t *R, s_tip_t * tip, int slot)
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
		current->tip_text = NULL;

	// send message to the console (unless it would clog it up)
	if (current->tip_text && current->tip_text != R->last_con_message)
	{
		CON_Printf("%s\n", current->tip_text);
		R->last_con_message = current->tip_text;
	}

	current->tip_graphic = tip->tip_graphic ?
		W_ImageLookup(tip->tip_graphic) : NULL;
	current->playsound   = tip->playsound ? true : false;
	current->scale       = tip->tip_graphic ? tip->gfx_scale : 1.0f;
	current->fade_time   = 0;

	// mark it as "set me up please"
	current->dirty = true;
}

//
// -AJA- 1999/09/07: Reworked to handle tips with multiple lines.
//
void RAD_DisplayTips(void)
{
	HUD_Reset();

	for (int slot=0; slot < MAXTIPSLOT; slot++)
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

		HUD_SetScale(current->scale);
		HUD_SetTextColor(current->color);
		HUD_SetAlpha(alpha);

		if (current->p.left_just)
			HUD_SetAlignment(-1, 0);
		else
			HUD_SetAlignment(0, 0);

		float x = current->p.x_pos * 320.0f;
		float y = current->p.y_pos * 200.0f;

		if (current->tip_graphic)
			HUD_DrawImage(x, y, current->tip_graphic);
		else
			HUD_DrawText(x, y, current->tip_text);

		HUD_SetAlignment();
		HUD_SetAlpha();
		HUD_SetScale();
		HUD_SetTextColor();
	}
}

//
// Does any tic-related RTS stuff.  For now, just update the tips.
//
void RAD_Ticker(void)
{
	for (int i=0; i < MAXTIPSLOT; i++)
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
				current->p.translucency += diff / (current->fade_time+1);
		}
	}
}


// --- Radius Trigger Actions -----------------------------------------------


static player_t *GetWhoDunnit(rad_trigger_t *R)
{
	// this IS NOT CORRECT, but matches old behavior
	if (numplayers == 1)
		return players[consoleplayer];

	if (R->acti_players == 0)
		return NULL;

	// does the activator list have only one player?
	// if so, return that one.
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
		if (R->acti_players == (1 << pnum))
			return players[pnum];

	// there are multiple players who triggered the script.
	// one option: select one of them (round robin style).
	// However the following is probably more correct.
	return NULL;
}


void RAD_ActNOP(rad_trigger_t *R, void *param)
{
	// No Operation
}

void RAD_ActTip(rad_trigger_t *R, void *param)
{
	s_tip_t *tip = (s_tip_t *) param;

	// Only display the tip to the player that stepped into the radius
	// trigger.

	if (numplayers > 1 && (R->acti_players & (1 << consoleplayer)) == 0)
		return;

	SendTip(R, tip, R->tip_slot);
}

void RAD_ActTipProps(rad_trigger_t *R, void *param)
{
	s_tip_prop_t *tp = (s_tip_prop_t *) param;
	drawtip_t *current;

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
			current->fade_time = tp->time;
		}
	}

	// make tip system recompute some stuff
	current->dirty = true;
}

void RAD_ActSpawnThing(rad_trigger_t *R, void *param)
{
	s_thing_t *t = (s_thing_t *) param;

	mobj_t *mo;
	const mobjtype_c *minfo;

	// These *MUST* happen to everyone to keep netgames consistent.
	// Spawn a new map object.

	if (t->thing_name)
		minfo = mobjtypes.Lookup(t->thing_name);
	else
		minfo = mobjtypes.Lookup(t->thing_type);

	if (minfo == NULL)
	{
		if (t->thing_name)
			I_Warning("Unknown thing type: %s in RTS trigger.\n", t->thing_name);
		else
			I_Warning("Unknown thing type: %d in RTS trigger.\n", t->thing_type);

		return;
	}

	// -AJA- 2007/09/04: allow individual when_appear flags
	if (! G_CheckWhenAppear(t->appear))
		return;

	// -AJA- 1999/10/02: -nomonsters check.
	if (level_flags.nomonsters && (minfo->extendedflags & EF_MONSTER))
		return;

	// -AJA- 1999/10/07: -noextra check.
	if (!level_flags.have_extra && (minfo->extendedflags & EF_EXTRA))
		return;

	// -AJA- 1999/09/11: Support for supplying Z value.

	if (t->spawn_effect)
	{
		mo = P_MobjCreateObject(t->x, t->y, t->z, minfo->respawneffect);
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

	mo->tag = t->tag;

	mo->spawnpoint.x = t->x;
	mo->spawnpoint.y = t->y;
	mo->spawnpoint.z = t->z;
	mo->spawnpoint.angle = t->angle;
	mo->spawnpoint.vertangle = M_ATan(t->slope);
	mo->spawnpoint.info = minfo;
	mo->spawnpoint.flags = t->ambush ? MF_AMBUSH : 0;
	mo->spawnpoint.tag = t->tag;

	if (t->ambush)
		mo->flags |= MF_AMBUSH;

	// -AJA- 1999/09/25: If radius trigger is a path node, then
	//       setup the thing to follow the path.

	if (R->info->next_in_path)
		mo->path_trigger = R->info;
}

void RAD_ActDamagePlayers(rad_trigger_t *R, void *param)
{
	s_damagep_t *damage = (s_damagep_t *) param;

	// Make sure these can happen to everyone within the radius.
	// Damage the player(s)
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (!RAD_WithinRadius(p->mo, R->info))
			continue;

		P_DamageMobj(p->mo, NULL, NULL, damage->damage_amount, NULL);
	}
}

void RAD_ActHealPlayers(rad_trigger_t *R, void *param)
{
	s_healp_t *heal = (s_healp_t *) param;

	// Heal the player(s)
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (!RAD_WithinRadius(p->mo, R->info))
			continue;

		if (p->health >= heal->limit)
			continue;

		if (p->health + heal->heal_amount >= heal->limit)
			p->health = heal->limit;
		else
			p->health += heal->heal_amount;

		p->mo->health = p->health;
	}
}

void RAD_ActArmourPlayers(rad_trigger_t *R, void *param)
{
	s_armour_t *armour = (s_armour_t *) param;

	// Armour for player(s)
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

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
	s_benefit_t *be = (s_benefit_t *) param;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (!RAD_WithinRadius(p->mo, R->info))
			continue;

		P_GiveBenefitList(p, NULL, be->benefit, be->lose_it);
	}
}

void RAD_ActDamageMonsters(rad_trigger_t *R, void *param)
{
	s_damage_monsters_t *mon = (s_damage_monsters_t *) param;

	const mobjtype_c *info = NULL;
	int tag = mon->thing_tag;

	if (mon->thing_name)
	{
		info = mobjtypes.Lookup(mon->thing_name);
	}
	else if (mon->thing_type > 0)
	{
		info = mobjtypes.Lookup(mon->thing_type);

		if (info == NULL)
			I_Error("RTS DAMAGE_MONSTERS: Unknown thing type %d.\n", mon->thing_type);
	}

	// scan the mobj list
	// FIXME: optimise for fixed-sized triggers

	mobj_t *mo;
	mobj_t *next;

	player_t *player = GetWhoDunnit(R);

	for (mo = mobjlisthead; mo != NULL; mo = next)
	{
		next = mo->next;

		if (info && mo->info != info)
			continue;

		if (tag && (mo->tag != tag))
			continue;

		if (! (mo->extendedflags & EF_MONSTER) || mo->health <= 0)
			continue;

		if (! RAD_WithinRadius(mo, R->info))
			continue;

		P_DamageMobj(mo, NULL, player ? player->mo : NULL, mon->damage_amount, NULL);
	}
}

void RAD_ActThingEvent(rad_trigger_t *R, void *param)
{
	s_thing_event_t *tev = (s_thing_event_t *) param;

	const mobjtype_c *info = NULL;
	int tag = tev->thing_tag;

	if (tev->thing_name)
	{
		info = mobjtypes.Lookup(tev->thing_name);

		if (info == NULL)
			I_Error("RTS THING_EVENT: Unknown thing name '%s'.\n", tev->thing_name);
	}
	else if (tev->thing_type > 0)
	{
		info = mobjtypes.Lookup(tev->thing_type);

		if (info == NULL)
			I_Error("RTS THING_EVENT: Unknown thing type %d.\n", tev->thing_type);
	}

	// scan the mobj list
	// FIXME: optimise for fixed-sized triggers

	mobj_t *mo;
	mobj_t *next;

	for (mo = mobjlisthead; mo != NULL; mo = next)
	{
		next = mo->next;

		if (info && (mo->info != info))
			continue;

		if (tag && (mo->tag != tag))
			continue;
		
		// ignore certain things (e.g. corpses)
		if (mo->health <= 0)
			continue;

		if (! RAD_WithinRadius(mo, R->info))
			continue;

		statenum_t state = P_MobjFindLabel(mo, tev->label);

		if (state)
			P_SetMobjStateDeferred(mo, state + tev->offset, 0);
	}
}

void RAD_ActGotoMap(rad_trigger_t *R, void *param)
{
	s_gotomap_t *go = (s_gotomap_t *) param;

	// Warp to level n
	if (go->is_hub)
		G_ExitToHub(go->map_name, go->tag);
	else
		G_ExitToLevel(go->map_name, 5, go->skip_all);
}

void RAD_ActExitLevel(rad_trigger_t *R, void *param)
{
	s_exit_t *exit = (s_exit_t *) param;

	if (exit->is_secret)
		G_SecretExitLevel(exit->exittime);
	else
		G_ExitLevel(exit->exittime);
}

void RAD_ActPlaySound(rad_trigger_t *R, void *param)
{
	s_sound_t *ambient = (s_sound_t *) param;

	int flags = 0;

	if (ambient->kind == PSOUND_BossMan)
		flags |= FX_Boss;

	// Ambient sound
    R->sfx_origin.x = ambient->x;
    R->sfx_origin.y = ambient->y;

	if (ambient->z == ONFLOORZ)
		R->sfx_origin.z = R_PointInSubsector(ambient->x, ambient->y)->sector->f_h;
	else
		R->sfx_origin.z = ambient->z;

	S_StartFX(ambient->sfx, SNCAT_Level, &R->sfx_origin, flags);
}

void RAD_ActKillSound(rad_trigger_t *R, void *param)
{
 	S_StopFX(&R->sfx_origin);
}

void RAD_ActChangeMusic(rad_trigger_t *R, void *param)
{
	s_music_t *music = (s_music_t *) param;

	S_ChangeMusic(music->playnum, music->looping);
}

void RAD_ActChangeTex(rad_trigger_t *R, void *param)
{
	s_changetex_t *ctex = (s_changetex_t *) param;

	const image_c *image;

	SYS_ASSERT(param);

	// find texture or flat
	if (ctex->what >= CHTEX_Floor)
		image = W_ImageLookup(ctex->texname, INS_Flat);
	else
		image = W_ImageLookup(ctex->texname, INS_Texture);

	if (ctex->what == CHTEX_Sky)
	{
		if (image)
			sky_image = image;
		return;
	}

	// handle the floor/ceiling case
	if (ctex->what >= CHTEX_Floor)
	{
		bool must_recompute_sky = false;

		sector_t *tsec;

		for (tsec = P_FindSectorFromTag(ctex->tag); tsec != NULL;
			 tsec = tsec->tag_next)
		{
			if (ctex->subtag)
			{
				bool valid = false;

				for (int i=0; i < tsec->linecount; i++)
				{
					if (tsec->lines[i]->tag == ctex->subtag)
					{
						valid = true;
						break;
					}
				}

				if (! valid)
					continue;
			}

			if (ctex->what == CHTEX_Floor)
				tsec->floor.image = image;
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

	for (int i=0; i < numlines; i++)
	{
		side_t *side = (ctex->what <= CHTEX_RightLower) ?
			lines[i].side[0] : lines[i].side[1];

		if (lines[i].tag != ctex->tag || !side)
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
	s_skill_t *skill = (s_skill_t *) param;

	// Skill selection trigger function
	// -ACB- 1998/07/30 replaced respawnmonsters with respawnsetting.
	// -ACB- 1998/08/27 removed fastparm temporaryly.

	gameskill = skill->skill;

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

	if (! P_CheckSolidSectorMove(sec, t->is_ceiling, dh))
		return;

	P_SolidSectorMove(sec, t->is_ceiling, dh);
}

void RAD_ActMoveSector(rad_trigger_t *R, void *param)
{
	s_movesector_t *t = (s_movesector_t *) param;
	int i;

	// SectorV compatibility
	if (t->tag == 0)
	{
		if (t->secnum < 0 || t->secnum >= numsectors)
			I_Error("RTS SECTORV: no such sector %d.\n", t->secnum);

		MoveOneSector(sectors + t->secnum, t);
		return;
	}

	// OPTIMISE !
	for (i=0; i < numsectors; i++)
	{
		if (sectors[i].tag == t->tag)
			MoveOneSector(sectors + i, t);
	}
}

static void LightOneSector(sector_t *sec, s_lightsector_t *t)
{
	if (t->relative)
		sec->props.lightlevel += I_ROUND(t->value);
	else
		sec->props.lightlevel = I_ROUND(t->value);
}

void RAD_ActLightSector(rad_trigger_t *R, void *param)
{
	s_lightsector_t *t = (s_lightsector_t *) param;
	int i;

	// SectorL compatibility
	if (t->tag == 0)
	{
		if (t->secnum < 0 || t->secnum >= numsectors)
			I_Error("RTS SECTORL: no such sector %d.\n", t->secnum);

		LightOneSector(sectors + t->secnum, t);
		return;
	}

	// OPTIMISE !
	for (i=0; i < numsectors; i++)
	{
		if (sectors[i].tag == t->tag)
			LightOneSector(sectors + i, t);
	}
}

void RAD_ActEnableScript(rad_trigger_t *R, void *param)
{
	s_enabler_t *t = (s_enabler_t *) param;
	rad_trigger_t *other;

	// Enable/Disable Scripts
	if (t->script_name)
	{
		other = RAD_FindTriggerByName(t->script_name);

		if (! other)
			return;

		other->disabled = t->new_disabled;
	}
	else
	{
		RAD_EnableByTag(NULL, t->tag, t->new_disabled);
	}
}

void RAD_ActActivateLinetype(rad_trigger_t *R, void *param)
{
	s_lineactivator_t *t = (s_lineactivator_t *) param;

	player_t *player = GetWhoDunnit(R);

	P_RemoteActivation(player ? player->mo : NULL, t->typenum, t->tag, 0, line_Any);
}

void RAD_ActUnblockLines(rad_trigger_t *R, void *param)
{
	s_lineunblocker_t *ub = (s_lineunblocker_t *) param;

	int i;

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		if (ld->tag != ub->tag)
			continue;

		if (! ld->side[0] || ! ld->side[1])
			continue;

		// clear standard flags
		ld->flags &= ~(MLF_Blocking | MLF_BlockMonsters);

		// clear EDGE's extended lineflags too
		ld->flags &= ~(MLF_SightBlock | MLF_ShootBlock);
	}
}

void RAD_ActBlockLines(rad_trigger_t *R, void *param)
{
	s_lineunblocker_t *ub = (s_lineunblocker_t *) param;

	int i;

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		if (ld->tag != ub->tag)
			continue;

		// set standard flags
		ld->flags |= (MLF_Blocking | MLF_BlockMonsters);
	}
}

void RAD_ActJump(rad_trigger_t *R, void *param)
{
	s_jump_t *t = (s_jump_t *) param;

	if (! P_RandomTest(t->random_chance))
		return;

	if (! t->cache_state)
	{
		// FIXME: do this in a post-parsing analysis
		t->cache_state = RAD_FindStateByLabel(R->info, t->label);

		if (! t->cache_state)
			I_Error("RTS: No such label `%s' for JUMP primitive.\n", t->label);
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
	R->activated = false;
	R->acti_players = 0;
}

void RAD_ActShowMenu(rad_trigger_t *R, void *param)
{
	s_show_menu_t *menu = (s_show_menu_t *) param;

	if (numplayers > 1 && (R->acti_players & (1 << consoleplayer)) == 0)
		return;

	if (rts_menuactive)
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
	s_menu_style_t *mm = (s_menu_style_t *) param;

	SV_FreeString(R->menu_style_name);

	R->menu_style_name = SV_DupString(mm->style);
}

void RAD_ActJumpOn(rad_trigger_t *R, void *param)
{
	s_jump_on_t *jm = (s_jump_on_t *) param;

	int count = 0;

	while ((count < 9) && jm->labels[count])
		count++;

	if (R->menu_result < 1 || R->menu_result > count)
		return;

	char *label = jm->labels[R->menu_result - 1];

	// FIXME: do this in a post-parsing analysis
	rts_state_t *cache_state = RAD_FindStateByLabel(R->info, label);

	if (! cache_state)
		I_Error("RTS: No such label `%s' for JUMP_ON primitive.\n", label);

	R->state = cache_state;

	// Jumps have a one tic surcharge, to prevent accidental infinite
	// loops within radius scripts.
	R->wait_tics += 1;
}

static bool WUD_Match(s_wait_until_dead_s *wud, const char *name)
{
	for (int i = 0 ; i < 10 ; i++)
	{
		if (! wud->mon_names[i])
			continue;

		if (DDF_CompareName(name, wud->mon_names[i]) == 0)
			return true;
	}

	return false;
}

void RAD_ActWaitUntilDead(rad_trigger_t *R, void *param)
{
	s_wait_until_dead_s *wud = (s_wait_until_dead_s *)param;

	R->wud_tag = wud->tag;
	R->wud_count = 0;

	// find all matching monsters
	mobj_t *mo;
	mobj_t *next;

	for (mo = mobjlisthead; mo != NULL; mo = next)
	{
		next = mo->next;

		if (! mo->info)
			continue;

		if (mo->health <= 0)
			continue;

		if (! WUD_Match(wud, mo->info->name))
			continue;

		if (! RAD_WithinRadius(mo, R->info))
			continue;

		// mark the monster
		mo->hyperflags |= HF_WAIT_UNTIL_DEAD;
		mo->tag = wud->tag;

		R->wud_count++;
	}

	if (R->wud_count == 0)
	{
		L_WriteDebug("RTS: waiting forever, no %s found\n",
		             wud->mon_names[0]);
		R->wud_count = 1;
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
