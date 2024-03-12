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

#include "am_map.h"
#include "con_main.h"
#include "font.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_misc.h"  //  R_Render
#include "r_modes.h"
#include "r_units.h"
#include "str_compare.h"

// FIXME: this seems totally arbitrary, review it.
static constexpr float kVerticalSpacing = 2.0f;

extern ConsoleLine    *quit_lines[kEndoomLines];
extern int             console_cursor;
extern Font         *endoom_font;
extern ConsoleVariable video_overlay;
extern ConsoleVariable double_framerate;

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
static Font   *current_font;
static RGBAColor current_color;

static float current_scale, current_alpha;
static int   current_x_alignment, current_y_alignment;

// mapping from hud X and Y coords to real (OpenGL) coords.
// note that Y coordinates get inverted.
static float margin_x;
static float margin_y;
static float margin_x_multiplier;
static float margin_y_multiplier;

static constexpr float kDoomPixelAspectRatio = (5.0f/6.0f);

std::vector<std::string> hud_overlays = {
    "",
    "OVERLAY_LINES_1X",
    "OVERLAY_LINES_2X",
    "OVERLAY_VERTICAL_1X",
    "OVERLAY_VERTICAL_2X",
    "OVERLAY_GRILL_1X",
    "OVERLAY_GRILL_2X",
};

static inline float HudToRealCoordinatesX(float x)
{
    return margin_x + x * margin_x_multiplier;
}

static inline float HudToRealCoordinatesY(float y)
{
    return margin_y - y * margin_y_multiplier;
}

