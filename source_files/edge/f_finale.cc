//----------------------------------------------------------------------------
// EDGE Finale Code on Game Completion
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
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
// -KM- 1998/09/27 sounds.ddf stuff: seesound -> DDF_LookupSound(seesound)
// -KM- 1998/11/25 Finale generalised.
//

#include "i_defs.h"
#include "i_defs_gl.h"

#include "main.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "dstrings.h"
#include "e_main.h"
#include "f_finale.h"
#include "f_interm.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_action.h"
#include "am_map.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_mdl.h"
#include "r_md2.h"
#include "r_voxel.h"
#include "r_modes.h"
#include "r_state.h"
#include "r_gldefs.h"
#include "s_sound.h"
#include "s_music.h"
#include "w_wad.h"
#include "w_model.h"
#include "hu_style.h"


typedef enum
{
	f_text,
	f_pic,
	f_bunny,
	f_cast,
	f_done
}
finalestage_e;

void operator++ (finalestage_e& f, int blah)
{
	f = (finalestage_e)(f + 1);
}

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
static finalestage_e finalestage;

// -ES- 2000/03/11 skip to the next finale stage
static bool skip_finale;
static int finalecount;
static int picnum;

#define TEXTSPEED 3
#define TEXTWAIT  250

static const char *finaletext;

static gameaction_e newgameaction;
static const map_finaledef_c *finale;

static void CastInitNew(int num);
static void CastTicker(void);
static void CastSkip(void);

static const image_c *finale_textback;
static float finale_textbackscale = 1.0f;
static rgbcol_t finale_textcol;

static style_c *wi_leveltext_style;
static style_c *wi_cast_style;

static bool HasFinale(const map_finaledef_c *F, finalestage_e cur)
{
	SYS_ASSERT(F);

	switch (cur)
	{
		case f_text:
			return F->text != "";

		case f_pic:
			return (F->pics.size() > 0);

		case f_bunny:
			return F->dobunny;

		case f_cast:
			return F->docast;

		default:
			I_Error("Bad parameter passed to HasFinale().\n");
	}

	return false; /* NOT REACHED */
}

// returns f_done if nothing found
static finalestage_e FindValidFinale(const map_finaledef_c *F, finalestage_e cur)
{
	SYS_ASSERT(F);

	for (; cur != f_done; cur++)
	{
		if (HasFinale(F, cur))
			return cur;
	}

	return f_done;
}

static void DoStartFinale(void)
{
	finalecount = 0;

	switch (finalestage)
	{
		case f_text:
			finaletext = language[finale->text];
			S_ChangeMusic(finale->music, true);
			break;

		case f_pic:
			picnum = 0;
			break;

		case f_bunny:
			if (currmap->episode)
				S_ChangeMusic(currmap->episode->special_music, true);
			break;

		case f_cast:
			CastInitNew(2);
			if (currmap->episode)
				S_ChangeMusic(currmap->episode->special_music, true);
			break;

		default:
			I_Error("DoStartFinale: bad stage #%d\n", (int)finalestage);
			break;
	}

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
		if (players[pnum])
			players[pnum]->cmd.buttons = 0;
}

static void DoBumpFinale(void)
{
	// find next valid Finale stage
	finalestage_e stage = finalestage;
	stage++;
	stage = FindValidFinale(finale, stage);

	if (stage != f_done)
	{
		if (gamestate != GS_INTERMISSION)
			E_ForceWipe();

		finalestage = stage;

		DoStartFinale();
		return;
	}

	// capture the screen _before_ changing any global state
	if (newgameaction != ga_nothing)
	{
		E_ForceWipe();
		gameaction = newgameaction;
	}

	gamestate = GS_NOTHING;  // hack ???  (cannot leave as GS_FINALE)
}


static void LookupFinaleStuff(void)
{
	// here is where we lookup the required images

	if (finale->text_flat != "")
		finale_textback = W_ImageLookup(finale->text_flat.c_str(), INS_Flat);
	else if (finale->text_back != "")
		finale_textback = W_ImageLookup(finale->text_back.c_str(), INS_Graphic);
	else
		finale_textback = NULL;

	finale_textcol = V_GetFontColor(finale->text_colmap);
	
	if (! wi_leveltext_style)
	{
		styledef_c *def = styledefs.Lookup("INTERLEVEL TEXT");
		if (! def) def = default_style;
		wi_leveltext_style = hu_styles.Lookup(def);
	}
	if (! wi_cast_style)
	{
		styledef_c *def = styledefs.Lookup("CAST_SCREEN");
		if (! def) def = default_style;
		wi_cast_style = hu_styles.Lookup(def);
	}
}


