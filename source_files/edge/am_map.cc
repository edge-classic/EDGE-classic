//----------------------------------------------------------------------------
//  EDGE Automap Functions
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

#include "AlmostEquals.h"
#include "str_compare.h"
#define DEBUG_TRUEBSP 0
#define DEBUG_COLLIDE 0

// Automap colors

// NOTE: this order must match the one in the COAL API script
static RGBAColor am_colors[AM_NUM_COLORS] = {
    epi::MakeRGBA(40, 40, 112),   // AMCOL_Grid
    epi::MakeRGBA(112, 112, 112), // AMCOL_Allmap
    epi::MakeRGBA(255, 0, 0),     // AMCOL_Wall
    epi::MakeRGBA(192, 128, 80),  // AMCOL_Step
    epi::MakeRGBA(192, 128, 80),  // AMCOL_Ledge
    epi::MakeRGBA(220, 220, 0),   // AMCOL_Ceil
    epi::MakeRGBA(0, 200, 200),   // AMCOL_Secret

    epi::MakeRGBA(255, 255, 255), // AMCOL_Player
    epi::MakeRGBA(0, 255, 0),     // AMCOL_Monster
    epi::MakeRGBA(220, 0, 0),     // AMCOL_Corpse
    epi::MakeRGBA(0, 0, 255),     // AMCOL_Item
    epi::MakeRGBA(255, 188, 0),   // AMCOL_Missile
    epi::MakeRGBA(120, 60, 30)    // AMCOL_Scenery
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

#define AM_NUMMARKPOINTS 9

//
// NOTE:
//   `F' in the names here means `Framebuffer', i.e. on-screen coords.
//   `M' in the names means `Map', i.e. coordinates in the level.
//

// scale on entry
#define MIN_MSCALE  (0.5f)
#define INIT_MSCALE (4.0f)
#define MAX_MSCALE  (100.0f)

// how much the automap moves window per tic in frame-buffer coordinates
// moves a whole screen-width in 1.5 seconds
#define F_PANINC 6.1

// how much zoom-in per tic
// goes to 3x in 1 second
#define M_ZOOMIN 1.03f

// how much zoom-in for each mouse-wheel click
// goes to 3x in 4 clicks
#define WHEEL_ZOOMIN 1.32f

bool automapactive = false;

DEF_CVAR(am_smoothing, "1", CVAR_ARCHIVE)
DEF_CVAR(am_gridsize, "128", CVAR_ARCHIVE)

static int cheating = 0;
static int grid     = 0;

static bool show_things = false;
static bool show_walls  = false;
static bool show_allmap = false;
static bool hide_lines  = false;

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

#define NO_MARK_X (-777)

// next point to be assigned
static int markpointnum = 0;

// specifies whether to follow the player around
static bool followplayer = true;

cheatseq_t cheat_amap = {0, 0};

static bool stopped = true;

static automap_arrow_e current_arrowtype = AMARW_DOOM;

bool rotatemap       = false;
bool am_keydoorblink = false;
DEF_CVAR(am_keydoortext, "0", CVAR_ARCHIVE)

extern cvar_c r_doubleframes;

extern style_c *automap_style; // FIXME: put in header

// translates between frame-buffer and map distances
static float XMTOF(float x)
{
    return x * m_scale * f_scale * 1.2f;
}
static float YMTOF(float y)
{
    return y * m_scale * f_scale;
}
static float FTOM(float x)
{
    return x / m_scale / f_scale;
}

// translates from map coordinates to frame-buffer
static float CXMTOF(float x, float dx)
{
    return f_x + f_w * 0.5 + XMTOF(x - dx);
}
static float CYMTOF(float y, float dy)
{
    return f_y + f_h * 0.5 - YMTOF(y - dy);
}

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
        map_min_x = HMM_MIN(map_min_x, vertexes[i].X);
        map_max_x = HMM_MAX(map_max_x, vertexes[i].X);

        map_min_y = HMM_MIN(map_min_y, vertexes[i].Y);
        map_max_y = HMM_MAX(map_max_y, vertexes[i].Y);
    }

    float map_w = map_max_x - map_min_x;
    float map_h = map_max_y - map_min_y;

    map_size = HMM_MAX(map_w, map_h);

    m_cx = (map_min_x + map_max_x) / 2.0;
    m_cy = (map_min_y + map_max_y) / 2.0;
}

