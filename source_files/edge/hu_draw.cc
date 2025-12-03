//----------------------------------------------------------------------------
//  EDGE 2D DRAWING STUFF
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

#include "hu_draw.h"

#include <map>

#include "am_map.h"
#include "con_main.h"
#include "ddf_font.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_misc.h" //  R_Render
#include "r_modes.h"
#include "r_texgl.h"
#include "r_units.h"

// FIXME: this seems totally arbitrary, review it.
static constexpr float kVerticalSpacing = 1.0f;

extern ConsoleLine    *quit_lines[kENDOOMLines];
extern int             console_cursor;
extern const Font     *endoom_font;
extern ConsoleVariable video_overlay;
extern ConsoleVariable fliplevels;

static Font *default_font;

extern int game_tic;
int        hud_tic;

int  hud_swirl_pass   = 0;
bool hud_thick_liquid = false;

float hud_x_left;
float hud_x_right;
float hud_x_middle;
float hud_visible_top;
float hud_visible_bottom;

float hud_y_top;
float hud_y_bottom;

// current state
static Font     *current_font;
static RGBAColor current_color;

static float current_scale, current_alpha;
static int   current_x_alignment, current_y_alignment;

// mapping from hud X and Y coords to real (OpenGL) coords.
// note that Y coordinates get inverted.
static float margin_x;
static float margin_y;
static float margin_x_multiplier;
static float margin_y_multiplier;

static constexpr float kDoomPixelAspectRatio = (5.0f / 6.0f);

std::map<std::string, std::pair<ImageData *, unsigned int>> available_overlays;