void HudSetCoordinateSystem(int width, int height)
{
    if (width < 1 || height < 1) return;

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

void HudSetFont(Font *font) { current_font = font ? font : default_font; }

void HudSetScale(float scale) { current_scale = scale; }

void HudSetTextColor(RGBAColor color) { current_color = color; }

void HudSetAlpha(float alpha) { current_alpha = alpha; }

float HudGetAlpha() { return current_alpha; }

void HudSetAlignment(int xa, int ya)
{
    current_x_alignment = xa;
    current_y_alignment = ya;
}

void HudReset()
{
    HudSetCoordinateSystem(320, 200);

    current_font        = default_font;
    current_color       = kRGBANoValue;
    current_scale       = 1.0f;
    current_alpha       = 1.0f;
    current_x_alignment = -1;
    current_y_alignment = -1;
}

void HudFrameSetup(void)
{
    if (default_font == nullptr)
    {
        // FIXME: get default font from DDF gamedef
        FontDefinition *DEF = fontdefs.Lookup("DOOM");
        SYS_ASSERT(DEF);

        default_font = hud_fonts.Lookup(DEF);
        SYS_ASSERT(default_font);
    }

    HudReset();

    hud_tic++;
}

static constexpr uint8_t kScissorStackMaximum = 10;
static int               scissor_stack[kScissorStackMaximum][4];
static int               scissor_stack_top = 0;

void HudPushScissor(float x1, float y1, float x2, float y2, bool expand)
{
    SYS_ASSERT(scissor_stack_top < kScissorStackMaximum);

    // expand rendered view to cover whole screen
    if (expand && x1 < 1 && x2 > hud_x_middle * 2 - 1)
    {
        x1 = 0;
        x2 = current_screen_width;
    }
    else
    {
        x1 = HudToRealCoordinatesX(x1);
        x2 = HudToRealCoordinatesX(x2);
    }

    std::swap(y1, y2);

    y1 = HudToRealCoordinatesY(y1);
    y2 = HudToRealCoordinatesY(y2);

    int sx1 = RoundToInteger(x1);
    int sy1 = RoundToInteger(y1);
    int sx2 = RoundToInteger(x2);
    int sy2 = RoundToInteger(y2);

    if (scissor_stack_top == 0)
    {
        glEnable(GL_SCISSOR_TEST);

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

    SYS_ASSERT(sx2 >= sx1);
    SYS_ASSERT(sy2 >= sy1);

    glScissor(sx1, sy1, sx2 - sx1, sy2 - sy1);

    // push current scissor
    int *xy = scissor_stack[scissor_stack_top];

    xy[0] = sx1;
    xy[1] = sy1;
    xy[2] = sx2;
    xy[3] = sy2;

    scissor_stack_top++;
}

void HudPopScissor()
{
    SYS_ASSERT(scissor_stack_top > 0);

    scissor_stack_top--;

    if (scissor_stack_top == 0) { glDisable(GL_SCISSOR_TEST); }
    else
    {
        // restore previous scissor
        int *xy = scissor_stack[scissor_stack_top];

        glScissor(xy[0], xy[1], xy[2] - xy[0], xy[3] - xy[1]);
    }
}

// Adapted from Quake 3 GPL release
void HudCalcScrollTexCoords(float x_scroll, float y_scroll, float *tx1,
                            float *ty1, float *tx2, float *ty2)
{
    float timeScale, adjustedScrollS, adjustedScrollT;

    timeScale = game_tic / (double_framerate.d_ ? 200.0f : 100.0f);

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
void HudCalcTurbulentTexCoords(float *tx, float *ty, float x, float y)
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
                *tx = *tx + sine_table[(int)((x * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
                *ty = *ty + sine_table[(int)((y * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
            }
            else
            {
                amplitude = 0;
                *tx = *tx - sine_table[(int)((x * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
                *ty = *ty - sine_table[(int)((y * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
            }
        }
        else
        {
            if (hud_swirl_pass == 1)
            {
                amplitude = 0.025;
                *tx = *tx + sine_table[(int)((x * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
                *ty = *ty + sine_table[(int)((y * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
            }
            else
            {
                amplitude = 0.015;
                *tx = *tx - sine_table[(int)((x * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
                *ty = *ty - sine_table[(int)((y * 1.0 / 128 * 0.125 + now) *
                                             kSineTableSize) &
                                       (kSineTableMask)] *
                                amplitude;
            }
        }
    }
    else
    {
        *tx = *tx +
              sine_table[(int)((x * 1.0 / 128 * 0.125 + now) * kSineTableSize) &
                         (kSineTableMask)] *
                  amplitude;
        *ty = *ty +
              sine_table[(int)((y * 1.0 / 128 * 0.125 + now) * kSineTableSize) &
                         (kSineTableMask)] *
                  amplitude;
    }
}

//----------------------------------------------------------------------------

void HudRawImage(float hx1, float hy1, float hx2, float hy2,
                 const Image *image, float tx1, float ty1, float tx2,
                 float ty2, float alpha, RGBAColor text_col,
                 const Colormap *palremap, float sx, float sy, char ch)
{
    int x1 = RoundToInteger(hx1);
    int y1 = RoundToInteger(hy1);
    int x2 = RoundToInteger(hx2 + 0.25f);
    int y2 = RoundToInteger(hy2 + 0.25f);

    if (x1 >= x2 || y1 >= y2) return;

    if (x2 < 0 || x1 > current_screen_width || y2 < 0 || y1 > current_screen_height) return;

    sg_color sgcol = sg_white;

    bool do_whiten = false;

    if (text_col != kRGBANoValue)
    {
        sgcol     = sg_make_color_1i(text_col);
        sgcol.a   = 1.0f;
        do_whiten = true;
    }

    if (epi::StringCaseCompareASCII(image->name_, "FONT_DUMMY_IMAGE") == 0)
    {
        if (current_font->definition_->type_ == kFontTypeTrueType)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_2D);
            if ((image_smoothing &&
                 current_font->definition_->truetype_smoothing_ ==
                     FontDefinition::kTrueTypeSmoothOnDemand) ||
                current_font->definition_->truetype_smoothing_ ==
                    FontDefinition::kTrueTypeSmoothAlways)
                glBindTexture(
                    GL_TEXTURE_2D,
                    current_font->truetype_smoothed_texture_id_[current_font_size]);
            else
                glBindTexture(GL_TEXTURE_2D,
                              current_font->truetype_texture_id_[current_font_size]);
        }
        else  // patch font
        {
            glEnable(GL_ALPHA_TEST);
            if (!(alpha < 0.11f || image->opacity_ == kOpacityComplex))
                glAlphaFunc(GL_GREATER, alpha * 0.66f);
            glEnable(GL_BLEND);
            glEnable(GL_TEXTURE_2D);
            if ((image_smoothing &&
                 current_font->definition_->truetype_smoothing_ ==
                     FontDefinition::kTrueTypeSmoothOnDemand) ||
                current_font->definition_->truetype_smoothing_ ==
                    FontDefinition::kTrueTypeSmoothAlways)
            {
                if (do_whiten)
                    glBindTexture(
                        GL_TEXTURE_2D,
                        current_font->patch_font_cache_.atlas_whitened_smoothed_texture_id);
                else
                    glBindTexture(GL_TEXTURE_2D,
                                  current_font->patch_font_cache_.atlas_smoothed_texture_id);
            }
            else
            {
                if (do_whiten)
                    glBindTexture(GL_TEXTURE_2D,
                                  current_font->patch_font_cache_.atlas_whitened_texture_id);
                else
                    glBindTexture(GL_TEXTURE_2D,
                                  current_font->patch_font_cache_.atlas_texture_id);
            }
        }
        glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);
        glBegin(GL_QUADS);
        glTexCoord2f(tx1, ty2);
        glVertex2f(hx1, hy1);
        glTexCoord2f(tx2, ty2);
        glVertex2f(hx2, hy1);
        glTexCoord2f(tx2, ty1);
        glVertex2f(hx2, hy2);
        glTexCoord2f(tx1, ty1);
        glVertex2f(hx1, hy2);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glAlphaFunc(GL_GREATER, 0);
        return;
    }

    // GLuint tex_id = ImageCache(image, true, palremap, do_whiten);
    GLuint tex_id = ImageCache(image, true, nullptr, do_whiten);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (alpha >= 0.99f && image->opacity_ == kOpacitySolid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (!(alpha < 0.11f || image->opacity_ == kOpacityComplex))
            glAlphaFunc(GL_GREATER, alpha * 0.66f);
    }

    if (image->opacity_ == kOpacityComplex || alpha < 0.99f) glEnable(GL_BLEND);

    GLint old_s_clamp = kDummyClamp;
    GLint old_t_clamp = kDummyClamp;

    if (sx != 0.0 || sy != 0.0)
    {
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &old_s_clamp);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_t_clamp);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        HudCalcScrollTexCoords(sx, sy, &tx1, &ty1, &tx2, &ty2);
    }

    if (epi::StringCaseCompareASCII(image->name_,
                                    hud_overlays.at(video_overlay.d_)) == 0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    bool hud_swirl = false;

    if (image->liquid_type_ > kLiquidImageNone && swirling_flats > kLiquidSwirlSmmu)
    {
        hud_swirl_pass = 1;
        hud_swirl      = true;
    }

    if (image->liquid_type_ == kLiquidImageThick) hud_thick_liquid = true;

    glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);

    glBegin(GL_QUADS);

    if (hud_swirl)
    {
        HudCalcTurbulentTexCoords(&tx1, &ty1, x1, y1);
        HudCalcTurbulentTexCoords(&tx2, &ty2, x2, y2);
    }

    glTexCoord2f(tx1, ty1);
    glVertex2i(x1, y1);

    glTexCoord2f(tx2, ty1);
    glVertex2i(x2, y1);

    glTexCoord2f(tx2, ty2);
    glVertex2i(x2, y2);

    glTexCoord2f(tx1, ty2);
    glVertex2i(x1, y2);

    glEnd();

    if (hud_swirl && swirling_flats == kLiquidSwirlParallax)
    {
        hud_swirl_pass = 2;
        tx1 += 0.2;
        tx2 += 0.2;
        ty1 += 0.2;
        ty2 += 0.2;
        HudCalcTurbulentTexCoords(&tx1, &ty1, x1, y1);
        HudCalcTurbulentTexCoords(&tx2, &ty2, x2, y2);
        alpha /= 2;
        glEnable(GL_ALPHA_TEST);

        glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);

        glEnable(GL_BLEND);
        glBegin(GL_QUADS);
        glTexCoord2f(tx1, ty1);
        glVertex2i(x1, y1);

        glTexCoord2f(tx2, ty1);
        glVertex2i(x2, y1);

        glTexCoord2f(tx2, ty2);
        glVertex2i(x2, y2);

        glTexCoord2f(tx1, ty2);
        glVertex2i(x1, y2);
        glEnd();
    }

    hud_swirl_pass   = 0;
    hud_thick_liquid = false;

    if (old_s_clamp != kDummyClamp)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, old_s_clamp);

    if (old_t_clamp != kDummyClamp)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_t_clamp);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    glAlphaFunc(GL_GREATER, 0);
}

void HudRawFromTexID(float hx1, float hy1, float hx2, float hy2,
                     unsigned int tex_id, ImageOpacity opacity, float tx1,
                     float ty1, float tx2, float ty2, float alpha)
{
    int x1 = RoundToInteger(hx1);
    int y1 = RoundToInteger(hy1);
    int x2 = RoundToInteger(hx2 + 0.25f);
    int y2 = RoundToInteger(hy2 + 0.25f);

    if (x1 >= x2 || y1 >= y2) return;

    if (x2 < 0 || x1 > current_screen_width || y2 < 0 || y1 > current_screen_height) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (alpha >= 0.99f && opacity == kOpacitySolid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (!(alpha < 0.11f || opacity == kOpacityComplex))
            glAlphaFunc(GL_GREATER, alpha * 0.66f);
    }

    if (opacity == kOpacityComplex || alpha < 0.99f) glEnable(GL_BLEND);

    glColor4f(1.0f, 1.0f, 1.0f, alpha);

    glBegin(GL_QUADS);

    glTexCoord2f(tx1, ty1);
    glVertex2i(x1, y1);

    glTexCoord2f(tx2, ty1);
    glVertex2i(x2, y1);

    glTexCoord2f(tx2, ty2);
    glVertex2i(x2, y2);

    glTexCoord2f(tx1, ty2);
    glVertex2i(x1, y2);

    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    glAlphaFunc(GL_GREATER, 0);
}

void HudStretchFromImageData(float x, float y, float w, float h,
                             const ImageData *img, unsigned int tex_id,
                             ImageOpacity opacity)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    float x1 = HudToRealCoordinatesX(x);
    float x2 = HudToRealCoordinatesX(x + w);

    float y1 = HudToRealCoordinatesY(y + h);
    float y2 = HudToRealCoordinatesY(y);

    HudRawFromTexID(x1, y1, x2, y2, tex_id, opacity, 0, 0,
                    (float)img->used_width_ / img->width_,
                    (float)img->used_height_ / img->height_, current_alpha);
}

void HudStretchImage(float x, float y, float w, float h, const Image *img,
                     float sx, float sy, const Colormap *colmap)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    x -= img->ScaledOffsetX();
    y -= img->ScaledOffsetY();

    float x1 = HudToRealCoordinatesX(x);
    float x2 = HudToRealCoordinatesX(x + w);

    float y1 = HudToRealCoordinatesY(y + h);
    float y2 = HudToRealCoordinatesY(y);

    RGBAColor text_col = kRGBANoValue;

    if (colmap) { text_col = GetFontColor(colmap); }

    // HudRawImage(x1, y1, x2, y2, img, 0, 0, img->Right(), img->Top(),
    // current_alpha, text_col, colmap, sx, sy);
    HudRawImage(x1, y1, x2, y2, img, 0, 0, img->Right(), img->Top(),
                current_alpha, text_col, nullptr, sx, sy);
}

void HudStretchImageNoOffset(float x, float y, float w, float h,
                             const Image *img, float sx, float sy)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    // x -= img->ScaledOffsetX();
    // y -= img->ScaledOffsetY();

    float x1 = HudToRealCoordinatesX(x);
    float x2 = HudToRealCoordinatesX(x + w);

    float y1 = HudToRealCoordinatesY(y + h);
    float y2 = HudToRealCoordinatesY(y);

    HudRawImage(x1, y1, x2, y2, img, 0, 0, img->Right(), img->Top(),
                current_alpha, kRGBANoValue, nullptr, sx, sy);
}

void HudDrawImageTitleWS(const Image *title_image)
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
    TempScale /= title_image->actual_height_;

    TempWidth = title_image->ScaledWidthActual() *
                TempScale;  // respect ASPECT in images.ddf at least
    TempHeight = title_image->actual_height_ * TempScale;

    // 2. Calculate centering on screen.
    CenterX = 160;
    CenterX -= TempWidth / 2;

    // 3. Draw it.
    HudStretchImage(CenterX, -0.1f, TempWidth, TempHeight + 0.1f, title_image,
                    0.0, 0.0);
}

float HudGetImageWidth(const Image *img)
{
    return (img->ScaledWidthActual() * current_scale);
}

float HudGetImageHeight(const Image *img)
{
    return (img->ScaledHeightActual() * current_scale);
}

void HudDrawImage(float x, float y, const Image *img, const Colormap *colmap)
{
    float w = img->ScaledWidthActual() * current_scale;
    float h = img->ScaledHeightActual() * current_scale;

    HudStretchImage(x, y, w, h, img, 0.0, 0.0, colmap);
}

void HudDrawImageNoOffset(float x, float y, const Image *img)
{
    float w = img->ScaledWidthActual() * current_scale;
    float h = img->ScaledHeightActual() * current_scale;

    HudStretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
}

void HudScrollImage(float x, float y, const Image *img, float sx, float sy)
{
    float w = img->ScaledWidthActual() * current_scale;
    float h = img->ScaledHeightActual() * current_scale;

    HudStretchImage(x, y, w, h, img, sx, sy);
}

void HudScrollImageNoOffset(float x, float y, const Image *img, float sx,
                            float sy)
{
    float w = img->ScaledWidthActual() * current_scale;
    float h = img->ScaledHeightActual() * current_scale;

    HudStretchImageNoOffset(x, y, w, h, img, sx, sy);
}

void HudTileImage(float x, float y, float w, float h, const Image *img,
                  float offset_x, float offset_y)
{
    if (current_x_alignment >= 0)
        x -= w / (current_x_alignment == 0 ? 2.0f : 1.0f);

    if (current_y_alignment >= 0)
        y -= h / (current_y_alignment == 0 ? 2.0f : 1.0f);

    offset_x /= w;
    offset_y /= -h;

    float tx_scale = w / img->ScaledWidthTotal() / current_scale;
    float ty_scale = h / img->ScaledHeightTotal() / current_scale;

    float x1 = HudToRealCoordinatesX(x);
    float x2 = HudToRealCoordinatesX(x + w);

    float y1 = HudToRealCoordinatesY(y + h);
    float y2 = HudToRealCoordinatesY(y);

    HudRawImage(x1, y1, x2, y2, img, (offset_x)*tx_scale, (offset_y)*ty_scale,
                (offset_x + 1) * tx_scale, (offset_y + 1) * ty_scale,
                current_alpha);
}

void HudSolidBox(float x1, float y1, float x2, float y2, RGBAColor col)
{
    // expand to cover wide screens
    if (x1 < hud_x_left && x2 > hud_x_right - 1 && y1 < hud_y_top + 1 &&
        y2 > hud_y_bottom - 1)
    {
        x1 = 0;
        x2 = current_screen_width;
        y1 = 0;
        y2 = current_screen_height;
    }
    else
    {
        std::swap(y1, y2);

        x1 = HudToRealCoordinatesX(x1);
        y1 = HudToRealCoordinatesY(y1);
        x2 = HudToRealCoordinatesX(x2);
        y2 = HudToRealCoordinatesY(y2);
    }

    if (current_alpha < 0.99f) glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);

    glBegin(GL_QUADS);

    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);

    glEnd();

    glDisable(GL_BLEND);
}

void HudSolidLine(float x1, float y1, float x2, float y2, RGBAColor col,
                  float thickness, bool smooth, float dx, float dy)
{
    x1 = HudToRealCoordinatesX(x1);
    y1 = HudToRealCoordinatesY(y1);
    x2 = HudToRealCoordinatesX(x2);
    y2 = HudToRealCoordinatesY(y2);

    dx = HudToRealCoordinatesX(dx) - HudToRealCoordinatesX(0);
    dy = HudToRealCoordinatesY(0) - HudToRealCoordinatesY(dy);

    glLineWidth(thickness);

    if (smooth) glEnable(GL_LINE_SMOOTH);

    if (smooth || current_alpha < 0.99f) glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);

    glBegin(GL_LINES);

    glVertex2i((int)x1 + (int)dx, (int)y1 + (int)dy);
    glVertex2i((int)x2 + (int)dx, (int)y2 + (int)dy);

    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
}

void HudThinBox(float x1, float y1, float x2, float y2, RGBAColor col,
                float thickness)
{
    std::swap(y1, y2);

    x1 = HudToRealCoordinatesX(x1);
    y1 = HudToRealCoordinatesY(y1);
    x2 = HudToRealCoordinatesX(x2);
    y2 = HudToRealCoordinatesY(y2);

    if (current_alpha < 0.99f) glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);

    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x1 + 2 + thickness, y2);
    glVertex2f(x1 + 2 + thickness, y1);
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x2 - 2 - thickness, y1);
    glVertex2f(x2 - 2 - thickness, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x1 + 2 + thickness, y1);
    glVertex2f(x1 + 2 + thickness, y1 + 2 + thickness);
    glVertex2f(x2 - 2 - thickness, y1 + 2 + thickness);
    glVertex2f(x2 - 2 - thickness, y1);
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x1 + 2 + thickness, y2 - 2 - thickness);
    glVertex2f(x1 + 2 + thickness, y2);
    glVertex2f(x2 - 2 - thickness, y2);
    glVertex2f(x2 - 2 - thickness, y2 - 2 - thickness);
    glEnd();

    glDisable(GL_BLEND);
}

void HudGradientBox(float x1, float y1, float x2, float y2, RGBAColor *cols)
{
    std::swap(y1, y2);

    x1 = HudToRealCoordinatesX(x1);
    y1 = HudToRealCoordinatesY(y1);
    x2 = HudToRealCoordinatesX(x2);
    y2 = HudToRealCoordinatesY(y2);

    if (current_alpha < 0.99f) glEnable(GL_BLEND);

    glBegin(GL_QUADS);

    sg_color sgcol = sg_make_color_1i(cols[1]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);
    glVertex2f(x1, y1);

    sgcol = sg_make_color_1i(cols[0]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);
    glVertex2f(x1, y2);

    sgcol = sg_make_color_1i(cols[2]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);
    glVertex2f(x2, y2);

    sgcol = sg_make_color_1i(cols[3]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);
    glVertex2f(x2, y1);

    glEnd();

    glDisable(GL_BLEND);
}

float HudFontWidth(void)
{
    return current_scale * current_font->NominalWidth();
}

float HudFontHeight(void)
{
    return current_scale * current_font->NominalHeight();
}

float HudStringWidth(const char *str)
{
    return current_scale * current_font->StringWidth(str);
}

float HudStringHeight(const char *str)
{
    int slines = current_font->StringLines(str);

    return slines * HudFontHeight() + (slines - 1) * kVerticalSpacing;
}

void HudDrawChar(float left_x, float top_y, const Image *img, char ch,
                 float size)
{
    float sc_x = current_scale;  // TODO * aspect;
    float sc_y = current_scale;

    float x = left_x - img->ScaledOffsetX() * sc_x;
    float y = top_y - img->ScaledOffsetY() * sc_y;

    float w, h;
    float tx1, tx2, ty1, ty2;

    if (epi::StringCaseCompareASCII(img->name_, "FONT_DUMMY_IMAGE") == 0)
    {
        if (current_font->definition_->type_ == kFontTypeTrueType)
        {
            stbtt_aligned_quad *q = current_font->truetype_glyph_map_.at((uint8_t)ch)
                                        .character_quad[current_font_size];
            y = top_y +
                (current_font->truetype_glyph_map_.at((uint8_t)ch)
                     .y_shift[current_font_size] *
                 (size > 0 ? (size / current_font->definition_->default_size_) : 1.0) *
                 sc_y);
            w = ((size > 0 ? (current_font->CharWidth(ch) *
                              (size / current_font->definition_->default_size_))
                           : current_font->CharWidth(ch)) -
                 current_font->spacing_) *
                sc_x;
            h = (current_font->truetype_glyph_map_.at((uint8_t)ch)
                     .height[current_font_size] *
                 (size > 0 ? (size / current_font->definition_->default_size_) : 1.0)) *
                sc_y;
            tx1 = q->s0;
            ty1 = q->t0;
            tx2 = q->s1;
            ty2 = q->t1;
        }
        else  // Patch font atlas
        {
            w = (size > 0 ? (size * current_font->patch_font_cache_.ratio)
                          : current_font->CharWidth(ch)) *
                sc_x;
            h = (size > 0 ? size
                          : (current_font->definition_->default_size_ > 0.0
                                 ? current_font->definition_->default_size_
                                 : current_font->patch_font_cache_.atlas_rectangles
                                       .at(kCP437UnicodeValues[(uint8_t)ch])
                                       .image_height)) *
                sc_y;
            x -= (current_font->patch_font_cache_.atlas_rectangles
                      .at(kCP437UnicodeValues[(uint8_t)ch])
                      .offset_x *
                  sc_x);
            y -= (current_font->patch_font_cache_.atlas_rectangles
                      .at(kCP437UnicodeValues[(uint8_t)ch])
                      .offset_y *
                  sc_y);
            tx1 = current_font->patch_font_cache_.atlas_rectangles.at((uint8_t)ch).texture_coordinate_x;
            ty2 = current_font->patch_font_cache_.atlas_rectangles.at((uint8_t)ch).texture_coordinate_y;
            tx2 = tx1 + current_font->patch_font_cache_.atlas_rectangles.at((uint8_t)ch).texture_coordinate_width;
            ty1 = ty2 + current_font->patch_font_cache_.atlas_rectangles.at((uint8_t)ch).texture_coordinate_height;
        }
    }
    else  // spritesheet font
    {
        w = ((size > 0 ? (size * current_font->CharRatio(ch))
                       : current_font->CharWidth(ch)) -
             current_font->spacing_) *
            sc_x;
        h      = (size > 0 ? size : current_font->image_character_height_) * sc_y;
        int px = (uint8_t)ch % 16;
        int py = 15 - (uint8_t)ch / 16;
        tx1    = (px)*current_font->font_image_->width_ratio_;
        tx2    = (px + 1) * current_font->font_image_->width_ratio_;
        float char_texcoord_adjust =
            ((tx2 - tx1) - ((tx2 - tx1) * (current_font->CharWidth(ch) /
                                           current_font->image_character_width_))) /
            2;
        tx1 += char_texcoord_adjust;
        tx2 -= char_texcoord_adjust;
        ty1 = (py)*current_font->font_image_->height_ratio_;
        ty2 = (py + 1) * current_font->font_image_->height_ratio_;
    }

    float x1 = HudToRealCoordinatesX(x);
    float x2 = HudToRealCoordinatesX(x + w);

    float y1 = HudToRealCoordinatesY(y + h);
    float y2 = HudToRealCoordinatesY(y);

    HudRawImage(x1, y1, x2, y2, img, tx1, ty1, tx2, ty2, current_alpha,
                current_color, nullptr, 0.0, 0.0, ch);
}

void HudDrawEndoomChar(float left_x, float top_y, float FNX, const Image *img,
                       char ch, RGBAColor color1, RGBAColor color2, bool blink)
{
    float w, h;
    float tx1, tx2, ty1, ty2;

    uint8_t character = (uint8_t)ch;

    if (blink && console_cursor >= 16) character = 0x20;

    uint8_t px = character % 16;
    uint8_t py = 15 - character / 16;
    tx1        = (px)*endoom_font->font_image_->width_ratio_;
    tx2        = (px + 1) * endoom_font->font_image_->width_ratio_;
    ty1        = (py)*endoom_font->font_image_->height_ratio_;
    ty2        = (py + 1) * endoom_font->font_image_->height_ratio_;

    w = FNX;
    h = FNX * 2;

    sg_color sgcol = sg_make_color_1i(color2);

    glDisable(GL_TEXTURE_2D);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);

    glBegin(GL_QUADS);

    glVertex2f(left_x, top_y);

    glVertex2f(left_x, top_y + h);

    glVertex2f(left_x + w, top_y + h);

    glVertex2f(left_x + w, top_y);

    glEnd();

    sgcol = sg_make_color_1i(color1);

    glEnable(GL_TEXTURE_2D);

    GLuint tex_id = ImageCache(img, true, (const Colormap *)0, true);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (img->opacity_ == kOpacitySolid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (img->opacity_ != kOpacityComplex) glAlphaFunc(GL_GREATER, 0.66f);
    }

    glColor4f(sgcol.r, sgcol.g, sgcol.b, current_alpha);

    glBegin(GL_QUADS);

    float width_adjust = FNX / 2 + .5;

    glTexCoord2f(tx1, ty1);
    glVertex2f(left_x - width_adjust, top_y);

    glTexCoord2f(tx2, ty1);
    glVertex2f(left_x + w + width_adjust, top_y);

    glTexCoord2f(tx2, ty2);
    glVertex2f(left_x + w + width_adjust, top_y + h);

    glTexCoord2f(tx1, ty2);
    glVertex2f(left_x - width_adjust, top_y + h);

    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    glAlphaFunc(GL_GREATER, 0);
}

