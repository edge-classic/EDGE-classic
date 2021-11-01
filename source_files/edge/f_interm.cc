//----------------------------------------------------------------------------
//  EDGE Intermission Code
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

#include "i_defs.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "f_finale.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "p_local.h"
#include "s_sound.h"
#include "s_music.h"
#include "r_misc.h"
#include "r_draw.h"
#include "r_modes.h"


//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

// GLOBAL LOCATIONS
#define WI_TITLEY   6
#define WI_SPACINGY 33

// SINGPLE-PLAYER STUFF
#define SP_STATSX 50
#define SP_STATSY 50

#define SP_TIMEX 16
#define SP_TIMEY (200-32)

//
// GENERAL DATA
//

// contains information passed into intermission
wistats_t wi_stats;

//
// Locally used stuff.
//

// States for single-player
#define SP_KILLS  0
#define SP_ITEMS  2
#define SP_SECRET 4
#define SP_FRAGS  6
#define SP_TIME   8
#define SP_PAR    ST_TIME

#define SP_PAUSE 1

// in seconds
#define SHOWNEXTLOCDELAY 4
#define SHOWLASTLOCDELAY SHOWNEXTLOCDELAY

// used to accelerate or skip a stage
static bool acceleratestage;

 // specifies current state
static stateenum_t state;

// used for general timing
static int cnt;

// used for timing of background animation
static int bcnt;

// signals to refresh everything for one frame
static int firstrefresh;

#define NUM_SHOWN  10

static int sp_state;

static int cnt_kills[NUM_SHOWN];
static int cnt_items[NUM_SHOWN];
static int cnt_secrets[NUM_SHOWN];
static int cnt_frags[NUM_SHOWN];
static int cnt_totals[NUM_SHOWN];

static int cnt_time;
static int cnt_par;
static int cnt_pause;

static int dm_state;

static int dm_frags[NUM_SHOWN];
static int dm_totals[NUM_SHOWN];
static int dm_rank[NUM_SHOWN];

static int dofrags;

static int ng_state;

static bool snl_pointeron = false;

static style_c *wi_sp_style;
static style_c *wi_net_style;

// GRAPHICS

// background
static const image_c *bg_image;

// You Are Here graphic
static const image_c *yah[2] = {NULL, NULL};

// splat
static const image_c *splat[2] = {NULL, NULL};

// %, : graphics
static const image_c *percent;
static const image_c *colon;

// 0-9 graphic
static const image_c *digits[10];   //FIXME: use FONT/STYLE

// minus sign
static const image_c *wiminus;

// "Finished!" graphics
static const image_c *finished;

// "Entering" graphic
static const image_c *entering;

// "secret"
static const image_c *sp_secret;

 // "Kills", "Scrt", "Items", "Frags"
static const image_c *kills;
static const image_c *secret;
static const image_c *items;
static const image_c *frags;

// Time sucks.
static const image_c *time_image; // -ACB- 1999/09/19 Removed Conflict with <time.h>
static const image_c *par;
static const image_c *sucks;

// "killers", "victims"
static const image_c *killers;
static const image_c *victims;

// "Total", your face, your dead face
static const image_c *total;
static const image_c *star;
static const image_c *bstar;

// Name graphics of each level (centered)
static const image_c *lnames[2];

//
// -ACB- 2004/06/25 Short-term containers for
//                  the world intermission data
//

class wi_mappos_c
{
public:
	wi_mapposdef_c *info;
	bool done;

public:
	 wi_mappos_c() { info = NULL; done = false; }
	~wi_mappos_c() {}

private:
	/* ... */
};

class wi_frame_c
{
public:
	wi_framedef_c *info;

	const image_c *image; 	// cached image

public:
	 wi_frame_c() { info = NULL; image = NULL; }
	~wi_frame_c() { }

private:
	/* ... */
};

class wi_anim_c
{
public:
	wi_animdef_c *info;
	
	// This array doesn't need to built up, so we stick to primitive form
	wi_frame_c *frames;
	int numframes;

	int count;
	int frameon;

public:
	wi_anim_c()
	{
		frames = NULL;
		numframes = 0;
	}

	~wi_anim_c()
	{
		Clear();
	}

private:
	/* ... */

public:
	void Clear(void)
	{
		if (frames)
		{
			delete [] frames;
			frames = NULL;

			numframes = 0;
		}
	}