void CollectOverlays()
{
    // Add the default (none) option first so it takes precedence
    // over an overlay that might somehow have the same file stem
    available_overlays.emplace("None", std::make_pair(nullptr, 0));

    // Check for overlays
    std::vector<epi::DirectoryEntry> ovd;
    std::string                      overlay_dir = epi::PathAppend(home_directory, "overlays");

    // Create home directory overlays folder if it doesn't aleady exist
    if (!epi::IsDirectory(overlay_dir))
        epi::MakeDirectory(overlay_dir);

    ovd.clear();

    if (!ReadDirectory(ovd, overlay_dir, "*.png"))
    {
        LogWarning("CollectOverlays: Failed to read '%s' directory!\n", overlay_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < ovd.size(); i++)
        {
            if (!ovd[i].is_dir)
            {
                std::string filename = epi::GetStem(ovd[i].name);
                if (!available_overlays.count(filename))
                {
                    epi::File *ovimg_file = epi::FileOpen(ovd[i].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                    if (ovimg_file)
                    {
                        ImageData *ovimg_data = LoadImageData(ovimg_file);
                        if (ovimg_data)
                        {
                            unsigned int tex_id = UploadTexture(ovimg_data, kUploadNone, (1 << 30));
                            available_overlays.emplace(filename, std::make_pair(ovimg_data, tex_id));
                        }
                        delete ovimg_file;
                    }
                }
            }
        }
    }
    ovd.clear();
    if (!ReadDirectory(ovd, overlay_dir, "*.tga"))
    {
        LogWarning("CollectOverlays: Failed to read '%s' directory!\n", overlay_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < ovd.size(); i++)
        {
            if (!ovd[i].is_dir)
            {
                std::string filename = epi::GetStem(ovd[i].name);
                if (!available_overlays.count(filename))
                {
                    epi::File *ovimg_file = epi::FileOpen(ovd[i].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                    if (ovimg_file)
                    {
                        ImageData *ovimg_data = LoadImageData(ovimg_file);
                        if (ovimg_data)
                        {
                            unsigned int tex_id = UploadTexture(ovimg_data, kUploadNone, (1 << 30));
                            available_overlays.emplace(filename, std::make_pair(ovimg_data, tex_id));
                        }
                        delete ovimg_file;
                    }
                }
            }
        }
    }

    if (home_directory != game_directory)
    {
        ovd.clear();

        // Read the program directory, but only add names we haven't encountered yet
        overlay_dir = epi::PathAppend(game_directory, "overlays");

        if (!ReadDirectory(ovd, overlay_dir, "*.png"))
        {
            LogWarning("CollectOverlays: Failed to read '%s' directory!\n", overlay_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < ovd.size(); i++)
            {
                if (!ovd[i].is_dir)
                {
                    std::string filename = epi::GetStem(ovd[i].name);
                    if (!available_overlays.count(filename))
                    {
                        epi::File *ovimg_file =
                            epi::FileOpen(ovd[i].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                        if (ovimg_file)
                        {
                            ImageData *ovimg_data = LoadImageData(ovimg_file);
                            if (ovimg_data)
                            {
                                unsigned int tex_id = UploadTexture(ovimg_data, kUploadNone, (1 << 30));
                                available_overlays.emplace(filename, std::make_pair(ovimg_data, tex_id));
                            }
                            delete ovimg_file;
                        }
                    }
                }
            }
        }
        ovd.clear();
        if (!ReadDirectory(ovd, overlay_dir, "*.tga"))
        {
            LogWarning("CollectOverlays: Failed to read '%s' directory!\n", overlay_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < ovd.size(); i++)
            {
                if (!ovd[i].is_dir)
                {
                    std::string filename = epi::GetStem(ovd[i].name);
                    if (!available_overlays.count(filename))
                    {
                        epi::File *ovimg_file =
                            epi::FileOpen(ovd[i].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                        if (ovimg_file)
                        {
                            ImageData *ovimg_data = LoadImageData(ovimg_file);
                            if (ovimg_data)
                            {
                                unsigned int tex_id = UploadTexture(ovimg_data, kUploadNone, (1 << 30));
                                available_overlays.emplace(filename, std::make_pair(ovimg_data, tex_id));
                            }
                            delete ovimg_file;
                        }
                    }
                }
            }
        }
    }

    // Check for previously saved overlay CVAR; revert if not present anymore
    if (!available_overlays.count(video_overlay.s_))
    {
        video_overlay = "None";
    }
}

float HUDToRealCoordinatesX(float x)
{
    return margin_x + x * margin_x_multiplier;
}

float HUDToRealCoordinatesY(float y)
{
    return margin_y - y * margin_y_multiplier;
}

void HUDSetCoordinateSystem(int width, int height)
{
    if (width < 1 || height < 1)
        return;

    float sw = (float)current_screen_width;
    float sh = (float)current_screen_height;

    /* compute Y stuff */

    hud_y_top    = 0.0f;
    hud_y_bottom = height;

    margin_y            = sh;
    margin_y_multiplier = sh / (float)height;

    /* compute X stuff */

    hud_x_middle = width * 0.5f;

    float side_dist = (float)width / 2.0;

    // compensate for size of window or screen.
    side_dist = side_dist * (sw / 320.0f) / (sh / 200.0f);

    // compensate for monitor's pixel aspect
    side_dist = side_dist * pixel_aspect_ratio.f_;

    // compensate for Doom's 5:6 pixel aspect ratio.
    side_dist = side_dist / kDoomPixelAspectRatio;

    hud_x_left  = hud_x_middle - side_dist;
    hud_x_right = hud_x_middle + side_dist;

    margin_x_multiplier = sw / side_dist / 2.0;
    margin_x            = 0.0f - hud_x_left * margin_x_multiplier;
}

void HUDSetFont(Font *font)
{
    current_font = font ? font : default_font;
}

void HUDSetScale(float scale)
{
    current_scale = scale;
}

void HUDSetTextColor(RGBAColor color)
{
    current_color = color;
}

void HUDSetAlpha(float alpha)
{
    current_alpha = alpha;
}

float HUDGetAlpha()
{
    return current_alpha;
}

void HUDSetAlignment(int xa, int ya)
{
    current_x_alignment = xa;
    current_y_alignment = ya;
}

void HUDReset()
{
    HUDSetCoordinateSystem(320, 200);

    current_font        = default_font;
    current_color       = kRGBANoValue;
    current_scale       = 1.0f;
    current_alpha       = 1.0f;
    current_x_alignment = -1;
    current_y_alignment = -1;
}

void HUDFrameSetup(void)
{
    if (default_font == nullptr)
    {
        // FIXME: get default font from DDF gamedef
        FontDefinition *DEF = fontdefs.Lookup("DOOM");
        EPI_ASSERT(DEF);

        default_font = hud_fonts.Lookup(DEF);
        EPI_ASSERT(default_font);
    }

    HUDReset();

    hud_tic = game_tic;
}

static constexpr uint8_t kScissorStackMaximum = 10;
static int               scissor_stack[kScissorStackMaximum][4];
static int               scissor_stack_top = 0;

void HUDPushScissor(float x1, float y1, float x2, float y2, bool expand)
{
    EPI_ASSERT(scissor_stack_top < kScissorStackMaximum);

    // expand rendered view to cover whole screen
    if (expand && x1 < 1 && x2 > hud_x_middle * 2 - 1)
    {
        x1 = 0;
        x2 = current_screen_width;
    }
    else
    {
        x1 = HUDToRealCoordinatesX(x1);
        x2 = HUDToRealCoordinatesX(x2);
    }

    std::swap(y1, y2);

    y1 = HUDToRealCoordinatesY(y1);
    y2 = HUDToRealCoordinatesY(y2);

    int sx1 = RoundToInteger(x1);
    int sy1 = RoundToInteger(y1);
    int sx2 = RoundToInteger(x2);
    int sy2 = RoundToInteger(y2);

    if (scissor_stack_top == 0)
    {
        render_state->Enable(GL_SCISSOR_TEST);

        sx1 = HMM_MAX(sx1, 0);
        sy1 = HMM_MAX(sy1, 0);

        sx2 = HMM_MIN(sx2, current_screen_width);
        sy2 = HMM_MIN(sy2, current_screen_height);
    }
    else
    {
        // clip to previous scissor
        int *xy = scissor_stack[scissor_stack_top - 1];

        sx1 = HMM_MAX(sx1, xy[0]);
        sy1 = HMM_MAX(sy1, xy[1]);

        sx2 = HMM_MIN(sx2, xy[2]);
        sy2 = HMM_MIN(sy2, xy[3]);
    }

    EPI_ASSERT(sx2 >= sx1);
    EPI_ASSERT(sy2 >= sy1);

    render_state->Scissor(sx1, sy1, sx2 - sx1, sy2 - sy1);

    // push current scissor
    int *xy = scissor_stack[scissor_stack_top];

    xy[0] = sx1;
    xy[1] = sy1;
    xy[2] = sx2;
    xy[3] = sy2;

    scissor_stack_top++;
}

void HUDPopScissor()
{
    EPI_ASSERT(scissor_stack_top > 0);

    scissor_stack_top--;

    if (scissor_stack_top == 0)
    {
        render_state->Disable(GL_SCISSOR_TEST);
    }
    else
    {
        // restore previous scissor
        int *xy = scissor_stack[scissor_stack_top];

        render_state->Scissor(xy[0], xy[1], xy[2] - xy[0], xy[3] - xy[1]);
    }
}

// Adapted from Quake 3 GPL release
void HUDCalcScrollTexCoords(float x_scroll, float y_scroll, float *tx1, float *ty1, float *tx2, float *ty2)
{
    float timeScale, adjustedScrollS, adjustedScrollT;

    timeScale = game_tic / 100.0f;

    adjustedScrollS = x_scroll * timeScale;
    adjustedScrollT = y_scroll * timeScale;

    // clamp so coordinates don't continuously get larger
    adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
    adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);

    *tx1 += adjustedScrollS;
    *ty1 += adjustedScrollT;
    *tx2 += adjustedScrollS;
    *ty2 += adjustedScrollT;
}

// Adapted from Quake 3 GPL release
void HUDCalcTurbulentTexCoords(float *tx, float *ty, float x, float y)
{
    float now;
    float phase     = 0;
    float frequency = hud_thick_liquid ? 0.5 : 1.0;
    float amplitude = 0.05;

    now = (phase + hud_tic / 100.0f * frequency);

    if (swirling_flats == kLiquidSwirlParallax)
    {
        frequency *= 2;
        if (hud_thick_liquid)
        {
            if (hud_swirl_pass == 1)
            {
                *tx = *tx +
                      sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
                *ty = *ty +
                      sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
            }
            else
            {
                amplitude = 0;
                *tx       = *tx -
                      sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
                *ty = *ty -
                      sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
            }
        }
        else
        {
            if (hud_swirl_pass == 1)
            {
                amplitude = 0.025;
                *tx       = *tx +
                      sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
                *ty = *ty +
                      sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
            }
            else
            {
                amplitude = 0.015;
                *tx       = *tx -
                      sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
                *ty = *ty -
                      sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
            }
        }
    }
    else
    {
        *tx = *tx + sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
        *ty = *ty + sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
    }
}

//----------------------------------------------------------------------------

void HUDRawImage(float hx1, float hy1, float hx2, float hy2, const Image *image, float tx1, float ty1, float tx2,
                 float ty2, float alpha, RGBAColor text_col, float sx, float sy, bool font_draw)
{
    if (hx1 >= hx2 || hy1 >= hy2)
        return;

    if (hx2 < 0 || hx1 > current_screen_width || hy2 < 0 || hy1 > current_screen_height)
        return;

    RGBAColor unit_col = kRGBAWhite;
    epi::SetRGBAAlpha(unit_col, alpha);
    BlendingMode blend;
    GLuint       tex_id = 0;

    bool do_whiten = false;

    if (text_col != kRGBANoValue)
    {
        unit_col = text_col;
        epi::SetRGBAAlpha(unit_col, alpha);
        do_whiten = true;
    }

    if (!image)
    {
        EPI_ASSERT(font_draw &&
                   (current_font->definition_->type_ == kFontTypeTrueType ||
                    current_font->definition_->type_ ==
                        kFontTypePatch)); // The only time we should be legitimately sending a null Image pointer

        if (current_font->definition_->type_ == kFontTypeTrueType)
        {
            const TTFFont *cur_font = (const TTFFont *)current_font;
            blend                   = kBlendingAlpha;
            if ((image_smoothing &&
                 cur_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothOnDemand) ||
                cur_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothAlways)
                tex_id = cur_font->truetype_smoothed_texture_id_[current_font_size];
            else
                tex_id = cur_font->truetype_texture_id_[current_font_size];
        }
        else // patch font
        {
            const PatchFont *cur_font = (const PatchFont *)current_font;
            if (alpha >= 0.11f)
                blend = kBlendingLess;
            else
                blend = kBlendingMasked;
            blend = (BlendingMode)(blend | kBlendingAlpha);
            if ((image_smoothing &&
                 cur_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothOnDemand) ||
                cur_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothAlways)
            {
                if (do_whiten)
                    tex_id = cur_font->patch_font_cache_.atlas_whitened_smoothed_texture_id;
                else
                    tex_id = cur_font->patch_font_cache_.atlas_smoothed_texture_id;
            }
            else
            {
                if (do_whiten)
                    tex_id = cur_font->patch_font_cache_.atlas_whitened_texture_id;
                else
                    tex_id = cur_font->patch_font_cache_.atlas_texture_id;
            }
        }

        StartUnitBatch(false);

        RendererVertex *glvert =
            BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty2}};
        glvert++->position             = {{hx1, hy1, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty2}};
        glvert++->position             = {{hx2, hy1, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty1}};
        glvert++->position             = {{hx2, hy2, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty1}};
        glvert->position               = {{hx1, hy2, 0}};

        EndRenderUnit(4);

        FinishUnitBatch();
        return;
    }

    tex_id = ImageCache(image, true, nullptr, do_whiten);

    if (alpha >= 0.99f && image->opacity_ == kOpacitySolid)
        blend = kBlendingNone;
    else
    {
        if (!(alpha < 0.11f || image->opacity_ == kOpacityComplex))
            blend = kBlendingLess;
        else
            blend = kBlendingMasked;
    }

    if (image->opacity_ == kOpacityComplex || alpha < 0.99f)
        blend = (BlendingMode)(blend | kBlendingAlpha);

    if (sx != 0.0 || sy != 0.0)
    {
        blend = (BlendingMode)(blend | kBlendingRepeatX | kBlendingRepeatY);

        HUDCalcScrollTexCoords(sx, sy, &tx1, &ty1, &tx2, &ty2);
    }

    bool hud_swirl = false;

    if (image->liquid_type_ > kLiquidImageNone && swirling_flats > kLiquidSwirlSmmu)
    {
        hud_swirl_pass = 1;
        hud_swirl      = true;
    }

    if (image->liquid_type_ == kLiquidImageThick)
        hud_thick_liquid = true;

    StartUnitBatch(false);

    RendererVertex *glvert =
        BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    if (hud_swirl)
    {
        HUDCalcTurbulentTexCoords(&tx1, &ty1, hx1, hy1);
        HUDCalcTurbulentTexCoords(&tx2, &ty2, hx2, hy2);
    }

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx1, ty1}};
    glvert++->position             = {{hx1, hy1, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx2, ty1}};
    glvert++->position             = {{hx2, hy1, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx2, ty2}};
    glvert++->position             = {{hx2, hy2, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx1, ty2}};
    glvert->position               = {{hx1, hy2, 0}};

    EndRenderUnit(4);

    if (hud_swirl && swirling_flats == kLiquidSwirlParallax)
    {
        hud_swirl_pass = 2;
        tx1 += 0.2;
        tx2 += 0.2;
        ty1 += 0.2;
        ty2 += 0.2;
        HUDCalcTurbulentTexCoords(&tx1, &ty1, hx1, hy1);
        HUDCalcTurbulentTexCoords(&tx2, &ty2, hx2, hy2);
        alpha /= 2;
        blend = (BlendingMode)(blend | kBlendingMasked | kBlendingAlpha);

        glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty1}};
        glvert++->position             = {{hx1, hy1, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty1}};
        glvert++->position             = {{hx2, hy1, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty2}};
        glvert++->position             = {{hx2, hy2, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty2}};
        glvert->position               = {{hx1, hy2, 0}};

        EndRenderUnit(4);
    }

    FinishUnitBatch();

    hud_swirl_pass   = 0;
    hud_thick_liquid = false;
}

