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

#include "i_defs.h"
#include "i_defs_gl.h"

#include "font.h"

#include "con_main.h"

#include "am_map.h"
#include "g_game.h"
#include "r_misc.h"
#include "r_gldefs.h"
#include "r_units.h"
#include "r_colormap.h"
#include "hu_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_misc.h" //  R_Render

#include "str_compare.h"

#define DUMMY_WIDTH(font) (4)
#define DUMMY_CLAMP       789

#define HU_CHAR(ch) (islower(ch) ? epi::ToUpperASCII(ch) : (ch))
#define HU_INDEX(c) ((unsigned char)HU_CHAR(c))

// FIXME: this seems totally arbitrary, review it.
#define VERT_SPACING 2.0f

extern console_line_c *quit_lines[ENDOOM_LINES];
extern int             con_cursor;
extern font_c         *endoom_font;
extern cvar_c          r_overlay;
extern cvar_c          r_doubleframes;

static font_c *default_font;

extern int gametic;
int        hudtic;

int  hud_swirl_pass   = 0;
bool hud_thick_liquid = false;

float hud_x_left;
float hud_x_right;
float hud_x_mid;
float hud_visible_top;
float hud_visible_bottom;

float hud_y_top;
float hud_y_bottom;

// current state
static font_c  *cur_font;
static RGBAColor cur_color;

static float cur_scale, cur_alpha;
static int   cur_x_align, cur_y_align;

// mapping from hud X and Y coords to real (OpenGL) coords.
// note that Y coordinates get inverted.
static float margin_X;
static float margin_Y;
static float margin_XMUL;
static float margin_YMUL;

std::vector<std::string> hud_overlays = {
    "",
    "OVERLAY_LINES_1X",
    "OVERLAY_LINES_2X",
    "OVERLAY_VERTICAL_1X",
    "OVERLAY_VERTICAL_2X",
    "OVERLAY_GRILL_1X",
    "OVERLAY_GRILL_2X",
};

static inline float COORD_X(float x)
{
    return margin_X + x * margin_XMUL;
}

static inline float COORD_Y(float y)
{
    return margin_Y - y * margin_YMUL;
}

DEF_CVAR(v_letterbox, "0", CVAR_ARCHIVE)
DEF_CVAR(v_pillarbox, "0", CVAR_ARCHIVE)

void HUD_SetCoordSys(int width, int height)
{
    if (width < 1 || height < 1)
        return;

    float sw = (float)SCREENWIDTH;
    float sh = (float)SCREENHEIGHT;

    /* compute Y stuff */

    hud_y_top    = 0.0f;
    hud_y_bottom = height;

    margin_Y    = sh;
    margin_YMUL = sh / (float)height;

    /* compute X stuff */

    hud_x_mid = width * 0.5f;

    if (false)
    {
        // simply stretch to fit [ bad!! ]

        margin_X    = 0.0;
        margin_XMUL = sw / (float)width;

        hud_x_left  = 0.0;
        hud_x_right = width * 1.0f;
    }
    else
    {
        float side_dist = (float)width / 2.0;

        // compensate for size of window or screen.
        side_dist = side_dist * (sw / 320.0f) / (sh / 200.0f);

        // compensate for monitor's pixel aspect
        side_dist = side_dist * v_pixelaspect.f;

        // compensate for Doom's 5:6 pixel aspect ratio.
        if (true)
        {
            side_dist = side_dist / DOOHMM_PIXEL_ASPECT;
        }

        hud_x_left  = hud_x_mid - side_dist;
        hud_x_right = hud_x_mid + side_dist;

        margin_XMUL = sw / side_dist / 2.0;
        margin_X    = 0.0f - hud_x_left * margin_XMUL;

        /* DEBUG
        fprintf(stderr, "side: %1.0f  L %1.0f R %1.0f  X %1.0f XMUL %1.6f\n",
            side_dist, hud_x_left, hud_x_right, margin_X, margin_XMUL);
        */
    }

    // TODO letterboxing and pillarboxing
}

void HUD_SetFont(font_c *font)
{
    cur_font = font ? font : default_font;
}

void HUD_SetScale(float scale)
{
    cur_scale = scale;
}

void HUD_SetTextColor(RGBAColor color)
{
    cur_color = color;
}

void HUD_SetAlpha(float alpha)
{
    cur_alpha = alpha;
}

float HUD_GetAlpha()
{
    return cur_alpha;
}

void HUD_SetAlignment(int xa, int ya)
{
    cur_x_align = xa;
    cur_y_align = ya;
}

