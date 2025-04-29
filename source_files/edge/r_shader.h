//----------------------------------------------------------------------------
//  EDGE Lighting Shaders
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

#pragma once

#include "ddf_types.h"
#include "i_defs_gl.h"
#include "p_mobj.h"
#include "r_units.h"

class ColorMixer
{
  public:
    int modulate_red_, modulate_green_, modulate_blue_;
    int add_red_, add_green_, add_blue_;

  public:
    ColorMixer()
    {
    }
    ~ColorMixer()
    {
    }

    void Clear()
    {
        modulate_red_ = modulate_green_ = modulate_blue_ = 0;
        add_red_ = add_green_ = add_blue_ = 0;
    }

    void operator+=(const ColorMixer &rhs)
    {
        modulate_red_ += rhs.modulate_red_;
        modulate_green_ += rhs.modulate_green_;
        modulate_blue_ += rhs.modulate_blue_;

        add_red_ += rhs.add_red_;
        add_green_ += rhs.add_green_;
        add_blue_ += rhs.add_blue_;
    }

    int mod_MAX() const
    {
        return HMM_MAX(modulate_red_, HMM_MAX(modulate_green_, modulate_blue_));
    }

    int add_MAX() const
    {
        return HMM_MAX(add_red_, HMM_MAX(add_green_, add_blue_));
    }

    void modulate_GIVE(RGBAColor rgb, float qty)
    {
        if (qty > 1.0f)
            qty = 1.0f;

        modulate_red_ += (int)(epi::GetRGBARed(rgb) * qty);
        modulate_green_ += (int)(epi::GetRGBAGreen(rgb) * qty);
        modulate_blue_ += (int)(epi::GetRGBABlue(rgb) * qty);
    }

    void add_GIVE(RGBAColor rgb, float qty)
    {
        if (qty > 1.0f)
            qty = 1.0f;

        add_red_ += (int)(epi::GetRGBARed(rgb) * qty);
        add_green_ += (int)(epi::GetRGBAGreen(rgb) * qty);
        add_blue_ += (int)(epi::GetRGBABlue(rgb) * qty);
    }
};

typedef void (*ShaderCoordinateFunction)(void *data, int v_idx, HMM_Vec3 *pos, RGBAColor *rgb, HMM_Vec2 *texc,
                                         HMM_Vec3 *normal, HMM_Vec3 *lit_pos);

/* abstract base class */
class AbstractShader
{
  public:
    AbstractShader()
    {
    }
    virtual ~AbstractShader()
    {
    }

    // used for arbitrary points in the world (sprites)
    virtual void Sample(ColorMixer *col, float x, float y, float z) = 0;

    // used for normal-based lighting (MD2 models)
    virtual void Corner(ColorMixer *col, float nx, float ny, float nz, MapObject *mod_pos, bool is_weapon = false) = 0;

    // used to render overlay textures (world polygons)
    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha, int *pass_var, BlendingMode blending,
                          bool masked, void *data, ShaderCoordinateFunction func) = 0;
};

// Delete all dynamic light "images"; cannot be done in the various shader
// destructors as these images are shared amongst multiple instances - Dasho
void DeleteAllLightImages();

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