void HUDRawFromTexID(float hx1, float hy1, float hx2, float hy2, unsigned int tex_id, ImageOpacity opacity, float tx1,
                     float ty1, float tx2, float ty2, float alpha)
{
    RGBAColor unit_col = kRGBAWhite;
    epi::SetRGBAAlpha(unit_col, alpha);
    BlendingMode blend = kBlendingNone;

    if (hx1 >= hx2 || hy1 >= hy2)
        return;

    if (hx2 < 0 || hx1 > current_screen_width || hy2 < 0 || hy1 > current_screen_height)
        return;

    if (alpha >= 0.99f && opacity == kOpacitySolid)
        blend = kBlendingNone;
    else
    {
        if (!(alpha < 0.11f || opacity == kOpacityComplex))
            blend = kBlendingLess;
        else
            blend = kBlendingMasked;
    }

    if (opacity == kOpacityComplex || alpha < 0.99f)
        blend = (BlendingMode(blend | kBlendingAlpha));

    StartUnitBatch(false);

    RendererVertex *glvert =
        BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx1, ty1}};
    glvert++->position             = {{hx1, hy1, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx2, ty1}};
    glvert++->position             = {{hx2, hy1, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx2, ty2}};
    glvert++->position             = {{hx2, hy2, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{tx1, ty2}};
    glvert->position               = {{hx1, hy2, 0}};

    EndRenderUnit(4);

    FinishUnitBatch();
}

