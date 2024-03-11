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

#include "am_map.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include "AlmostEquals.h"
#include "con_main.h"
#include "con_var.h"
#include "dm_data.h"
#include "e_input.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "n_network.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "sokol_color.h"
#include "str_compare.h"

EDGE_DEFINE_CONSOLE_VARIABLE(automap_debug_bsp, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(automap_debug_collisions, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(automap_gridsize, "128", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(automap_keydoor_text, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(automap_smoothing, "1", kConsoleVariableFlagArchive)

extern unsigned int root_node;

// Automap colors

// NOTE: this order must match the one in the COAL API script
static RGBAColor am_colors[kTotalAutomapColors] = {
    epi::MakeRGBA(40, 40, 112),    // kAutomapColorGrid
    epi::MakeRGBA(112, 112, 112),  // kAutomapColorAllmap
    epi::MakeRGBA(255, 0, 0),      // kAutomapColorWall
    epi::MakeRGBA(192, 128, 80),   // kAutomapColorStep
    epi::MakeRGBA(192, 128, 80),   // kAutomapColorLedge
    epi::MakeRGBA(220, 220, 0),    // kAutomapColorCeil
    epi::MakeRGBA(0, 200, 200),    // kAutomapColorSecret

    epi::MakeRGBA(255, 255, 255),  // kAutomapColorPlayer
    epi::MakeRGBA(0, 255, 0),      // kAutomapColorMonster
    epi::MakeRGBA(220, 0, 0),      // kAutomapColorCorpse
    epi::MakeRGBA(0, 0, 255),      // kAutomapColorItem
    epi::MakeRGBA(255, 188, 0),    // kAutomapColorMissile
    epi::MakeRGBA(120, 60, 30)     // kAutomapColorScenery
};

// Automap keys
// Ideally these would be configurable...

int key_automap_up;
int key_automap_down;
int key_automap_left;
int key_automap_right;

int key_automap_zoom_in;
int key_automap_zoom_out;

int key_automap_follow;
int key_automap_grid;
int key_automap_mark;
int key_automap_clear;

static constexpr uint8_t kAutomapTotalMarkPoints = 9;

// scale on entry
static constexpr float kAutomapMinimumScale = 0.5f;
static constexpr float kAutomapInitialScale = 4.0f;
static constexpr float kAutomapMaximumScale = 100.0f;

// how much the automap moves window per tic in frame-buffer coordinates
// moves a whole screen-width in 1.5 seconds
static constexpr float kAutomapFrameBufferPanIncrement = 6.1f;

// how much zoom-in per tic
// goes to 3x in 1 second
static constexpr float kAutomapZoomPerTic = 1.03f;

// how much zoom-in for each mouse-wheel click
// goes to 3x in 4 clicks
static constexpr float kAutomapMouseWheelZoomIncrement = 1.32f;

bool automap_active = false;

static int cheating = 0;
static int grid     = 0;

static bool show_things = false;
static bool show_walls  = false;
static bool show_allmap = false;
static bool hide_lines  = false;

// location and size of window on screen
static float frame_x, frame_y;
static float frame_width, frame_height;

// scale value which makes the whole map fit into the on-screen area
// (multiplying map coords by this value).
static float frame_scale;

static MapObject *frame_focus;

// location on map which the map is centred on
static float map_center_x, map_center_y;

// relative scaling: 1.0 = map fits the on-screen area,
//                   2.0 = map is twice as big
//                   8.0 = map is eight times as big
static float map_scale;

// largest size of map along X or Y axis
static float map_size;

static float map_minimum_x;
static float map_minimum_y;
static float map_maximum_x;
static float map_maximum_y;

// how far the window pans each tic (map coords)
static float panning_x = 0;
static float panning_y = 0;

// how far the window zooms in each tic (map coords)
static float zooming = -1;

// where the points are
static AutomapPoint mark_points[kAutomapTotalMarkPoints];

static constexpr int16_t kAutomapNoMarkX = -777;

// next point to be assigned
static int mark_point_number = 0;

// specifies whether to follow the player around
static bool follow_player = true;

CheatSequence cheat_automap = {0, 0};

static bool stopped = true;

static AutomapArrowStyle current_arrow_type = kAutomapArrowStyleDoom;

bool rotate_map            = false;
bool automap_keydoor_blink = false;

extern ConsoleVariable double_framerate;

extern Style *automap_style;  // FIXME: put in header

// translates between frame-buffer and map distances
static inline float MapToFrameDistanceX(float x)
{
    return x * map_scale * frame_scale * 1.2f;
}
static inline float MapToFrameDistanceY(float y)
{
    return y * map_scale * frame_scale;
}
static inline float FrameToMapScale(float x)
{
    return x / map_scale / frame_scale;
}

// translates from map coordinates to frame-buffer
static inline float MapToFrameCoordinatesX(float x, float dx)
{
    return frame_x + frame_width * 0.5 + MapToFrameDistanceX(x - dx);
}
static inline float MapToFrameCoordinatesY(float y, float dy)
{
    return frame_y + frame_height * 0.5 - MapToFrameDistanceY(y - dy);
}

//
// adds a marker at the current location
//
static void AddMark(void)
{
    mark_points[mark_point_number].x = map_center_x;
    mark_points[mark_point_number].y = map_center_y;

    mark_point_number = (mark_point_number + 1) % kAutomapTotalMarkPoints;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
static void FindMinMaxBoundaries(void)
{
    map_minimum_x = +9e9;
    map_minimum_y = +9e9;

    map_maximum_x = -9e9;
    map_maximum_y = -9e9;

    for (int i = 0; i < total_level_vertexes; i++)
    {
        map_minimum_x = HMM_MIN(map_minimum_x, level_vertexes[i].X);
        map_maximum_x = HMM_MAX(map_maximum_x, level_vertexes[i].X);

        map_minimum_y = HMM_MIN(map_minimum_y, level_vertexes[i].Y);
        map_maximum_y = HMM_MAX(map_maximum_y, level_vertexes[i].Y);
    }

    float map_w = map_maximum_x - map_minimum_x;
    float map_h = map_maximum_y - map_minimum_y;

    map_size = HMM_MAX(map_w, map_h);

    map_center_x = (map_minimum_x + map_maximum_x) / 2.0;
    map_center_y = (map_minimum_y + map_maximum_y) / 2.0;
}

static void ClearMarks(void)
{
    for (int i = 0; i < kAutomapTotalMarkPoints; i++)
        mark_points[i].x = kAutomapNoMarkX;

    mark_point_number = 0;
}

void AutomapSetArrow(AutomapArrowStyle type)
{
    if (type >= kAutomapArrowStyleDoom && type < kTotalAutomapArrowStyles)
        current_arrow_type = type;
}

void AutomapInitLevel(void)
{
    if (!cheat_automap.sequence) { cheat_automap.sequence = language["iddt"]; }

    ClearMarks();

    FindMinMaxBoundaries();

    map_scale = kAutomapInitialScale;
}

void AutomapStop(void)
{
    automap_active = false;
    stopped        = true;

    panning_x = 0;
    panning_y = 0;
    zooming   = -1;
}

static void AutomapHide(void)
{
    automap_active = false;

    panning_x = 0;
    panning_y = 0;
    zooming   = -1;
}

static void AutomapShow(void)
{
    automap_active = true;

    if (!stopped)
        ///	AutomapStop();
        return;

    AutomapInitLevel();

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
    map_scale *= factor;
    map_scale = HMM_MAX(map_scale, kAutomapMinimumScale);
    map_scale = HMM_MIN(map_scale, kAutomapMaximumScale);
}

//
// Handle events (user inputs) in automap mode
//
bool AutomapResponder(InputEvent *ev)
{
    int sym = ev->value.key.sym;

    // check the enable/disable key
    if (ev->type == kInputEventKeyDown && EventMatchesKey(key_map, sym))
    {
        if (automap_active)
            AutomapHide();
        else
            AutomapShow();
        return true;
    }

    if (!automap_active) return false;

    // --- handle key releases ---

    if (ev->type == kInputEventKeyUp)
    {
        if (EventMatchesKey(key_automap_left, sym) ||
            EventMatchesKey(key_automap_right, sym))
            panning_x = 0;

        if (EventMatchesKey(key_automap_up, sym) ||
            EventMatchesKey(key_automap_down, sym))
            panning_y = 0;

        if (EventMatchesKey(key_automap_zoom_in, sym) ||
            EventMatchesKey(key_automap_zoom_out, sym))
            zooming = -1;

        return false;
    }

    // --- handle key presses ---

    if (ev->type != kInputEventKeyDown) return false;

    // Had to move the automap cheat check up here thanks to Heretic's 'ravmap'
    // cheat - Dasho -ACB- 1999/09/28 Proper casting -AJA- 2022: allow this in
    // deathmatch (as we don't have real multiplayer)
    if (CheatCheckSequence(&cheat_automap, (char)sym))
    {
        cheating = (cheating + 1) % 3;

        show_things = (cheating == 2) ? true : false;
        show_walls  = (cheating >= 1) ? true : false;
    }

    if (!follow_player)
    {
        if (EventMatchesKey(key_automap_left, sym))
        {
            panning_x = -FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (EventMatchesKey(key_automap_right, sym))
        {
            panning_x = FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (EventMatchesKey(key_automap_up, sym))
        {
            panning_y = FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (EventMatchesKey(key_automap_down, sym))
        {
            panning_y = -FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
    }

    if (EventMatchesKey(key_automap_zoom_in, sym))
    {
        zooming = kAutomapZoomPerTic;
        return true;
    }
    else if (EventMatchesKey(key_automap_zoom_out, sym))
    {
        zooming = 1.0 / kAutomapZoomPerTic;
        return true;
    }

    if (EventMatchesKey(key_automap_follow, sym))
    {
        follow_player = !follow_player;

        // -ACB- 1998/08/10 Use DDF Lang Reference
        if (follow_player)
            ConsolePlayerMessageLDF(console_player, "AutoMapFollowOn");
        else
            ConsolePlayerMessageLDF(console_player, "AutoMapFollowOff");

        return true;
    }

    if (EventMatchesKey(key_automap_grid, sym))
    {
        grid = !grid;
        // -ACB- 1998/08/10 Use DDF Lang Reference
        if (grid)
            ConsolePlayerMessageLDF(console_player, "AutoMapGridOn");
        else
            ConsolePlayerMessageLDF(console_player, "AutoMapGridOff");

        return true;
    }

    if (EventMatchesKey(key_automap_mark, sym))
    {
        // -ACB- 1998/08/10 Use DDF Lang Reference
        ConsolePlayerMessage(console_player, "%s %d", language["AutoMapMarkedSpot"],
                          mark_point_number + 1);
        AddMark();
        return true;
    }

    if (EventMatchesKey(key_automap_clear, sym))
    {
        // -ACB- 1998/08/10 Use DDF Lang Reference
        ConsolePlayerMessageLDF(console_player, "AutoMapMarksClear");
        ClearMarks();
        return true;
    }

    // -AJA- 2007/04/18: mouse-wheel support
    if (sym == kMouseWheelDown)
    {
        ChangeWindowScale(1.0 / kAutomapMouseWheelZoomIncrement);
        return true;
    }
    else if (sym == kMouseWheelUp)
    {
        ChangeWindowScale(kAutomapMouseWheelZoomIncrement);
        return true;
    }

    return false;
}

//
// Updates on game tick
//
void AutomapTicker(void)
{
    if (!automap_active) return;

    // Change x,y location
    if (!follow_player)
    {
        map_center_x += panning_x;
        map_center_y += panning_y;

        // limit position, don't go outside of the map
        map_center_x = HMM_MIN(map_center_x, map_maximum_x);
        map_center_x = HMM_MAX(map_center_x, map_minimum_x);

        map_center_y = HMM_MIN(map_center_y, map_maximum_y);
        map_center_y = HMM_MAX(map_center_y, map_minimum_y);
    }

    // Change the zoom if necessary
    if (zooming > 0) ChangeWindowScale(zooming);
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
static inline void Rotate(float &x, float &y, BAMAngle a)
{
    float bam_sin = epi::BAMSin(a);
    float bam_cos = epi::BAMCos(a);
    float new_x   = x * bam_cos - y * bam_sin;
    float new_y   = x * bam_sin + y * bam_cos;

    x = new_x;
    y = new_y;
}

static void GetRotatedCoords(float sx, float sy, float &dx, float &dy)
{
    dx = sx;
    dy = sy;

    if (rotate_map)
    {
        // rotate coordinates so they are on the map correctly
        dx -= frame_focus->x;
        dy -= frame_focus->y;

        Rotate(dx, dy, kBAMAngle90 - frame_focus->angle_);

        dx += frame_focus->x;
        dy += frame_focus->y;
    }
}

static inline BAMAngle GetRotatedAngle(BAMAngle src)
{
    if (rotate_map) return src + kBAMAngle90 - frame_focus->angle_;

    return src;
}

//
// Draw visible parts of lines.
//
static void DrawMLine(AutomapLine *ml, RGBAColor rgb, bool thick = true)
{
    if (hide_lines) return;

    if (!automap_smoothing.d_) thick = false;

    float x1 = MapToFrameCoordinatesX(ml->a.x, 0);
    float y1 = MapToFrameCoordinatesY(ml->a.y, 0);

    float x2 = MapToFrameCoordinatesX(ml->b.x, 0);
    float y2 = MapToFrameCoordinatesY(ml->b.y, 0);

    // these are separate to reduce the wobblies
    float dx = MapToFrameDistanceX(-map_center_x);
    float dy = MapToFrameDistanceY(-map_center_y);

    HudSolidLine(x1, y1, x2, y2, rgb, thick ? 1.5f : 1.0f, thick, dx, dy);
}

// Lobo 2022: keyed doors automap colouring
static void DrawMLineDoor(AutomapLine *ml, RGBAColor rgb)
{
    if (hide_lines) return;

    float x1 = MapToFrameCoordinatesX(ml->a.x, 0);
    float y1 = MapToFrameCoordinatesY(ml->a.y, 0);

    float x2 = MapToFrameCoordinatesX(ml->b.x, 0);
    float y2 = MapToFrameCoordinatesY(ml->b.y, 0);

    float dx = MapToFrameDistanceX(-map_center_x);
    float dy = MapToFrameDistanceY(-map_center_y);

    float linewidth = 3.5f;

    // Lobo 2023: Make keyed doors pulse
    if (automap_keydoor_blink)
    {
        linewidth = game_tic % (32 * (double_framerate.d_ ? 2 : 1));

        if (linewidth >= 16)
            linewidth = 2.0 + (linewidth * 0.1f);
        else
            linewidth = 2.0 - (linewidth * 0.1f);
    }

    HudSolidLine(x1, y1, x2, y2, rgb, linewidth, true, dx, dy);
}

static AutomapLine player_dagger[] = {
    {{-0.75f, 0.0f}, {0.0f, 0.0f}},  // center line

    {{-0.75f, 0.125f}, {1.0f, 0.0f}},  // blade
    {{-0.75f, -0.125f}, {1.0f, 0.0f}},

    {{-0.75, -0.25}, {-0.75, 0.25}},  // crosspiece
    {{-0.875, -0.25}, {-0.875, 0.25}},
    {{-0.875, -0.25}, {-0.75, -0.25}},  // crosspiece connectors
    {{-0.875, 0.25}, {-0.75, 0.25}},
    {{-1.125, 0.125}, {-1.125, -0.125}},  // pommel
    {{-1.125, 0.125}, {-0.875, 0.125}},
    {{-1.125, -0.125}, {-0.875, -0.125}}};

static constexpr uint8_t kAutomapPlayerDaggerLines =
    (sizeof(player_dagger) / sizeof(AutomapLine));

static void DrawLineCharacter(AutomapLine *lineguy, int lineguylines,
                              float radius, BAMAngle angle, RGBAColor rgb,
                              float x, float y)
{
    float cx, cy;

    GetRotatedCoords(x, y, cx, cy);

    cx = MapToFrameCoordinatesX(cx, map_center_x);
    cy = MapToFrameCoordinatesY(cy, map_center_y);

    if (radius < FrameToMapScale(2)) radius = FrameToMapScale(2);

    angle = GetRotatedAngle(angle);

    for (int i = 0; i < lineguylines; i++)
    {
        float ax = lineguy[i].a.x;
        float ay = lineguy[i].a.y;

        if (angle) Rotate(ax, ay, angle);

        float bx = lineguy[i].b.x;
        float by = lineguy[i].b.y;

        if (angle) Rotate(bx, by, angle);

        ax = ax * MapToFrameDistanceX(radius);
        ay = ay * MapToFrameDistanceY(radius);
        bx = bx * MapToFrameDistanceX(radius);
        by = by * MapToFrameDistanceY(radius);

        HudSolidLine(cx + ax, cy - ay, cx + bx, cy - by, rgb);
    }
}

// Aux2StringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
std::string Aux2StringReplaceAll(std::string str, const std::string &from,
                                 const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos +=
            to.length();  // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// Lobo 2023: draw some key info in the middle of a line
static void DrawKeyOnLine(AutomapLine *ml, int theKey,
                          RGBAColor rgb = SG_WHITE_RGBA32)
{
    if (hide_lines) return;

    if (automap_keydoor_text.d_ ==
        0)  // Only if we have Keyed Doors Named turned on
        return;

    static const MapObjectDefinition *TheObject;
    std::string                       CleanName;
    CleanName.clear();

    if (theKey == kDoorKeyStrictlyAllKeys) { CleanName = "All keys"; }
    else
    {
        TheObject = mobjtypes.LookupDoorKey(theKey);
        if (!TheObject) return;  // Very rare, only zombiesTC hits this so far
        CleanName = Aux2StringReplaceAll(TheObject->name_, std::string("_"),
                                         std::string(" "));
    }

    // *********************
    // Draw Text description
    // Calculate midpoint
    float midx = (ml->a.x + ml->b.x) / 2;
    float midy = (ml->a.y + ml->b.y) / 2;

    // Translate map coords to hud coords
    float x1 = MapToFrameCoordinatesX(midx, map_center_x);
    float y1 = MapToFrameCoordinatesY(midy, map_center_y);

    Font *am_font = automap_style->fonts_[0];

    HudSetFont(am_font);
    HudSetAlignment(0, 0);  // centre the characters
    HudSetTextColor(rgb);
    float TextSize = 0.4f * map_scale;
    if (map_scale > 5.0f)  // only draw the text if we're zoomed in?
    {
        if (automap_keydoor_text.d_ == 1)
        {
            HudDrawText(x1, y1, CleanName.c_str(), TextSize);
        }
        else if (automap_keydoor_text.d_ > 1)
        {
            if (TheObject)
            {
                static State *idlestate;
                idlestate = &states[TheObject->idle_state_];
                if (!(idlestate->flags &
                      kStateFrameFlagModel))  // Can't handle 3d models...yet
                {
                    bool           flip;
                    const Image *img = RendererGetOtherSprite(
                        idlestate->sprite, idlestate->frame, &flip);

                    if (epi::StringCaseCompareASCII("DUMMY_SPRITE",
                                                    img->name_) != 0)
                        HudDrawImageNoOffset(x1, y1, img);
                    // HudStretchImage(x1, y1, 16, 16, img, 0.0, 0.0);
                }
            }
        }
    }

    HudSetFont();
    HudSetTextColor();
    HudSetAlignment();

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
    AutomapLine ml;

    int grid_size = HMM_MAX(4, automap_gridsize.d_);

    int mx0 = int(map_center_x);
    int my0 = int(map_center_y);

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
        float x1 = MapToFrameCoordinatesX(mx0 - jx * grid_size, map_center_x);
        float x2 = MapToFrameCoordinatesX(mx0 + jx * grid_size, map_center_x);

        if (x1 < frame_x && x2 >= frame_x + frame_width) break;

        ml.a.x = mx0 + jx * ((j & 1) ? -grid_size : grid_size);
        ml.b.x = ml.a.x;

        ml.a.y = -9e6;
        ml.b.y = +9e6;

        DrawMLine(&ml, am_colors[kAutomapColorGrid], false);
    }

    for (int k = 1; k < 1024; k++)
    {
        int ky = ((k & ~1) >> 1);

        // stop when both lines are off the screen
        float y1 = MapToFrameCoordinatesY(my0 + ky * grid_size, map_center_y);
        float y2 = MapToFrameCoordinatesY(my0 - ky * grid_size, map_center_y);

        if (y1 < frame_y && y2 >= frame_y + frame_height) break;

        ml.a.x = -9e6;
        ml.b.x = +9e6;

        ml.a.y = my0 + ky * ((k & 1) ? -grid_size : grid_size);
        ml.b.y = ml.a.y;

        DrawMLine(&ml, am_colors[kAutomapColorGrid], false);
    }
}

//
// Checks whether the two sectors' regions are similiar.  If they are
// different enough, a line will be drawn on the automap.
//
// -AJA- 1999/12/07: written.
//
static bool CheckSimiliarRegions(Sector *front, Sector *back)
{
    Extrafloor *F, *B;

    if (front->tag == back->tag) return true;

    // Note: doesn't worry about liquids

    F = front->bottom_extrafloor;
    B = back->bottom_extrafloor;

    while (F && B)
    {
        if (!AlmostEquals(F->top_height, B->top_height)) return false;

        if (!AlmostEquals(F->bottom_height, B->bottom_height)) return false;

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
static void AutomapWalkSeg(Seg *seg)
{
    AutomapLine l;
    Line     *line;

    Sector *front = seg->front_sector;
    Sector *back  = seg->back_sector;

    if (seg->miniseg)
    {
        if (automap_debug_bsp.d_)
        {
            if (seg->partner && seg > seg->partner) return;

            GetRotatedCoords(seg->vertex_1->X, seg->vertex_1->Y, l.a.x, l.a.y);
            GetRotatedCoords(seg->vertex_2->X, seg->vertex_2->Y, l.b.x, l.b.y);
            DrawMLine(&l, epi::MakeRGBA(0, 0, 128), false);
        }
        return;
    }

    line = seg->linedef;
    SYS_ASSERT(line);

    // only draw segs on the _right_ side of linedefs
    if (line->side[1] == seg->sidedef) return;

    GetRotatedCoords(seg->vertex_1->X, seg->vertex_1->Y, l.a.x, l.a.y);
    GetRotatedCoords(seg->vertex_2->X, seg->vertex_2->Y, l.b.x, l.b.y);

    if ((line->flags & MLF_Mapped) || show_walls)
    {
        if ((line->flags & MLF_DontDraw) && !show_walls) return;

        if (!front || !back) { DrawMLine(&l, am_colors[kAutomapColorWall]); }
        else
        {
            // Lobo 2022: give keyed doors the colour of the required key
            if (line->special)
            {
                if (line->special->keys_)
                {
                    if (line->special->keys_ & kDoorKeyStrictlyAllKeys)
                    {
                        DrawMLineDoor(&l, SG_PURPLE_RGBA32);  // purple
                        DrawKeyOnLine(&l, kDoorKeyStrictlyAllKeys);
                    }
                    else if (line->special->keys_ & kDoorKeyBlueCard ||
                             line->special->keys_ & kDoorKeyBlueSkull)
                    {
                        DrawMLineDoor(&l, SG_BLUE_RGBA32);  // blue
                        if (line->special->keys_ &
                            (kDoorKeyBlueSkull | kDoorKeyBlueCard))
                        {
                            DrawKeyOnLine(&l, kDoorKeyBlueCard);
                            DrawKeyOnLine(&l, kDoorKeyBlueSkull);
                        }
                        else if (line->special->keys_ & kDoorKeyBlueCard)
                            DrawKeyOnLine(&l, kDoorKeyBlueCard);
                        else
                            DrawKeyOnLine(&l, kDoorKeyBlueSkull);
                    }
                    else if (line->special->keys_ & kDoorKeyYellowCard ||
                             line->special->keys_ & kDoorKeyYellowSkull)
                    {
                        DrawMLineDoor(&l, SG_YELLOW_RGBA32);  // yellow
                        if (line->special->keys_ &
                            (kDoorKeyYellowSkull | kDoorKeyYellowCard))
                        {
                            DrawKeyOnLine(&l, kDoorKeyYellowCard);
                            DrawKeyOnLine(&l, kDoorKeyYellowSkull);
                        }
                        else if (line->special->keys_ & kDoorKeyYellowCard)
                            DrawKeyOnLine(&l, kDoorKeyYellowCard);
                        else
                            DrawKeyOnLine(&l, kDoorKeyYellowSkull);
                    }
                    else if (line->special->keys_ & kDoorKeyRedCard ||
                             line->special->keys_ & kDoorKeyRedSkull)
                    {
                        DrawMLineDoor(&l, SG_RED_RGBA32);  // red
                        if (line->special->keys_ &
                            (kDoorKeyRedSkull | kDoorKeyRedCard))
                        {
                            DrawKeyOnLine(&l, kDoorKeyRedCard);
                            DrawKeyOnLine(&l, kDoorKeyRedSkull);
                        }
                        else if (line->special->keys_ & kDoorKeyRedCard)
                            DrawKeyOnLine(&l, kDoorKeyRedCard);
                        else
                            DrawKeyOnLine(&l, kDoorKeyRedSkull);
                    }
                    else if (line->special->keys_ & kDoorKeyGreenCard ||
                             line->special->keys_ & kDoorKeyGreenSkull)
                    {
                        DrawMLineDoor(&l, SG_GREEN_RGBA32);  // green
                        if (line->special->keys_ &
                            (kDoorKeyGreenSkull | kDoorKeyGreenCard))
                        {
                            DrawKeyOnLine(&l, kDoorKeyGreenCard);
                            DrawKeyOnLine(&l, kDoorKeyGreenSkull);
                        }
                        else if (line->special->keys_ & kDoorKeyGreenCard)
                            DrawKeyOnLine(&l, kDoorKeyGreenCard);
                        else
                            DrawKeyOnLine(&l, kDoorKeyGreenSkull);
                    }
                    else
                    {
                        DrawMLineDoor(&l, SG_PURPLE_RGBA32);  // purple
                    }
                    return;
                }
            }
            if (line->flags & MLF_Secret)
            {
                // secret door
                if (show_walls)
                    DrawMLine(&l, am_colors[kAutomapColorSecret]);
                else
                    DrawMLine(&l, am_colors[kAutomapColorWall]);
            }
            else if (!AlmostEquals(back->floor_height, front->floor_height))
            {
                float diff = fabs(back->floor_height - front->floor_height);

                // floor level change
                if (diff > 24)
                    DrawMLine(&l, am_colors[kAutomapColorLedge]);
                else
                    DrawMLine(&l, am_colors[kAutomapColorStep]);
            }
            else if (!AlmostEquals(back->ceiling_height, front->ceiling_height))
            {
                // ceiling level change
                DrawMLine(&l, am_colors[kAutomapColorCeil]);
            }
            else if ((front->extrafloor_used > 0 || back->extrafloor_used > 0) &&
                     (front->extrafloor_used != back->extrafloor_used ||
                      !CheckSimiliarRegions(front, back)))
            {
                // -AJA- 1999/10/09: extra floor change.
                DrawMLine(&l, am_colors[kAutomapColorLedge]);
            }
            else if (show_walls)
            {
                DrawMLine(&l, am_colors[kAutomapColorAllmap]);
            }
            else if (line->slide_door)
            {  // Lobo: draw sliding doors on automap
                DrawMLine(&l, am_colors[kAutomapColorCeil]);
            }
        }
    }
    else if (frame_focus->player_ &&
             (show_allmap ||
              !AlmostEquals(frame_focus->player_->powers_[kPowerTypeAllMap],
                            0.0f)))
    {
        if (!(line->flags & MLF_DontDraw))
            DrawMLine(&l, am_colors[kAutomapColorAllmap]);
    }
}

static void DrawObjectBounds(MapObject *mo, RGBAColor rgb)
{
    float R = mo->radius_;

    if (R < 2) R = 2;

    float lx = mo->x - R;
    float ly = mo->y - R;
    float hx = mo->x + R;
    float hy = mo->y + R;

    AutomapLine ml;

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

static RGBAColor player_colors[8] = {
    epi::MakeRGBA(5, 255, 5),      // GREEN,
    epi::MakeRGBA(80, 80, 80),     // GRAY + GRAY_LEN*2/3,
    epi::MakeRGBA(160, 100, 50),   // BROWN,
    epi::MakeRGBA(255, 255, 255),  // RED + RED_LEN/2,
    epi::MakeRGBA(255, 176, 5),    // ORANGE,
    epi::MakeRGBA(170, 170, 170),  // GRAY + GRAY_LEN*1/3,
    epi::MakeRGBA(255, 5, 5),      // RED,
    epi::MakeRGBA(255, 185, 225),  // PINK
};

//
// The vector graphics for the automap.
//
// A line drawing of the player pointing right, starting from the
// middle.

static AutomapLine player_arrow[] = {
    {{-0.875f, 0.0f}, {1.0f, 0.0f}},  // -----

    {{1.0f, 0.0f}, {0.5f, 0.25f}},  // ----->
    {{1.0f, 0.0f}, {0.5f, -0.25f}},

    {{-0.875f, 0.0f}, {-1.125f, 0.25f}},  // >---->
    {{-0.875f, 0.0f}, {-1.125f, -0.25f}},

    {{-0.625f, 0.0f}, {-0.875f, 0.25f}},  // >>--->
    {{-0.625f, 0.0f}, {-0.875f, -0.25f}}};

static constexpr uint8_t kAutomapPlayerArrowLines =
    (sizeof(player_arrow) / sizeof(AutomapLine));

static AutomapLine cheat_player_arrow[] = {
    {{-0.875f, 0.0f}, {1.0f, 0.0f}},  // -----

    {{1.0f, 0.0f}, {0.5f, 0.167f}},  // ----->
    {{1.0f, 0.0f}, {0.5f, -0.167f}},

    {{-0.875f, 0.0f}, {-1.125f, 0.167f}},  // >----->
    {{-0.875f, 0.0f}, {-1.125f, -0.167f}},

    {{-0.625f, 0.0f}, {-0.875f, 0.167f}},  // >>----->
    {{-0.625f, 0.0f}, {-0.875f, -0.167f}},

    {{-0.5f, 0.0f}, {-0.5f, -0.167f}},  // >>-d--->
    {{-0.5f, -0.167f}, {-0.5f + 0.167f, -0.167f}},
    {{-0.5f + 0.167f, -0.167f}, {-0.5f + 0.167f, 0.25f}},

    {{-0.167f, 0.0f}, {-0.167f, -0.167f}},  // >>-dd-->
    {{-0.167f, -0.167f}, {0.0f, -0.167f}},
    {{0.0f, -0.167f}, {0.0f, 0.25f}},

    {{0.167f, 0.25f}, {0.167f, -0.143f}},  // >>-ddt->
    {{0.167f, -0.143f}, {0.167f + 0.031f, -0.143f - 0.031f}},
    {{0.167f + 0.031f, -0.143f - 0.031f}, {0.167f + 0.1f, -0.143f}}};

static constexpr uint8_t kAutomapCheatPlayerArrowLines =
    (sizeof(cheat_player_arrow) / sizeof(AutomapLine));

static AutomapLine thin_triangle_guy[] = {{{-0.5f, -0.7f}, {1.0f, 0.0f}},
                                          {{1.0f, 0.0f}, {-0.5f, 0.7f}},
                                          {{-0.5f, 0.7f}, {-0.5f, -0.7f}}};

static constexpr uint8_t kAutomapThinTriangleGuyLines =
    (sizeof(thin_triangle_guy) / sizeof(AutomapLine));

static void AutomapDrawPlayer(MapObject *mo)
{
    if (automap_debug_collisions.d_)
        DrawObjectBounds(mo, am_colors[kAutomapColorPlayer]);

    if (!network_game)
    {
        switch (current_arrow_type)
        {
            case kAutomapArrowStyleHeretic:
                DrawLineCharacter(player_dagger, kAutomapPlayerDaggerLines,
                                  mo->radius_, mo->angle_,
                                  am_colors[kAutomapColorPlayer], mo->x, mo->y);
                break;
            case kAutomapArrowStyleDoom:
            default:
                if (cheating)
                    DrawLineCharacter(cheat_player_arrow,
                                      kAutomapCheatPlayerArrowLines, mo->radius_,
                                      mo->angle_, am_colors[kAutomapColorPlayer],
                                      mo->x, mo->y);
                else
                    DrawLineCharacter(player_arrow, kAutomapPlayerArrowLines,
                                      mo->radius_, mo->angle_,
                                      am_colors[kAutomapColorPlayer], mo->x,
                                      mo->y);
                break;
        }
        return;
    }

    DrawLineCharacter(player_arrow, kAutomapPlayerArrowLines, mo->radius_,
                      mo->angle_, player_colors[mo->player_->player_number_ & 0x07], mo->x,
                      mo->y);
}

static void AutomapWalkThing(MapObject *mo)
{
    int index = kAutomapColorScenery;

    if (mo->player_ && mo->player_->map_object_ == mo)
    {
        AutomapDrawPlayer(mo);
        return;
    }

    if (!show_things) return;

    // -AJA- more colourful things
    if (mo->flags_ & kMapObjectFlagSpecial)
        index = kAutomapColorItem;
    else if (mo->flags_ & kMapObjectFlagMissile)
        index = kAutomapColorMissile;
    else if (mo->extended_flags_ & kExtendedFlagMonster && mo->health_ <= 0)
        index = kAutomapColorCorpse;
    else if (mo->extended_flags_ & kExtendedFlagMonster)
        index = kAutomapColorMonster;

    if (automap_debug_collisions.d_)
    {
        DrawObjectBounds(mo, am_colors[index]);
        return;
    }

    DrawLineCharacter(thin_triangle_guy, kAutomapThinTriangleGuyLines,
                      mo->radius_, mo->angle_, am_colors[index], mo->x, mo->y);
}

//
// Visit a subsector and draw everything.
//
static void AutomapWalkSubsector(unsigned int num)
{
    Subsector *sub = &level_subsectors[num];

    // handle each seg
    for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
    {
        AutomapWalkSeg(seg);
    }

    // handle each thing
    for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
    {
        AutomapWalkThing(mo);
    }
}

//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
static bool AutomapCheckBBox(float *bspcoord)
{
    float L = bspcoord[kBoundingBoxLeft];
    float R = bspcoord[kBoundingBoxRight];
    float T = bspcoord[kBoundingBoxTop];
    float B = bspcoord[kBoundingBoxBottom];

    if (rotate_map)
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
    float x1 = MapToFrameCoordinatesX(L, map_center_x);
    float x2 = MapToFrameCoordinatesX(R, map_center_x);

    float y1 = MapToFrameCoordinatesY(T, map_center_y);
    float y2 = MapToFrameCoordinatesY(B, map_center_y);

    return !(x2 < frame_x - 1 || x1 > frame_x + frame_width + 1 ||
             y2 < frame_y - 1 || y1 > frame_y + frame_height + 1);
}

//
// Walks all subsectors below a given node, traversing subtree
// recursively.  Just call with BSP root.
//
static void AutomapWalkBSPNode(unsigned int bspnum)
{
    BspNode *node;
    int     side;

    // Found a subsector?
    if (bspnum & NF_V5_SUBSECTOR)
    {
        AutomapWalkSubsector(bspnum & (~NF_V5_SUBSECTOR));
        return;
    }

    node = &level_nodes[bspnum];
    side = 0;

    // Recursively divide right space
    if (AutomapCheckBBox(node->bounding_boxes[0]))
        AutomapWalkBSPNode(node->children[side]);

    // Recursively divide back space
    if (AutomapCheckBBox(node->bounding_boxes[side ^ 1]))
        AutomapWalkBSPNode(node->children[side ^ 1]);
}

static void DrawMarks(void)
{
    Font *am_font = automap_style->fonts_[0];

    HudSetFont(am_font);
    HudSetAlignment(0, 0);  // centre the characters

    char buffer[4];

    for (int i = 0; i < kAutomapTotalMarkPoints; i++)
    {
        if (AlmostEquals(mark_points[i].x, (float)kAutomapNoMarkX)) continue;

        float mx, my;

        GetRotatedCoords(mark_points[i].x, mark_points[i].y, mx, my);

        buffer[0] = ('1' + i);
        buffer[1] = 0;

        HudDrawText(MapToFrameCoordinatesX(mx, map_center_x),
                     MapToFrameCoordinatesY(my, map_center_y), buffer);
    }

    HudSetFont();
    HudSetAlignment();
}

void AutomapRender(float x, float y, float w, float h, MapObject *focus)
{
    frame_x      = x;
    frame_y      = y;
    frame_width  = w;
    frame_height = h;

    frame_scale = HMM_MAX(frame_width, frame_height) / map_size / 2.0f;
    frame_focus = focus;

    if (follow_player)
    {
        map_center_x = frame_focus->x;
        map_center_y = frame_focus->y;
    }

    SYS_ASSERT(automap_style);

    if (automap_style->background_image_)
    {
        float old_alpha = HudGetAlpha();
        HudSetAlpha(automap_style->definition_->bg_.translucency_);
        if (automap_style->definition_->special_ == 0)
            HudStretchImage(-90, 0, 500, 200, automap_style->background_image_, 0.0,
                             0.0);
        else
            HudTileImage(-90, 0, 500, 200, automap_style->background_image_, 0.0, 0.0);
        HudSetAlpha(old_alpha);
    }
    else if (automap_style->definition_->bg_.colour_ != kRGBANoValue)
    {
        float old_alpha = HudGetAlpha();
        HudSetAlpha(automap_style->definition_->bg_.translucency_);
        HudSolidBox(x, y, x + w, y + h, automap_style->definition_->bg_.colour_);
        HudSetAlpha(old_alpha);
    }

    if (grid && !rotate_map) DrawGrid();

    // walk the bsp tree
    AutomapWalkBSPNode(root_node);

    DrawMarks();
}

void AutomapSetColor(int which, RGBAColor color)
{
    SYS_ASSERT(0 <= which && which < kTotalAutomapColors);

    am_colors[which] = color;
}

void AutomapGetState(int *state, float *zoom)
{
    *state = 0;

    if (grid) *state |= kAutomapStateGrid;

    if (follow_player) *state |= kAutomapStateFollow;

    if (rotate_map) *state |= kAutomapStateRotate;

    if (show_things) *state |= kAutomapStateThings;

    if (show_walls) *state |= kAutomapStateWalls;

    if (hide_lines) *state |= kAutomapStateHideLines;

    // nothing required for kAutomapStateAllmap flag (no actual state)

    *zoom = map_scale;
}

void AutomapSetState(int state, float zoom)
{
    grid          = (state & kAutomapStateGrid) ? true : false;
    follow_player = (state & kAutomapStateFollow) ? true : false;
    rotate_map    = (state & kAutomapStateRotate) ? true : false;

    show_things = (state & kAutomapStateThings) ? true : false;
    show_walls  = (state & kAutomapStateWalls) ? true : false;
    show_allmap = (state & kAutomapStateAllmap) ? true : false;
    hide_lines  = (state & kAutomapStateHideLines) ? true : false;

    map_scale = zoom;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