static void ClearMarks(void)
{
    for (int i = 0; i < AM_NUMMARKPOINTS; i++)
        markpoints[i].x = NO_MARK_X;

    markpointnum = 0;
}

void AM_SetArrow(automap_arrow_e type)
{
    if (type >= AMARW_DOOM && type < AMARW_NUMTYPES)
        current_arrowtype = type;
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
    stopped       = true;

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

    if (!stopped)
        ///	AM_Stop();
        return;

    AM_InitLevel();

    stopped = false;

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

    m_scale = HMM_MAX(m_scale, MIN_MSCALE);
    m_scale = HMM_MIN(m_scale, MAX_MSCALE);
}

//
// Handle events (user inputs) in automap mode
//
bool AM_Responder(event_t *ev)
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

    if (!automapactive)
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

    // Had to move the automap cheat check up here thanks to Heretic's 'ravmap' cheat - Dasho
    // -ACB- 1999/09/28 Proper casting
    // -AJA- 2022: allow this in deathmatch (as we don't have real multiplayer)
    if (M_CheckCheat(&cheat_amap, (char)sym))
    {
        cheating = (cheating + 1) % 3;

        show_things = (cheating == 2) ? true : false;
        show_walls  = (cheating >= 1) ? true : false;
    }

    if (!followplayer)
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
        CON_PlayerMessage(consoleplayer, "%s %d", language["AutoMapMarkedSpot"], markpointnum + 1);
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

    return false;
}