	void Load(wi_animdef_c *def)
	{
		int size;

		// Frames...
		size = def->frames.GetSize();
		if (size>0)
		{
			int i;

			frames = new wi_frame_c[size];
			for (i=0; i<size; i++)
				frames[i].info = def->frames[i];
		}

		info = def;
		numframes = size;
	}

	void Reset(void)
	{
		count = 0;
		frameon = -1;
	}
};

class wi_c
{
public:
	// This array doesn't need to built up, so we stick to primitive form
	wi_anim_c *anims;
	int numanims;

	// This array doesn't need to built up, so we stick to primitive form
	wi_mappos_c *mappos;
	int nummappos;

public:
	wi_c()
	{
		gamedef = NULL;

		anims = NULL;
		numanims = 0;

		mappos = NULL;
		nummappos = 0;
	}

	~wi_c()
	{
		Clear();
	}

private:
	gamedef_c *gamedef;

	void Clear(void)
	{
		if (anims)
		{
			delete [] anims;
			anims = NULL;

			numanims = 0;
		}

		if (mappos)
		{
			delete [] mappos;
			mappos = NULL;

			nummappos = 0;
		}
	}

	void Load(gamedef_c *_gamedef)
	{
		// Animations
		int size = _gamedef->anims.GetSize();

		if (size > 0)
		{
			anims = new wi_anim_c[size];

			for (int i = 0; i < size; i++)
				anims[i].Load(_gamedef->anims[i]);

			numanims = size;
		}

		// Map positions
		size = _gamedef->mappos.GetSize();
		if (size > 0)
		{
			mappos = new wi_mappos_c[size];

			for (int i = 0; i < size; i++)
				mappos[i].info = _gamedef->mappos[i];

			nummappos = size;
		}
	}

	void Reset(void)
	{
		for (int i = 0; i < numanims; i++)
			anims[i].Reset();
	}

public:
	void Init(gamedef_c *_gamedef)
	{
		if (_gamedef != gamedef)
		{
			// Clear
			Clear();

			if (_gamedef)
				Load(_gamedef);
		}

		if (_gamedef)
			Reset();

		gamedef = _gamedef;
		return;
	}
};

static wi_c worldint;


//
// CODE
//

void WI_Clear(void)
{
	worldint.Init(NULL);
}


// Draws "<Levelname> Finished!"
static void DrawLevelFinished(void)
{
	float y = WI_TITLEY;

	// draw <LevelName> 
	SYS_ASSERT(lnames[0]);

	float w = IM_WIDTH(lnames[0]);
	float h = IM_HEIGHT(lnames[0]);

	float w2 = IM_WIDTH(finished);

	HUD_DrawImage(160 - w/2,  y, lnames[0]);
	HUD_DrawImage(160 - w2/2, y + h * 5/4, finished);
}

// Draws "Entering <LevelName>"
static void DrawEnteringLevel(void)
{
	float y = WI_TITLEY;

	// -KM- 1998/11/25 If there is no level to enter, don't draw it.
	//      (Stop Map30 from crashing)
	if (! lnames[1])
		return;

	float h = IM_HEIGHT(entering);
	float w = IM_WIDTH(entering);
	float w2 = IM_WIDTH(lnames[1]);

	HUD_DrawImage(160 - w/2,  y, entering);
	HUD_DrawImage(160 - w2/2, y + h * 5/4, lnames[1]);
}

static void DrawOnLnode(wi_mappos_c* mappos, const image_c * images[2])
{
	int i;

	// -AJA- this is used to select between Left and Right pointing
	// arrows (WIURH0 and WIURH1).  Smells like a massive HACK.

	for (i=0; i < 2; i++)
	{
		if (images[i] == NULL)
			continue;

		float left = mappos->info->x - IM_OFFSETX(images[i]);
		float top  = mappos->info->y - IM_OFFSETY(images[i]);

		float right  = left + IM_WIDTH(images[i]);
		float bottom = top + IM_HEIGHT(images[i]);

		if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
		{
			/* this one fits on the screen */
			break;
		}
	}

	if (i < 2)
	{
		HUD_DrawImage(mappos->info->x, mappos->info->y, images[i]);
	}
	else
	{
		L_WriteDebug("Could not place patch on level '%s'\n", 
		  mappos->info->name.c_str());
	}
}