//
// Write a string using the current font
//
void HudDrawText(float x, float y, const char *str, float size)
{
    SYS_ASSERT(current_font);

    float cy = y;

    if (current_y_alignment >= 0)
    {
        float total_h = HudStringHeight(str);

        if (current_y_alignment == 0) total_h /= 2.0f;

        cy -= total_h;
    }

    // handle each line

    if (!str) return;

    while (*str)
    {
        // get the length of the line
        int len = 0;
        while (str[len] && str[len] != '\n') len++;

        float cx      = x;
        float total_w = 0;

        for (int i = 0; i < len; i++)
        {
            if (current_font->definition_->type_ == kFontTypeTrueType)
            {
                float factor =
                    size > 0 ? (size / current_font->definition_->default_size_) : 1;
                total_w +=
                    current_font->CharWidth(str[i]) * factor * current_scale;
                if (str[i + 1])
                {
                    total_w += stbtt_GetGlyphKernAdvance(
                                   current_font->truetype_info_,
                                   current_font->GetGlyphIndex(str[i]),
                                   current_font->GetGlyphIndex(str[i + 1])) *
                               current_font->truetype_kerning_scale_[current_font_size] *
                               factor * current_scale;
                }
            }
            else if (current_font->definition_->type_ == kFontTypeImage)
                total_w += (size > 0 ? size * current_font->CharRatio(str[i]) +
                                           current_font->spacing_
                                     : current_font->CharWidth(str[i])) *
                           current_scale;
            else
                total_w += (size > 0 ? size * current_font->patch_font_cache_.ratio +
                                           current_font->spacing_
                                     : current_font->CharWidth(str[i])) *
                           current_scale;
        }

        if (current_x_alignment >= 0)
        {
            if (current_x_alignment == 0) total_w /= 2.0f;

            cx -= total_w;
        }

        for (int k = 0; k < len; k++)
        {
            char ch = str[k];

            const Image *img = current_font->CharImage(ch);

            if (img) HudDrawChar(cx, cy, img, ch, size);

            if (current_font->definition_->type_ == kFontTypeTrueType)
            {
                float factor =
                    size > 0 ? (size / current_font->definition_->default_size_) : 1;
                cx += current_font->CharWidth(ch) * factor * current_scale;
                if (str[k + 1])
                {
                    cx += stbtt_GetGlyphKernAdvance(
                              current_font->truetype_info_,
                              current_font->GetGlyphIndex(str[k]),
                              current_font->GetGlyphIndex(str[k + 1])) *
                          current_font->truetype_kerning_scale_[current_font_size] *
                          factor * current_scale;
                }
            }
            else if (current_font->definition_->type_ == kFontTypeImage)
                cx += (size > 0 ? size * current_font->CharRatio(ch) +
                                      current_font->spacing_
                                : current_font->CharWidth(ch)) *
                      current_scale;
            else
                cx += (size > 0 ? size * current_font->patch_font_cache_.ratio +
                                      current_font->spacing_
                                : current_font->CharWidth(ch)) *
                      current_scale;
        }

        if (str[len] == 0) break;

        str += (len + 1);
        cy += (size > 0 ? size : HudFontHeight()) + kVerticalSpacing;
    }
}