void F_StartFinale(const map_finaledef_c *F, gameaction_e newaction)
{
	SYS_ASSERT(F);

	newgameaction = newaction;
	automapactive = false;

	finalestage_e stage = FindValidFinale(F, f_text);

	if (stage == f_done)
	{
		if (newgameaction != ga_nothing)
			gameaction = newgameaction;

		return /* false */;
	}

	// capture the screen _before_ changing any global state
	//--- E_ForceWipe();   // CRASH with IDCLEV

	finale = F;
	finalestage = stage;

	LookupFinaleStuff();

	gamestate = GS_FINALE;

	DoStartFinale();
}


bool F_Responder(event_t * event)
{
	SYS_ASSERT(gamestate == GS_FINALE);

	// FIXME: use WI_CheckAccelerate() in netgames
	if (event->type != ev_keydown)
		return false;

	if (finalecount > TICRATE)
	{
		if (finalestage == f_pic && finale->picwait == INT_MAX)
			return false;

		skip_finale = true;
		return true;
	}

	return false;
}



void F_Ticker(void)
{
	SYS_ASSERT(gamestate == GS_FINALE);

	// advance animation
	finalecount++;

	switch (finalestage)
	{
		case f_text:
			if (skip_finale && finalecount < (int)strlen(finaletext) * TEXTSPEED)
			{
				finalecount = TEXTSPEED * strlen(finaletext);
				skip_finale = false;
			}
			else if (skip_finale || finalecount > TEXTWAIT + (int)strlen(finaletext) * TEXTSPEED)
			{
				DoBumpFinale();
				skip_finale = false;
			}
			break;

		case f_pic:
			if (skip_finale || finalecount > (int)finale->picwait)
			{
				picnum++;
				finalecount = 0;
				skip_finale = false;
			}
			if (picnum >= (int)finale->pics.size())
			{
				DoBumpFinale();
			}
			break;

		case f_bunny:
			if (skip_finale && finalecount < 1100)
			{
				finalecount = 1100;
				skip_finale = false;
			}
			break;

		case f_cast:
			if (skip_finale)
			{
				CastSkip();
				skip_finale = false;
			}
			else
				CastTicker();

			break;

		default:
			I_Error("F_Ticker: bad finalestage #%d\n", (int)finalestage);
			break;
	}


	if (finalestage == f_done)
	{
		if (newgameaction != ga_nothing)
		{
			gameaction = newgameaction;

			// don't come here again (for E_ForceWipe)
			newgameaction = ga_nothing;

			if (gamestate == GS_FINALE)
				E_ForceWipe();
		}
	}
}


static void TextWrite(void)
{
	// 98-7-10 KM erase the entire screen to a tiled background
	if (finale_textback)
	{
		HUD_SetScale(finale_textbackscale);
		
		if (finale->text_flat[0])
		{
			// AJA 2022: make the flats be square, not squished
			HUD_SetCoordSys(266, 200);

			//Lobo: if it's a flat, tile it
			HUD_TileImage(hud_x_left, 0, hud_x_right - hud_x_left, 200, finale_textback); //Lobo: Widescreen support
		}
		else
		{
			if (r_titlescaling.d == 2) // Stretch
				HUD_StretchImage(hud_x_left, 0, hud_x_right-hud_x_left, 200, finale_textback, 0, 0);
			else
			{
				if (r_titlescaling.d == 3) // Fill Border
				{
					if ((float)finale_textback->actual_w/finale_textback->actual_h < (float)SCREENWIDTH/SCREENHEIGHT)
					{
						if (!finale_textback->blurred_version)
							W_ImageStoreBlurred(finale_textback, 0.75f);
						HUD_StretchImage(-320, -200, 960, 600, finale_textback->blurred_version, 0, 0);
					}
				}
				HUD_DrawImageTitleWS(finale_textback);
			}
		}

		// reset coordinate system
		HUD_Reset();
	}
	
	style_c *style;
	style=wi_leveltext_style;
	int t_type = styledef_c::T_TEXT;
	
	// draw some of the text onto the screen
	int cx = 10;
	//int cy = 10;

	const char *ch = finaletext;

	int count = (int) ((finalecount - 10) / finale->text_speed);
	if (count < 0)
		count = 0;

	SYS_ASSERT(finale);

	//HUD_SetFont();
	//HUD_SetScale();
	HUD_SetTextColor(finale_textcol); //set a default
	
	float txtscale = 0.9; //set a default
	if(style->def->text[t_type].scale)
	{
		txtscale=style->def->text[t_type].scale;
		HUD_SetScale(txtscale);
	}

	if(style->def->text[t_type].colmap)
	{
		const colourmap_c *colmap = style->def->text[t_type].colmap;
		HUD_SetTextColor(V_GetFontColor(colmap));
	}
	
	int h = 11; //set a default
	if(style->fonts[t_type])
	{
		HUD_SetFont(style->fonts[t_type]);
		h = style->fonts[t_type]->NominalHeight();
		h = h + (3 * txtscale); //bit of spacing
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
			HUD_DrawText(cx, cy, line);
			break;
		}

		int c = *ch++; count--;

		if (c == '\n' || pos > (int)sizeof(line)-4)
		{
			HUD_DrawText(cx, cy, line);

			pos = 0;
			line[0] = 0;

			cy += h; //11; 
			continue;
		}

		line[pos++] = c;
		line[pos] = 0;
	}
	
	//set back to defaults
	HUD_SetFont();
	HUD_SetScale();
	HUD_SetTextColor();
}