void HUD_Reset()
{
    HUD_SetCoordSys(320, 200);

    cur_font    = default_font;
    cur_color   = kRGBANoValue;
    cur_scale   = 1.0f;
    cur_alpha   = 1.0f;
    cur_x_align = -1;
    cur_y_align = -1;
}

void HUD_FrameSetup(void)
{
    if (default_font == NULL)
    {
        // FIXME: get default font from DDF gamedef
        fontdef_c *DEF = fontdefs.Lookup("DOOM");
        SYS_ASSERT(DEF);

        default_font = hu_fonts.Lookup(DEF);
        SYS_ASSERT(default_font);
    }

    HUD_Reset();

    hudtic++;
}

#define MAX_SCISSOR_STACK 10

static int scissor_stack[MAX_SCISSOR_STACK][4];
static int sci_stack_top = 0;

void HUD_PushScissor(float x1, float y1, float x2, float y2, bool expand)
{
    SYS_ASSERT(sci_stack_top < MAX_SCISSOR_STACK);

    // expand rendered view to cover whole screen
    if (expand && x1 < 1 && x2 > hud_x_mid * 2 - 1)
    {
        x1 = 0;
        x2 = SCREENWIDTH;
    }
    else
    {
        x1 = COORD_X(x1);
        x2 = COORD_X(x2);
    }

    std::swap(y1, y2);

    y1 = COORD_Y(y1);
    y2 = COORD_Y(y2);

    int sx1 = I_ROUND(x1);
    int sy1 = I_ROUND(y1);
    int sx2 = I_ROUND(x2);
    int sy2 = I_ROUND(y2);

    if (sci_stack_top == 0)
    {
        glEnable(GL_SCISSOR_TEST);

        sx1 = HMM_MAX(sx1, 0);
        sy1 = HMM_MAX(sy1, 0);

        sx2 = HMM_MIN(sx2, SCREENWIDTH);
        sy2 = HMM_MIN(sy2, SCREENHEIGHT);
    }
    else
    {
        // clip to previous scissor
        int *xy = scissor_stack[sci_stack_top - 1];

        sx1 = HMM_MAX(sx1, xy[0]);
        sy1 = HMM_MAX(sy1, xy[1]);

        sx2 = HMM_MIN(sx2, xy[2]);
        sy2 = HMM_MIN(sy2, xy[3]);
    }

    SYS_ASSERT(sx2 >= sx1);
    SYS_ASSERT(sy2 >= sy1);

    glScissor(sx1, sy1, sx2 - sx1, sy2 - sy1);

    // push current scissor
    int *xy = scissor_stack[sci_stack_top];

    xy[0] = sx1;
    xy[1] = sy1;
    xy[2] = sx2;
    xy[3] = sy2;

    sci_stack_top++;
}

void HUD_PopScissor()
{
    SYS_ASSERT(sci_stack_top > 0);

    sci_stack_top--;

    if (sci_stack_top == 0)
    {
        glDisable(GL_SCISSOR_TEST);
    }
    else
    {
        // restore previous scissor
        int *xy = scissor_stack[sci_stack_top];

        glScissor(xy[0], xy[1], xy[2] - xy[0], xy[3] - xy[1]);
    }
}