//
// Draws a number.
//
// If numdigits > 0, then use that many digits minimum,
//  otherwise only use as many as necessary.
// Returns new x position.
//
static float DrawNum(float x, float y, int n, int numdigits)
{
	int neg;
	int temp;

	neg = n < 0;

	if (neg)
		n = -n;

	// if non-number, do not draw it
	if (n == 1994)
		return 0;

	if (numdigits < 0)
	{
		if (!n)
		{
			// make variable-length zeros 1 digit long
			numdigits = 1;
		}
		else
		{
			// figure out # of digits in #
			numdigits = 0;
			temp = n;

			while (temp)
			{
				temp /= 10;
				numdigits++;
			}
		}
	}

	// draw the new number
	for (; numdigits > 0; n /= 10, numdigits--)
	{
		x -= IM_WIDTH(digits[0]);
		HUD_DrawImage(x, y, digits[n % 10]);
	}

	// draw a minus sign if necessary
	if (neg)
	{
		x -= IM_WIDTH(wiminus);
		HUD_DrawImage(x, y, wiminus);
	}

	return x;
}

static void DrawPercent(float x, float y, int p)
{
	if (p < 0)
		return;

	HUD_DrawImage(x, y, percent);

	DrawNum(x, y, p, -1);
}

//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
static void DrawTime(float x, float y, int t)
{
	int div;
	int n;

	if (t < 0)
		return;

	if (t <= 61 * 59)
	{
		div = 1;

		do
		{
			n = (t / div) % 60;
			x = DrawNum(x, y, n, 2) - IM_WIDTH(colon);
			div *= 60;

			// draw
			if (div == 60 || t / div)
				HUD_DrawImage(x, y, colon);
		}
		while (t / div);
	}
	else
	{
		// "sucks"
		HUD_DrawImage(x - IM_WIDTH(sucks), y, sucks);
	}
}

static void WI_End(void)
{
	E_ForceWipe();

	background_camera_mo = NULL;

	F_StartFinale(&currmap->f_end, nextmap ? ga_finale : ga_nothing);
}

static void InitNoState(void)
{
	state = NoState;
	acceleratestage = false;
	cnt = 10;
}

static void UpdateNoState(void)
{
	cnt--;

	if (cnt == 0)
	{
		WI_End();
	}
}

static void InitShowNextLoc(void)
{
	int i;

	state = ShowNextLoc;
	acceleratestage = false;

	for (i = 0; i < worldint.nummappos; i++)
	{
		if (!strcmp(worldint.mappos[i].info->name, wi_stats.cur->name))
			worldint.mappos[i].done = true;
	}

	cnt = SHOWNEXTLOCDELAY * TICRATE;
}

static void UpdateShowNextLoc(void)
{
	if (!--cnt || acceleratestage)
		InitNoState();
	else
		snl_pointeron = (cnt & 31) < 20;
}

static void DrawShowNextLoc(void)
{
	int i;

	for (i = 0; i < worldint.nummappos; i++)
	{
		if (worldint.mappos[i].done)
			DrawOnLnode(&worldint.mappos[i], splat);

		if (wi_stats.next)
			if (snl_pointeron && !strcmp(wi_stats.next->name, worldint.mappos[i].info->name))
				DrawOnLnode(&worldint.mappos[i], yah);
	}

	if (wi_stats.next)
		DrawEnteringLevel();
}

static void DrawNoState(void)
{
	snl_pointeron = true;
	DrawShowNextLoc();
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
			if (score[i] < score[i+1])
			{
				int tmp = score[i];
				score[i] = score[i + 1];
				score[i + 1] = tmp;

				tmp = rank[i];
				rank[i] = rank[i + 1];
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
		return players[pl]->totalfrags * 2 + players[pl]->frags;
	}

	return -999;
}

static void InitDeathmatchStats(void)
{
	SYS_ASSERT(NUM_SHOWN <= MAXPLAYERS);

	state = StatCount;
	acceleratestage = false;
	dm_state = 1;

	cnt_pause = TICRATE;

	int rank[MAXPLAYERS];
	int score[MAXPLAYERS];

	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		rank[i] = players[i] ? i : -1;
		score[i] = DeathmatchScore(rank[i]);
	}

	SortRanks(rank, score);

	for (i = 0; i < NUM_SHOWN; i++)
	{
		dm_frags[i] = dm_totals[i] = 0;
		dm_rank[i] = rank[i];
	}
}