void HUDStretchFromImageData(float x, float y, float w, float h, const ImageData *img, unsigned int tex_id,
                             ImageOpacity opacity)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    float x1 = HUDToRealCoordinatesX(x);
    float x2 = HUDToRealCoordinatesX(x + w);

    float y1 = HUDToRealCoordinatesY(y + h);
    float y2 = HUDToRealCoordinatesY(y);

    HUDRawFromTexID(x1, y1, x2, y2, tex_id, opacity, 0, 0, (float)img->width_ / img->width_,
                    (float)img->height_ / img->height_, current_alpha);
}

void HUDStretchImage(float x, float y, float w, float h, const Image *img, float sx, float sy, const Colormap *colmap)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    x -= img->ScaledOffsetX();
    y -= img->ScaledOffsetY();

    float x1 = HUDToRealCoordinatesX(x);
    float x2 = HUDToRealCoordinatesX(x + w);

    float y1 = HUDToRealCoordinatesY(y + h);
    float y2 = HUDToRealCoordinatesY(y);

    RGBAColor text_col = kRGBANoValue;

    if (colmap)
    {
        text_col = GetFontColor(colmap);
    }

    // HUDRawImage(x1, y1, x2, y2, img, 0, 0, img->Right(), img->Top(),
    // current_alpha, text_col, colmap, sx, sy);
    HUDRawImage(x1, y1, x2, y2, img, 0, 0, 1.0f, 1.0f, current_alpha, text_col, sx, sy);
}

void HUDStretchImageNoOffset(float x, float y, float w, float h, const Image *img, float sx, float sy)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    // x -= img->ScaledOffsetX();
    // y -= img->ScaledOffsetY();

    float x1 = HUDToRealCoordinatesX(x);
    float x2 = HUDToRealCoordinatesX(x + w);

    float y1 = HUDToRealCoordinatesY(y + h);
    float y2 = HUDToRealCoordinatesY(y);

    HUDRawImage(x1, y1, x2, y2, img, 0, 0, 1.0f, 1.0f, current_alpha, kRGBANoValue, sx, sy);
}

void HUDDrawImageTitleWS(const Image *title_image)
{
    // Lobo: Widescreen titlescreen support.
    // In the case of titlescreens we will ignore any scaling
    // set in DDFImages and always calculate our own.
    // This is to ensure that we always get 200 height.
    // The width we don't care about, hence widescreen ;)
    float TempWidth  = 0;
    float TempHeight = 0;
    float TempScale  = 0;
    float CenterX    = 0;

    // 1. Calculate scaling to apply.

    TempScale = 200;
    TempScale /= title_image->height_;

    TempWidth  = title_image->ScaledWidth() * TempScale; // respect ASPECT in images.ddf at least
    TempHeight = title_image->height_ * TempScale;

    // 2. Calculate centering on screen.
    CenterX = 160;
    CenterX -= TempWidth / 2;

    // 3. Draw it.
    // Lobo 2025: we need to ignore offsets for TITLESCREENs to line up with what most other ports do
    HUDStretchImageNoOffset(CenterX, -0.1f, TempWidth, TempHeight + 0.1f, title_image, 0.0, 0.0);
}

float HUDGetImageWidth(const Image *img)
{
    return (img->ScaledWidth() * current_scale);
}

float HUDGetImageHeight(const Image *img)
{
    return (img->ScaledHeight() * current_scale);
}

void HUDDrawImage(float x, float y, const Image *img, const Colormap *colmap)
{
    float w = img->ScaledWidth() * current_scale;
    float h = img->ScaledHeight() * current_scale;

    HUDStretchImage(x, y, w, h, img, 0.0, 0.0, colmap);
}

void HUDDrawImageNoOffset(float x, float y, const Image *img)
{
    float w = img->ScaledWidth() * current_scale;
    float h = img->ScaledHeight() * current_scale;

    HUDStretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
}