//
// Updates on game tick
//
void AM_Ticker(void)
{
    if (!automapactive)
        return;

    // Change x,y location
    if (!followplayer)
    {
        m_cx += panning_x;
        m_cy += panning_y;

        // limit position, don't go outside of the map
        m_cx = HMM_MIN(m_cx, map_max_x);
        m_cx = HMM_MAX(m_cx, map_min_x);

        m_cy = HMM_MIN(m_cy, map_max_y);
        m_cy = HMM_MAX(m_cy, map_min_y);
    }

    // Change the zoom if necessary
    if (zooming > 0)
        ChangeWindowScale(zooming);
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
static inline void Rotate(float &x, float &y, BAMAngle a)
{
    float bam_sin = epi::BAMSin(a);
    float bam_cos = epi::BAMCos(a);
    float new_x = x * bam_cos - y * bam_sin;
    float new_y = x * bam_sin + y * bam_cos;

    x = new_x;
    y = new_y;
}

static void GetRotatedCoords(float sx, float sy, float &dx, float &dy)
{
    dx = sx;
    dy = sy;

    if (rotatemap)
    {
        // rotate coordinates so they are on the map correctly
        dx -= f_focus->x;
        dy -= f_focus->y;

        Rotate(dx, dy, kBAMAngle90 - f_focus->angle);

        dx += f_focus->x;
        dy += f_focus->y;
    }
}

static inline BAMAngle GetRotatedAngle(BAMAngle src)
{
    if (rotatemap)
        return src + kBAMAngle90 - f_focus->angle;

    return src;
}

//
// Draw visible parts of lines.
//
static void DrawMLine(mline_t *ml, RGBAColor rgb, bool thick = true)
{
    if (hide_lines)
        return;

    if (!am_smoothing.d)
        thick = false;

    float x1 = CXMTOF(ml->a.x, 0);
    float y1 = CYMTOF(ml->a.y, 0);

    float x2 = CXMTOF(ml->b.x, 0);
    float y2 = CYMTOF(ml->b.y, 0);

    // these are separate to reduce the wobblies
    float dx = XMTOF(-m_cx);
    float dy = YMTOF(-m_cy);

    HUD_SolidLine(x1, y1, x2, y2, rgb, thick ? 1.5f : 1.0f, thick, dx, dy);
}

// Lobo 2022: keyed doors automap colouring
static void DrawMLineDoor(mline_t *ml, RGBAColor rgb)
{
    if (hide_lines)
        return;

    float x1 = CXMTOF(ml->a.x, 0);
    float y1 = CYMTOF(ml->a.y, 0);

    float x2 = CXMTOF(ml->b.x, 0);
    float y2 = CYMTOF(ml->b.y, 0);

    float dx = XMTOF(-m_cx);
    float dy = YMTOF(-m_cy);

    float linewidth = 3.5f;

    // Lobo 2023: Make keyed doors pulse
    if (am_keydoorblink)
    {
        linewidth = gametic % (32 * (r_doubleframes.d ? 2 : 1));

        if (linewidth >= 16)
            linewidth = 2.0 + (linewidth * 0.1f);
        else
            linewidth = 2.0 - (linewidth * 0.1f);
    }

    HUD_SolidLine(x1, y1, x2, y2, rgb, linewidth, true, dx, dy);
}

/*
static mline_t door_key[] =
{
    {{-2, 0}, {-1.7, -0.5}},
    {{-1.7, -0.5}, {-1.5, -0.7}},
    {{-1.5, -0.7}, {-0.8, -0.5}},
    {{-0.8, -0.5}, {-0.6, 0}},
    {{-0.6, 0}, {-0.8, 0.5}},
    {{-1.5, 0.7}, {-0.8, 0.5}},
    {{-1.7, 0.5}, {-1.5, 0.7}},
    {{-2, 0}, {-1.7, 0.5}},
    {{-0.6, 0}, {2, 0}},
    {{1.7, 0}, {1.7, -1}},
    {{1.5, 0}, {1.5, -1}},
    {{1.3, 0}, {1.3, -1}}
};

#define NUMDOORKEYLINES (sizeof(door_key)/sizeof(mline_t))
*/

static mline_t player_dagger[] = {
    {{-0.75f, 0.0f}, {0.0f, 0.0f}}, // center line

    {{-0.75f, 0.125f}, {1.0f, 0.0f}}, // blade
    {{-0.75f, -0.125f}, {1.0f, 0.0f}},

    {{-0.75, -0.25}, {-0.75, 0.25}},                                         // crosspiece
    {{-0.875, -0.25}, {-0.875, 0.25}},  {{-0.875, -0.25}, {-0.75, -0.25}},   // crosspiece connectors
    {{-0.875, 0.25}, {-0.75, 0.25}},    {{-1.125, 0.125}, {-1.125, -0.125}}, // pommel
    {{-1.125, 0.125}, {-0.875, 0.125}}, {{-1.125, -0.125}, {-0.875, -0.125}}};

#define NUMPLYRDGGRLINES (sizeof(player_dagger) / sizeof(mline_t))

static void DrawLineCharacter(mline_t *lineguy, int lineguylines, float radius, BAMAngle angle, RGBAColor rgb, float x,
                              float y)
{
    float cx, cy;

    GetRotatedCoords(x, y, cx, cy);

    cx = CXMTOF(cx, m_cx);
    cy = CYMTOF(cy, m_cy);

    if (radius < FTOM(2))
        radius = FTOM(2);

    angle = GetRotatedAngle(angle);

    for (int i = 0; i < lineguylines; i++)
    {
        float ax = lineguy[i].a.x;
        float ay = lineguy[i].a.y;

        if (angle)
            Rotate(ax, ay, angle);

        float bx = lineguy[i].b.x;
        float by = lineguy[i].b.y;

        if (angle)
            Rotate(bx, by, angle);

        ax = ax * XMTOF(radius);
        ay = ay * YMTOF(radius);
        bx = bx * XMTOF(radius);
        by = by * YMTOF(radius);

        HUD_SolidLine(cx + ax, cy - ay, cx + bx, cy - by, rgb);
    }
}

// Aux2StringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
std::string Aux2StringReplaceAll(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// Lobo 2023: draw some key info in the middle of a line
static void DrawKeyOnLine(mline_t *ml, int theKey, RGBAColor rgb = SG_WHITE_RGBA32)
{
    if (hide_lines)
        return;

    if (am_keydoortext.d == 0) // Only if we have Keyed Doors Named turned on
        return;

    static const mobjtype_c *TheObject;
    std::string              CleanName;
    CleanName.clear();

    if (theKey == KF_STRICTLY_ALL)
    {
        CleanName = "All keys";
    }
    else
    {
        TheObject = mobjtypes.LookupDoorKey(theKey);
        if (!TheObject)
            return; // Very rare, only zombiesTC hits this so far
        CleanName = Aux2StringReplaceAll(TheObject->name, std::string("_"), std::string(" "));
    }

    // *********************
    // Draw Text description
    // Calculate midpoint
    float midx = (ml->a.x + ml->b.x) / 2;
    float midy = (ml->a.y + ml->b.y) / 2;

    // Translate map coords to hud coords
    float x1 = CXMTOF(midx, m_cx);
    float y1 = CYMTOF(midy, m_cy);

    font_c *am_font = automap_style->fonts[0];

    HUD_SetFont(am_font);
    HUD_SetAlignment(0, 0); // centre the characters
    HUD_SetTextColor(rgb);
    float TextSize = 0.4f * m_scale;
    if (m_scale > 5.0f) // only draw the text if we're zoomed in?
    {
        if (am_keydoortext.d == 1)
        {
            HUD_DrawText(x1, y1, CleanName.c_str(), TextSize);
        }
        else if (am_keydoortext.d > 1)
        {
            if (TheObject)
            {
                static state_t *idlestate;
                idlestate = &states[TheObject->idle_state];
                if (!(idlestate->flags & SFF_Model)) // Can't handle 3d models...yet
                {
                    bool           flip;
                    const image_c *img = R2_GetOtherSprite(idlestate->sprite, idlestate->frame, &flip);

                    if (epi::StringCaseCompareASCII("DUMMY_SPRITE", img->name) != 0)
                        HUD_DrawImageNoOffset(x1, y1, img);
                    // HUD_StretchImage(x1, y1, 16, 16, img, 0.0, 0.0);
                }
            }
        }
    }

    HUD_SetFont();
    HUD_SetTextColor();
    HUD_SetAlignment();

    /*
    // *********************
    // Draw Graphical description
        float x2 = ml->a.x;
        float y2 = ml->a.y;

        DrawLineCharacter(door_key, NUMDOORKEYLINES,
                    5, kBAMAngle90,
                    epi::MakeRGBA(0,0,255), x2, y2);
    */

    return;
}

//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
static void DrawGrid()
{
    mline_t ml;

    int grid_size = HMM_MAX(4, am_gridsize.d);

    int mx0 = int(m_cx);
    int my0 = int(m_cy);

    if (mx0 < 0)
        mx0 -= -(-mx0 % grid_size);
    else
        mx0 -= mx0 % grid_size;
    if (my0 < 0)
        my0 -= -(-my0 % grid_size);
    else
        my0 -= my0 % grid_size;

    for (int j = 1; j < 1024; j++)
    {
        int jx = ((j & ~1) >> 1);

        // stop when both lines are off the screen
        float x1 = CXMTOF(mx0 - jx * grid_size, m_cx);
        float x2 = CXMTOF(mx0 + jx * grid_size, m_cx);

        if (x1 < f_x && x2 >= f_x + f_w)
            break;

        ml.a.x = mx0 + jx * ((j & 1) ? -grid_size : grid_size);
        ml.b.x = ml.a.x;

        ml.a.y = -9e6;
        ml.b.y = +9e6;

        DrawMLine(&ml, am_colors[AMCOL_Grid], false);
    }

    for (int k = 1; k < 1024; k++)
    {
        int ky = ((k & ~1) >> 1);

        // stop when both lines are off the screen
        float y1 = CYMTOF(my0 + ky * grid_size, m_cy);
        float y2 = CYMTOF(my0 - ky * grid_size, m_cy);

        if (y1 < f_y && y2 >= f_y + f_h)
            break;

        ml.a.x = -9e6;
        ml.b.x = +9e6;

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
        if (!AlmostEquals(F->top_h, B->top_h))
            return false;

        if (!AlmostEquals(F->bottom_h, B->bottom_h))
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

        GetRotatedCoords(seg->v1->x, seg->v1->y, l.a.x, l.a.y);
        GetRotatedCoords(seg->v2->x, seg->v2->y, l.b.x, l.b.y);

        DrawMLine(&l, epi::MakeRGBA(0, 0, 128), false);
#endif
        return;
    }

    line = seg->linedef;
    SYS_ASSERT(line);

    // only draw segs on the _right_ side of linedefs
    if (line->side[1] == seg->sidedef)
        return;

    GetRotatedCoords(seg->v1->X, seg->v1->Y, l.a.x, l.a.y);
    GetRotatedCoords(seg->v2->X, seg->v2->Y, l.b.x, l.b.y);

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
            // Lobo 2022: give keyed doors the colour of the required key
            if (line->special)
            {
                if (line->special->keys)
                {
                    if (line->special->keys & KF_STRICTLY_ALL)
                    {
                        DrawMLineDoor(&l, SG_PURPLE_RGBA32); // purple
                        DrawKeyOnLine(&l, KF_STRICTLY_ALL);
                    }
                    else if (line->special->keys & KF_BlueCard || line->special->keys & KF_BlueSkull)
                    {
                        DrawMLineDoor(&l, SG_BLUE_RGBA32); // blue
                        if (line->special->keys & (KF_BlueSkull | KF_BlueCard))
                        {
                            DrawKeyOnLine(&l, KF_BlueCard);
                            DrawKeyOnLine(&l, KF_BlueSkull);
                        }
                        else if (line->special->keys & KF_BlueCard)
                            DrawKeyOnLine(&l, KF_BlueCard);
                        else
                            DrawKeyOnLine(&l, KF_BlueSkull);
                    }
                    else if (line->special->keys & KF_YellowCard || line->special->keys & KF_YellowSkull)
                    {
                        DrawMLineDoor(&l, SG_YELLOW_RGBA32); // yellow
                        if (line->special->keys & (KF_YellowSkull | KF_YellowCard))
                        {
                            DrawKeyOnLine(&l, KF_YellowCard);
                            DrawKeyOnLine(&l, KF_YellowSkull);
                        }
                        else if (line->special->keys & KF_YellowCard)
                            DrawKeyOnLine(&l, KF_YellowCard);
                        else
                            DrawKeyOnLine(&l, KF_YellowSkull);
                    }
                    else if (line->special->keys & KF_RedCard || line->special->keys & KF_RedSkull)
                    {
                        DrawMLineDoor(&l, SG_RED_RGBA32); // red
                        if (line->special->keys & (KF_RedSkull | KF_RedCard))
                        {
                            DrawKeyOnLine(&l, KF_RedCard);
                            DrawKeyOnLine(&l, KF_RedSkull);
                        }
                        else if (line->special->keys & KF_RedCard)
                            DrawKeyOnLine(&l, KF_RedCard);
                        else
                            DrawKeyOnLine(&l, KF_RedSkull);
                    }
                    else if (line->special->keys & KF_GreenCard || line->special->keys & KF_GreenSkull)
                    {
                        DrawMLineDoor(&l, SG_GREEN_RGBA32); // green
                        if (line->special->keys & (KF_GreenSkull | KF_GreenCard))
                        {
                            DrawKeyOnLine(&l, KF_GreenCard);
                            DrawKeyOnLine(&l, KF_GreenSkull);
                        }
                        else if (line->special->keys & KF_GreenCard)
                            DrawKeyOnLine(&l, KF_GreenCard);
                        else
                            DrawKeyOnLine(&l, KF_GreenSkull);
                    }
                    else
                    {
                        DrawMLineDoor(&l, SG_PURPLE_RGBA32); // purple
                    }
                    return;
                }
            }
            if (line->flags & MLF_Secret)
            {
                // secret door
                if (show_walls)
                    DrawMLine(&l, am_colors[AMCOL_Secret]);
                else
                    DrawMLine(&l, am_colors[AMCOL_Wall]);
            }
            else if (!AlmostEquals(back->f_h, front->f_h))
            {
                float diff = fabs(back->f_h - front->f_h);

                // floor level change
                if (diff > 24)
                    DrawMLine(&l, am_colors[AMCOL_Ledge]);
                else
                    DrawMLine(&l, am_colors[AMCOL_Step]);
            }
            else if (!AlmostEquals(back->c_h, front->c_h))
            {
                // ceiling level change
                DrawMLine(&l, am_colors[AMCOL_Ceil]);
            }
            else if ((front->exfloor_used > 0 || back->exfloor_used > 0) &&
                     (front->exfloor_used != back->exfloor_used || !CheckSimiliarRegions(front, back)))
            {
                // -AJA- 1999/10/09: extra floor change.
                DrawMLine(&l, am_colors[AMCOL_Ledge]);
            }
            else if (show_walls)
            {
                DrawMLine(&l, am_colors[AMCOL_Allmap]);
            }
            else if (line->slide_door)
            { // Lobo: draw sliding doors on automap
                DrawMLine(&l, am_colors[AMCOL_Ceil]);
            }
        }
    }
    else if (f_focus->player && (show_allmap || !AlmostEquals(f_focus->player->powers[PW_AllMap], 0.0f)))
    {
        if (!(line->flags & MLF_DontDraw))
            DrawMLine(&l, am_colors[AMCOL_Allmap]);
    }
}