static void UpdateDeathmatchStats(void)
{
	bool stillticking;

	const gamedef_c *gd = wi_stats.cur->episode;

	if (acceleratestage && dm_state != 4)
	{
		acceleratestage = false;

		for (int i = 0; i < NUM_SHOWN; i++)
		{
			int p = dm_rank[i];
	
			if (p < 0)
				break;

			dm_frags[i] = players[p]->frags;
			dm_totals[i] = players[p]->totalfrags;
		}

		S_StartFX(gd->done);
		dm_state = 4;
	}

	switch (dm_state)
	{
		case 2:
			if (!(bcnt & 3))
				S_StartFX(gd->percent);

			stillticking = false;
			for (int i = 0; i < NUM_SHOWN; i++)
			{
				int p = dm_rank[i];

				if (p < 0)
					break;

				if (dm_frags[i] < players[p]->frags)
				{
					dm_frags[i]++;
					stillticking = true;
				}
				if (dm_totals[i] < players[p]->totalfrags)
				{
					dm_totals[i]++;
					stillticking = true;
				}
			}

			if (!stillticking)
			{
				S_StartFX(gd->done);
				dm_state++;
			}
			break;

		case 4:
			if (acceleratestage)
			{
				S_StartFX(gd->accel_snd);

				// Skip next loc on no map -ACB- 2004/06/27
				if (!worldint.nummappos || !wi_stats.next)	
					InitNoState();
				else
					InitShowNextLoc();
			}
			break;

		default:
			if (!--cnt_pause)
			{
				dm_state++;
				cnt_pause = TICRATE;
			}
			break;
	}
}

static void DrawDeathmatchStats(void)
{
	DrawLevelFinished();

	int t_type = styledef_c::T_TITLE;
	int y = 40;

	HL_WriteText(wi_net_style, t_type,  20, y, "Player");
	HL_WriteText(wi_net_style, t_type, 100, y, "Frags");
	HL_WriteText(wi_net_style, t_type, 200, y, "Total");

	for (int i = 0; i < NUM_SHOWN; i++)
	{
		int p = dm_rank[i];

		if (p < 0)
			break;

		y += 12;

		t_type = styledef_c::T_TEXT;

		// hightlight the console player
#if 1
		if (p == consoleplayer)
			t_type = styledef_c::T_ALT;
#else
		if (p == consoleplayer && ((bcnt & 31) < 16))
			continue;
#endif

		char temp[40];

		sprintf(temp, "%s", players[p]->playername);
		HL_WriteText(wi_net_style, t_type, 20, y, temp);

		sprintf(temp, "%5d", dm_frags[i]);
		HL_WriteText(wi_net_style, t_type, 100, y, temp);

		sprintf(temp, "%11d", dm_totals[i]);
		HL_WriteText(wi_net_style, t_type, 200, y, temp);
	}
}

// Calculates value of this player for ranking
static int CoopScore(int pl)
{
	if (pl >= 0)
	{
		int kills = players[pl]->killcount * 400 / wi_stats.kills;
		int items = players[pl]->itemcount * 100 / wi_stats.items;
		int secret = players[pl]->secretcount * 200 / wi_stats.secret;
		int frags = (players[pl]->frags + players[pl]->totalfrags) * 25;

		return kills + items + secret - frags;
	}

	return -999;
}

static void InitCoopStats(void)
{
	SYS_ASSERT(NUM_SHOWN <= MAXPLAYERS);

	state = StatCount;
	acceleratestage = false;
	ng_state = 1;

	cnt_pause = TICRATE;

	int rank[MAXPLAYERS];
	int score[MAXPLAYERS];

	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		rank[i] = players[i] ? i : -1;
		score[i] = CoopScore(rank[i]);
	}

	SortRanks(rank, score);

	dofrags = 0;

	for (i = 0; i < NUM_SHOWN; i++)
	{
		dm_rank[i] = rank[i];

		if (dm_rank[i] < 0)
			continue;

		cnt_kills[i] = cnt_items[i] = cnt_secrets[i] = cnt_frags[i] = cnt_totals[i] = 0;

		dofrags += players[dm_rank[i]]->frags + players[dm_rank[i]]->totalfrags;
	}
}