//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//

static const mobjtype_c *castorder;
static const char *casttitle;
static int casttics;
static state_t *caststate;
static bool castdeath;
static int castframes;
static int castonmelee;
static bool castattacking;

//
// CastSetState, CastPerformAction
// 
// -AJA- 2001/05/28: separated this out from CastTicker
// 
static void CastSetState(statenum_t st)
{
	if (st == S_NULL)
		return;

	caststate = &states[st];

	casttics = caststate->tics;
	if (casttics < 0)
		casttics = 15;
}

static void CAST_RangeAttack(const atkdef_c *range)
{
	sfx_t *sfx = NULL;

	SYS_ASSERT(range);

	if (range->attackstyle == ATK_SHOT)
	{
		sfx = range->sound;
	}
	else if (range->attackstyle == ATK_SKULLFLY)
	{
		sfx = range->initsound;
	}
	else if (range->attackstyle == ATK_SPAWNER)
	{
		if (range->spawnedobj && range->spawnedobj->rangeattack)
			sfx = range->spawnedobj->rangeattack->initsound;
	}
	else if (range->attackstyle == ATK_TRACKER)
	{
		sfx = range->initsound;
	}
	else if (range->atk_mobj)
	{
		sfx = range->atk_mobj->seesound;
	}

	S_StartFX(sfx);
}

static void CastPerformAction(void)
{
	sfx_t *sfx = NULL;

	// Yuk, handles sounds

	if (caststate->action == P_ActMakeCloseAttemptSound)
	{
		if (castorder->closecombat)
			sfx = castorder->closecombat->initsound;
	}
	else if (caststate->action == P_ActMeleeAttack)
	{
		if (castorder->closecombat)
			sfx = castorder->closecombat->sound;
	}
	else if (caststate->action == P_ActMakeRangeAttemptSound)
	{
		if (castorder->rangeattack)
			sfx = castorder->rangeattack->initsound;
	}
	else if (caststate->action == P_ActRangeAttack)
	{
		if (castorder->rangeattack)
			CAST_RangeAttack(castorder->rangeattack);
	}
	else if (caststate->action == P_ActComboAttack)
	{
		if (castonmelee && castorder->closecombat)
		{
			sfx = castorder->closecombat->sound;
		}
		else if (castorder->rangeattack)
		{
			CAST_RangeAttack(castorder->rangeattack);
		}
	}
	else if (castorder->activesound && (M_Random() < 2) && !castdeath)
	{
		sfx = castorder->activesound;
	}
	else if (caststate->action == P_ActWalkSoundChase)
	{
		sfx = castorder->walksound;
	}

	S_StartFX(sfx);
}