#if (DEBUG_COLLIDE == 1)
static void DrawObjectBounds(mobj_t *mo, RGBAColor rgb)
{
    float R = mo->radius;

    if (R < 2)
        R = 2;

    float lx = mo->x - R;
    float ly = mo->y - R;
    float hx = mo->x + R;
    float hy = mo->y + R;

    mline_t ml;

    GetRotatedCoords(lx, ly, ml.a.x, ml.a.y);
    GetRotatedCoords(lx, hy, ml.b.x, ml.b.y);
    DrawMLine(&ml, rgb);

    GetRotatedCoords(lx, hy, ml.a.x, ml.a.y);
    GetRotatedCoords(hx, hy, ml.b.x, ml.b.y);
    DrawMLine(&ml, rgb);

    GetRotatedCoords(hx, hy, ml.a.x, ml.a.y);
    GetRotatedCoords(hx, ly, ml.b.x, ml.b.y);
    DrawMLine(&ml, rgb);

    GetRotatedCoords(hx, ly, ml.a.x, ml.a.y);
    GetRotatedCoords(lx, ly, ml.b.x, ml.b.y);
    DrawMLine(&ml, rgb);
}
#endif

static RGBAColor player_colors[8] = {
    epi::MakeRGBA(5, 255, 5),     // GREEN,
    epi::MakeRGBA(80, 80, 80),    // GRAY + GRAY_LEN*2/3,
    epi::MakeRGBA(160, 100, 50),  // BROWN,
    epi::MakeRGBA(255, 255, 255), // RED + RED_LEN/2,
    epi::MakeRGBA(255, 176, 5),   // ORANGE,
    epi::MakeRGBA(170, 170, 170), // GRAY + GRAY_LEN*1/3,
    epi::MakeRGBA(255, 5, 5),     // RED,
    epi::MakeRGBA(255, 185, 225), // PINK
};