static void UpdateCoopStats(void)
{
	bool stillticking;

	const gamedef_c *gd = wi_stats.cur->episode;

	if (acceleratestage && ng_state != 10)
	{
		acceleratestage = false;

		for (int i = 0; i < NUM_SHOWN; i++)
		{
			int p = dm_rank[i];

			if (p < 0)
				break;

			cnt_kills[i] = (players[p]->killcount * 100) / wi_stats.kills;
			cnt_items[i] = (players[p]->itemcount * 100) / wi_stats.items;
			cnt_secrets[i] = (players[p]->secretcount * 100) / wi_stats.secret;

			if (dofrags)
			{
				cnt_frags[i] = players[p]->frags;
				cnt_totals[i] = players[p]->totalfrags;
			}
		}

		S_StartFX(gd->done);
		ng_state = 10;
	}

	switch (ng_state)
	{
		case 2:
			if (!(bcnt & 3))
				S_StartFX(gd->percent);

			stillticking = false;

			for (int i = 0; i < NUM_SHOWN; i++)
			{
				int p = dm_rank[i];

				if (p < 0)
					break;

				cnt_kills[i] += 2;

				if (cnt_kills[i] >= (players[p]->killcount * 100) / wi_stats.kills)
					cnt_kills[i] = (players[p]->killcount * 100) / wi_stats.kills;
				else
					stillticking = true;
			}

			if (!stillticking)
			{
				S_StartFX(gd->done);
				ng_state++;
			}
			break;

		case 4:
			if (!(bcnt & 3))
				S_StartFX(gd->percent);

			stillticking = false;

			for (int i = 0; i < NUM_SHOWN; i++)
			{
				int p = dm_rank[i];

				if (p < 0)
					break;

				cnt_items[i] += 2;
				if (cnt_items[i] >= (players[p]->itemcount * 100) / wi_stats.items)
					cnt_items[i] = (players[p]->itemcount * 100) / wi_stats.items;
				else
					stillticking = true;
			}

			if (!stillticking)
			{
				S_StartFX(gd->done);
				ng_state++;
			}
			break;

		case 6:
			if (!(bcnt & 3))
				S_StartFX(gd->percent);

			stillticking = false;

			for (int i = 0; i < NUM_SHOWN; i++)
			{
				int p = dm_rank[i];

				if (p < 0)
					break;

				cnt_secrets[i] += 2;

				if (cnt_secrets[i] >= (players[p]->secretcount * 100) / wi_stats.secret)
					cnt_secrets[i] = (players[p]->secretcount * 100) / wi_stats.secret;
				else
					stillticking = true;
			}

			if (!stillticking)
			{
				S_StartFX(gd->done);
				ng_state += 1 + 2 * !dofrags;
			}
			break;

		case 8:
			if (!(bcnt & 3))
				S_StartFX(gd->percent);

			stillticking = false;

			for (int i = 0; i < NUM_SHOWN; i++)
			{
				int p = dm_rank[i];

				if (p < 0)
					break;

				cnt_frags[i]++;
				cnt_totals[i]++;

				if (cnt_frags[i] >= players[p]->frags)
					cnt_frags[i] = players[p]->frags;
				else if (cnt_totals[i] >= players[p]->totalfrags)
					cnt_totals[i] = players[p]->totalfrags;
				else
					stillticking = true;
			}

			if (!stillticking)
			{
				S_StartFX(gd->frag_snd);
				ng_state++;
			}
			break;

		case 10:
			if (acceleratestage)
			{
				S_StartFX(gd->nextmap);

				// Skip next loc on no map -ACB- 2004/06/27
				if (!worldint.nummappos || !wi_stats.next)
					InitNoState();
				else
					InitShowNextLoc();
			}

		default:
			if (!--cnt_pause)
			{
				ng_state++;
				cnt_pause = TICRATE;
			}
	}
}

static void DrawCoopStats(void)
{
	DrawLevelFinished();

	int t_type = styledef_c::T_TITLE;
	int y = 40;

	// FIXME: better alignment

	HL_WriteText(wi_net_style, t_type,   6, y, "Player");
	HL_WriteText(wi_net_style, t_type,  56, y, "Kills");
	HL_WriteText(wi_net_style, t_type,  98, y, "Items");
	HL_WriteText(wi_net_style, t_type, 142, y, "Secret");

	if (dofrags)
	{
		HL_WriteText(wi_net_style, t_type, 190, y, "Frags");
		HL_WriteText(wi_net_style, t_type, 232, y, "Total");
	}

	for (int i = 0; i < NUM_SHOWN; i++)
	{
		int p = dm_rank[i];

		if (p < 0)
			break;

		y += 12;

		t_type = styledef_c::T_TEXT;

		// hightlight the console player
#if 1
		if (p == consoleplayer)
			t_type = styledef_c::T_ALT;
#else
		if (p == consoleplayer && ((bcnt & 31) < 16))
			continue;
#endif

		char temp[40];

		sprintf(temp, "%s", players[p]->playername);
		HL_WriteText(wi_net_style, t_type, 6, y, temp);

		sprintf(temp, "%3d%%", cnt_kills[i]);
		HL_WriteText(wi_net_style, t_type, 64, y, temp);

		sprintf(temp, "%3d%%", cnt_items[i]);
		HL_WriteText(wi_net_style, t_type, 106, y, temp);

		sprintf(temp, "%3d%%", cnt_secrets[i]);
		HL_WriteText(wi_net_style, t_type, 158, y, temp);

		if (dofrags)
		{
			sprintf(temp, "%5d", cnt_frags[i]);
			HL_WriteText(wi_net_style, t_type, 190, y, temp);

			sprintf(temp, "%11d", cnt_totals[i]);
			HL_WriteText(wi_net_style, t_type, 232, y, temp);
		}
	}
}