void HUDScrollImage(float x, float y, const Image *img, float sx, float sy)
{
    float w = img->ScaledWidth() * current_scale;
    float h = img->ScaledHeight() * current_scale;

    HUDStretchImage(x, y, w, h, img, sx, sy);
}

void HUDScrollImageNoOffset(float x, float y, const Image *img, float sx, float sy)
{
    float w = img->ScaledWidth() * current_scale;
    float h = img->ScaledHeight() * current_scale;

    HUDStretchImageNoOffset(x, y, w, h, img, sx, sy);
}

void HUDTileImage(float x, float y, float w, float h, const Image *img, float offset_x, float offset_y)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    offset_x /= w;
    offset_y /= -h;

    float tx_scale = w / img->ScaledWidth() / current_scale;
    float ty_scale = h / img->ScaledHeight() / current_scale;

    float x1 = HUDToRealCoordinatesX(x);
    float x2 = HUDToRealCoordinatesX(x + w);

    float y1 = HUDToRealCoordinatesY(y + h);
    float y2 = HUDToRealCoordinatesY(y);

    HUDRawImage(x1, y1, x2, y2, img, (offset_x)*tx_scale, (offset_y)*ty_scale, (offset_x + 1) * tx_scale,
                (offset_y + 1) * ty_scale, current_alpha);
}

void HUDSolidBox(float x1, float y1, float x2, float y2, RGBAColor col)
{
    // expand to cover wide screens
    if (x1 < hud_x_left && x2 > hud_x_right - 1 && y1 < hud_y_top + 1 && y2 > hud_y_bottom - 1)
    {
        x1 = 0;
        x2 = current_screen_width;
        y1 = 0;
        y2 = current_screen_height;
    }
    else
    {
        std::swap(y1, y2);

        x1 = HUDToRealCoordinatesX(x1);
        y1 = HUDToRealCoordinatesY(y1);
        x2 = HUDToRealCoordinatesX(x2);
        y2 = HUDToRealCoordinatesY(y2);
    }

    StartUnitBatch(false);

    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0,
                                             current_alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    RGBAColor unit_col = col;
    epi::SetRGBAAlpha(unit_col, current_alpha);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y2, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y2, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y1, 0}};

    EndRenderUnit(4);

    FinishUnitBatch();
}

void HUDSolidLine(float x1, float y1, float x2, float y2, RGBAColor col)
{
    x1 = HUDToRealCoordinatesX(x1);
    y1 = HUDToRealCoordinatesY(y1);
    x2 = HUDToRealCoordinatesX(x2);
    y2 = HUDToRealCoordinatesY(y2);

    render_state->Enable(GL_LINE_SMOOTH);

    StartUnitBatch(false);

    RGBAColor unit_col = col;
    epi::SetRGBAAlpha(unit_col, current_alpha);
    BlendingMode blend = kBlendingNone;

    if (current_alpha < 0.99f)
        blend = kBlendingAlpha;

    RendererVertex *glvert =
        BeginRenderUnit(GL_LINES, 2, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x2, y2, 0}};

    EndRenderUnit(2);

    FinishUnitBatch();
    render_state->Disable(GL_LINE_SMOOTH);
}

void HUDThinBox(float x1, float y1, float x2, float y2, RGBAColor col, float thickness, BlendingMode special_blend)
{
    std::swap(y1, y2);

    x1 = HUDToRealCoordinatesX(x1);
    y1 = HUDToRealCoordinatesY(y1);
    x2 = HUDToRealCoordinatesX(x2);
    y2 = HUDToRealCoordinatesY(y2);

    StartUnitBatch(false);
    RGBAColor unit_col = col;
    epi::SetRGBAAlpha(unit_col, current_alpha);
    BlendingMode blend = kBlendingNone;

    if (current_alpha < 0.99f)
        blend = kBlendingAlpha;

    if (special_blend != kBlendingNone)
        blend = special_blend;

    RendererVertex *glvert =
        BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y2, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1 + 2 + thickness, y2, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x1 + 2 + thickness, y1, 0}};

    EndRenderUnit(4);

    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x2 - 2 - thickness, y1, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2 - 2 - thickness, y2, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y2, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x2, y1, 0}};

    EndRenderUnit(4);

    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1 + 2 + thickness, y1, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1 + 2 + thickness, y1 + 2 + thickness, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2 - 2 - thickness, y1 + 2 + thickness, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x2 - 2 - thickness, y1, 0}};

    EndRenderUnit(4);

    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1 + 2 + thickness, y2 - 2 - thickness, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1 + 2 + thickness, y2, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2 - 2 - thickness, y2, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x2 - 2 - thickness, y2 - 2 - thickness, 0}};

    EndRenderUnit(4);

    FinishUnitBatch();
}

void HUDGradientBox(float x1, float y1, float x2, float y2, RGBAColor *cols)
{
    std::swap(y1, y2);

    x1 = HUDToRealCoordinatesX(x1);
    y1 = HUDToRealCoordinatesY(y1);
    x2 = HUDToRealCoordinatesX(x2);
    y2 = HUDToRealCoordinatesY(y2);

    StartUnitBatch(false);
    BlendingMode blend = kBlendingNone;

    if (current_alpha < 0.99f)
        blend = kBlendingAlpha;

    RGBAColor unit_col = cols[1];
    epi::SetRGBAAlpha(unit_col, current_alpha);

    RendererVertex *glvert =
        BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, 0}};

    unit_col = cols[0];
    epi::SetRGBAAlpha(unit_col, current_alpha);
    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y2, 0}};

    unit_col = cols[2];
    epi::SetRGBAAlpha(unit_col, current_alpha);
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y2, 0}};

    unit_col = cols[3];
    epi::SetRGBAAlpha(unit_col, current_alpha);
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y1, 0}};

    EndRenderUnit(4);

    FinishUnitBatch();
}

float HUDFontWidth(void)
{
    return current_scale * current_font->NominalWidth();
}

float HUDFontHeight(void)
{
    return current_scale * current_font->NominalHeight();
}