//
// The vector graphics for the automap.
//
// A line drawing of the player pointing right, starting from the
// middle.

static mline_t player_arrow[] = {{{-0.875f, 0.0f}, {1.0f, 0.0f}}, // -----

                                 {{1.0f, 0.0f}, {0.5f, 0.25f}}, // ----->
                                 {{1.0f, 0.0f}, {0.5f, -0.25f}},

                                 {{-0.875f, 0.0f}, {-1.125f, 0.25f}}, // >---->
                                 {{-0.875f, 0.0f}, {-1.125f, -0.25f}},

                                 {{-0.625f, 0.0f}, {-0.875f, 0.25f}}, // >>--->
                                 {{-0.625f, 0.0f}, {-0.875f, -0.25f}}};

#define NUMPLYRLINES (sizeof(player_arrow) / sizeof(mline_t))

static mline_t cheat_player_arrow[] = {{{-0.875f, 0.0f}, {1.0f, 0.0f}}, // -----

                                       {{1.0f, 0.0f}, {0.5f, 0.167f}}, // ----->
                                       {{1.0f, 0.0f}, {0.5f, -0.167f}},

                                       {{-0.875f, 0.0f}, {-1.125f, 0.167f}}, // >----->
                                       {{-0.875f, 0.0f}, {-1.125f, -0.167f}},

                                       {{-0.625f, 0.0f}, {-0.875f, 0.167f}}, // >>----->
                                       {{-0.625f, 0.0f}, {-0.875f, -0.167f}},

                                       {{-0.5f, 0.0f}, {-0.5f, -0.167f}}, // >>-d--->
                                       {{-0.5f, -0.167f}, {-0.5f + 0.167f, -0.167f}},
                                       {{-0.5f + 0.167f, -0.167f}, {-0.5f + 0.167f, 0.25f}},

                                       {{-0.167f, 0.0f}, {-0.167f, -0.167f}}, // >>-dd-->
                                       {{-0.167f, -0.167f}, {0.0f, -0.167f}},
                                       {{0.0f, -0.167f}, {0.0f, 0.25f}},

                                       {{0.167f, 0.25f}, {0.167f, -0.143f}}, // >>-ddt->
                                       {{0.167f, -0.143f}, {0.167f + 0.031f, -0.143f - 0.031f}},
                                       {{0.167f + 0.031f, -0.143f - 0.031f}, {0.167f + 0.1f, -0.143f}}};