typedef enum
{
	sp_paused = 1,
	sp_kills = 2,
	sp_items = 4,
	sp_scrt = 6,
	sp_time = 8,
	sp_end = 10
}
sp_state_e;

static void InitSinglePlayerStats(void)
{
	state = StatCount;
	acceleratestage = false;
	sp_state = sp_paused;
	cnt_kills[0] = cnt_items[0] = cnt_secrets[0] = -1;
	cnt_time = cnt_par = -1;
	cnt_pause = TICRATE;

	//WI_initAnimatedBack()
}

static void UpdateSinglePlayerStats(void)
{
	//WI_updateAnimatedBack();

	player_t *con_plyr = players[consoleplayer];

	const gamedef_c *gd = wi_stats.cur->episode;

	if (acceleratestage && sp_state != sp_end)
	{
		acceleratestage = false;
		cnt_kills[0] = (con_plyr->killcount * 100) / wi_stats.kills;
		cnt_items[0] = (con_plyr->itemcount * 100) / wi_stats.items;
		cnt_secrets[0] = (con_plyr->secretcount * 100) / wi_stats.secret;
		cnt_time = con_plyr->leveltime / TICRATE;
		cnt_par = wi_stats.partime / TICRATE;
		S_StartFX(gd->done);
		sp_state = sp_end;
	}

	if (sp_state == sp_kills)
	{
		cnt_kills[0] += 2;

		if (!(bcnt & 3))
			S_StartFX(gd->percent);

		if (cnt_kills[0] >= (con_plyr->killcount * 100) / wi_stats.kills)
		{
			cnt_kills[0] = (con_plyr->killcount * 100) / wi_stats.kills;
			S_StartFX(gd->done);
			sp_state++;
		}
	}
	else if (sp_state == sp_items)
	{
		cnt_items[0] += 2;

		if (!(bcnt & 3))
			S_StartFX(gd->percent);

		if (cnt_items[0] >= (con_plyr->itemcount * 100) / wi_stats.items)
		{
			cnt_items[0] = (con_plyr->itemcount * 100) / wi_stats.items;
			S_StartFX(gd->done);
			sp_state++;
		}
	}
	else if (sp_state == sp_scrt)
	{
		cnt_secrets[0] += 2;

		if (!(bcnt & 3))
			S_StartFX(gd->percent);

		if (cnt_secrets[0] >= (con_plyr->secretcount * 100) / wi_stats.secret)
		{
			cnt_secrets[0] = (con_plyr->secretcount * 100) / wi_stats.secret;
			S_StartFX(gd->done);
			sp_state++;
		}
	}

	else if (sp_state == sp_time)
	{
		if (!(bcnt & 3))
			S_StartFX(gd->percent);

		cnt_time += 3;

		if (cnt_time >= con_plyr->leveltime / TICRATE)
			cnt_time = con_plyr->leveltime / TICRATE;

		cnt_par += 3;

		if (cnt_par >= wi_stats.partime / TICRATE)
		{
			cnt_par = wi_stats.partime / TICRATE;

			if (cnt_time >= con_plyr->leveltime / TICRATE)
			{
				S_StartFX(gd->done);
				sp_state++;
			}
		}
	}
	else if (sp_state == sp_end)
	{
		if (acceleratestage)
		{
			S_StartFX(gd->nextmap);

			// Skip next loc on no map -ACB- 2004/06/27
			if (!worldint.nummappos || !wi_stats.next)
				InitNoState();
			else
				InitShowNextLoc();
		}
	}
	else if (sp_state & sp_paused)
	{
		if (!--cnt_pause)
		{
			sp_state++;
			cnt_pause = TICRATE;
		}
	}
}


