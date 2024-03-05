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

#include "r_draw.h"

#include <vector>

#include "g_game.h"
#include "i_defs_gl.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_units.h"
#include "sokol_color.h"

void RendererNewScreenSize(int width, int height, int bits)
{
    //!!! quick hack
    RendererSetupMatrices2D();

    // prevent a visible border with certain cards/drivers
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RendererDrawImage(float x, float y, float w, float h, const Image *image,
                       float tx1, float ty1, float tx2, float ty2,
                       const Colormap *textmap, float alpha,
                       const Colormap *palremap)
{
    int x1 = RoundToInteger(x);
    int y1 = RoundToInteger(y);
    int x2 = RoundToInteger(x + w + 0.25f);
    int y2 = RoundToInteger(y + h + 0.25f);

    if (x1 == x2 || y1 == y2) return;

    sg_color sgcol = sg_white;

    GLuint tex_id = ImageCache(
        image, true,
        (textmap && (textmap->special_ & kColorSpecialWhiten)) ? nullptr
                                                               : palremap,
        (textmap && (textmap->special_ & kColorSpecialWhiten)) ? true : false);

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

    if (textmap) sgcol = sg_make_color_1i(GetFontColor(textmap));

    glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);

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

void RendererReadScreen(int x, int y, int w, int h, uint8_t *rgb_buffer)
{
    glFlush();

    glPixelZoom(1.0f, 1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (; h > 0; h--, y++)
    {
        glReadPixels(x, y, w, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb_buffer);

        rgb_buffer += w * 3;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