// Adapted from Quake 3 GPL release
void HUD_CalcScrollTexCoords(float x_scroll, float y_scroll, float *tx1, float *ty1, float *tx2, float *ty2)
{
    float timeScale, adjustedScrollS, adjustedScrollT;

    timeScale = gametic / (r_doubleframes.d ? 200.0f : 100.0f);

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
void HUD_CalcTurbulentTexCoords(float *tx, float *ty, float x, float y)
{
    float now;
    float phase     = 0;
    float frequency = hud_thick_liquid ? 0.5 : 1.0;
    float amplitude = 0.05;

    now = (phase + hudtic / 100.0f * frequency);

    if (swirling_flats == SWIRL_PARALLAX)
    {
        frequency *= 2;
        if (hud_thick_liquid)
        {
            if (hud_swirl_pass == 1)
            {
                *tx = *tx +
                      r_sintable[(int)((x * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
                *ty = *ty +
                      r_sintable[(int)((y * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
            }
            else
            {
                amplitude = 0;
                *tx       = *tx -
                      r_sintable[(int)((x * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
                *ty = *ty -
                      r_sintable[(int)((y * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
            }
        }
        else
        {
            if (hud_swirl_pass == 1)
            {
                amplitude = 0.025;
                *tx       = *tx +
                      r_sintable[(int)((x * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
                *ty = *ty +
                      r_sintable[(int)((y * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
            }
            else
            {
                amplitude = 0.015;
                *tx       = *tx -
                      r_sintable[(int)((x * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
                *ty = *ty -
                      r_sintable[(int)((y * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
            }
        }
    }
    else
    {
        *tx = *tx + r_sintable[(int)((x * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
        *ty = *ty + r_sintable[(int)((y * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE) & (FUNCTABLE_MASK)] * amplitude;
    }
}

//----------------------------------------------------------------------------

void HUD_RawImage(float hx1, float hy1, float hx2, float hy2, const image_c *image, float tx1, float ty1, float tx2,
                  float ty2, float alpha, RGBAColor text_col, const colourmap_c *palremap, float sx, float sy, char ch)
{
    int x1 = I_ROUND(hx1);
    int y1 = I_ROUND(hy1);
    int x2 = I_ROUND(hx2 + 0.25f);
    int y2 = I_ROUND(hy2 + 0.25f);

    if (x1 >= x2 || y1 >= y2)
        return;

    if (x2 < 0 || x1 > SCREENWIDTH || y2 < 0 || y1 > SCREENHEIGHT)
        return;

    sg_color sgcol = sg_white;

    bool do_whiten = false;

    if (text_col != kRGBANoValue)
    {
        sgcol = sg_make_color_1i(text_col);
        sgcol.a = 1.0f;
        do_whiten = true;
    }

    if (epi::StringCaseCompareASCII(image->name, "FONT_DUMMY_IMAGE") == 0)
    {
        if (cur_font->def->type == FNTYP_TrueType)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_2D);
            if ((var_smoothing && cur_font->def->ttf_smoothing == cur_font->def->TTF_SMOOTH_ON_DEMAND) ||
                cur_font->def->ttf_smoothing == cur_font->def->TTF_SMOOTH_ALWAYS)
                glBindTexture(GL_TEXTURE_2D, cur_font->ttf_smoothed_tex_id[current_font_size]);
            else
                glBindTexture(GL_TEXTURE_2D, cur_font->ttf_tex_id[current_font_size]);
        }
        else // patch font
        {
            glEnable(GL_ALPHA_TEST);
            glEnable(GL_BLEND);
            glEnable(GL_TEXTURE_2D);
            if ((var_smoothing && cur_font->def->ttf_smoothing == cur_font->def->TTF_SMOOTH_ON_DEMAND) ||
                cur_font->def->ttf_smoothing == cur_font->def->TTF_SMOOTH_ALWAYS)
            {
                if (do_whiten)
                    glBindTexture(GL_TEXTURE_2D, cur_font->p_cache.atlas_whitened_smoothed_texid);
                else
                    glBindTexture(GL_TEXTURE_2D, cur_font->p_cache.atlas_smoothed_texid);
            }
            else
            {
                if (do_whiten)
                    glBindTexture(GL_TEXTURE_2D, cur_font->p_cache.atlas_whitened_texid);
                else
                    glBindTexture(GL_TEXTURE_2D, cur_font->p_cache.atlas_texid);
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
        return;
    }

    // GLuint tex_id = W_ImageCache(image, true, palremap, do_whiten);
    GLuint tex_id = W_ImageCache(image, true, nullptr, do_whiten);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (alpha >= 0.99f && image->opacity == OPAC_Solid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (!(alpha < 0.11f || image->opacity == OPAC_Complex))
            glAlphaFunc(GL_GREATER, alpha * 0.66f);
    }

    if (image->opacity == OPAC_Complex || alpha < 0.99f)
        glEnable(GL_BLEND);

    GLint old_s_clamp = DUMMY_CLAMP;
    GLint old_t_clamp = DUMMY_CLAMP;

    if (sx != 0.0 || sy != 0.0)
    {
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &old_s_clamp);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_t_clamp);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        HUD_CalcScrollTexCoords(sx, sy, &tx1, &ty1, &tx2, &ty2);
    }

    if (epi::StringCaseCompareASCII(image->name, hud_overlays.at(r_overlay.d)) == 0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    bool hud_swirl = false;

    if (image->liquid_type > LIQ_None && swirling_flats > SWIRL_SMMU)
    {
        hud_swirl_pass = 1;
        hud_swirl      = true;
    }

    if (image->liquid_type == LIQ_Thick)
        hud_thick_liquid = true;

    glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);

    glBegin(GL_QUADS);

    if (hud_swirl)
    {
        HUD_CalcTurbulentTexCoords(&tx1, &ty1, x1, y1);
        HUD_CalcTurbulentTexCoords(&tx2, &ty2, x2, y2);
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

    if (hud_swirl && swirling_flats == SWIRL_PARALLAX)
    {
        hud_swirl_pass = 2;
        tx1 += 0.2;
        tx2 += 0.2;
        ty1 += 0.2;
        ty2 += 0.2;
        HUD_CalcTurbulentTexCoords(&tx1, &ty1, x1, y1);
        HUD_CalcTurbulentTexCoords(&tx2, &ty2, x2, y2);
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

    if (old_s_clamp != DUMMY_CLAMP)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, old_s_clamp);

    if (old_t_clamp != DUMMY_CLAMP)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_t_clamp);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    glAlphaFunc(GL_GREATER, 0);
}

void HUD_RawFromTexID(float hx1, float hy1, float hx2, float hy2, unsigned int tex_id, image_opacity_e opacity,
                      float tx1, float ty1, float tx2, float ty2, float alpha)
{
    int x1 = I_ROUND(hx1);
    int y1 = I_ROUND(hy1);
    int x2 = I_ROUND(hx2 + 0.25f);
    int y2 = I_ROUND(hy2 + 0.25f);

    if (x1 >= x2 || y1 >= y2)
        return;

    if (x2 < 0 || x1 > SCREENWIDTH || y2 < 0 || y1 > SCREENHEIGHT)
        return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (alpha >= 0.99f && opacity == OPAC_Solid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (!(alpha < 0.11f || opacity == OPAC_Complex))
            glAlphaFunc(GL_GREATER, alpha * 0.66f);
    }

    if (opacity == OPAC_Complex || alpha < 0.99f)
        glEnable(GL_BLEND);

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

void HUD_StretchFromImageData(float x, float y, float w, float h, const image_data_c *img, unsigned int tex_id,
                              image_opacity_e opacity)
{
    if (cur_x_align >= 0)
        x -= w / (cur_x_align == 0 ? 2.0f : 1.0f);

    if (cur_y_align >= 0)
        y -= h / (cur_y_align == 0 ? 2.0f : 1.0f);

    float x1 = COORD_X(x);
    float x2 = COORD_X(x + w);

    float y1 = COORD_Y(y + h);
    float y2 = COORD_Y(y);

    HUD_RawFromTexID(x1, y1, x2, y2, tex_id, opacity, 0, 0, (float)img->used_w / img->width,
                     (float)img->used_h / img->height, cur_alpha);
}

void HUD_StretchImage(float x, float y, float w, float h, const image_c *img, float sx, float sy,
                      const colourmap_c *colmap)
{
    if (cur_x_align >= 0)
        x -= w / (cur_x_align == 0 ? 2.0f : 1.0f);

    if (cur_y_align >= 0)
        y -= h / (cur_y_align == 0 ? 2.0f : 1.0f);

    x -= IM_OFFSETX(img);
    y -= IM_OFFSETY(img);

    float x1 = COORD_X(x);
    float x2 = COORD_X(x + w);

    float y1 = COORD_Y(y + h);
    float y2 = COORD_Y(y);

    RGBAColor text_col = kRGBANoValue;

    if (colmap)
    {
        text_col = V_GetFontColor(colmap);
    }

    // HUD_RawImage(x1, y1, x2, y2, img, 0, 0, IM_RIGHT(img), IM_TOP(img), cur_alpha, text_col, colmap, sx, sy);
    HUD_RawImage(x1, y1, x2, y2, img, 0, 0, IM_RIGHT(img), IM_TOP(img), cur_alpha, text_col, nullptr, sx, sy);
}

void HUD_StretchImageNoOffset(float x, float y, float w, float h, const image_c *img, float sx, float sy)
{
    if (cur_x_align >= 0)
        x -= w / (cur_x_align == 0 ? 2.0f : 1.0f);

    if (cur_y_align >= 0)
        y -= h / (cur_y_align == 0 ? 2.0f : 1.0f);

    // x -= IM_OFFSETX(img);
    // y -= IM_OFFSETY(img);

    float x1 = COORD_X(x);
    float x2 = COORD_X(x + w);

    float y1 = COORD_Y(y + h);
    float y2 = COORD_Y(y);

    HUD_RawImage(x1, y1, x2, y2, img, 0, 0, IM_RIGHT(img), IM_TOP(img), cur_alpha, kRGBANoValue, NULL, sx, sy);
}

void HUD_DrawImageTitleWS(const image_c *title_image)
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
    TempScale /= title_image->actual_h;

    TempWidth  = IM_WIDTH(title_image) * TempScale; // respect ASPECT in images.ddf at least
    TempHeight = title_image->actual_h * TempScale;

    // 2. Calculate centering on screen.
    CenterX = 160;
    CenterX -= TempWidth / 2;

    // 3. Draw it.
    HUD_StretchImage(CenterX, -0.1f, TempWidth, TempHeight + 0.1f, title_image, 0.0, 0.0);
}

float HUD_GetImageWidth(const image_c *img)
{
    return (IM_WIDTH(img) * cur_scale);
}

float HUD_GetImageHeight(const image_c *img)
{
    return (IM_HEIGHT(img) * cur_scale);
}

void HUD_DrawImage(float x, float y, const image_c *img, const colourmap_c *colmap)
{
    float w = IM_WIDTH(img) * cur_scale;
    float h = IM_HEIGHT(img) * cur_scale;

    HUD_StretchImage(x, y, w, h, img, 0.0, 0.0, colmap);
}

void HUD_DrawImageNoOffset(float x, float y, const image_c *img)
{
    float w = IM_WIDTH(img) * cur_scale;
    float h = IM_HEIGHT(img) * cur_scale;

    HUD_StretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
}

void HUD_ScrollImage(float x, float y, const image_c *img, float sx, float sy)
{
    float w = IM_WIDTH(img) * cur_scale;
    float h = IM_HEIGHT(img) * cur_scale;

    HUD_StretchImage(x, y, w, h, img, sx, sy);
}

void HUD_ScrollImageNoOffset(float x, float y, const image_c *img, float sx, float sy)
{
    float w = IM_WIDTH(img) * cur_scale;
    float h = IM_HEIGHT(img) * cur_scale;

    HUD_StretchImageNoOffset(x, y, w, h, img, sx, sy);
}

void HUD_TileImage(float x, float y, float w, float h, const image_c *img, float offset_x, float offset_y)
{
    if (cur_x_align >= 0)
        x -= w / (cur_x_align == 0 ? 2.0f : 1.0f);

    if (cur_y_align >= 0)
        y -= h / (cur_y_align == 0 ? 2.0f : 1.0f);

    offset_x /= w;
    offset_y /= -h;

    float tx_scale = w / IM_TOTAL_WIDTH(img) / cur_scale;
    float ty_scale = h / IM_TOTAL_HEIGHT(img) / cur_scale;

    float x1 = COORD_X(x);
    float x2 = COORD_X(x + w);

    float y1 = COORD_Y(y + h);
    float y2 = COORD_Y(y);

    HUD_RawImage(x1, y1, x2, y2, img, (offset_x)*tx_scale, (offset_y)*ty_scale, (offset_x + 1) * tx_scale,
                 (offset_y + 1) * ty_scale, cur_alpha);
}

void HUD_SolidBox(float x1, float y1, float x2, float y2, RGBAColor col)
{
    // expand to cover wide screens
    if (x1 < hud_x_left && x2 > hud_x_right - 1 && y1 < hud_y_top + 1 && y2 > hud_y_bottom - 1)
    {
        x1 = 0;
        x2 = SCREENWIDTH;
        y1 = 0;
        y2 = SCREENHEIGHT;
    }
    else
    {
        std::swap(y1, y2);

        x1 = COORD_X(x1);
        y1 = COORD_Y(y1);
        x2 = COORD_X(x2);
        y2 = COORD_Y(y2);
    }

    if (cur_alpha < 0.99f)
        glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);

    glBegin(GL_QUADS);

    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);

    glEnd();

    glDisable(GL_BLEND);
}

void HUD_SolidLine(float x1, float y1, float x2, float y2, RGBAColor col, float thickness, bool smooth, float dx,
                   float dy)
{
    x1 = COORD_X(x1);
    y1 = COORD_Y(y1);
    x2 = COORD_X(x2);
    y2 = COORD_Y(y2);

    dx = COORD_X(dx) - COORD_X(0);
    dy = COORD_Y(0) - COORD_Y(dy);

    glLineWidth(thickness);

    if (smooth)
        glEnable(GL_LINE_SMOOTH);

    if (smooth || cur_alpha < 0.99f)
        glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);

    glBegin(GL_LINES);

    glVertex2i((int)x1 + (int)dx, (int)y1 + (int)dy);
    glVertex2i((int)x2 + (int)dx, (int)y2 + (int)dy);

    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
}

void HUD_ThinBox(float x1, float y1, float x2, float y2, RGBAColor col, float thickness)
{
    std::swap(y1, y2);

    x1 = COORD_X(x1);
    y1 = COORD_Y(y1);
    x2 = COORD_X(x2);
    y2 = COORD_Y(y2);

    if (cur_alpha < 0.99f)
        glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);

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

void HUD_GradientBox(float x1, float y1, float x2, float y2, RGBAColor *cols)
{
    std::swap(y1, y2);

    x1 = COORD_X(x1);
    y1 = COORD_Y(y1);
    x2 = COORD_X(x2);
    y2 = COORD_Y(y2);

    if (cur_alpha < 0.99f)
        glEnable(GL_BLEND);

    glBegin(GL_QUADS);

    sg_color sgcol = sg_make_color_1i(cols[1]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);
    glVertex2f(x1, y1);

    sgcol = sg_make_color_1i(cols[0]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);
    glVertex2f(x1, y2);

    sgcol = sg_make_color_1i(cols[2]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);
    glVertex2f(x2, y2);

    sgcol = sg_make_color_1i(cols[3]);
    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);
    glVertex2f(x2, y1);

    glEnd();

    glDisable(GL_BLEND);
}

float HUD_FontWidth(void)
{
    return cur_scale * cur_font->NominalWidth();
}

float HUD_FontHeight(void)
{
    return cur_scale * cur_font->NominalHeight();
}

float HUD_StringWidth(const char *str)
{
    return cur_scale * cur_font->StringWidth(str);
}

float HUD_StringHeight(const char *str)
{
    int slines = cur_font->StringLines(str);

    return slines * HUD_FontHeight() + (slines - 1) * VERT_SPACING;
}

void HUD_DrawChar(float left_x, float top_y, const image_c *img, char ch, float size)
{
    float sc_x = cur_scale; // TODO * aspect;
    float sc_y = cur_scale;

    float x = left_x - IM_OFFSETX(img) * sc_x;
    float y = top_y - IM_OFFSETY(img) * sc_y;

    float w, h;
    float tx1, tx2, ty1, ty2;

    if (epi::StringCaseCompareASCII(img->name, "FONT_DUMMY_IMAGE") == 0)
    {
        if (cur_font->def->type == FNTYP_TrueType)
        {
            stbtt_aligned_quad *q = cur_font->ttf_glyph_map.at((uint8_t)ch).char_quad[current_font_size];
            y                     = top_y + (cur_font->ttf_glyph_map.at((uint8_t)ch).y_shift[current_font_size] *
                        (size > 0 ? (size / cur_font->def->default_size) : 1.0) * sc_y);
            w = ((size > 0 ? (cur_font->CharWidth(ch) * (size / cur_font->def->default_size)) : cur_font->CharWidth(ch)) -
                cur_font->spacing) *
                sc_x;
            h = (cur_font->ttf_glyph_map.at((uint8_t)ch).height[current_font_size] *
                (size > 0 ? (size / cur_font->def->default_size) : 1.0)) *
                sc_y;
            tx1 = q->s0;
            ty1 = q->t0;
            tx2 = q->s1;
            ty2 = q->t1;
        }
        else // Patch font atlas
        {
            w = (size > 0 ? (size * cur_font->p_cache.ratio) : cur_font->CharWidth(ch)) * sc_x;
            h = (size > 0 ? size : (cur_font->def->default_size > 0.0 ? cur_font->def->default_size : 
                cur_font->p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)ch]).ih)) * sc_y;
            x -= (cur_font->p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)ch]).off_x * sc_x);
            y -= (cur_font->p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)ch]).off_y * sc_y);
            tx1 = cur_font->p_cache.atlas_rects.at((uint8_t)ch).tx;
            ty2 = cur_font->p_cache.atlas_rects.at((uint8_t)ch).ty;
            tx2 = tx1 + cur_font->p_cache.atlas_rects.at((uint8_t)ch).tw;
            ty1 = ty2 + cur_font->p_cache.atlas_rects.at((uint8_t)ch).th;
        }
    }
    else // spritesheet font
    {
        w      = ((size > 0 ? (size * cur_font->CharRatio(ch)) : cur_font->CharWidth(ch)) - cur_font->spacing) * sc_x;
        h      = (size > 0 ? size : cur_font->im_char_height) * sc_y;
        int px = (uint8_t)ch % 16;
        int py = 15 - (uint8_t)ch / 16;
        tx1    = (px)*cur_font->font_image->ratio_w;
        tx2    = (px + 1) * cur_font->font_image->ratio_w;
        float char_texcoord_adjust =
            ((tx2 - tx1) - ((tx2 - tx1) * (cur_font->CharWidth(ch) / cur_font->im_char_width))) / 2;
        tx1 += char_texcoord_adjust;
        tx2 -= char_texcoord_adjust;
        ty1 = (py)*cur_font->font_image->ratio_h;
        ty2 = (py + 1) * cur_font->font_image->ratio_h;
    }

    float x1 = COORD_X(x);
    float x2 = COORD_X(x + w);

    float y1 = COORD_Y(y + h);
    float y2 = COORD_Y(y);

    HUD_RawImage(x1, y1, x2, y2, img, tx1, ty1, tx2, ty2, cur_alpha, cur_color, NULL, 0.0, 0.0, ch);
}

void HUD_DrawEndoomChar(float left_x, float top_y, float FNX, const image_c *img, char ch, RGBAColor color1,
                        RGBAColor color2, bool blink)
{
    float w, h;
    float tx1, tx2, ty1, ty2;

    uint8_t character = (uint8_t)ch;

    if (blink && con_cursor >= 16)
        character = 0x20;

    uint8_t px = character % 16;
    uint8_t py = 15 - character / 16;
    tx1     = (px)*endoom_font->font_image->ratio_w;
    tx2     = (px + 1) * endoom_font->font_image->ratio_w;
    ty1     = (py)*endoom_font->font_image->ratio_h;
    ty2     = (py + 1) * endoom_font->font_image->ratio_h;

    w = FNX;
    h = FNX * 2;

    sg_color sgcol = sg_make_color_1i(color2);

    glDisable(GL_TEXTURE_2D);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);

    glBegin(GL_QUADS);

    glVertex2f(left_x, top_y);

    glVertex2f(left_x, top_y + h);

    glVertex2f(left_x + w, top_y + h);

    glVertex2f(left_x + w, top_y);

    glEnd();

    sgcol = sg_make_color_1i(color1);

    glEnable(GL_TEXTURE_2D);

    GLuint tex_id = W_ImageCache(img, true, (const colourmap_c *)0, true);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (img->opacity == OPAC_Solid)
        glDisable(GL_ALPHA_TEST);
    else
    {
        glEnable(GL_ALPHA_TEST);

        if (img->opacity != OPAC_Complex)
            glAlphaFunc(GL_GREATER, 0.66f);
    }

    glColor4f(sgcol.r, sgcol.g, sgcol.b, cur_alpha);

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
void HUD_DrawText(float x, float y, const char *str, float size)
{
    SYS_ASSERT(cur_font);

    float cy = y;

    if (cur_y_align >= 0)
    {
        float total_h = HUD_StringHeight(str);

        if (cur_y_align == 0)
            total_h /= 2.0f;

        cy -= total_h;
    }

    // handle each line

    if (!str)
        return;

    while (*str)
    {
        // get the length of the line
        int len = 0;
        while (str[len] && str[len] != '\n')
            len++;

        float cx      = x;
        float total_w = 0;

        for (int i = 0; i < len; i++)
        {
            if (cur_font->def->type == FNTYP_TrueType)
            {
                float factor = size > 0 ? (size / cur_font->def->default_size) : 1;
                total_w += cur_font->CharWidth(str[i]) * factor * cur_scale;
                if (str[i + 1])
                {
                    total_w += stbtt_GetGlyphKernAdvance(cur_font->ttf_info, cur_font->GetGlyphIndex(str[i]),
                                                         cur_font->GetGlyphIndex(str[i + 1])) *
                               cur_font->ttf_kern_scale[current_font_size] * factor * cur_scale;
                }
            }
            else if (cur_font->def->type == FNTYP_Image)
                total_w +=
                    (size > 0 ? size * cur_font->CharRatio(str[i]) + cur_font->spacing : cur_font->CharWidth(str[i])) *
                    cur_scale;
            else
                total_w +=
                    (size > 0 ? size * cur_font->p_cache.ratio + cur_font->spacing : cur_font->CharWidth(str[i])) *
                    cur_scale;
        }

        if (cur_x_align >= 0)
        {
            if (cur_x_align == 0)
                total_w /= 2.0f;

            cx -= total_w;
        }

        for (int k = 0; k < len; k++)
        {
            char ch = str[k];

            const image_c *img = cur_font->CharImage(ch);

            if (img)
                HUD_DrawChar(cx, cy, img, ch, size);

            if (cur_font->def->type == FNTYP_TrueType)
            {
                float factor = size > 0 ? (size / cur_font->def->default_size) : 1;
                cx += cur_font->CharWidth(ch) * factor * cur_scale;
                if (str[k + 1])
                {
                    cx += stbtt_GetGlyphKernAdvance(cur_font->ttf_info, cur_font->GetGlyphIndex(str[k]),
                                                    cur_font->GetGlyphIndex(str[k + 1])) *
                          cur_font->ttf_kern_scale[current_font_size] * factor * cur_scale;
                }
            }
            else if (cur_font->def->type == FNTYP_Image)
                cx += (size > 0 ? size * cur_font->CharRatio(ch) + cur_font->spacing : cur_font->CharWidth(ch)) *
                      cur_scale;
            else
                cx += (size > 0 ? size * cur_font->p_cache.ratio + cur_font->spacing : cur_font->CharWidth(ch)) *
                      cur_scale;
        }

        if (str[len] == 0)
            break;

        str += (len + 1);
        cy += (size > 0 ? size : HUD_FontHeight()) + VERT_SPACING;
    }
}

void HUD_DrawQuitText(int line, float FNX, float FNY, float cx)
{
    SYS_ASSERT(quit_lines[line]);

    float cy = (float)SCREENHEIGHT - ((25 - line) * FNY);

    const image_c *img = endoom_font->font_image;

    SYS_ASSERT(img);

    for (int i = 0; i < 80; i++)
    {
        uint8_t info = quit_lines[line]->endoom_bytes.at(i);

        HUD_DrawEndoomChar(cx, cy, FNX, img, quit_lines[line]->line.at(i), endoom_colors[info & 15],
                           endoom_colors[(info >> 4) & 7], info & 128);

        cx += FNX;
    }
}

//
// Draw the ENDOOM screen
//

void HUD_DrawQuitScreen()
{
    SYS_ASSERT(endoom_font);

    if (quit_lines[0])
    {
        float FNX = HMM_MIN((float)SCREENWIDTH / 80.0f, 320.0f / 80.0f * ((float)SCREENHEIGHT * 0.90f / 200.0f));
        float FNY = FNX * 2;
        float cx  = HMM_MAX(0, (((float)SCREENWIDTH - (FNX * 80.0f)) / 2.0f));
        for (int i = 0; i < ENDOOM_LINES; i++)
        {
            HUD_DrawQuitText(i, FNX, FNY, cx);
        }
        HUD_SetAlignment(0, -1);
        HUD_DrawText(160, 195 - HUD_StringHeight("Are you sure you want to quit? (Y/N)"),
                     "Are you sure you want to quit? (Y/N)");
    }
    else
    {
        HUD_SetAlignment(0, -1);
        HUD_DrawText(160, 100 - (HUD_StringHeight("Are you sure you want to quit? (Y/N)") / 2),
                     "Are you sure you want to quit? (Y/N)");
    }
}

void HUD_RenderWorld(float x, float y, float w, float h, mobj_t *camera, int flags)
{
    HUD_PushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    hud_visible_bottom = y + h;
    hud_visible_top    = 200 - hud_visible_bottom;

    int *xy = scissor_stack[sci_stack_top - 1];

    bool full_height = h > (hud_y_bottom - hud_y_top) * 0.95;

    // FIXME explain this weirdness
    float width    = COORD_X(x + w) - COORD_X(x);
    float expand_w = (xy[2] - xy[0]) / width;

    // renderer needs true (OpenGL) coordinates.
    // get from scissor due to the expansion thing [ FIXME: HACKY ]
    float x1 = xy[0]; // COORD_X(x);
    float y1 = xy[1]; // COORD_Y(y);
    float x2 = xy[2]; // COORD_X(x+w);
    float y2 = xy[3]; // COORD_Y(y+h);

    R_Render(x1, y1, x2 - x1, y2 - y1, camera, full_height, expand_w);

    HUD_PopScissor();
}

void HUD_RenderAutomap(float x, float y, float w, float h, mobj_t *player, int flags)
{
    HUD_PushScissor(x, y, x + w, y + h, (flags & 1) == 0);

    // [ FIXME HACKY ]
    if ((flags & 1) == 0)
    {
        if (x < 1 && x + w > hud_x_mid * 2 - 1)
        {
            x = hud_x_left;
            w = hud_x_right - x;
        }
    }

    AM_Render(x, y, w, h, player);

    HUD_PopScissor();
}

void HUD_GetCastPosition(float *x, float *y, float *scale_x, float *scale_y)
{
    *x = COORD_X(160);
    *y = COORD_Y(170);

    // FIXME REVIEW THIS
    //*scale_y = 4.0;
    *scale_x = *scale_y / v_pixelaspect.f;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