void HudDrawQuitText(int line, float FNX, float FNY, float cx)
{
    SYS_ASSERT(quit_lines[line]);

    float cy = (float)current_screen_height - ((25 - line) * FNY);

    const Image *img = endoom_font->font_image_;

    SYS_ASSERT(img);

    for (int i = 0; i < 80; i++)
    {
        uint8_t info = quit_lines[line]->endoom_bytes_.at(i);

        HudDrawEndoomChar(cx, cy, FNX, img, quit_lines[line]->line_.at(i),
                          endoom_colors[info & 15],
                          endoom_colors[(info >> 4) & 7], info & 128);

        cx += FNX;
    }
}

//
// Draw the ENDOOM screen
//

void HudDrawQuitScreen()
{
    SYS_ASSERT(endoom_font);

    if (quit_lines[0])
    {
        float FNX =
            HMM_MIN((float)current_screen_width / 80.0f,
                    320.0f / 80.0f * ((float)current_screen_height * 0.90f / 200.0f));
        float FNY = FNX * 2;
        float cx  = HMM_MAX(0, (((float)current_screen_width - (FNX * 80.0f)) / 2.0f));
        for (int i = 0; i < kEndoomLines; i++)
        {
            HudDrawQuitText(i, FNX, FNY, cx);
        }
        HudSetAlignment(0, -1);
        HudDrawText(
            160, 195 - HudStringHeight("Are you sure you want to quit? (Y/N)"),
            "Are you sure you want to quit? (Y/N)");
    }
    else
    {
        HudSetAlignment(0, -1);
        HudDrawText(
            160,
            100 - (HudStringHeight("Are you sure you want to quit? (Y/N)") / 2),
            "Are you sure you want to quit? (Y/N)");
    }
}

