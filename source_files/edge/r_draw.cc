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

#include "epi.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_units.h"

void NewScreenSize()
{

    render_backend->SetRenderLayer(kRenderLayerHUD);

    // prevent a visible border with certain cards/drivers
    render_state->ClearColor(kRGBATransparent);
    render_state->Clear(GL_COLOR_BUFFER_BIT);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