#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow) / sizeof(mline_t))

static mline_t thin_triangle_guy[] = {
    {{-0.5f, -0.7f}, {1.0f, 0.0f}}, {{1.0f, 0.0f}, {-0.5f, 0.7f}}, {{-0.5f, 0.7f}, {-0.5f, -0.7f}}};

#define NUMTHINTRIANGLEGUYLINES (sizeof(thin_triangle_guy) / sizeof(mline_t))

static void AM_DrawPlayer(mobj_t *mo)
{
#if (DEBUG_COLLIDE == 1)
    DrawObjectBounds(mo, am_colors[AMCOL_Player]);
#endif

    if (!netgame)
    {
        switch (current_arrowtype)
        {
        case AMARW_HERETIC:
            DrawLineCharacter(player_dagger, NUMPLYRDGGRLINES, mo->radius, mo->angle, am_colors[AMCOL_Player], mo->x,
                              mo->y);
            break;
        case AMARW_DOOM:
        default:
            if (cheating)
                DrawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, mo->radius, mo->angle, am_colors[AMCOL_Player],
                                  mo->x, mo->y);
            else
                DrawLineCharacter(player_arrow, NUMPLYRLINES, mo->radius, mo->angle, am_colors[AMCOL_Player], mo->x,
                                  mo->y);
            break;
        }
        return;
    }