float HUDFontWidthNew(float size)
{
    float factor   = 1;
    float TheWidth = 0;

    if (current_font->definition_->type_ == kFontTypeTrueType)
    {
        if (size > 0)
            factor = size / current_font->definition_->default_size_;

        TheWidth = current_font->CharWidth('W') * factor;
    }
    else if (current_font->definition_->type_ == kFontTypeImage)
    {
        if (size > 0)
        {
            factor = size * (current_font->CharRatio('W') + current_font->spacing_);
        }
        else
        {
            factor = current_font->CharWidth('W');
        }
        TheWidth = factor;
    }
    else
    {
        if (size > 0)
        {
            const PatchFont *pfont = (const PatchFont *)current_font;
            factor                 = size * pfont->patch_font_cache_.ratio + pfont->spacing_;
        }
        else
        {
            factor = current_font->CharWidth('W');
        }
        TheWidth = factor;
    }
    return (TheWidth * current_scale);
}

float HUDStringWidth(const char *str)
{
    return current_scale * current_font->StringWidth(str);
}

float HUDStringWidthNew(const char *str, float size)
{
    float factor = 1;

    if (current_font->definition_->type_ == kFontTypeTrueType)
    {
        if (size > 0)
        {
            factor = size / current_font->definition_->default_size_;
        }
        return (current_scale * current_font->StringWidth(str)) * factor;
    }
    else if (current_font->definition_->type_ == kFontTypeImage)
    {
        factor = current_font->CharWidth('W');
        if (size > 0)
        {
            factor = size * (current_font->CharRatio('W') + current_font->spacing_);
        }
    }
    else
    {
        factor = current_font->CharWidth('W');
        if (size > 0)
        {
            const PatchFont *pfont = (const PatchFont *)current_font;
            factor                 = size * pfont->patch_font_cache_.ratio + pfont->spacing_;
        }
    }
    // get the length of the line
    int len = 0;
    while (str[len] && str[len] != '\n')
        len++;

    factor *= len;
    return current_scale * factor;
}

float HUDStringHeight(const char *str)
{
    int slines = StringLines(str);

    return slines * HUDFontHeight() + (slines - 1) * kVerticalSpacing;
}

void HUDDrawChar(float left_x, float top_y, char ch, float size)
{
    const Image *img = nullptr;

    float sc_x = current_scale; // TODO * aspect;
    float sc_y = current_scale;

    float x = left_x;
    float y = top_y;

    float w, h;
    float tx1, tx2, ty1, ty2;

    if (current_font->definition_->type_ == kFontTypeTrueType)
    {
        TTFFont            *cur_font = (TTFFont *)current_font;
        stbtt_aligned_quad *q        = &cur_font->truetype_glyph_map_.at((uint8_t)ch).character_quad[current_font_size];
        y = top_y + (cur_font->truetype_glyph_map_.at((uint8_t)ch).y_shift[current_font_size] *
                     (size > 0 ? (size / cur_font->definition_->default_size_) : 1.0) * sc_y);
        w = ((size > 0 ? (cur_font->CharWidth(ch) * (size / cur_font->definition_->default_size_))
                       : cur_font->CharWidth(ch)) -
             cur_font->spacing_) *
            sc_x;
        h = (cur_font->truetype_glyph_map_.at((uint8_t)ch).height[current_font_size] *
             (size > 0 ? (size / cur_font->definition_->default_size_) : 1.0)) *
            sc_y;
        tx1 = q->s0;
        ty1 = q->t0;
        tx2 = q->s1;
        ty2 = q->t1;
    }
    else if (current_font->definition_->type_ == kFontTypePatch)
    {
        PatchFont *cur_font = (PatchFont *)current_font;
        w                   = (size > 0 ? (size * cur_font->patch_font_cache_.ratio) : cur_font->CharWidth(ch)) * sc_x;
        h                   = (size > 0 ? size
                                        : (cur_font->definition_->default_size_ > 0.0
                                               ? cur_font->definition_->default_size_
                                               : cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch])
                                   .image_height)) *
            sc_y;
        x -= (cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).offset_x * sc_x);
        y -= (cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).offset_y * sc_y);
        tx1 = cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).texture_coordinate_x;
        ty2 = cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).texture_coordinate_y;
        tx2 =
            tx1 +
            cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).texture_coordinate_width;
        ty1 =
            ty2 +
            cur_font->patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]).texture_coordinate_height;
    }
    else // spritesheet font
    {
        ImageFont *cur_font = (ImageFont *)current_font;
        img                 = cur_font->font_image_;

        EPI_ASSERT(img);

        x -= img->ScaledOffsetX() * sc_x;
        y -= img->ScaledOffsetY() * sc_y;

        w      = ((size > 0 ? (size * cur_font->CharRatio(ch)) : cur_font->CharWidth(ch)) - cur_font->spacing_) * sc_x;
        h      = (size > 0 ? size : cur_font->image_character_height_) * sc_y;
        int px = (uint8_t)ch % 16;
        int py = 15 - (uint8_t)ch / 16;
        tx1    = (float)(px) * 0.0625f;
        tx2    = (float)(px + 1) * 0.0625f;
        float char_texcoord_adjust =
            ((tx2 - tx1) - ((tx2 - tx1) * (cur_font->CharWidth(ch) / cur_font->image_character_width_))) / 2;
        tx1 += char_texcoord_adjust;
        tx2 -= char_texcoord_adjust;
        ty1 = (float)(py) * 0.0625f;
        ty2 = (float)(py + 1) * 0.0625f;
    }

    float x1 = HUDToRealCoordinatesX(x);
    float x2 = HUDToRealCoordinatesX(x + w);

    float y1 = HUDToRealCoordinatesY(y + h);
    float y2 = HUDToRealCoordinatesY(y);

    HUDRawImage(x1, y1, x2, y2, img, tx1, ty1, tx2, ty2, current_alpha, current_color, 0.0, 0.0, true);
}