static void DrawSinglePlayerStats(void)
{
	// line height
	float lh = IM_HEIGHT(digits[0]) * 3/2;

	// draw animated background
	//WI_drawAnimatedBack();

	DrawLevelFinished();

	HUD_DrawImage(SP_STATSX, SP_STATSY, kills);
	DrawPercent(320 - SP_STATSX, SP_STATSY, cnt_kills[0]);

	HUD_DrawImage(SP_STATSX, SP_STATSY + lh, items);
	DrawPercent(320 - SP_STATSX, SP_STATSY + lh, cnt_items[0]);

	HUD_DrawImage(SP_STATSX, SP_STATSY + 2 * lh, sp_secret);
	DrawPercent(320 - SP_STATSX, SP_STATSY + 2 * lh, cnt_secrets[0]);

	HUD_DrawImage(SP_TIMEX, SP_TIMEY, time_image);
	DrawTime(160 - SP_TIMEX, SP_TIMEY, cnt_time);

	// -KM- 1998/11/25 Removed episode check. Replaced with partime check
	if (wi_stats.partime)
	{
		HUD_DrawImage(160 + SP_TIMEX, SP_TIMEY, par);
		DrawTime(320 - SP_TIMEX, SP_TIMEY, cnt_par);
	}
}


bool WI_CheckForAccelerate(void)
{
	bool do_accel = false;

	// check for button presses to skip delays
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *player = players[pnum];
		if (! player) continue;

		if (player->cmd.buttons & BT_ATTACK)
		{
			if (!player->attackdown[0])
			{
				player->attackdown[0] = true;
				do_accel = true;
			}
		}
		else
			player->attackdown[0] = false;

		if (player->cmd.buttons & BT_USE)
		{
			if (!player->usedown)
			{
				player->usedown = true;
				do_accel = true;
			}
		}
		else
			player->usedown = false;
	}

	return do_accel;
}


void WI_Ticker(void)
{
	// Updates stuff each tick

	SYS_ASSERT(gamestate == GS_INTERMISSION);

	int i;

	// counter for general background animation
	bcnt++;

	if (bcnt == 1)
	{
		// intermission music
		S_ChangeMusic(wi_stats.cur->episode->music, true);
	}

	if (WI_CheckForAccelerate())
		acceleratestage = true;

	for (i = 0; i < worldint.numanims; i++)
	{
		if (worldint.anims[i].count >= 0)
		{
			if (!worldint.anims[i].count)
			{
				worldint.anims[i].frameon
					= (worldint.anims[i].frameon + 1) % worldint.anims[i].numframes;
				worldint.anims[i].count
					= worldint.anims[i].frames[worldint.anims[i].frameon].info->tics;
			}
			worldint.anims[i].count--;
		}
	}

	switch (state)
	{
		case StatCount:
			if (SP_MATCH())
				UpdateSinglePlayerStats();
			else if (DEATHMATCH())
				UpdateDeathmatchStats();
			else
				UpdateCoopStats();
			break;

		case ShowNextLoc:
			UpdateShowNextLoc();
			break;

		case NoState:
			UpdateNoState();
			break;
	}
}


void WI_Drawer(void)
{
	SYS_ASSERT(gamestate == GS_INTERMISSION);

	HUD_Reset();

	if (background_camera_mo)
	{
		HUD_RenderWorld(0, 0, 320, 200, background_camera_mo);
	} 
	else
	{
		//HUD_StretchImage(0, 0, 320, 200, bg_image);
		HUD_DrawImageTitleWS(bg_image); //Lobo: Widescreen support
		
		for (int i = 0; i < worldint.numanims; i++)
		{
			wi_anim_c *a = &worldint.anims[i];

			if (a->frameon == -1)
				continue;

			wi_frame_c *f = NULL;

			if (a->info->type == wi_animdef_c::WI_LEVEL)
			{
				if (!wi_stats.next)
					f = NULL;
				else if (!strcmp(wi_stats.next->name, a->info->level))
					f = &a->frames[a->frameon];
			}
			else
				f = &a->frames[a->frameon];

			if (f)
				HUD_DrawImage(f->info->x, f->info->y, f->image);
		}
	}

	switch (state)
	{
		case StatCount:
			if (SP_MATCH())
				DrawSinglePlayerStats();
			else if (DEATHMATCH())
				DrawDeathmatchStats();
			else
				DrawCoopStats();
			break;

		case ShowNextLoc:
			DrawShowNextLoc();
			break;

		case NoState:
			DrawNoState();
			break;
	}
}


