//----------------------------------------------------------------------------
//  EDGE Lighting Shaders
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "main.h"

#include "image_data.h"

#include "p_mobj.h"
#include "r_defs.h"
#include "r_gldefs.h"
#include "r_image.h"   // W_ImageCache
#include "r_misc.h"
#include "r_shader.h"
#include "r_state.h"
#include "r_texgl.h"
#include "r_units.h"


//----------------------------------------------------------------------------
//  LIGHT IMAGES
//----------------------------------------------------------------------------

#define LIM_CURVE_SIZE  32

class light_image_c
{
public:
	std::string name;

	const image_c *image;

	rgbcol_t curve[LIM_CURVE_SIZE];

public:
	light_image_c(const char * _name, const image_c *_img) : name(_name), image(_img)
	{ }

	~light_image_c()
	{ }

	inline GLuint tex_id() const
	{
		return W_ImageCache(image, false);
	}

	void MakeStdCurve() // TEMP CRUD
	{
		for (int i = 0; i < LIM_CURVE_SIZE-1; i++)
		{
			float d = i / (float)(LIM_CURVE_SIZE - 1);

			float sq = exp(-5.44 * d * d);

			int r = (int)(255 * sq);
			int g = (int)(255 * sq);
			int b = (int)(255 * sq);

			curve[i] = RGB_MAKE(r, g, b);
		}

		curve[LIM_CURVE_SIZE-1] = RGB_MAKE(0, 0, 0);
	}

	rgbcol_t CurvePoint(float d, rgbcol_t tint)
	{
		// d is distance away from centre, between 0.0 and 1.0

		d *= (float)LIM_CURVE_SIZE;

		if (d >= LIM_CURVE_SIZE-1.01)
			return curve[LIM_CURVE_SIZE-1];

		// linearly interpolate between curve points

		int p1 = (int)floor(d);
		int dd = (int)(256 * (d - p1));

		int r1 = RGB_RED(curve[p1]);
		int g1 = RGB_GRN(curve[p1]);
		int b1 = RGB_BLU(curve[p1]);

		int r2 = RGB_RED(curve[p1+1]);
		int g2 = RGB_GRN(curve[p1+1]);
		int b2 = RGB_BLU(curve[p1+1]);

		r1 = (r1 * (256-dd) + r2 * dd) >> 8;
		g1 = (g1 * (256-dd) + g2 * dd) >> 8;
		b1 = (b1 * (256-dd) + b2 * dd) >> 8;

		r1 = r1 * RGB_RED(tint) / 255;
		g1 = g1 * RGB_GRN(tint) / 255;
		b1 = b1 * RGB_BLU(tint) / 255;

		return RGB_MAKE(r1, g1, b1);
	}
};

static light_image_c *GetLightImage(const mobjtype_c *info, int DL)
{
	// Intentional Const Overrides
	dlight_info_c *D_info = (dlight_info_c *) &info->dlight[DL];
	
	if (! D_info->cache_data)
	{
		// FIXME !!!! share light_image_c instances

		const char *shape = D_info->shape.c_str();

		SYS_ASSERT(shape && strlen(shape) > 0);

		const image_c *image = W_ImageLookup(shape, INS_Graphic, ILF_Null);

		if (! image)
			I_Error("Missing dynamic light graphic: %s\n", shape);

		light_image_c *lim = new light_image_c(shape, image);

		if (true) //!!! (DDF_CompareName(shape, "DLIGHT_EXP") == 0)
		{
			lim->MakeStdCurve();
		}
		else
		{
			// FIXME !!!! we need the EPI::BASIC_IMAGE in order to compute the curve
			I_Error("Custom DLIGHT shapes not yet supported.\n");
		}

		D_info->cache_data = lim;
	}

	return (light_image_c *) D_info->cache_data;
}


//----------------------------------------------------------------------------
//  DYNAMIC LIGHTS
//----------------------------------------------------------------------------

class dynlight_shader_c : public abstract_shader_c
{
private:
	mobj_t *mo;

	light_image_c *lim[2];

public:
	dynlight_shader_c(mobj_t *object) : mo(object)
	{
		// Note: these are shared, we must not delete them
		lim[0] = GetLightImage(mo->info, 0);
		lim[1] = GetLightImage(mo->info, 1);
	}

