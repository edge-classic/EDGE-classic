//----------------------------------------------------------------------------
//  EDGE Automap Functions
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2010  The EDGE Team.
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

#include "i_defs.h"
#include "i_defs_gl.h"

#include "con_main.h"
#include "e_input.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "n_network.h"
#include "p_local.h"
#include "am_map.h"
#include "r_draw.h"
#include "r_colormap.h"
#include "r_modes.h"

#include <stdio.h>
#include <float.h>
#include <math.h>

#define DEBUG_TRUEBSP  0
#define DEBUG_COLLIDE  0

// Automap colors

// NOTE: this order must match the one in the COAL API script
static rgbcol_t am_colors[AM_NUM_COLORS] =
{
	RGB_MAKE( 40, 40,112),  // AMCOL_Grid
    RGB_MAKE(112,112,112),  // AMCOL_Allmap
    RGB_MAKE(255,  0,  0),  // AMCOL_Wall
    RGB_MAKE(192,128, 80),  // AMCOL_Step
    RGB_MAKE(192,128, 80),  // AMCOL_Ledge
    RGB_MAKE(220,220,  0),  // AMCOL_Ceil
    RGB_MAKE(  0,200,200),  // AMCOL_Secret

    RGB_MAKE(255,255,255),  // AMCOL_Player
    RGB_MAKE(  0,255,  0),  // AMCOL_Monster
    RGB_MAKE(220,  0,  0),  // AMCOL_Corpse
    RGB_MAKE(  0,  0,255),  // AMCOL_Item
    RGB_MAKE(255,188,  0),  // AMCOL_Missile
    RGB_MAKE(120, 60, 30)   // AMCOL_Scenery
};


// Automap keys
// Ideally these would be configurable...

int key_am_up;
int key_am_down;
int key_am_left;
int key_am_right;

int key_am_zoomin;
int key_am_zoomout;

int key_am_follow;
int key_am_grid;
int key_am_mark;
int key_am_clear;

#define AM_NUMMARKPOINTS  9

//
// NOTE:
//   `F' in the names here means `Framebuffer', i.e. on-screen coords.
//   `M' in the names means `Map', i.e. coordinates in the level.
//

// scale on entry
#define INIT_MSCALE (4.0f)
#define  MAX_MSCALE (200.0f)

// how much the automap moves window per tic in frame-buffer coordinates
// moves a whole screen-width in 1.5 seconds
#define F_PANINC 6.1

// how much zoom-in per tic
// goes to 3x in 1 second
#define M_ZOOMIN  1.03f

// how much zoom-in for each mouse-wheel click
// goes to 3x in 4 clicks
#define WHEEL_ZOOMIN  1.32f


bool automapactive = false;

cvar_c am_smoothing;
cvar_c am_gridsize;

static int cheating = 0;
static int grid = 0;

static bool show_things = false;
static bool show_walls  = false;
static bool show_allmap = false;


// location and size of window on screen
static float f_x, f_y;
static float f_w, f_h;

// scale value which makes the whole map fit into the on-screen area
// (multiplying map coords by this value).
static float f_scale;

static mobj_t *f_focus;


// location on map which the map is centred on
static float m_cx, m_cy;

// relative scaling: 1.0 = map fits the on-screen area,
//                   2.0 = map is twice as big
//                   8.0 = map is eight times as big
static float m_scale;


// translates between frame-buffer and map distances
#define MTOF(xx) (  (int)((xx) * m_scale * f_scale))
#define FTOM(xx) ((float)((xx) / m_scale / f_scale))

// translates between frame-buffer and map coordinates
#define CXMTOF(xx)  (f_x + f_w*0.5 + MTOF((xx) - m_cx))
#define CYMTOF(yy)  (f_y + f_h*0.5 - MTOF((yy) - m_cy))


// largest size of map along X or Y axis
static float map_size;

static float map_min_x;
static float map_min_y;
static float map_max_x;
static float map_max_y;


// how far the window pans each tic (map coords)
static float panning_x = 0;
static float panning_y = 0;

// how far the window zooms in each tic (map coords)
static float zooming = -1;


// where the points are
static mpoint_t markpoints[AM_NUMMARKPOINTS];

#define NO_MARK_X  (-777)