#if 0 //!!!! TEMP DISABLED, NETWORK DEBUGGING
	if (DEATHMATCH() && mo->player != p)
		return;
#endif

    DrawLineCharacter(player_arrow, NUMPLYRLINES, mo->radius, mo->angle, player_colors[mo->player->pnum & 0x07], mo->x,
                      mo->y);
}

static void AM_WalkThing(mobj_t *mo)
{
    int index = AMCOL_Scenery;

    if (mo->player && mo->player->mo == mo)
    {
        AM_DrawPlayer(mo);
        return;
    }

    if (!show_things)
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

    DrawLineCharacter(thin_triangle_guy, NUMTHINTRIANGLEGUYLINES, mo->radius, mo->angle, am_colors[index], mo->x,
                      mo->y);
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
    float L = bspcoord[BOXLEFT];
    float R = bspcoord[BOXRIGHT];
    float T = bspcoord[BOXTOP];
    float B = bspcoord[BOXBOTTOM];

    if (rotatemap)
    {
        float x1, x2, x3, x4;
        float y1, y2, y3, y4;

        GetRotatedCoords(L, T, x1, y1);
        GetRotatedCoords(R, T, x2, y2);
        GetRotatedCoords(L, B, x3, y3);
        GetRotatedCoords(R, B, x4, y4);

        L = HMM_MIN(HMM_MIN(x1, x2), HMM_MIN(x3, x4));
        B = HMM_MIN(HMM_MIN(y1, y2), HMM_MIN(y3, y4));

        R = HMM_MAX(HMM_MAX(x1, x2), HMM_MAX(x3, x4));
        T = HMM_MAX(HMM_MAX(y1, y2), HMM_MAX(y3, y4));
    }

    // convert from map to hud coordinates
    float x1 = CXMTOF(L, m_cx);
    float x2 = CXMTOF(R, m_cx);

    float y1 = CYMTOF(T, m_cy);
    float y2 = CYMTOF(B, m_cy);

    return !(x2 < f_x - 1 || x1 > f_x + f_w + 1 || y2 < f_y - 1 || y1 > f_y + f_h + 1);
}

