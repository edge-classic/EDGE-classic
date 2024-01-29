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

#ifndef __R_SHADER_H__
#define __R_SHADER_H__

#include "types.h"

/// #include "r_units.h"

class multi_color_c
{
  public:
    int mod_R, mod_G, mod_B;
    int add_R, add_G, add_B;

  public:
    multi_color_c()
    {
    }
    ~multi_color_c()
    {
    }

    void Clear()
    {
        mod_R = mod_G = mod_B = 0;
        add_R = add_G = add_B = 0;
    }

    void operator+=(const multi_color_c &rhs)
    {
        mod_R += rhs.mod_R;
        mod_G += rhs.mod_G;
        mod_B += rhs.mod_B;

        add_R += rhs.add_R;
        add_G += rhs.add_G;
        add_B += rhs.add_B;
    }

    int mod_MAX() const
    {
        return MAX(mod_R, MAX(mod_G, mod_B));
    }

    int add_MAX() const
    {
        return MAX(add_R, MAX(add_G, add_B));
    }

    void mod_Give(RGBAColor rgb, float qty)
    {
        if (qty > 1.0f)
            qty = 1.0f;

        mod_R += (int)(epi::GetRGBARed(rgb) * qty);
        mod_G += (int)(epi::GetRGBAGreen(rgb) * qty);
        mod_B += (int)(epi::GetRGBABlue(rgb) * qty);
    }

    void add_Give(RGBAColor rgb, float qty)
    {
        if (qty > 1.0f)
            qty = 1.0f;

        add_R += (int)(epi::GetRGBARed(rgb) * qty);
        add_G += (int)(epi::GetRGBAGreen(rgb) * qty);
        add_B += (int)(epi::GetRGBABlue(rgb) * qty);
    }
};

typedef void (*shader_coord_func_t)(void *data, int v_idx, HMM_Vec3 *pos, float *rgb, HMM_Vec2 *texc, HMM_Vec3 *normal,
                                    HMM_Vec3 *lit_pos);

/* abstract base class */
class abstract_shader_c
{
  public:
    abstract_shader_c()
    {
    }
    virtual ~abstract_shader_c()
    {
    }

    // used for arbitrary points in the world (sprites)
    virtual void Sample(multi_color_c *col, float x, float y, float z) = 0;

    // used for normal-based lighting (MD2 models)
    virtual void Corner(multi_color_c *col, float nx, float ny, float nz, struct mobj_s *mod_pos,
                        bool is_weapon = false) = 0;

    // used to render overlay textures (world polygons)
    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha, int *pass_var, int blending, bool masked,
                          void *data, shader_coord_func_t func) = 0;
};

/* FUNCTIONS */

#endif /* __R_SHADER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