static void CastInitNew(int num)
{
	castorder = mobjtypes.LookupCastMember(num);

	// FIXME!!! Better handling of the finale
	if (!castorder)
		castorder = mobjtypes.Lookup(0);

	casttitle = castorder->cast_title != "" ?
		language[castorder->cast_title] : castorder->name.c_str();

	castdeath = false;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;

	SYS_ASSERT(castorder->chase_state);  // checked in ddf_mobj.c
	CastSetState(castorder->chase_state);
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
	casttics--;
	if (casttics > 0)
		return;

	// switch from deathstate to next monster
	if (caststate->tics == -1 || caststate->nextstate == S_NULL ||
			(castdeath && castframes >= 30))
	{
		CastInitNew(castorder->castorder + 1);

		if (castorder->seesound)
			S_StartFX(castorder->seesound);

		return;
	}

	CastPerformAction();

	// advance to next state in animation
	// -AJA- if there's a jumpstate, enter it occasionally

	if (caststate->action == P_ActJump && caststate->jumpstate &&
		(M_Random() < 64))
		st = caststate->jumpstate;
	else
		st = caststate->nextstate;

	CastSetState(st);
	castframes++;

	// go into attack frame
	if (castframes == 24 && !castdeath)
	{
		castonmelee ^= 1;
		st = castonmelee ? castorder->melee_state : castorder->missile_state;

		if (st == S_NULL)
		{
			castonmelee ^= 1;
			st = castonmelee ? castorder->melee_state : castorder->missile_state;
		}

		// check if missing both melee and missile states
		if (st != S_NULL)
		{
			castattacking = true;
			CastSetState(st);

			if (castorder->attacksound)
				S_StartFX(castorder->attacksound);
		}
	}

	// leave attack frames after a certain time
	if (castattacking && (castframes == 48 || 
				caststate == &states[castorder->chase_state]))
	{
		castattacking = false;
		castframes = 0;
		CastSetState(castorder->chase_state);
	}
}

//
// CastSkip
//
static void CastSkip(void)
{
	if (castdeath)
		return;  // already in dying frames

	// go into death frame
	castdeath = true;

	if (castorder->overkill_state && (M_Random() < 32))
		caststate = &states[castorder->overkill_state];
	else
	{
		SYS_ASSERT(castorder->death_state);  // checked in ddf_mobj.c
		caststate = &states[castorder->death_state];
	}

	casttics = caststate->tics;
	castframes = 0;
	castattacking = false;

	if (castorder->deathsound)
		S_StartFX(castorder->deathsound);
}


//
// CastDrawer
//
static void CastDrawer(void)
{
	float TempScale = 1.0;

	const image_c *image;

	if (wi_cast_style->bg_image)
	{
		wi_cast_style->DrawBackground();
	}
	else
	{
		image = W_ImageLookup("BOSSBACK");
		if (r_titlescaling.d == 2) // Stretch
			HUD_StretchImage(hud_x_left, 0, hud_x_right-hud_x_left, 200, image, 0, 0);
		else
		{
			if (r_titlescaling.d == 3) // Fill Border
			{
				if ((float)image->actual_w/image->actual_h < (float)SCREENWIDTH/SCREENHEIGHT)
				{
					if (!image->blurred_version)
							W_ImageStoreBlurred(image, 0.75f);
					HUD_StretchImage(-320, -200, 960, 600, image->blurred_version, 0, 0);
				}
			}
			HUD_DrawImageTitleWS(image);
		}
	}
	
	HUD_SetAlignment(0, -1);

	if (wi_cast_style->def->text[styledef_c::T_TEXT].colmap)
	{
		HUD_SetTextColor(V_GetFontColor(wi_cast_style->def->text[styledef_c::T_TEXT].colmap));
	}
	else
	{	
		HUD_SetTextColor(T_YELLOW); 
	}

	TempScale = wi_cast_style->def->text[styledef_c::T_TEXT].scale;
	HUD_SetScale(TempScale);
	
	if(wi_cast_style->fonts[styledef_c::T_TEXT])
	{
		HUD_SetFont(wi_cast_style->fonts[styledef_c::T_TEXT]);
	}
	

	HUD_DrawText(160, 180, casttitle);

	HUD_Reset();

	bool flip;

	float pos_x, pos_y;
	float scale_x, scale_y;

	
	TempScale = wi_cast_style->def->text[styledef_c::T_HEADER].scale;
	if (TempScale < 1.0 || TempScale > 1.0)
	{
		scale_y = wi_cast_style->def->text[styledef_c::T_HEADER].scale;
	}
	else
		scale_y = 3;

	HUD_GetCastPosition(&pos_x, &pos_y, &scale_x, &scale_y);

	if (caststate->flags & SFF_Model)
	{
		modeldef_c *md = W_GetModel(caststate->sprite);

		const image_c *skin_img = md->skins[castorder->model_skin];

		if (! skin_img)
			skin_img = W_ImageForDummySkin();

		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		if (md->md2_model)
			MD2_RenderModel_2D(md->md2_model, skin_img, caststate->frame,
							pos_x, pos_y, scale_x, scale_y, castorder);
		else if (md->mdl_model)
			MDL_RenderModel_2D(md->mdl_model, skin_img, caststate->frame,
							pos_x, pos_y, scale_x, scale_y, castorder);
		else
			VXL_RenderModel_2D(md->vxl_model, pos_x, pos_y,
							scale_x, scale_y, castorder);

		glDisable(GL_DEPTH_TEST);
		return;
	}

	// draw the current frame in the middle of the screen
	image = R2_GetOtherSprite(caststate->sprite, caststate->frame, &flip);

	if (! image)
		return;

	scale_x *= castorder->scale * castorder->aspect;
	scale_y *= castorder->scale;

	float width    = IM_WIDTH(image);
	float height   = IM_HEIGHT(image);

	float offset_x = IM_OFFSETX(image);
	float offset_y = IM_OFFSETY(image);

	if (flip)
		offset_x = -offset_x;

	offset_x  = (width/2.0f + offset_x) * scale_x;
	offset_y *= scale_y;

	width  *= scale_x;
	height *= scale_y;

	RGL_DrawImage(pos_x - offset_x, pos_y + offset_y,
			      width, height, image, 
				  flip ? IM_RIGHT(image) : 0, 0,
				  flip ? 0 : IM_RIGHT(image), IM_TOP(image),
				  NULL, 1.0f, castorder->palremap);
}

