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


#include "i_defs_gl.h"

#include "g_game.h"
#include "r_misc.h"
#include "r_gldefs.h"
#include "r_units.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"

#include <vector>

void RGL_NewScreenSize(int width, int height, int bits)
{
    //!!! quick hack
    RGL_SetupMatrices2D();

    // prevent a visible border with certain cards/drivers
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RGL_DrawImage(float x, float y, float w, float h, const image_c *image, float tx1, float ty1, float tx2, float ty2,
                   const Colormap *textmap, float alpha, const Colormap *palremap)
{
    int x1 = RoundToInteger(x);
    int y1 = RoundToInteger(y);
    int x2 = RoundToInteger(x + w + 0.25f);
    int y2 = RoundToInteger(y + h + 0.25f);

    if (x1 == x2 || y1 == y2)
        return;

    sg_color sgcol = sg_white;

    GLuint tex_id = W_ImageCache(image, true, (textmap && (textmap->special_ & kColorSpecialWhiten)) ? nullptr : palremap,
                                 (textmap && (textmap->special_ & kColorSpecialWhiten)) ? true : false);

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

    if (textmap)
        sgcol = sg_make_color_1i(V_GetFontColor(textmap));

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

void RGL_ReadScreen(int x, int y, int w, int h, uint8_t *rgb_buffer)
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