//
// Walks all subsectors below a given node, traversing subtree
// recursively.  Just call with BSP root.
//
static void AM_WalkBSPNode(unsigned int bspnum)
{
    node_t *node;
    int     side;

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
        if (AlmostEquals(markpoints[i].x, (float)NO_MARK_X))
            continue;

        float mx, my;

        GetRotatedCoords(markpoints[i].x, markpoints[i].y, mx, my);

        buffer[0] = ('1' + i);
        buffer[1] = 0;

        HUD_DrawText(CXMTOF(mx, m_cx), CYMTOF(my, m_cy), buffer);
    }

    HUD_SetFont();
    HUD_SetAlignment();
}

void AM_Render(float x, float y, float w, float h, mobj_t *focus)
{
    f_x = x;
    f_y = y;
    f_w = w;
    f_h = h;

    f_scale = HMM_MAX(f_w, f_h) / map_size / 2.0f;
    f_focus = focus;

    if (followplayer)
    {
        m_cx = f_focus->x;
        m_cy = f_focus->y;
    }

    SYS_ASSERT(automap_style);

    if (automap_style->bg_image)
    {
        float old_alpha = HUD_GetAlpha();
        HUD_SetAlpha(automap_style->def->bg.translucency);
        if (automap_style->def->special == 0)
            HUD_StretchImage(-90, 0, 500, 200, automap_style->bg_image, 0.0, 0.0);
        else
            HUD_TileImage(-90, 0, 500, 200, automap_style->bg_image, 0.0, 0.0);
        HUD_SetAlpha(old_alpha);
    }
    else if (automap_style->def->bg.colour != kRGBANoValue)
    {
        float old_alpha = HUD_GetAlpha();
        HUD_SetAlpha(automap_style->def->bg.translucency);
        HUD_SolidBox(x, y, x + w, y + h, automap_style->def->bg.colour);
        HUD_SetAlpha(old_alpha);
    }

    if (grid && !rotatemap)
        DrawGrid();

    // walk the bsp tree
    AM_WalkBSPNode(root_node);

    DrawMarks();
}

void AM_SetColor(int which, RGBAColor color)
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

    if (hide_lines)
        *state |= AMST_HideLines;

    // nothing required for AMST_Allmap flag (no actual state)

    *zoom = m_scale;
}

void AM_SetState(int state, float zoom)
{
    grid         = (state & AMST_Grid) ? true : false;
    followplayer = (state & AMST_Follow) ? true : false;
    rotatemap    = (state & AMST_Rotate) ? true : false;

    show_things = (state & AMST_Things) ? true : false;
    show_walls  = (state & AMST_Walls) ? true : false;
    show_allmap = (state & AMST_Allmap) ? true : false;
    hide_lines  = (state & AMST_HideLines) ? true : false;

    m_scale = zoom;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