//
// Write a string using the current font
//
void HUDDrawText(float x, float y, const char *str, float size)
{
    EPI_ASSERT(current_font);

    // handle each line

    if (!str)
        return;

    float cy      = y;
    float total_h = (size > 0 ? size : HUDStringHeight(str)) * current_scale;

    if (current_y_alignment >= 0)
    {
        if (current_y_alignment == 0)
            total_h /= 2.0f;

        cy -= total_h;
    }

    while (*str)
    {
        // get the length of the line
        int len = 0;
        while (str[len] && str[len] != '\n')
            len++;

        float cx      = x;
        float total_w = 0;
        float yoff    = 0;
        float line_h  = ((size > 0 ? size : HUDFontHeight()) + kVerticalSpacing) * current_scale;

        if (current_font->definition_->type_ == kFontTypeTrueType)
        {
            TTFFont *cur_font = (TTFFont *)current_font;
            for (int i = 0; i < len; i++)
            {
                float factor = size > 0 ? (size / cur_font->definition_->default_size_) : 1;
                total_w += cur_font->CharWidth(str[i]) * factor * current_scale;
                if (str[i + 1])
                {
                    total_w += stbtt_GetGlyphKernAdvance(cur_font->truetype_info_, cur_font->GetGlyphIndex(str[i]),
                                                         cur_font->GetGlyphIndex(str[i + 1])) *
                               cur_font->truetype_kerning_scale_[current_font_size] * factor * current_scale;
                }
            }
        }
        else if (current_font->definition_->type_ == kFontTypeImage)
        {
            for (int i = 0; i < len; i++)
            {
                total_w += (size > 0 ? size * current_font->CharRatio(str[i]) + current_font->spacing_
                                     : current_font->CharWidth(str[i])) *
                           current_scale;
            }
        }
        else
        {
            PatchFont *cur_font = (PatchFont *)current_font;
            for (int i = 0; i < len; i++)
            {
                float xoff = 0;
                if (cur_font->HasChar(str[i]))
                {
                    xoff            = cur_font->GetCharXOffset(str[i]);
                    float off_check = cur_font->GetCharYOffset(str[i]);
                    if (HMM_ABS(off_check) > HMM_ABS(yoff))
                        yoff = off_check;
                }
                total_w += ((size > 0 ? size * cur_font->patch_font_cache_.ratio + cur_font->spacing_
                                      : cur_font->CharWidth(str[i])) -
                            xoff) *
                           current_scale;
            }
        }

        line_h += HMM_ABS(yoff) * current_scale;

        if (current_x_alignment >= 0)
        {
            if (current_x_alignment == 0)
                total_w /= 2.0f;

            cx -= total_w;
        }

        if (current_font->definition_->type_ == kFontTypeTrueType)
        {
            TTFFont *cur_font = (TTFFont *)current_font;
            for (int k = 0; k < len; k++)
            {
                char ch = str[k];

                if (cur_font->HasChar(ch))
                    HUDDrawChar(cx, cy, ch, size);

                float factor = size > 0 ? (size / cur_font->definition_->default_size_) : 1;
                cx += cur_font->CharWidth(ch) * factor * current_scale;
                if (str[k + 1])
                {
                    cx += stbtt_GetGlyphKernAdvance(cur_font->truetype_info_, cur_font->GetGlyphIndex(str[k]),
                                                    cur_font->GetGlyphIndex(str[k + 1])) *
                          cur_font->truetype_kerning_scale_[current_font_size] * factor * current_scale;
                }
            }
        }
        else if (current_font->definition_->type_ == kFontTypeImage)
        {
            for (int k = 0; k < len; k++)
            {
                char ch = str[k];

                if (current_font->HasChar(ch))
                    HUDDrawChar(cx, cy, ch, size);

                cx += (size > 0 ? size * current_font->CharRatio(ch) + current_font->spacing_
                                : current_font->CharWidth(ch)) *
                      current_scale;
            }
        }
        else
        {
            PatchFont *cur_font = (PatchFont *)current_font;
            for (int k = 0; k < len; k++)
            {
                char  ch   = str[k];
                float xoff = 0;

                if (cur_font->HasChar(ch))
                {
                    HUDDrawChar(cx, cy, ch, size);
                    xoff = cur_font->GetCharXOffset(ch);
                }

                cx += ((size > 0 ? size * cur_font->patch_font_cache_.ratio + cur_font->spacing_
                                 : cur_font->CharWidth(ch)) -
                       xoff) *
                      current_scale;
            }
        }

        if (str[len] == 0)
            break;

        str += (len + 1);
        cy += line_h + kVerticalSpacing;
    }
}

//
// Draw the ENDOOM screen
//

