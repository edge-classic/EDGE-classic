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
#include "dm_state.h"
#include "e_input.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_str_compare.h"
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

EDGE_DEFINE_CONSOLE_VARIABLE(automap_debug_collisions, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(automap_keydoor_text, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(automap_gridsize, "128", kConsoleVariableFlagArchive, 16, 1024)

extern unsigned int root_node;

// Automap colors

// NOTE: this order must match the one in the COAL API script
static RGBAColor am_colors[kTotalAutomapColors] = {
    epi::MakeRGBA(40, 40, 112),   // kAutomapColorGrid
    epi::MakeRGBA(112, 112, 112), // kAutomapColorAllmap
    epi::MakeRGBA(255, 0, 0),     // kAutomapColorWall
    epi::MakeRGBA(192, 128, 80),  // kAutomapColorStep
    epi::MakeRGBA(192, 128, 80),  // kAutomapColorLedge
    epi::MakeRGBA(220, 220, 0),   // kAutomapColorCeil
    epi::MakeRGBA(0, 200, 200),   // kAutomapColorSecret

    epi::MakeRGBA(255, 255, 255), // kAutomapColorPlayer
    epi::MakeRGBA(0, 255, 0),     // kAutomapColorMonster
    epi::MakeRGBA(220, 0, 0),     // kAutomapColorCorpse
    epi::MakeRGBA(0, 0, 255),     // kAutomapColorItem
    epi::MakeRGBA(255, 188, 0),   // kAutomapColorMissile
    epi::MakeRGBA(120, 60, 30)    // kAutomapColorScenery
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

static constexpr float kAutomapInitialScale = 2.0f;
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
static float    frame_x, frame_y;
static float    frame_width, frame_height;
static float    frame_lerped_x, frame_lerped_y;
static BAMAngle frame_lerped_ang;

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
static HMM_Vec2 mark_points[kAutomapTotalMarkPoints];

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

extern Style *automap_style; // FIXME: put in header

// Used for batching GL_LINES (or Sokol equivalent) calls
static float map_alpha       = 1.0f;
static float map_pulse_width = 2.0f;
static float map_dx          = 0.0f;
static float map_dy          = 0.0f;

struct AutomapKey
{
    float x;
    float y;
    int   type;
};

static std::vector<AutomapKey> automap_keys;

std::vector<AutomapLine *> automap_lines;
static size_t              automap_line_position = 0;

// Automap line "buckets"
// 4 potential thicknesses (1.0, 1.5, 3.5, and whatever the pulsing
// door thickness is at that tic)
static std::vector<AutomapLine *> map_line_pointers[4];

static AutomapLine *GetMapLine()
{
    if (automap_line_position == automap_lines.size())
        automap_lines.push_back(new AutomapLine());

    return automap_lines[automap_line_position++];
}

static constexpr uint16_t kMaximumLineVerts = kDefaultAutomapLines / 2;

static void DrawAllLines()
{
    size_t          current_vert_count = 0;
    RendererVertex *current_glvert     = nullptr;
    for (int i = 3; i >= 0; i--)
    {
        if (!map_line_pointers[i].empty())
        {
            StartUnitBatch(false);
            if (i == 3)
                render_state->LineWidth(map_pulse_width);
            else if (i == 2)
                render_state->LineWidth(3.5f);
            else if (i == 1)
                render_state->LineWidth(1.5f);
            else
                render_state->LineWidth(1.0f);
            current_glvert = BeginRenderUnit(GL_LINES, kMaximumLineVerts, GL_MODULATE, 0,
                                             (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);
            for (AutomapLine *line : map_line_pointers[i])
            {
                RGBAColor col    = line->color;
                HMM_Vec4 *points = &line->points;
                if (current_vert_count > kMaximumLineVerts - 2)
                {
                    EndRenderUnit(current_vert_count);
                    FinishUnitBatch();
                    current_vert_count = 0;
                    StartUnitBatch(false);
                    current_glvert = BeginRenderUnit(GL_LINES, kMaximumLineVerts, GL_MODULATE, 0,
                                                     (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);
                }
                current_glvert->position = {{points->X, points->Y, 0}};
                current_glvert++->rgba   = col;
                current_glvert->position = {{points->Z, points->W, 0}};
                current_glvert++->rgba   = col;
                current_vert_count += 2;
            }
            EndRenderUnit(current_vert_count);
            FinishUnitBatch();
            current_vert_count = 0;
        }
    }
    render_state->LineWidth(1.0f);
    automap_line_position = 0;
    map_line_pointers[0].clear();
    map_line_pointers[1].clear();
    map_line_pointers[2].clear();
    map_line_pointers[3].clear();
}

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
    mark_points[mark_point_number].X = map_center_x;
    mark_points[mark_point_number].Y = map_center_y;

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
        mark_points[i].X = kAutomapNoMarkX;

    mark_point_number = 0;
}

void AutomapSetArrow(AutomapArrowStyle type)
{
    if (type >= kAutomapArrowStyleDoom && type < kTotalAutomapArrowStyles)
        current_arrow_type = type;
}

void AutomapInitLevel(void)
{
    if (!cheat_automap.sequence)
    {
        cheat_automap.sequence = language["iddt"];
    }

    ClearMarks();

    FindMinMaxBoundaries();

    // Initial reservation if necessary
    if (map_line_pointers[0].capacity() < kDefaultAutomapLines)
    {
        map_line_pointers[0].reserve(kDefaultAutomapLines);
        map_line_pointers[1].reserve(kDefaultAutomapLines);
        map_line_pointers[2].reserve(kDefaultAutomapLines);
        map_line_pointers[3].reserve(kDefaultAutomapLines);
    }

    if (map_scale == 0.0f) // Not been changed yet so set a default
    {
        map_scale = kAutomapInitialScale;
    }
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
    if (ev->type == kInputEventKeyDown && CheckKeyMatch(key_map, sym))
    {
        if (automap_active)
            AutomapHide();
        else
            AutomapShow();
        return true;
    }

    if (!automap_active)
        return false;

    // --- handle key releases ---

    if (ev->type == kInputEventKeyUp)
    {
        if (CheckKeyMatch(key_automap_left, sym) || CheckKeyMatch(key_automap_right, sym))
            panning_x = 0;

        if (CheckKeyMatch(key_automap_up, sym) || CheckKeyMatch(key_automap_down, sym))
            panning_y = 0;

        if (CheckKeyMatch(key_automap_zoom_in, sym) || CheckKeyMatch(key_automap_zoom_out, sym))
            zooming = -1;

        return false;
    }

    // --- handle key presses ---

    if (ev->type != kInputEventKeyDown)
        return false;

    // Had to move the automap cheat check up here thanks to Heretic's 'ravmap'
    // cheat - Dasho -ACB- 1999/09/28 Proper casting
    if (CheckCheatSequence(&cheat_automap, (char)sym) && !InDeathmatch())
    {
        cheating = (cheating + 1) % 3;

        show_things = (cheating == 2) ? true : false;
        show_walls  = (cheating >= 1) ? true : false;
    }

    if (!follow_player)
    {
        if (CheckKeyMatch(key_automap_left, sym))
        {
            panning_x = -FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (CheckKeyMatch(key_automap_right, sym))
        {
            panning_x = FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (CheckKeyMatch(key_automap_up, sym))
        {
            panning_y = FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
        else if (CheckKeyMatch(key_automap_down, sym))
        {
            panning_y = -FrameToMapScale(kAutomapFrameBufferPanIncrement);
            return true;
        }
    }

    if (CheckKeyMatch(key_automap_zoom_in, sym))
    {
        zooming = kAutomapZoomPerTic;
        return true;
    }
    else if (CheckKeyMatch(key_automap_zoom_out, sym))
    {
        zooming = 1.0 / kAutomapZoomPerTic;
        return true;
    }

    if (CheckKeyMatch(key_automap_follow, sym))
    {
        follow_player = !follow_player;

        // -ACB- 1998/08/10 Use DDF Lang Reference
        if (follow_player)
            ConsoleMessage(kConsoleHUDTop, "%s", language["AutoMapFollowOn"]);
        else
            ConsoleMessage(kConsoleHUDTop, "%s", language["AutoMapFollowOff"]);

        return true;
    }

    if (CheckKeyMatch(key_automap_grid, sym))
    {
        grid = !grid;
        // -ACB- 1998/08/10 Use DDF Lang Reference
        if (grid)
            ConsoleMessage(kConsoleHUDTop, "%s", language["AutoMapGridOn"]);
        else
            ConsoleMessage(kConsoleHUDTop, "%s", language["AutoMapGridOff"]);

        return true;
    }

    if (CheckKeyMatch(key_automap_mark, sym))
    {
        // -ACB- 1998/08/10 Use DDF Lang Reference
        ConsoleMessage(kConsoleHUDTop, "%s %d", language["AutoMapMarkedSpot"], mark_point_number + 1);
        AddMark();
        return true;
    }

    if (CheckKeyMatch(key_automap_clear, sym))
    {
        // -ACB- 1998/08/10 Use DDF Lang Reference
        ConsoleMessage(kConsoleHUDTop, "%s", language["AutoMapMarksClear"]);
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
    if (!automap_active)
        return;

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
        if (!console_active && !paused && !menu_active)
        {
            dx -= frame_lerped_x;
            dy -= frame_lerped_y;

            Rotate(dx, dy, kBAMAngle90 - frame_lerped_ang);

            dx += frame_lerped_x;
            dy += frame_lerped_y;
        }
        else
        {
            dx -= frame_focus->x;
            dy -= frame_focus->y;

            Rotate(dx, dy, kBAMAngle90 - frame_focus->angle_);

            dx += frame_focus->x;
            dy += frame_focus->y;
        }
    }
}

static inline BAMAngle GetRotatedAngle(BAMAngle src)
{
    if (rotate_map)
    {
        if (!console_active && !paused && !menu_active)
        {
            float ang = epi::BAMInterpolate(frame_focus->old_angle_, frame_focus->angle_, fractional_tic);
            return src + kBAMAngle90 - ang;
        }
        else
        {
            return src + kBAMAngle90 - frame_focus->angle_;
        }
    }

    return src;
}

//
// Draw visible parts of lines.
//
static void DrawMLine(AutomapLine *ml, bool thick = true)
{
    HMM_Vec4 *points = &ml->points;

    points->X = HUDToRealCoordinatesX(MapToFrameCoordinatesX(points->X, 0)) + map_dx;
    points->Y = HUDToRealCoordinatesY(MapToFrameCoordinatesY(points->Y, 0)) + map_dy;

    points->Z = HUDToRealCoordinatesX(MapToFrameCoordinatesX(points->Z, 0)) + map_dx;
    points->W = HUDToRealCoordinatesY(MapToFrameCoordinatesY(points->W, 0)) + map_dy;

    epi::SetRGBAAlpha(ml->color, map_alpha);

    if (thick)
        map_line_pointers[1].push_back(ml); // 1.5f
    else
        map_line_pointers[0].push_back(ml); // 1.0f
}

// Lobo 2022: keyed doors automap colouring
static void DrawMLineDoor(AutomapLine *ml)
{
    HMM_Vec4 *points = &ml->points;

    points->X = HUDToRealCoordinatesX(MapToFrameCoordinatesX(points->X, 0)) + map_dx;
    points->Y = HUDToRealCoordinatesY(MapToFrameCoordinatesY(points->Y, 0)) + map_dy;

    points->Z = HUDToRealCoordinatesX(MapToFrameCoordinatesX(points->Z, 0)) + map_dx;
    points->W = HUDToRealCoordinatesY(MapToFrameCoordinatesY(points->W, 0)) + map_dy;

    epi::SetRGBAAlpha(ml->color, map_alpha);

    // Lobo 2023: Make keyed doors pulse
    if (automap_keydoor_blink)
        map_line_pointers[3].push_back(ml); // variable pulse width
    else
        map_line_pointers[2].push_back(ml); // 3.5f
}

static HMM_Vec4 player_dagger[] = {
    {{{{{-0.75f, 0.0f, 0.0f}}}, 0.0f}},                                              // center line

    {{{{{-0.75f, 0.125f, 1.0f}}}, 0.0f}},                                            // blade
    {{{{{-0.75f, -0.125f, 1.0f}}}, 0.0f}},

    {{{{{-0.75, -0.25, -0.75}}}, 0.25}},                                             // crosspiece
    {{{{{-0.875, -0.25, -0.875}}}, 0.25}},  {{{{{-0.875, -0.25, -0.75}}}, -0.25}},   // crosspiece connectors
    {{{{{-0.875, 0.25, -0.75}}}, 0.25}},    {{{{{-1.125, 0.125, -1.125}}}, -0.125}}, // pommel
    {{{{{-1.125, 0.125, -0.875}}}, 0.125}}, {{{{{-1.125, -0.125, -0.875}}}, -0.125}}};

static constexpr uint8_t kAutomapPlayerDaggerLines = (sizeof(player_dagger) / sizeof(HMM_Vec4));

static void DrawLineCharacter(HMM_Vec4 *lineguy, int lineguylines, float radius, BAMAngle angle, RGBAColor rgb, float x,
                              float y)
{
    float cx, cy;

    GetRotatedCoords(x, y, cx, cy);

    cx = MapToFrameCoordinatesX(cx, map_center_x);
    cy = MapToFrameCoordinatesY(cy, map_center_y);

    if (radius < FrameToMapScale(2))
        radius = FrameToMapScale(2);

    angle = GetRotatedAngle(angle);

    RGBAColor line_col = rgb;
    epi::SetRGBAAlpha(line_col, map_alpha);

    for (int i = 0; i < lineguylines; i++)
    {
        float ax = lineguy[i].X;
        float ay = lineguy[i].Y;

        if (angle)
            Rotate(ax, ay, angle);

        float bx = lineguy[i].Z;
        float by = lineguy[i].W;

        if (angle)
            Rotate(bx, by, angle);

        ax = ax * MapToFrameDistanceX(radius);
        ay = ay * MapToFrameDistanceY(radius);
        bx = bx * MapToFrameDistanceX(radius);
        by = by * MapToFrameDistanceY(radius);

        AutomapLine *ml = GetMapLine();

        HMM_Vec4 *points = &ml->points;

        points->X = HUDToRealCoordinatesX(cx + ax);
        points->Y = HUDToRealCoordinatesY(cy - ay);

        points->Z = HUDToRealCoordinatesX(cx + bx);
        points->W = HUDToRealCoordinatesY(cy - by);

        ml->color = line_col;

        map_line_pointers[0].push_back(ml); // 1.0f
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

// Lobo: default to showing the keycard instead of the skullkey for non-boom doors
static int GetKeyNumber(int KeyType)
{
    int converted = KeyType;
   
    //If it doesn't matter Card or Skull, prefer card and swap it in.
    switch (KeyType)
    {
        case (kDoorKeyBlueCard | kDoorKeyBlueSkull):
            converted = kDoorKeyBlueCard;
            break;

        case (kDoorKeyRedCard | kDoorKeyRedSkull):
            converted = kDoorKeyRedCard;
             break;

        case (kDoorKeyYellowCard | kDoorKeyYellowSkull):
            converted = kDoorKeyYellowCard;
            break;
    }
    return converted;
}


// Lobo 2023: draw some key info in the middle of a line
static void DrawKeys()
{
    if (automap_keydoor_text.d_ == 0) // Only if we have Keyed Doors Named turned on
        return;

    const MapObjectDefinition *TheObject = nullptr;
    Font                      *am_font   = nullptr;
    std::string                CleanName;

    if (automap_keydoor_text.d_ == 1)
    {
        am_font = automap_style->fonts_[0];
        EPI_ASSERT(am_font);
        HUDSetFont(am_font);
        HUDSetTextColor(kRGBAWhite);
    }

    HUDSetAlignment(0, 0); // centre

    for (const AutomapKey &key : automap_keys)
    {
        
        //TheObject = mobjtypes.LookupDoorKey(key.type);
        TheObject = mobjtypes.LookupDoorKey(GetKeyNumber(key.type));
        if (!TheObject)
            continue; // Very rare, only zombiesTC hits this so far


        if (automap_keydoor_text.d_ == 1)
        {
            CleanName.clear();

            if (key.type == kDoorKeyStrictlyAllKeys)
            {
                CleanName = "All keys";
            }
            else
            {
                CleanName = Aux2StringReplaceAll(TheObject->name_, std::string("_"), std::string(" "));
            }

            HUDDrawText(key.x, key.y, CleanName.c_str(), 0.75f * map_scale);
        }
        else if (automap_keydoor_text.d_ > 1)
        {
            State *idlestate = &states[TheObject->idle_state_];
            if (!(idlestate->flags & kStateFrameFlagModel)) // Can't handle 3d models...yet
            {
                bool         flip;
                const Image *img = GetOtherSprite(idlestate->sprite, idlestate->frame, &flip);

                if (epi::StringCaseCompareASCII("DUMMY_SPRITE", img->name_) != 0)
                    HUDStretchImageNoOffset(key.x, key.y, 2 * map_scale * ((float)img->width_ / img->height_),
                                            2 * map_scale, img, 0, 0);
            }
        }
    }

    if (automap_keydoor_text.d_ == 1)
    {
        HUDSetFont();
        HUDSetTextColor();
    }

    HUDSetAlignment();

    automap_keys.clear();
}

//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
static void CollectGridLines()
{
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

        if (x1 < frame_x && x2 >= frame_x + frame_width)
            break;

        AutomapLine *ml = GetMapLine();

        HMM_Vec4 *points = &ml->points;

        points->X = mx0 + jx * ((j & 1) ? -grid_size : grid_size);
        points->Z = points->X;

        points->Y = -9e6;
        points->W = +9e6;

        ml->color = am_colors[kAutomapColorGrid];

        DrawMLine(ml, false);
    }

    for (int k = 1; k < 1024; k++)
    {
        int ky = ((k & ~1) >> 1);

        // stop when both lines are off the screen
        float y1 = MapToFrameCoordinatesY(my0 + ky * grid_size, map_center_y);
        float y2 = MapToFrameCoordinatesY(my0 - ky * grid_size, map_center_y);

        if (y1 < frame_y && y2 >= frame_y + frame_height)
            break;

        AutomapLine *ml = GetMapLine();

        HMM_Vec4 *points = &ml->points;

        points->X = -9e6;
        points->Z = +9e6;

        points->Y = my0 + ky * ((k & 1) ? -grid_size : grid_size);
        points->W = points->Y;

        ml->color = am_colors[kAutomapColorGrid];

        DrawMLine(ml, false);
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

    if (front->tag == back->tag)
        return true;

    // Note: doesn't worry about liquids

    F = front->bottom_extrafloor;
    B = back->bottom_extrafloor;

    while (F && B)
    {
        if (!AlmostEquals(F->top_height, B->top_height))
            return false;

        if (!AlmostEquals(F->bottom_height, B->bottom_height))
            return false;

        F = F->higher;
        B = B->higher;
    }

    return (F || B) ? false : true;
}


//
// Determines visible lines, draws them.
//
static void AddWall(const Line *line)
{
    EPI_ASSERT(line);

    if ((line->flags & kLineFlagMapped) || show_walls)
    {
        if ((line->flags & kLineFlagDontDraw) && !show_walls)
            return;

        AutomapLine *l = GetMapLine();
        GetRotatedCoords(line->vertex_1->X, line->vertex_1->Y, l->points.X, l->points.Y);
        GetRotatedCoords(line->vertex_2->X, line->vertex_2->Y, l->points.Z, l->points.W);

        // clip to map frame
        float x1 = MapToFrameCoordinatesX(l->points.X, map_center_x);
        float x2 = MapToFrameCoordinatesX(l->points.Z, map_center_x);
        float y1 = MapToFrameCoordinatesY(l->points.Y, map_center_y);
        float y2 = MapToFrameCoordinatesY(l->points.W, map_center_y);
        if ((x1 < frame_x && x2 < frame_x) || (x1 > frame_x + frame_width && x2 > frame_x + frame_width) ||
            (y1 < frame_y && y2 < frame_y) || (y1 > frame_y + frame_height && y2 > frame_y + frame_height))
        {
            automap_line_position--;
            return;
        }

        Sector *front = line->front_sector;
        Sector *back  = line->back_sector;

        if (!front || !back)
        {
            l->color = am_colors[kAutomapColorWall];
            DrawMLine(l);
        }
        else
        {
            // Lobo 2022: give keyed doors the colour of the required key
            if (line->special)
            {
                if (line->special->keys_)
                {
                    float midx = MapToFrameCoordinatesX((l->points.X + l->points.Z) / 2, map_center_x);
                    float midy = MapToFrameCoordinatesY((l->points.Y + l->points.W) / 2, map_center_y);

                    if (line->special->keys_ & kDoorKeyStrictlyAllKeys)
                    {
                        l->color = kRGBAPurple;
                        DrawMLineDoor(l); // purple
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back({midx, midy, (int)kDoorKeyStrictlyAllKeys});
                    }
                    else if (line->special->keys_ == (kDoorKeyRedCard | kDoorKeyRedSkull | kDoorKeyBlueCard | kDoorKeyBlueSkull | kDoorKeyYellowCard
                                     | kDoorKeyYellowSkull))
                    {
                        l->color = kRGBAFuchsia;
                        DrawMLineDoor(l); // orange
                        GetKeyNumber(line->special->keys_);
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back(
                                {midx, midy,
                                 (int)(line->special->keys_)});
                    }
                    else if (line->special->keys_ & (kDoorKeyBlueSkull | kDoorKeyBlueCard))
                    {
                        l->color = kRGBABlue;
                        DrawMLineDoor(l); // blue
                        GetKeyNumber(line->special->keys_);
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back(
                                {midx, midy,
                                 (int)(line->special->keys_)});
                    }
                    else if (line->special->keys_ & (kDoorKeyYellowSkull | kDoorKeyYellowCard))
                    {
                        l->color = kRGBAYellow;
                        DrawMLineDoor(l); // yellow
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back(
                                {midx, midy,
                                 (int)(line->special->keys_)});
                    }
                    else if (line->special->keys_ & (kDoorKeyRedSkull | kDoorKeyRedCard))
                    {
                        l->color = kRGBARed;
                        DrawMLineDoor(l); // red
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back(
                                {midx, midy,
                                 (int)(line->special->keys_)});
                    }
                    else if (line->special->keys_ & (kDoorKeyGreenSkull | kDoorKeyGreenCard))
                    {
                        l->color = kRGBAGreen;
                        DrawMLineDoor(l); // green
                        if (automap_keydoor_text.d_ > 0)
                            automap_keys.push_back(
                                {midx, midy,
                                 (int)(line->special->keys_)});
                    }
                    else
                    {
                        l->color = kRGBAPurple;
                        DrawMLineDoor(l); // purple
                    }
                    return;
                }
            }
            if (line->flags & kLineFlagSecret)
            {
                // secret door
                if (show_walls)
                    l->color = am_colors[kAutomapColorSecret];
                else
                    l->color = am_colors[kAutomapColorWall];
                DrawMLine(l);
            }
            else if (!AlmostEquals(back->floor_height, front->floor_height))
            {
                float diff = fabs(back->floor_height - front->floor_height);

                // floor level change
                if (diff > 24)
                    l->color = am_colors[kAutomapColorLedge];
                else
                    l->color = am_colors[kAutomapColorStep];
                DrawMLine(l);
            }
            else if (!AlmostEquals(back->ceiling_height, front->ceiling_height))
            {
                // ceiling level change
                l->color = am_colors[kAutomapColorCeil];
                DrawMLine(l);
            }
            else if ((front->extrafloor_used > 0 || back->extrafloor_used > 0) &&
                     (front->extrafloor_used != back->extrafloor_used || !CheckSimiliarRegions(front, back)))
            {
                // -AJA- 1999/10/09: extra floor change.
                l->color = am_colors[kAutomapColorLedge];
                DrawMLine(l);
            }
            else if (show_walls)
            {
                l->color = am_colors[kAutomapColorAllmap];
                DrawMLine(l);
            }
            else if (line->slide_door)
            { // Lobo: draw sliding doors on automap
                l->color = am_colors[kAutomapColorCeil];
                DrawMLine(l);
            }
        }
    }
    else if (frame_focus->player_ &&
             (show_allmap || !AlmostEquals(frame_focus->player_->powers_[kPowerTypeAllMap], 0.0f)))
    {
        if (!(line->flags & kLineFlagDontDraw))
        {
            AutomapLine *l = GetMapLine();
            GetRotatedCoords(line->vertex_1->X, line->vertex_1->Y, l->points.X, l->points.Y);
            GetRotatedCoords(line->vertex_2->X, line->vertex_2->Y, l->points.Z, l->points.W);
            // clip to map frame
            float x1 = MapToFrameCoordinatesX(l->points.X, map_center_x);
            float x2 = MapToFrameCoordinatesX(l->points.Z, map_center_x);
            float y1 = MapToFrameCoordinatesY(l->points.Y, map_center_y);
            float y2 = MapToFrameCoordinatesY(l->points.W, map_center_y);
            if ((x1 < frame_x && x2 < frame_x) || (x1 > frame_x + frame_width && x2 > frame_x + frame_width) ||
                (y1 < frame_y && y2 < frame_y) || (y1 > frame_y + frame_height && y2 > frame_y + frame_height))
            {
                automap_line_position--;
                return;
            }
            l->color = am_colors[kAutomapColorAllmap];
            DrawMLine(l);
        }
    }
}

static void DrawObjectBounds(MapObject *mo, RGBAColor rgb)
{
    float R = mo->radius_;
    float lx, ly, hx, hy;

    if (R < 2)
        R = 2;

    if (!console_active && !paused && !menu_active)
    {
        lx = HMM_Lerp(mo->old_x_, fractional_tic, mo->x) - R;
        ly = HMM_Lerp(mo->old_y_, fractional_tic, mo->y) - R;
        hx = HMM_Lerp(mo->old_x_, fractional_tic, mo->x) + R;
        hy = HMM_Lerp(mo->old_y_, fractional_tic, mo->y) + R;
    }
    else
    {
        lx = mo->x - R;
        ly = mo->y - R;
        hx = mo->x + R;
        hy = mo->y + R;
    }

    AutomapLine *ml = GetMapLine();

    GetRotatedCoords(lx, ly, ml->points.X, ml->points.Y);
    GetRotatedCoords(lx, hy, ml->points.Z, ml->points.W);
    ml->color = rgb;
    DrawMLine(ml);

    ml = GetMapLine();

    GetRotatedCoords(lx, hy, ml->points.X, ml->points.Y);
    GetRotatedCoords(hx, hy, ml->points.Z, ml->points.W);
    ml->color = rgb;
    DrawMLine(ml);

    ml = GetMapLine();

    GetRotatedCoords(hx, hy, ml->points.X, ml->points.Y);
    GetRotatedCoords(hx, ly, ml->points.Z, ml->points.W);
    ml->color = rgb;
    DrawMLine(ml);

    ml = GetMapLine();

    GetRotatedCoords(hx, ly, ml->points.X, ml->points.Y);
    GetRotatedCoords(lx, ly, ml->points.Z, ml->points.W);
    ml->color = rgb;
    DrawMLine(ml);
}

//
// The vector graphics for the automap.
//
// A line drawing of the player pointing right, starting from the
// middle.

static HMM_Vec4 player_arrow[] = {{{{{{-0.875f, 0.0f, 1.0f}}}, 0.0f}},     // -----

                                  {{{{{1.0f, 0.0f, 0.5f}}}, 0.25f}},       // ----->
                                  {{{{{1.0f, 0.0f, 0.5f}}}, -0.25f}},

                                  {{{{{-0.875f, 0.0f, -1.125f}}}, 0.25f}}, // >---->
                                  {{{{{-0.875f, 0.0f, -1.125f}}}, -0.25f}},

                                  {{{{{-0.625f, 0.0f, -0.875f}}}, 0.25f}}, // >>--->
                                  {{{{{-0.625f, 0.0f, -0.875f}}}, -0.25f}}};

static constexpr uint8_t kAutomapPlayerArrowLines = (sizeof(player_arrow) / sizeof(HMM_Vec4));

static HMM_Vec4 cheat_player_arrow[] = {{{{{{-0.875f, 0.0f, 1.0f}}}, 0.0f}},      // -----

                                        {{{{{1.0f, 0.0f, 0.5f}}}, 0.167f}},       // ----->
                                        {{{{{1.0f, 0.0f, 0.5f}}}, -0.167f}},

                                        {{{{{-0.875f, 0.0f, -1.125f}}}, 0.167f}}, // >----->
                                        {{{{{-0.875f, 0.0f, -1.125f}}}, -0.167f}},

                                        {{{{{-0.625f, 0.0f, -0.875f}}}, 0.167f}}, // >>----->
                                        {{{{{-0.625f, 0.0f, -0.875f}}}, -0.167f}},

                                        {{{{{-0.5f, 0.0f, -0.5f}}}, -0.167f}},    // >>-d--->
                                        {{{{{-0.5f, -0.167f, -0.5f + 0.167f}}}, -0.167f}},
                                        {{{{{-0.5f + 0.167f, -0.167f, -0.5f + 0.167f}}}, 0.25f}},

                                        {{{{{-0.167f, 0.0f, -0.167f}}}, -0.167f}}, // >>-dd-->
                                        {{{{{-0.167f, -0.167f, 0.0f}}}, -0.167f}},
                                        {{{{{0.0f, -0.167f, 0.0f}}}, 0.25f}},

                                        {{{{{0.167f, 0.25f, 0.167f}}}, -0.143f}}, // >>-ddt->
                                        {{{{{0.167f, -0.143f, 0.167f + 0.031f}}}, -0.143f - 0.031f}},
                                        {{{{{0.167f + 0.031f, -0.143f - 0.031f, 0.167f + 0.1f}}}, -0.143f}}};

static constexpr uint8_t kAutomapCheatPlayerArrowLines = (sizeof(cheat_player_arrow) / sizeof(HMM_Vec4));

static HMM_Vec4 thin_triangle_guy[] = {
    {{{{{-0.5f, -0.7f, 1.0f}}}, 0.0f}}, {{{{{1.0f, 0.0f, -0.5f}}}, 0.7f}}, {{{{{-0.5f, 0.7f, -0.5f}}}, -0.7f}}};

static constexpr uint8_t kAutomapThinTriangleGuyLines = (sizeof(thin_triangle_guy) / sizeof(HMM_Vec4));

static void AddPlayer(MapObject *mo)
{
    // clip to map frame
    float x = MapToFrameCoordinatesX(mo->x, map_center_x);
    float y = MapToFrameCoordinatesY(mo->y, map_center_y);
    if (x < frame_x || x > frame_x + frame_width || y < frame_y || y > frame_y + frame_height)
    {
        return;
    }

    if (automap_debug_collisions.d_)
        DrawObjectBounds(mo, am_colors[kAutomapColorPlayer]);

    float mx, my, ma;

    if (!console_active && !paused && !menu_active && mo->interpolate_)
    {
        mx = HMM_Lerp(mo->old_x_, fractional_tic, mo->x);
        my = HMM_Lerp(mo->old_y_, fractional_tic, mo->y);
        ma = epi::BAMInterpolate(mo->old_angle_, mo->angle_, fractional_tic);
    }
    else
    {
        mx = mo->x;
        my = mo->y;
        ma = mo->angle_;
    }

    switch (current_arrow_type)
    {
    case kAutomapArrowStyleHeretic:
        DrawLineCharacter(player_dagger, kAutomapPlayerDaggerLines, mo->radius_, ma, am_colors[kAutomapColorPlayer], mx,
                          my);
        break;
    case kAutomapArrowStyleDoom:
    default: {
        if (cheating)
            DrawLineCharacter(cheat_player_arrow, kAutomapCheatPlayerArrowLines, mo->radius_, ma,
                              am_colors[kAutomapColorPlayer], mx, my);
        else
            DrawLineCharacter(player_arrow, kAutomapPlayerArrowLines, mo->radius_, ma, am_colors[kAutomapColorPlayer],
                              mx, my);
        break;
    }
    }
}

static void AddThing(MapObject *mo)
{
    // clip to map frame
    float x = MapToFrameCoordinatesX(mo->x, map_center_x);
    float y = MapToFrameCoordinatesY(mo->y, map_center_y);
    if (x < frame_x || x > frame_x + frame_width || y < frame_y || y > frame_y + frame_height)
    {
        return;
    }

    int index = kAutomapColorScenery;

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

    float mx, my, ma;

    if (!console_active && !paused && !menu_active && mo->interpolate_)
    {
        mx = HMM_Lerp(mo->old_x_, fractional_tic, mo->x);
        my = HMM_Lerp(mo->old_y_, fractional_tic, mo->y);
        ma = epi::BAMInterpolate(mo->old_angle_, mo->angle_, fractional_tic);
    }
    else
    {
        mx = mo->x;
        my = mo->y;
        ma = mo->angle_;
    }

    DrawLineCharacter(thin_triangle_guy, kAutomapThinTriangleGuyLines, mo->radius_, ma, am_colors[index], mx, my);
}

static void CollectMapLines()
{
    if (!hide_lines)
    {
        for (int i = 0; i < total_level_lines; i++)
        {
            AddWall(&level_lines[i]);
        }
    }

    // draw player arrows first, then things
    // if we are cheating
    for (int i = 0; i < total_players; i++)
    {
        if (i == display_player || InCooperativeMatch())
            AddPlayer(players[i]->map_object_);
    }

    if (show_things)
    {
        for (MapObject *mo = map_object_list_head; mo; mo = mo->next_)
        {
            if (!mo->player_)
                AddThing(mo);
        }
    }
}

static void DrawMarks(void)
{
    Font *am_font = automap_style->fonts_[0];

    HUDSetFont(am_font);
    HUDSetAlignment(0, 0); // centre the characters

    char buffer[4];

    for (int i = 0; i < kAutomapTotalMarkPoints; i++)
    {
        if (AlmostEquals(mark_points[i].X, (float)kAutomapNoMarkX))
            continue;

        float mx, my;

        GetRotatedCoords(mark_points[i].X, mark_points[i].Y, mx, my);

        buffer[0] = ('1' + i);
        buffer[1] = 0;

        HUDDrawText(MapToFrameCoordinatesX(mx, map_center_x), MapToFrameCoordinatesY(my, map_center_y), buffer);
    }

    HUDSetFont();
    HUDSetAlignment();
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
        if (!console_active && !paused && !menu_active)
        {
            map_center_x = HMM_Lerp(frame_focus->old_x_, fractional_tic, frame_focus->x);
            map_center_y = HMM_Lerp(frame_focus->old_y_, fractional_tic, frame_focus->y);
        }
        else
        {
            map_center_x = frame_focus->x;
            map_center_y = frame_focus->y;
        }
    }

    EPI_ASSERT(automap_style);

    map_alpha = HUDGetAlpha();
    HUDSetAlpha(automap_style->definition_->bg_.translucency_);

    if (automap_style->background_image_)
    {
        if (automap_style->definition_->special_ == 0)
            HUDStretchImage(-90, 0, 500, 200, automap_style->background_image_, 0.0, 0.0);
        else
            HUDTileImage(-90, 0, 500, 200, automap_style->background_image_, 0.0, 0.0);
    }
    else if (automap_style->definition_->bg_.colour_ != kRGBANoValue)
    {
        HUDSolidBox(x, y, x + w, y + h, automap_style->definition_->bg_.colour_);
    }
    else
    {
        // Dasho: Draw a black background as a fallback. We need to explicitly do this, as if draw
        // culling is enabled the background would be the culling fog color instead
        HUDSolidBox(x, y, x + w, y + h, kRGBABlack);
    }

    HUDSetAlpha(map_alpha);

    // Update various render values
    map_pulse_width = (float)(game_tic % 32);
    if (map_pulse_width >= 16.0f)
        map_pulse_width = 2.0 + (map_pulse_width * 0.1f);
    else
        map_pulse_width = 2.0 - (map_pulse_width * 0.1f);
    map_dx           = HUDToRealCoordinatesX(MapToFrameDistanceX(-map_center_x)) - HUDToRealCoordinatesX(0);
    map_dy           = HUDToRealCoordinatesY(0) - HUDToRealCoordinatesY(MapToFrameDistanceY(-map_center_y));
    frame_lerped_x   = HMM_Lerp(frame_focus->old_x_, fractional_tic, frame_focus->x);
    frame_lerped_y   = HMM_Lerp(frame_focus->old_y_, fractional_tic, frame_focus->y);
    frame_lerped_ang = epi::BAMInterpolate(frame_focus->old_angle_, frame_focus->angle_, fractional_tic);

    if (grid && !rotate_map)
        CollectGridLines();

    DrawAllLines();

    CollectMapLines();

    DrawAllLines();

    DrawMarks();

    DrawKeys();
}

void AutomapSetColor(int which, RGBAColor color)
{
    EPI_ASSERT(0 <= which && which < kTotalAutomapColors);

    am_colors[which] = color;
}

void AutomapGetState(int *state, float *zoom)
{
    *state = 0;

    if (grid)
        *state |= kAutomapStateGrid;

    if (follow_player)
        *state |= kAutomapStateFollow;

    if (rotate_map)
        *state |= kAutomapStateRotate;

    if (show_things)
        *state |= kAutomapStateThings;

    if (show_walls)
        *state |= kAutomapStateWalls;

    if (hide_lines)
        *state |= kAutomapStateHideLines;

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