// next point to be assigned
static int markpointnum = 0;

// specifies whether to follow the player around
static bool followplayer = true;


cheatseq_t cheat_amap = {0, 0};

static bool stopped = true;

bool rotatemap = false;

extern style_c *automap_style;  // FIXME: put in header


//
// adds a marker at the current location
//
static void AddMark(void)
{
	markpoints[markpointnum].x = m_cx;
	markpoints[markpointnum].y = m_cy;

	markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
static void FindMinMaxBoundaries(void)
{
	map_min_x = +9e9;
	map_min_y = +9e9;

	map_max_x = -9e9;
	map_max_y = -9e9;

	for (int i = 0; i < numvertexes; i++)
	{
		map_min_x = MIN(map_min_x, vertexes[i].x);
		map_max_x = MAX(map_max_x, vertexes[i].x);

		map_min_y = MIN(map_min_y, vertexes[i].y);
		map_max_y = MAX(map_max_y, vertexes[i].y);
	}

	float map_w = map_max_x - map_min_x;
	float map_h = map_max_y - map_min_y;

	map_size = MAX(map_w, map_h);

	m_cx = (map_min_x + map_max_x) / 2.0;
	m_cy = (map_min_y + map_max_y) / 2.0;
}


static void ClearMarks(void)
{
	for (int i = 0; i < AM_NUMMARKPOINTS; i++)
		markpoints[i].x = NO_MARK_X;

	markpointnum = 0;
}


void AM_InitLevel(void)
{
	if (!cheat_amap.sequence)
	{
		cheat_amap.sequence = language["iddt"];
	}

	ClearMarks();

	FindMinMaxBoundaries();

	m_scale = INIT_MSCALE;
}


void AM_Stop(void)
{
	automapactive = false;
	stopped = true;

	panning_x = 0;
	panning_y = 0;
	zooming   = -1;
}

static void AM_Hide(void)
{
	automapactive = false;

	panning_x = 0;
	panning_y = 0;
	zooming   = -1;
}

static void AM_Show(void)
{
	automapactive = true;

	if (! stopped)
	///	AM_Stop();
		return;

	AM_InitLevel();

	stopped  = false;

	panning_x = 0;
	panning_y = 0;
	zooming   = -1;
}


//
// Zooming
//
static void ChangeWindowScale(float factor)
{
	m_scale *= factor;

	m_scale = MAX(m_scale, 1.0);
	m_scale = MIN(m_scale, MAX_MSCALE);
}


//
// Handle events (user inputs) in automap mode
//
bool AM_Responder(event_t * ev)
{
	int sym = ev->value.key.sym;

	// check the enable/disable key
	if (ev->type == ev_keydown && E_MatchesKey(key_map, sym))
	{
		if (automapactive)
			AM_Hide();
		else
			AM_Show();
		return true;
	}

	if (! automapactive)
		return false;

    // --- handle key releases ---

	if (ev->type == ev_keyup)
	{
		if (E_MatchesKey(key_am_left, sym) || E_MatchesKey(key_am_right, sym))
			panning_x = 0;

		if (E_MatchesKey(key_am_up, sym) || E_MatchesKey(key_am_down, sym))
			panning_y = 0;

		if (E_MatchesKey(key_am_zoomin, sym) || E_MatchesKey(key_am_zoomout, sym))
			zooming = -1;

		return false;
	}

    // --- handle key presses ---

	if (ev->type != ev_keydown)
		return false;

	if (! followplayer)
	{
		if (E_MatchesKey(key_am_left, sym))
		{
			panning_x = -FTOM(F_PANINC);
			return true;
		}
		else if (E_MatchesKey(key_am_right, sym))
		{
			panning_x = FTOM(F_PANINC);
			return true;
		}
		else if (E_MatchesKey(key_am_up, sym))
		{
			panning_y = FTOM(F_PANINC);
			return true;
		}
		else if (E_MatchesKey(key_am_down, sym))
		{
			panning_y = -FTOM(F_PANINC);
			return true;
		}
	}

	if (E_MatchesKey(key_am_zoomin, sym))
	{
		zooming = M_ZOOMIN;
		return true;
	}
	else if (E_MatchesKey(key_am_zoomout, sym))
	{
		zooming = 1.0 / M_ZOOMIN;
		return true;
	}

	if (E_MatchesKey(key_am_follow, sym))
	{
		followplayer = !followplayer;

		// -ACB- 1998/08/10 Use DDF Lang Reference
		if (followplayer)
			CON_PlayerMessageLDF(consoleplayer, "AutoMapFollowOn");
		else
			CON_PlayerMessageLDF(consoleplayer, "AutoMapFollowOff");

		return true;
	}

	if (E_MatchesKey(key_am_grid, sym))
	{
		grid = !grid;
		// -ACB- 1998/08/10 Use DDF Lang Reference
		if (grid)
			CON_PlayerMessageLDF(consoleplayer, "AutoMapGridOn");
		else
			CON_PlayerMessageLDF(consoleplayer, "AutoMapGridOff");

		return true;
	}

	if (E_MatchesKey(key_am_mark, sym))
	{
		// -ACB- 1998/08/10 Use DDF Lang Reference
		CON_PlayerMessage(consoleplayer, "%s %d",
			language["AutoMapMarkedSpot"], markpointnum);
		AddMark();
		return true;
	}

	if (E_MatchesKey(key_am_clear, sym))
	{
		// -ACB- 1998/08/10 Use DDF Lang Reference
		CON_PlayerMessageLDF(consoleplayer, "AutoMapMarksClear");
		ClearMarks();
		return true;
	}

	// -AJA- 2007/04/18: mouse-wheel support
	if (sym == KEYD_WHEEL_DN)
	{
		ChangeWindowScale(1.0 / WHEEL_ZOOMIN);
		return true;
	}
	else if (sym == KEYD_WHEEL_UP)
	{
		ChangeWindowScale(WHEEL_ZOOMIN);
		return true;
	}

	// -ACB- 1999/09/28 Proper casting
	if (!DEATHMATCH() && M_CheckCheat(&cheat_amap, (char)sym))
	{
		cheating = (cheating + 1) % 3;

		show_things = (cheating == 2) ? true : false;
		show_walls  = (cheating >= 1) ? true : false;
	}

	return false;
}


//
// Updates on game tick
//
void AM_Ticker(void)
{
	if (! automapactive)
		return;

	// Change x,y location
	if (! followplayer)
	{
		m_cx += panning_x;
		m_cy += panning_y;

		// limit position, don't go outside of the map
		m_cx = MIN(m_cx, map_max_x);
		m_cx = MAX(m_cx, map_min_x);

		m_cy = MIN(m_cy, map_max_y);
		m_cy = MAX(m_cy, map_min_y);
	}

	// Change the zoom if necessary
	if (zooming > 0)
		ChangeWindowScale(zooming);
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
static inline void Rotate(float * x, float * y, angle_t a)
{
	float new_x = *x * M_Cos(a) - *y * M_Sin(a);
	float new_y = *x * M_Sin(a) + *y * M_Cos(a);

	*x = new_x;
	*y = new_y;
}

static inline void GetRotatedCoords(float sx, float sy, float *dx, float *dy)
{
	*dx = sx;
	*dy = sy;

	if (rotatemap)
	{
		// rotate coordinates so they are on the map correctly
		*dx -= f_focus->x;
		*dy -= f_focus->y;

		Rotate(dx, dy, ANG90 - f_focus->angle);

		*dx += f_focus->x;
		*dy += f_focus->y;
	}
}

static inline angle_t GetRotatedAngle(angle_t src)
{
	if (rotatemap)
		return src + ANG90 - f_focus->angle;

	return src;
}


//
// Draw visible parts of lines.
//
static void DrawMLine(mline_t * ml, rgbcol_t rgb, bool thick = true)
{
	if (! am_smoothing.d)
		thick = false;

	float x1 = f_x + f_w*0.5 + MTOF(ml->a.x);
	float y1 = f_y + f_h*0.5 - MTOF(ml->a.y);

	float x2 = f_x + f_w*0.5 + MTOF(ml->b.x);
	float y2 = f_y + f_h*0.5 - MTOF(ml->b.y);

	float dx = MTOF(- m_cx);
	float dy = MTOF(- m_cy);

	HUD_SolidLine(x1, y1, x2, y2, rgb, thick, thick, dx, dy);
}


//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
static void DrawGrid()
{
	mline_t ml;

	int grid_size = MAX(4, am_gridsize.d);

	int mx0 = int(m_cx);
	int my0 = int(m_cy);

	if (mx0 < 0) mx0 -= -(-mx0 % grid_size); else mx0 -= mx0 % grid_size;
	if (my0 < 0) my0 -= -(-my0 % grid_size); else my0 -= my0 % grid_size;

	for (int j = 1; j < 1024; j++)
	{
		int jx = ((j & ~1) >> 1);

		// stop when both lines are off the screen
		float x1 = CXMTOF(mx0 - jx * grid_size);
		float x2 = CXMTOF(mx0 + jx * grid_size);

		if (x1 < f_x && x2 >= f_x + f_w)
			break;

		ml.a.x = mx0 + jx * ((j & 1) ? -grid_size : grid_size);
		ml.b.x = ml.a.x;

		ml.a.y = -40000;
		ml.b.y = +40000;

		DrawMLine(&ml, am_colors[AMCOL_Grid], false);
	}

	for (int k = 1; k < 1024; k++)
	{
		int ky = ((k & ~1) >> 1);

		// stop when both lines are off the screen
		float y1 = CYMTOF(my0 + ky * grid_size);
		float y2 = CYMTOF(my0 - ky * grid_size);

		if (y1 < f_y && y2 >= f_y + f_h)
			break;

		ml.a.x = -40000;
		ml.b.x = +40000;

		ml.a.y = my0 + ky * ((k & 1) ? -grid_size : grid_size);
		ml.b.y = ml.a.y;

		DrawMLine(&ml, am_colors[AMCOL_Grid], false);
	}
}


//
// Checks whether the two sectors' regions are similiar.  If they are
// different enough, a line will be drawn on the automap.
//
// -AJA- 1999/12/07: written.
//
static bool CheckSimiliarRegions(sector_t *front, sector_t *back)
{
	extrafloor_t *F, *B;

	if (front->tag == back->tag)
		return true;

	// Note: doesn't worry about liquids

	F = front->bottom_ef;
	B = back->bottom_ef;

	while (F && B)
	{
		if (F->top_h != B->top_h)
			return false;

		if (F->bottom_h != B->bottom_h)
			return false;

		F = F->higher;
		B = B->higher;
	}

	return (F || B) ? false : true;
}


//
// Determines visible lines, draws them.
//
// -AJA- This is now *lineseg* based, not linedef.
//
static void AM_WalkSeg(seg_t *seg)
{
	mline_t l;
	line_t *line;

	sector_t *front = seg->frontsector;
	sector_t *back  = seg->backsector;

	if (seg->miniseg)
	{
#if (DEBUG_TRUEBSP == 1)
		if (seg->partner && seg > seg->partner)
			return;

		GetRotatedCoords(seg->v1->x, seg->v1->y, &l.a.x, &l.a.y);
		GetRotatedCoords(seg->v2->x, seg->v2->y, &l.b.x, &l.b.y);

		DrawMLine(&l, RGB_MAKE(0,0,128), false);
#endif
		return;
	}

	line = seg->linedef;
	SYS_ASSERT(line);

	// only draw segs on the _right_ side of linedefs
	if (line->side[1] == seg->sidedef)
		return;

	GetRotatedCoords(seg->v1->x, seg->v1->y, &l.a.x, &l.a.y);
	GetRotatedCoords(seg->v2->x, seg->v2->y, &l.b.x, &l.b.y);

	if ((line->flags & MLF_Mapped) || show_walls)
	{
		if ((line->flags & MLF_DontDraw) && !show_walls)
			return;

		if (!front || !back)
		{
			DrawMLine(&l, am_colors[AMCOL_Wall]);
		}
		else
		{
			if (line->flags & MLF_Secret)
			{  
				// secret door
				if (show_walls)
					DrawMLine(&l, am_colors[AMCOL_Secret]);
				else
					DrawMLine(&l, am_colors[AMCOL_Wall]);
			}
			else if (back->f_h != front->f_h)
			{
				float diff = fabs(back->f_h - front->f_h);

				// floor level change
				if (diff > 24)
					DrawMLine(&l, am_colors[AMCOL_Ledge]);
				else
					DrawMLine(&l, am_colors[AMCOL_Step]);
			}
			else if (back->c_h != front->c_h)
			{
				// ceiling level change
				DrawMLine(&l, am_colors[AMCOL_Ceil]);
			}
			else if ((front->exfloor_used > 0 || back->exfloor_used > 0) &&
				(front->exfloor_used != back->exfloor_used ||
				! CheckSimiliarRegions(front, back)))
			{
				// -AJA- 1999/10/09: extra floor change.
				DrawMLine(&l, am_colors[AMCOL_Ledge]);
			}
			else if (show_walls)
			{
				DrawMLine(&l, am_colors[AMCOL_Allmap]);
			}
		}
	}
	else if (f_focus->player && (show_allmap || f_focus->player->powers[PW_AllMap] != 0))
	{
		if (! (line->flags & MLF_DontDraw))
			DrawMLine(&l, am_colors[AMCOL_Allmap]);
	}
}


static void DrawLineCharacter(mline_t *lineguy, int lineguylines, 
							  float radius, angle_t angle,
							  rgbcol_t rgb, float x, float y)
{
	float cx, cy;

	GetRotatedCoords(x, y, &cx, &cy);

	cx = CXMTOF(cx);
	cy = CYMTOF(cy);

	radius = MTOF(radius);

	if (radius < 2)
		radius = 2;

	angle = GetRotatedAngle(angle);

	for (int i = 0; i < lineguylines; i++)
	{
		float ax = lineguy[i].a.x * radius;
		float ay = lineguy[i].a.y * radius;

		if (angle)
			Rotate(&ax, &ay, angle);

		float bx = lineguy[i].b.x * radius;
		float by = lineguy[i].b.y * radius;

		if (angle)
			Rotate(&bx, &by, angle);

		HUD_SolidLine(cx+ax, cy-ay, cx+bx, cy-by, rgb);
	}
}


#if (DEBUG_COLLIDE == 1)
static void DrawObjectBounds(mobj_t *mo, rgbcol_t rgb)
{
	float R = mo->radius;

	if (R < 2)
		R = 2;

	float lx = mo->x - R;
	float ly = mo->y - R;
	float hx = mo->x + R;
	float hy = mo->y + R;

	mline_t ml;

	GetRotatedCoords(lx, ly, &ml.a.x, &ml.a.y);
	GetRotatedCoords(lx, hy, &ml.b.x, &ml.b.y);
	DrawMLine(&ml, rgb);

	GetRotatedCoords(lx, hy, &ml.a.x, &ml.a.y);
	GetRotatedCoords(hx, hy, &ml.b.x, &ml.b.y);
	DrawMLine(&ml, rgb);

	GetRotatedCoords(hx, hy, &ml.a.x, &ml.a.y);
	GetRotatedCoords(hx, ly, &ml.b.x, &ml.b.y);
	DrawMLine(&ml, rgb);

	GetRotatedCoords(hx, ly, &ml.a.x, &ml.a.y);
	GetRotatedCoords(lx, ly, &ml.b.x, &ml.b.y);
	DrawMLine(&ml, rgb);
}
#endif


static rgbcol_t player_colors[8] =
{
	RGB_MAKE(  5,255,  5),  // GREEN,
	RGB_MAKE( 80, 80, 80),  // GRAY + GRAY_LEN*2/3,
	RGB_MAKE(160,100, 50),  // BROWN,
	RGB_MAKE(255,255,255),  // RED + RED_LEN/2,
	RGB_MAKE(255,176,  5),  // ORANGE,
	RGB_MAKE(170,170,170),  // GRAY + GRAY_LEN*1/3,
	RGB_MAKE(255,  5,  5),  // RED,
	RGB_MAKE(255,185,225),  // PINK
};

//
// The vector graphics for the automap.
//
// A line drawing of the player pointing right, starting from the
// middle.

static mline_t player_arrow[] =
{
	{{-0.875f, 0.0f}, {1.0f, 0.0f}},   // -----

	{{1.0f, 0.0f}, {0.5f,  0.25f}},  // ----->
	{{1.0f, 0.0f}, {0.5f, -0.25f}},

	{{-0.875f, 0.0f}, {-1.125f,  0.25f}},  // >---->
	{{-0.875f, 0.0f}, {-1.125f, -0.25f}},

	{{-0.625f, 0.0f}, {-0.875f,  0.25f}},  // >>--->
	{{-0.625f, 0.0f}, {-0.875f, -0.25f}}
};

#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

static mline_t cheat_player_arrow[] =
{
	{{-0.875f, 0.0f}, {1.0f, 0.0f}},    // -----

	{{1.0f, 0.0f}, {0.5f,  0.167f}},  // ----->
	{{1.0f, 0.0f}, {0.5f, -0.167f}},

	{{-0.875f, 0.0f}, {-1.125f,  0.167f}},  // >----->
	{{-0.875f, 0.0f}, {-1.125f, -0.167f}},

	{{-0.625f, 0.0f}, {-0.875f,  0.167f}},  // >>----->
	{{-0.625f, 0.0f}, {-0.875f, -0.167f}},

	{{-0.5f, 0.0f}, {-0.5f, -0.167f}},      // >>-d--->
	{{-0.5f, -0.167f}, {-0.5f + 0.167f, -0.167f}},
	{{-0.5f + 0.167f, -0.167f}, {-0.5f + 0.167f, 0.25f}},

	{{-0.167f, 0.0f}, {-0.167f, -0.167f}},  // >>-dd-->
	{{-0.167f, -0.167f}, {0.0f, -0.167f}},
	{{0.0f, -0.167f}, {0.0f, 0.25f}},

	{{0.167f, 0.25f}, {0.167f, -0.143f}},  // >>-ddt->
	{{0.167f, -0.143f}, {0.167f + 0.031f, -0.143f - 0.031f}},
	{{0.167f + 0.031f, -0.143f - 0.031f}, {0.167f + 0.1f, -0.143f}}
};

#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))

static mline_t thin_triangle_guy[] =
{
	{{-0.5f, -0.7f}, {1.0f, 0.0f}},
	{{1.0f, 0.0f}, {-0.5f, 0.7f}},
	{{-0.5f, 0.7f}, {-0.5f, -0.7f}}
};

#define NUMTHINTRIANGLEGUYLINES (sizeof(thin_triangle_guy)/sizeof(mline_t))

static void AM_DrawPlayer(mobj_t *mo)
{
#if (DEBUG_COLLIDE == 1)
	DrawObjectBounds(mo, am_colors[AMCOL_Player]);
#endif

	if (!netgame)
	{
		if (cheating)
			DrawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, 
				mo->radius, mo->angle,
				am_colors[AMCOL_Player], mo->x, mo->y);
		else
			DrawLineCharacter(player_arrow, NUMPLYRLINES, 
				mo->radius, mo->angle,
				am_colors[AMCOL_Player], mo->x, mo->y);

		return;
	}

#if 0 //!!!! TEMP DISABLED, NETWORK DEBUGGING
	if ((DEATHMATCH() && !singledemo) && mo->player != p)
		return;
#endif

	DrawLineCharacter(player_arrow, NUMPLYRLINES, 
		mo->radius, mo->angle,
		player_colors[mo->player->pnum & 0x07],
		mo->x, mo->y);
}


static void AM_WalkThing(mobj_t *mo)
{
	int index = AMCOL_Scenery;

	if (mo->player && mo->player->mo == mo)
	{
		AM_DrawPlayer(mo);
		return;
	}

	if (! show_things)
		return;

	// -AJA- more colourful things
	if (mo->flags & MF_SPECIAL)
		index = AMCOL_Item;
	else if (mo->flags & MF_MISSILE)
		index = AMCOL_Missile;
	else if (mo->extendedflags & EF_MONSTER && mo->health <= 0)
		index = AMCOL_Corpse;
	else if (mo->extendedflags & EF_MONSTER)
		index = AMCOL_Monster;

#if (DEBUG_COLLIDE == 1)
	DrawObjectBounds(mo, am_colors[index]);
	return;
#endif

	DrawLineCharacter(
		thin_triangle_guy, NUMTHINTRIANGLEGUYLINES,
		mo->radius, mo->angle, am_colors[index], mo->x, mo->y);
}


//
// Visit a subsector and draw everything.
//
static void AM_WalkSubsector(unsigned int num)
{
	subsector_t *sub = &subsectors[num];

	// handle each seg
	for (seg_t *seg = sub->segs; seg; seg = seg->sub_next)
	{
		AM_WalkSeg(seg);
	}

	// handle each thing
	for (mobj_t *mo = sub->thinglist; mo; mo = mo->snext)
	{
		AM_WalkThing(mo);
	}
}


//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
static bool AM_CheckBBox(float *bspcoord)
{
	float xl = bspcoord[BOXLEFT];
	float yt = bspcoord[BOXTOP];
	float xr = bspcoord[BOXRIGHT];
	float yb = bspcoord[BOXBOTTOM];

	// TODO: improve this quick'n'dirty hack
	if (rotatemap)
		return true;

	float x1 = CXMTOF(xl);
	float x2 = CXMTOF(xr);

	float y1 = CYMTOF(yt);
	float y2 = CYMTOF(yb);

	// some part of bbox is visible?
	return HUD_ScissorTest(x1, y1, x2, y2);
}


//
// Walks all subsectors below a given node, traversing subtree
// recursively.  Just call with BSP root.
//
static void AM_WalkBSPNode(unsigned int bspnum)
{
	node_t *node;
	int side;

	// Found a subsector?
	if (bspnum & NF_V5_SUBSECTOR)
	{
		AM_WalkSubsector(bspnum & (~NF_V5_SUBSECTOR));
		return;
	}

	node = &nodes[bspnum];
	side = 0;

	// Recursively divide right space
	if (AM_CheckBBox(node->bbox[0]))
		AM_WalkBSPNode(node->children[side]);

	// Recursively divide back space
	if (AM_CheckBBox(node->bbox[side ^ 1]))
		AM_WalkBSPNode(node->children[side ^ 1]);
}


static void DrawMarks(void)
{
	font_c *am_font = automap_style->fonts[0];

	HUD_SetFont(am_font);
	HUD_SetAlignment(0, 0); // centre the characters

	char buffer[4];
		
	for (int i = 0; i < AM_NUMMARKPOINTS; i++)
	{
		if (markpoints[i].x == NO_MARK_X)
			continue;

		float mx, my;

		GetRotatedCoords(markpoints[i].x, markpoints[i].y, &mx, &my);

		buffer[0] = ('1' + i);
		buffer[1] = 0;

		HUD_DrawText(CXMTOF(mx), CYMTOF(my), buffer);
	}

	HUD_SetFont();
	HUD_SetAlignment();
}


static void AM_RenderScene(void)
{
	HUD_PushScissor(f_x, f_y, f_x+f_w, f_y+f_h, true);

	// walk the bsp tree
	AM_WalkBSPNode(root_node);

	HUD_PopScissor();
}


void AM_Drawer(float x, float y, float w, float h, mobj_t *focus)
{
	f_x = x;
	f_y = y;
	f_w = w;
	f_h = h;

	f_scale = MAX(f_w, f_h) / map_size / 2.0f;
	f_focus = focus;

	if (followplayer)
	{
		m_cx = f_focus->x;
		m_cy = f_focus->y;
	}

	SYS_ASSERT(automap_style);

	if (grid && !rotatemap)
		DrawGrid();

	AM_RenderScene();

	DrawMarks();
}


void AM_SetColor(int which, rgbcol_t color)
{
	SYS_ASSERT(0 <= which && which < AM_NUM_COLORS);

	am_colors[which] = color;
}


void AM_GetState(int *state, float *zoom)
{
	*state = 0;

	if (grid)
		*state |= AMST_Grid;

	if (followplayer)
		*state |= AMST_Follow;
	
	if (rotatemap)
		*state |= AMST_Rotate;

	if (show_things)
		*state |= AMST_Things;

	if (show_walls)
		*state |= AMST_Walls;

	// nothing required for AMST_Allmap flag (no actual state)

	*zoom = m_scale;
}


void AM_SetState(int state, float zoom)
{
	grid         = (state & AMST_Grid)   ? true : false;
	followplayer = (state & AMST_Follow) ? true : false;
	rotatemap    = (state & AMST_Rotate) ? true : false;

	show_things  = (state & AMST_Things) ? true : false;
	show_walls   = (state & AMST_Walls)  ? true : false;
	show_allmap  = (state & AMST_Allmap) ? true : false;

	m_scale = zoom;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