void HudRenderWorld(float x, float y, float w, float h, MapObject *camera,
                    int flags)
{
    HudPushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    hud_visible_bottom = y + h;
    hud_visible_top    = 200 - hud_visible_bottom;

    int *xy = scissor_stack[scissor_stack_top - 1];

    bool full_height = h > (hud_y_bottom - hud_y_top) * 0.95;

    // FIXME explain this weirdness
    float width    = HudToRealCoordinatesX(x + w) - HudToRealCoordinatesX(x);
    float expand_w = (xy[2] - xy[0]) / width;

    // renderer needs true (OpenGL) coordinates.
    // get from scissor due to the expansion thing [ FIXME: HACKY ]
    float x1 = xy[0];  // HudToRealCoordinatesX(x);
    float y1 = xy[1];  // HudToRealCoordinatesY(y);
    float x2 = xy[2];  // HudToRealCoordinatesX(x+w);
    float y2 = xy[3];  // HudToRealCoordinatesY(y+h);

    RenderView(x1, y1, x2 - x1, y2 - y1, camera, full_height, expand_w);

    HudPopScissor();
}

void HudRenderAutomap(float x, float y, float w, float h, MapObject *player,
                      int flags)
{
    HudPushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    // [ FIXME HACKY ]
    if ((flags & 1) == 0)
    {
        if (x < 1 && x + w > hud_x_middle * 2 - 1)
        {
            x = hud_x_left;
            w = hud_x_right - x;
        }
    }

    AutomapRender(x, y, w, h, player);

    HudPopScissor();
}

void HudGetCastPosition(float *x, float *y, float *scale_x, float *scale_y)
{
    *x = HudToRealCoordinatesX(160);
    *y = HudToRealCoordinatesY(170);

    // FIXME REVIEW THIS
    //*scale_y = 4.0;
    *scale_x = *scale_y / pixel_aspect_ratio.f_;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