void HUDDrawQuitScreen()
{
    if (quit_lines[0] && quit_lines[0]->endoom_bytes_.size() == kENDOOMBytesPerLine)
    {
        EPI_ASSERT(endoom_font);
        float FNX = HMM_MIN((float)current_screen_width / 80.0f,
                            320.0f / 80.0f * ((float)current_screen_height * 0.90f / 200.0f));
        float FNY = FNX * 2;
        StartUnitBatch(false);
        RendererVertex *endoom_vert       = BeginRenderUnit(GL_QUADS, kENDOOMTotalVerts, GL_MODULATE, 0,
                                                            (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);
        uint32_t        endoom_vert_count = 0;
        // First pass, draw solid blocks
        for (int i = 0; i < kENDOOMLines; i++)
        {
            float cy = (float)current_screen_height - ((i + 1) * FNY);
            float cx = HMM_MAX(0, (((float)current_screen_width - (FNX * 80.0f)) / 2.0f));
            for (int j = 1; j < kENDOOMBytesPerLine; j += 2)
            {
                uint8_t   info     = quit_lines[i]->endoom_bytes_[j];
                RGBAColor unit_col = kENDOOMColors[(info >> 4) & 7];

                endoom_vert->rgba       = unit_col;
                endoom_vert++->position = {{cx, cy, 0}};
                endoom_vert->rgba       = unit_col;
                endoom_vert++->position = {{cx, cy + FNX * 2, 0}};
                endoom_vert->rgba       = unit_col;
                endoom_vert++->position = {{cx + FNX, cy + FNX * 2, 0}};
                endoom_vert->rgba       = unit_col;
                endoom_vert++->position = {{cx + FNX, cy, 0}};

                cx += FNX;
                endoom_vert_count += 4;
            }
        }
        EndRenderUnit(endoom_vert_count);
        // Second pass, draw characters
        ImageFont   *en_font = (ImageFont *)endoom_font;
        const Image *img     = en_font->font_image_;
        EPI_ASSERT(img);
        GLuint       tex_id = ImageCache(img, true, (const Colormap *)0, true);
        BlendingMode blend  = kBlendingNone;
        if (img->opacity_ == kOpacitySolid)
            blend = kBlendingNone;
        else
        {
            if (img->opacity_ != kOpacityComplex)
                blend = kBlendingLess;
            else
                blend = kBlendingAlpha;
        }
        endoom_vert       = BeginRenderUnit(GL_QUADS, kENDOOMTotalVerts, GL_MODULATE, tex_id,
                                            (GLuint)kTextureEnvironmentDisable, 0, 0, blend);
        endoom_vert_count = 0;
        for (int i = 0; i < kENDOOMLines; i++)
        {
            float cy = (float)current_screen_height - ((i + 1) * FNY);
            float cx = HMM_MAX(0, (((float)current_screen_width - (FNX * 80.0f)) / 2.0f));
            for (int j = 0; j < kENDOOMBytesPerLine; j += 2)
            {
                uint8_t info = quit_lines[i]->endoom_bytes_[j + 1];
                // Check for blinking
                if ((info & 128) && console_cursor >= 16)
                {
                    cx += FNX;
                    continue;
                }

                float     tx1, tx2, ty1, ty2;
                uint8_t   character = quit_lines[i]->endoom_bytes_[j];
                RGBAColor unit_col  = kENDOOMColors[info & 15];

                uint8_t px = character % 16;
                uint8_t py = 15 - character / 16;
                tx1        = (float)(px) * 0.0625f;
                tx2        = (float)(px + 1) * 0.0625f;
                ty1        = (float)(py) * 0.0625f;
                ty2        = (float)(py + 1) * 0.0625f;

                float width_adjust = FNX / 2 + .5;

                endoom_vert->rgba                   = unit_col;
                endoom_vert->texture_coordinates[0] = {{tx1, ty1}};
                endoom_vert++->position             = {{cx - width_adjust, cy, 0}};
                endoom_vert->rgba                   = unit_col;
                endoom_vert->texture_coordinates[0] = {{tx2, ty1}};
                endoom_vert++->position             = {{cx + FNX + width_adjust, cy, 0}};
                endoom_vert->rgba                   = unit_col;
                endoom_vert->texture_coordinates[0] = {{tx2, ty2}};
                endoom_vert++->position             = {{cx + FNX + width_adjust, cy + FNX * 2, 0}};
                endoom_vert->rgba                   = unit_col;
                endoom_vert->texture_coordinates[0] = {{tx1, ty2}};
                endoom_vert++->position             = {{cx - width_adjust, cy + FNX * 2, 0}};

                cx += FNX;
                endoom_vert_count += 4;
            }
        }
        EndRenderUnit(endoom_vert_count);
        FinishUnitBatch();
        HUDSetAlignment(0, -1);
        HUDDrawText(160, 195 - HUDStringHeight(language["PressToQuit"]), language["PressToQuit"]);
    }
    else
    {
        HUDSetAlignment(0, -1);
        HUDDrawText(160, 100 - (HUDStringHeight(language["PressToQuit"]) / 2), language["PressToQuit"]);
    }
}

void HUDRenderWorld(float x, float y, float w, float h, MapObject *camera, int flags)
{
    render_backend->BeginWorldRender();

    HUDPushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    hud_visible_bottom = y + h;
    hud_visible_top    = 200 - hud_visible_bottom;

    int *xy = scissor_stack[scissor_stack_top - 1];

    bool full_height = h > (hud_y_bottom - hud_y_top) * 0.95;

    // FIXME explain this weirdness
    float width    = HUDToRealCoordinatesX(x + w) - HUDToRealCoordinatesX(x);
    float expand_w = (xy[2] - xy[0]) / width;

    // renderer needs true (OpenGL) coordinates.
    // get from scissor due to the expansion thing [ FIXME: HACKY ]
    float x1 = xy[0]; // HUDToRealCoordinatesX(x);
    float y1 = xy[1]; // HUDToRealCoordinatesY(y);
    float x2 = xy[2]; // HUDToRealCoordinatesX(x+w);
    float y2 = xy[3]; // HUDToRealCoordinatesY(y+h);

    RenderView(x1, y1, x2 - x1, y2 - y1, camera, full_height, expand_w);

    HUDPopScissor();

    render_backend->FinishWorldRender();
}

void HUDRenderAutomap(float x, float y, float w, float h, MapObject *player, int flags)
{
    HUDPushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    // [ FIXME HACKY ]
    if ((flags & 1) == 0)
    {
        if (x < 1 && x + w > hud_x_middle * 2 - 1)
        {
            x = hud_x_left;
            w = hud_x_right - x;
        }
    }

    if (fliplevels.d_)
        render_backend->SetupMatrices2D(true);

    AutomapRender(x, y, w, h, player);

    if (fliplevels.d_)
        render_backend->SetupMatrices2D(false);

    HUDPopScissor();
}

void HUDGetCastPosition(float *x, float *y, float *scale_x, float *scale_y)
{
    *x = HUDToRealCoordinatesX(160);
    *y = HUDToRealCoordinatesY(170);

    // FIXME REVIEW THIS
    //*scale_y = 4.0;
    *scale_x = *scale_y / pixel_aspect_ratio.f_;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