//
// BunnyScroll
//
// -KM- 1998/07/31 Made our bunny friend take up more screen space.
// -KM- 1998/12/16 Removed fading routine.
// -Lobo- 2021/11/02 Widescreen support: both images must be the same size
static void BunnyScroll(void)
{
	int scrolled;
	const image_c *p1;
	const image_c *p2;
	char name[10];
	int stage;
	static int laststage;

	p1 = W_ImageLookup("PFUB2");
	p2 = W_ImageLookup("PFUB1");

	float TempWidth = 0;
	float TempHeight = 0;
	float TempScale = 0;
	float CenterX = 0;
	//1. Calculate scaling to apply.
	TempScale = 200;
	TempScale /= p1->actual_h;
	TempWidth = p1->actual_w * TempScale;
	TempHeight = p1->actual_h * TempScale;
	//2. Calculate centering on screen.
	CenterX = 160;
	CenterX -= (p1->actual_w * TempScale)/ 2;

	scrolled = (TempWidth + CenterX) - (finalecount - 230) / 2;
	if (scrolled > (TempWidth + CenterX))
		scrolled = (TempWidth + CenterX);
	if (scrolled < 0)
		scrolled = 0;

	HUD_StretchImage(CenterX  - scrolled, 0, TempWidth, TempHeight, p1, 0.0, 0.0);
	HUD_StretchImage((CenterX + TempWidth) - (scrolled + 1), 0, TempWidth, TempHeight, p2, 0.0, 0.0);

	if (finalecount < 1130)
		return;

	if (finalecount < 1180)
	{
		p1 = W_ImageLookup("END0");

		HUD_DrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
		laststage = 0;
		return;
	}

	stage = (finalecount - 1180) / 5;

	if (stage > 6)
		stage = 6;

	if (stage > laststage)
	{
		S_StartFX(sfx_pistol);
		laststage = stage;
	}

	sprintf(name, "END%i", stage);

	p1 = W_ImageLookup(name);

	HUD_DrawImage((320 - 13 * 8) / 2, (200 - 8 * 8) / 2, p1);
}


void F_Drawer(void)
{
	SYS_ASSERT(gamestate == GS_FINALE);

	switch (finalestage)
	{
		case f_text:
			TextWrite();
			break;

		case f_pic:
			{
				const image_c *image = W_ImageLookup(finale->pics[MIN((size_t)picnum, finale->pics.size()-1)].c_str());
				if (r_titlescaling.d == 2) // Stretch
					HUD_StretchImage(hud_x_left, 0, hud_x_right-hud_x_left, 200, image, 0, 0);
				else
				{
					if (r_titlescaling.d == 3) // Fill Border
					{
						if ((float)image->actual_w/image->actual_h < (float)SCREENWIDTH/SCREENHEIGHT)
						{
							if (!image->blurred_version)
								W_ImageStoreBlurred(image, 0.75f);
							HUD_StretchImage(-320, -200, 960, 600, image->blurred_version, 0, 0);
						}
					}
					HUD_DrawImageTitleWS(image);
				}
			}
			break;

		case f_bunny:
			BunnyScroll();
			break;

		case f_cast:
			CastDrawer();
			break;

		default:
			I_Error("F_Drawer: bad finalestage #%d\n", (int)finalestage);
			break;
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