	virtual ~dynlight_shader_c()
	{ /* nothing to do */ }

private:
	inline float TexCoord(vec2_t *texc, float r,
		const vec3_t *lit_pos, const vec3_t *normal)
	{
		float mx = mo->x;
		float my = mo->y;
		float mz = MO_MIDZ(mo);

		MIR_Coordinate(mx, my);
		MIR_Height(mz);

		float dx = lit_pos->x - mx;
		float dy = lit_pos->y - my;
		float dz = lit_pos->z - mz;

		float nx = normal->x;
		float ny = normal->y;
		float nz = normal->z;

		if (fabs(nz) > 50*(fabs(nx)+fabs(ny)))
		{
			/* horizontal plane */
			texc->x = (1 + dx / r) / 2.0;
			texc->y = (1 + dy / r) / 2.0;

			return fabs(dz) / r;
		}
		else
		{
			float n_len = sqrt(nx*nx + ny*ny + nz*nz);

			nx /= n_len;
			ny /= n_len;
			nz /= n_len;

			float dxy = nx * dy - ny * dx;

			r /= sqrt(nx*nx + ny*ny);  // correct ??

			texc->y = (1 + dz  / r) / 2.0;
			texc->x = (1 + dxy / r) / 2.0;

			return fabs(nx*dx + ny*dy + nz*dz) / r;
		}
	}

	inline float WhatRadius(int DL)
	{
		if (DL == 0)
			return mo->dlight.r * MIR_XYScale();

		return mo->info->dlight[1].radius * mo->dlight.r /
			   mo->info->dlight[0].radius * MIR_XYScale();
	}

	inline rgbcol_t WhatColor(int DL)
	{
		return (DL == 0) ? mo->dlight.color : mo->info->dlight[1].colour;
	}

	inline dlight_type_e WhatType(int DL)
	{
		return mo->info->dlight[DL].type;
	}

public:

	virtual void Sample(multi_color_c *col, float x, float y, float z)
	{
		float mx = mo->x;
		float my = mo->y;
		float mz = MO_MIDZ(mo);

		MIR_Coordinate(mx, my);
		MIR_Height(mz);

		float dx = x - mx;
		float dy = y - my;
		float dz = z - mz;

		float dist = sqrt(dx*dx + dy*dy + dz*dz);

		for (int DL = 0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			rgbcol_t new_col = lim[DL]->CurvePoint(dist / WhatRadius(DL),
					WhatColor(DL));

			float L = mo->state->bright / 255.0;

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void Corner(multi_color_c *col, float nx, float ny, float nz,
			            struct mobj_s *mod_pos, bool is_weapon)
	{
		float mx = mo->x;
		float my = mo->y;
		float mz = MO_MIDZ(mo);

		if (is_weapon)
		{
			mx += viewcos * 24;
			my += viewsin * 24;
		}

		MIR_Coordinate(mx, my);
		MIR_Height(mz);

		float dx = mod_pos->x;
		float dy = mod_pos->y;
		float dz = MO_MIDZ(mod_pos);

		MIR_Coordinate(dx, dy);
		MIR_Height(dz);

		dx -= mx; dy -= my; dz -= mz;

		float dist = sqrt(dx*dx + dy*dy + dz*dz);

		dx /= dist;
		dy /= dist;
		dz /= dist;

		dist = MAX(1.0, dist - mod_pos->radius * MIR_XYScale());

		float L = 0.6 - 0.7 * (dx*nx + dy*ny + dz*nz);

		L *= mo->state->bright / 255.0;

		for (int DL = 0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			rgbcol_t new_col = lim[DL]->CurvePoint(dist / WhatRadius(DL),
					WhatColor(DL));

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void WorldMix(GLuint shape, int num_vert,
		GLuint tex, float alpha, int *pass_var, int blending,
		bool masked, void *data, shader_coord_func_t func)
	{
		for (int DL = 0; DL < 2; DL++)
		{
			if (detail_level == 0 && DL > 0)
				continue;

			if (WhatType(DL) == DLITE_None)
				break;

			bool is_additive = (WhatType(DL) == DLITE_Add);

			rgbcol_t col = WhatColor(DL);

			float L = mo->state->bright / 255.0;

			float R = L * RGB_RED(col) / 255.0;
			float G = L * RGB_GRN(col) / 255.0;
			float B = L * RGB_BLU(col) / 255.0;


			local_gl_vert_t *glvert = RGL_BeginUnit(shape, num_vert,
						(is_additive && masked) ? ENV_SKIP_RGB :
						 is_additive ? ENV_NONE : GL_MODULATE,
						(is_additive && !masked) ? 0 : tex,
						GL_MODULATE, lim[DL]->tex_id(),
						*pass_var, blending);

			for (int v_idx=0; v_idx < num_vert; v_idx++)
			{
				local_gl_vert_t *dest = glvert + v_idx;

				vec3_t lit_pos;

				(*func)(data, v_idx, &dest->pos, dest->rgba,
						&dest->texc[0], &dest->normal, &lit_pos);

				float dist = TexCoord(&dest->texc[1], WhatRadius(DL),
						 &lit_pos, &dest->normal);

				float ity = exp(-5.44 * dist * dist);

				dest->rgba[0] = R * ity;
				dest->rgba[1] = G * ity;
				dest->rgba[2] = B * ity;
				dest->rgba[3] = alpha;
			}

			RGL_EndUnit(num_vert);

			(*pass_var) += 1;
		}
	}

};

abstract_shader_c *MakeDLightShader(mobj_t *mo)
{
	return new dynlight_shader_c(mo);
}



//----------------------------------------------------------------------------
//  SECTOR GLOWS
//----------------------------------------------------------------------------

class plane_glow_c : public abstract_shader_c
{
private:
	mobj_t *mo;

	light_image_c *lim[2];

public:
	plane_glow_c(mobj_t *_glower) : mo(_glower)
	{
		lim[0] = GetLightImage(mo->info, 0);
		lim[1] = GetLightImage(mo->info, 1);
	}

	virtual ~plane_glow_c()
	{ /* nothing to do */ }

private:
	inline float Dist(const sector_t *sec, float z)
	{
		if (mo->info->glow_type == GLOW_Floor)
			return fabs(sec->f_h - z);
		else
			return fabs(sec->c_h - z);  // GLOW_Ceiling
	}

	inline void TexCoord(vec2_t *texc, float r,
		const sector_t *sec, const vec3_t *lit_pos, const vec3_t *normal)
	{
		texc->x = 0.5;
		texc->y = 0.5 + Dist(sec, lit_pos->z) / r / 2.0;
	}

	inline float WhatRadius(int DL)
	{
		if (DL == 0)
			return mo->dlight.r * MIR_XYScale();

		return mo->info->dlight[1].radius * mo->dlight.r /
			   mo->info->dlight[0].radius * MIR_XYScale();
	}

	inline rgbcol_t WhatColor(int DL)
	{
		return (DL == 0) ? mo->dlight.color : mo->info->dlight[1].colour;
	}

	inline dlight_type_e WhatType(int DL)
	{
		return mo->info->dlight[DL].type;
	}

public:

	virtual void Sample(multi_color_c *col, float x, float y, float z)
	{
		const sector_t *sec = mo->subsector->sector;

		float dist = Dist(sec, z);

		for (int DL = 0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			rgbcol_t new_col = lim[DL]->CurvePoint(dist / WhatRadius(DL),
					WhatColor(DL));

			float L = mo->state->bright / 255.0;

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void Corner(multi_color_c *col, float nx, float ny, float nz,
			            struct mobj_s *mod_pos, bool is_weapon)
	{
		const sector_t *sec = mo->subsector->sector;

		float dz = (mo->info->glow_type == GLOW_Floor) ? +1 : -1;
		float dist;

		if (is_weapon)
		{
			float weapon_z = mod_pos->z + mod_pos->height *
				PERCENT_2_FLOAT(mod_pos->info->shotheight);

			if (mo->info->glow_type == GLOW_Floor)
				dist = weapon_z - sec->f_h;
			else
				dist = sec->c_h - weapon_z;
		}
		else if (mo->info->glow_type == GLOW_Floor)
			dist = mod_pos->z - sec->f_h;
		else
			dist = sec->c_h - (mod_pos->z + mod_pos->height);
		
		dist = MAX(1.0, fabs(dist));


		float L = 0.6 - 0.7 * (dz*nz);

		L *= mo->state->bright / 255.0;

		for (int DL = 0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			rgbcol_t new_col = lim[DL]->CurvePoint(dist / WhatRadius(DL),
					WhatColor(DL));

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void WorldMix(GLuint shape, int num_vert,
		GLuint tex, float alpha, int *pass_var, int blending,
		bool masked, void *data, shader_coord_func_t func)
	{
		const sector_t *sec = mo->subsector->sector;

		for (int DL = 0; DL < 2; DL++)
		{
			if (detail_level == 0 && DL > 0)
				continue;

			if (WhatType(DL) == DLITE_None)
				break;

			bool is_additive = (WhatType(DL) == DLITE_Add);

			rgbcol_t col = WhatColor(DL);

			float L = mo->state->bright / 255.0;

			float R = L * RGB_RED(col) / 255.0;
			float G = L * RGB_GRN(col) / 255.0;
			float B = L * RGB_BLU(col) / 255.0;


			local_gl_vert_t *glvert = RGL_BeginUnit(shape, num_vert,
						(is_additive && masked) ? ENV_SKIP_RGB :
						 is_additive ? ENV_NONE : GL_MODULATE,
						(is_additive && !masked) ? 0 : tex,
						GL_MODULATE, lim[DL]->tex_id(),
						*pass_var, blending);
			
			for (int v_idx=0; v_idx < num_vert; v_idx++)
			{
				local_gl_vert_t *dest = glvert + v_idx;

				vec3_t lit_pos;

				(*func)(data, v_idx, &dest->pos, dest->rgba,
						&dest->texc[0], &dest->normal, &lit_pos);

				TexCoord(&dest->texc[1], WhatRadius(DL), sec,
						 &lit_pos, &dest->normal);

				dest->rgba[0] = R;
				dest->rgba[1] = G;
				dest->rgba[2] = B;
				dest->rgba[3] = alpha;
			}

			RGL_EndUnit(num_vert);

			(*pass_var) += 1;
		}
	}
};

abstract_shader_c *MakePlaneGlow(mobj_t *mo)
{
	return new plane_glow_c(mo);
}



//----------------------------------------------------------------------------
//  WALL GLOWS
//----------------------------------------------------------------------------

class wall_glow_c : public abstract_shader_c
{
private:
	line_t *ld;
	mobj_t *mo;

	float norm_x, norm_y; // normal

	light_image_c *lim[2];

	inline float Dist(float x, float y)
	{
		return (ld->v1->x - x) * norm_x + (ld->v1->y - y) * norm_y;
	}

	inline void TexCoord(vec2_t *texc, float r,
		const sector_t *sec, const vec3_t *lit_pos, const vec3_t *normal)
	{
		texc->x = 0.5;
		texc->y = 0.5 + Dist(lit_pos->x, lit_pos->y) / r / 2.0;
	}

	inline float WhatRadius(int DL)
	{
		if (DL == 0)
			return mo->dlight.r * MIR_XYScale();

		return mo->info->dlight[1].radius * mo->dlight.r /
			   mo->info->dlight[0].radius * MIR_XYScale();
	}

	inline rgbcol_t WhatColor(int DL)
	{
		return (DL == 0) ? mo->dlight.color : mo->info->dlight[1].colour;
	}

	inline dlight_type_e WhatType(int DL)
	{
		return mo->info->dlight[DL].type;
	}

public:
	wall_glow_c(line_t *_wall, mobj_t *_glower) :
		ld(_wall), mo(_glower)
	{
		norm_x = (ld->v1->y - ld->v2->y) / ld->length;
		norm_y = (ld->v2->x - ld->v1->x) / ld->length;
		// Note: these are shared, we must not delete them
		lim[0] = GetLightImage(mo->info, 0);
		lim[1] = GetLightImage(mo->info, 1);
	}

	virtual ~wall_glow_c()
	{ /* nothing to do */ }


	virtual void Sample(multi_color_c *col, float x, float y, float z)
	{
		float dist = Dist(x, y);

		for (int DL = 0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			rgbcol_t new_col = lim[DL]->CurvePoint(dist / WhatRadius(DL),
					WhatColor(DL));

			float L = exp(-5.44 * dist * dist);

			L = L * mo->state->bright / 255.0;

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void Corner(multi_color_c *col, float nx, float ny, float nz,
			            struct mobj_s *mod_pos, bool is_weapon = false)
	{
		/* TODO */
	}

	virtual void WorldMix(GLuint shape, int num_vert,
		GLuint tex, float alpha, int *pass_var, int blending,
		bool masked, void *data, shader_coord_func_t func)
	{
		const sector_t *sec = mo->subsector->sector;

		for (int DL = 0; DL < 2; DL++)
		{
			if (detail_level == 0 && DL > 0)
				continue;

			if (WhatType(DL) == DLITE_None)
				break;

			bool is_additive = (WhatType(DL) == DLITE_Add);

			rgbcol_t col = WhatColor(DL);

			float L = mo->state->bright / 255.0;

			float R = L * RGB_RED(col) / 255.0;
			float G = L * RGB_GRN(col) / 255.0;
			float B = L * RGB_BLU(col) / 255.0;


			local_gl_vert_t *glvert = RGL_BeginUnit(shape, num_vert,
						(is_additive && masked) ? ENV_SKIP_RGB :
						 is_additive ? ENV_NONE : GL_MODULATE,
						(is_additive && !masked) ? 0 : tex,
						GL_MODULATE, lim[DL]->tex_id(),
						*pass_var, blending);
			
			for (int v_idx=0; v_idx < num_vert; v_idx++)
			{
				local_gl_vert_t *dest = glvert + v_idx;

				vec3_t lit_pos;

				(*func)(data, v_idx, &dest->pos, dest->rgba,
						&dest->texc[0], &dest->normal, &lit_pos);

				TexCoord(&dest->texc[1], WhatRadius(DL), sec,
						 &lit_pos, &dest->normal);

				dest->rgba[0] = R;
				dest->rgba[1] = G;
				dest->rgba[2] = B;
				dest->rgba[3] = alpha;
			}

			RGL_EndUnit(num_vert);

			(*pass_var) += 1;
		}
	}
};

abstract_shader_c *MakeWallGlow(line_t *wall, mobj_t *mo)
{
	return new wall_glow_c(wall, mo);
}


//----------------------------------------------------------------------------
//  LASER GLOWS
//----------------------------------------------------------------------------

#if 0  // POSSIBLE FUTURE FEATURE

class laser_glow_c : public abstract_shader_c
{
private:
	vec3_t s, e;

	float length;
	vec3_t normal;

	const mobjtype_c *info;
	float bright;

	light_image_c *lim[2];

public:
	laser_glow_c(const vec3_t& _v1, const vec3_t& _v2,
				 const mobjtype_c *_info, float _intensity) :
		s(_v1), e(_v2), info(_info), bright(_intensity)
	{
		normal.x = e.x - s.x;
		normal.y = e.y - s.y;
		normal.z = e.z - s.z;

		length = sqrt(normal.x * normal.x + normal.y * normal.y +
				      normal.z * normal.z);

		if (length < 0.1)
			length = 0.1;

		normal.x /= length;
		normal.y /= length;
		normal.z /= length;

		lim[0] = GetLightImage(info, 0);
		lim[1] = GetLightImage(info, 1);
	}

	virtual ~laser_glow_c()
	{ /* nothing to do */ }

private:
	inline float WhatRadius(int DL)
	{
		return info->dlight[DL].radius * MIR_XYScale();
	}

	inline rgbcol_t WhatColor(int DL)
	{
		return info->dlight[DL].colour;
	}

	inline dlight_type_e WhatType(int DL)
	{
		return info->dlight[DL].type;
	}

public:

	virtual void Sample(multi_color_c *col, float x, float y, float z)
	{
		x -= s.x;
		y -= s.y;
		z -= s.z;

		/* get perpendicular and along distances */
		
		// dot product
		float along = x*normal.x + y*normal.y + z*normal.z;

		// cross product
		float cx = y * normal.z - normal.y * z;
		float cy = z * normal.x - normal.z * x;
		float cz = x * normal.y - normal.x * y;

		float dist = (cx*cx + cy*cy + cz*cz);

		for (int DL=0; DL < 2; DL++)
		{
			if (WhatType(DL) == DLITE_None)
				break;

			float d = dist;

			if (along < 0)
				d -= along;
			else if (along > length)
				d += (along - length);

			rgbcol_t new_col = lim[DL]->CurvePoint(d / WhatRadius(DL),
					WhatColor(DL));

			float L = bright / 255.0;

			if (new_col != RGB_MAKE(0,0,0) && L > 1/256.0)
			{
				if (WhatType(DL) == DLITE_Add)
					col->add_Give(new_col, L); 
				else
					col->mod_Give(new_col, L); 
			}
		}
	}

	virtual void WorldMix(GLuint shape, int num_vert,
		GLuint tex, float alpha, int *pass_var, int blending,
		void *data, shader_coord_func_t func)
	{
		/* TODO */
	}
};

#endif


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