static void LoadData(void)
{
	int i, j;

	// find styles
	if (! wi_sp_style)
	{
		styledef_c *def = styledefs.Lookup("STATS");
		if (! def) def = default_style;
		wi_sp_style = hu_styles.Lookup(def);
	}

	if (! wi_net_style)
	{
		styledef_c *def = styledefs.Lookup("NET STATS");
		if (! def) def = default_style;
		wi_net_style = hu_styles.Lookup(def);
	}

	const gamedef_c *gd = wi_stats.cur->episode;

	// background
	bg_image = W_ImageLookup(gd->background);

	lnames[0] = W_ImageLookup(wi_stats.cur->namegraphic);

	if (wi_stats.next)
		lnames[1] = W_ImageLookup(wi_stats.next->namegraphic);

	if (gd->yah[0][0])
		yah[0] = W_ImageLookup(gd->yah[0]);
	if (gd->yah[1][0])
		yah[1] = W_ImageLookup(gd->yah[1]);
	if (gd->splatpic[0])
		splat[0] = W_ImageLookup(gd->splatpic);
	
	wiminus = W_ImageLookup("WIMINUS"); //!!! FIXME: use the style!
	percent = W_ImageLookup("WIPCNT");
	colon = W_ImageLookup("WICOLON");

	finished = W_ImageLookup("WIF");
	entering = W_ImageLookup("WIENTER");
	kills = W_ImageLookup("WIOSTK");
	secret = W_ImageLookup("WIOSTS");  // "scrt"

	sp_secret = W_ImageLookup("WISCRT2");  // "secret"

	items = W_ImageLookup("WIOSTI");
	frags = W_ImageLookup("WIFRGS");
	time_image = W_ImageLookup("WITIME");
	sucks = W_ImageLookup("WISUCKS");
	par = W_ImageLookup("WIPAR");
	killers = W_ImageLookup("WIKILRS");  // "killers" (vertical)

	victims = W_ImageLookup("WIVCTMS");  // "victims" (horiz)

	total = W_ImageLookup("WIMSTT");
	star = W_ImageLookup("STFST01");  // your face

	bstar = W_ImageLookup("STFDEAD0");  // dead face

	for (i = 0; i < 10; i++)
	{
		// numbers 0-9
		char name[64];
		sprintf(name, "WINUM%d", i);
		digits[i] = W_ImageLookup(name);
	}

	for (i = 0; i < worldint.numanims; i++)
	{
		for (j = 0; j < worldint.anims[i].numframes; j++)
		{
			// FIXME!!! Shorten :)
			L_WriteDebug("WI_LoadData: '%s'\n", 
				worldint.anims[i].frames[j].info->pic.c_str());

			worldint.anims[i].frames[j].image = 
				W_ImageLookup(worldint.anims[i].frames[j].info->pic);
		}
	}
}

static void InitVariables(void)
{
	wi_stats.level   = wi_stats.cur->name.c_str();
	wi_stats.partime = wi_stats.cur->partime;

	acceleratestage = false;
	cnt = bcnt = 0;
	firstrefresh = 1;

	if (wi_stats.kills <= 0)
		wi_stats.kills = 1;

	if (wi_stats.items <= 0)
		wi_stats.items = 1;

	if (wi_stats.secret <= 0)
		wi_stats.secret = 1;

	gamedef_c *def = wi_stats.cur->episode;

	SYS_ASSERT(def);

	worldint.Init(def);

	LoadData();
}


void WI_Start(void)
{
	InitVariables();

	const gamedef_c *gd = wi_stats.cur->episode;
	SYS_ASSERT(gd);

	if (SP_MATCH())
		InitSinglePlayerStats();
	else if (DEATHMATCH())
		InitDeathmatchStats();
	else
		InitCoopStats();

	// -AJA- 1999/10/22: background cameras.
	background_camera_mo = NULL;

	if (gd->bg_camera.c_str() && gd->bg_camera.c_str()[0])
	{
		for (mobj_t *mo = mobjlisthead; mo != NULL; mo = mo->next)
		{
			if (DDF_CompareName(mo->info->name, gd->bg_camera.c_str()) != 0)
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
