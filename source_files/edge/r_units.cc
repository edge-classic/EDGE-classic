//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Unit batching)
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
//
// -AJA- 2000/10/09: Began work on this new unit system.
//

#include "i_defs.h"
#include "i_defs_gl.h"

#include <vector>
#include <algorithm>

#include "image_data.h"

#include "m_argv.h"
#include "r_gldefs.h"
#include "r_units.h"
#include "z_zone.h"

#include "r_misc.h"
#include "r_image.h"
#include "r_texgl.h"
#include "r_shader.h"

/*std::vector<GLfloat> unit_vertices;
std::vector<GLfloat> unit_normals;
std::vector<GLboolean> unit_edgeflags;
std::vector<GLfloat> unit_texcoords0;
std::vector<GLfloat> unit_texcoords1;*/

cvar_c r_colorlighting;
cvar_c r_colormaterial;

cvar_c r_dumbsky;
cvar_c r_dumbmulti;
cvar_c r_dumbcombine;
cvar_c r_dumbclamp;


#define MAX_L_VERT  4096
#define MAX_L_UNIT  (MAX_L_VERT / 4)

#define DUMMY_CLAMP  789


// a single unit (polygon, quad, etc) to pass to the GL
typedef struct local_gl_unit_s
{
	// unit mode (e.g. GL_TRIANGLE_FAN)
	GLuint shape;

	// environment modes (GL_REPLACE, GL_MODULATE, GL_DECAL, GL_ADD)
	GLuint env[2];

	// texture(s) used
	GLuint tex[2];

	// pass number (multiple pass rendering)
	int pass;

	// blending flags
	int blending;

	// range of local vertices
	int first, count;
}
local_gl_unit_t;


static local_gl_vert_t local_verts[MAX_L_VERT];
static local_gl_unit_t local_units[MAX_L_UNIT];

static std::vector<local_gl_unit_t *> local_unit_map;

static int cur_vert;
static int cur_unit;

static bool batch_sort;


//
// RGL_InitUnits
//
// Initialise the unit system.  Once-only call.
//
void RGL_InitUnits(void)
{
	// Run the soft init code
	RGL_SoftInitUnits();
}

//
// RGL_SoftInitUnits
//
// -ACB- 2004/02/15 Quickly-hacked routine to reinit stuff lost on res change
//
void RGL_SoftInitUnits()
{
}


//
// RGL_StartUnits
//
// Starts a fresh batch of units.
//
// When 'sort_em' is true, the units will be sorted to keep
// texture changes to a minimum.  Otherwise, the batch is
// drawn in the same order as given.
//
void RGL_StartUnits(bool sort_em)
{
	cur_vert = cur_unit = 0;

	batch_sort = sort_em;

	local_unit_map.resize(MAX_L_UNIT);
}

//
// RGL_FinishUnits
//
// Finishes a batch of units, drawing any that haven't been drawn yet.
//
void RGL_FinishUnits(void)
{
	RGL_DrawUnits();
}


static inline void myActiveTexture(GLuint id)
{
	if (GL_VERSION_1_3)
		glActiveTexture(id);
	else /* GLEW_ARB_multitexture */
		glActiveTextureARB(id);
}

static inline void myMultiTexCoord2f(GLuint id, GLfloat s, GLfloat t)
{
	if (GL_VERSION_1_3)
		glMultiTexCoord2f(id, s, t);
	else /* GLEW_ARB_multitexture */
		glMultiTexCoord2fARB(id, s, t);
}

//
// RGL_BeginUnit
//
// Begin a new unit, with the given parameters (mode and texture ID).
// `max_vert' is the maximum expected vertices of the quad/poly (the
// actual number can be less, but never more).  Returns a pointer to
// the first vertex structure.  `masked' should be true if the texture
// contains "holes" (like sprites).  `blended' should be true if the
// texture should be blended (like for translucent water or sprites).
//
local_gl_vert_t *RGL_BeginUnit(GLuint shape, int max_vert, 
		                       GLuint env1, GLuint tex1,
							   GLuint env2, GLuint tex2,
							   int pass, int blending)
{
	local_gl_unit_t *unit;

	SYS_ASSERT(max_vert > 0);
	SYS_ASSERT(pass >= 0);

	SYS_ASSERT((blending & BL_CULL_BOTH) != BL_CULL_BOTH);

	// check we have enough space left
	if (cur_vert + max_vert > MAX_L_VERT || cur_unit >= MAX_L_UNIT)
	{
		RGL_DrawUnits();
	}

	unit = local_units + cur_unit;

	if (env1 == ENV_NONE) tex1 = 0;
	if (env2 == ENV_NONE) tex2 = 0;

	unit->shape  = shape;
	unit->env[0] = env1;
	unit->env[1] = env2;
	unit->tex[0] = tex1;
	unit->tex[1] = tex2;

	unit->pass     = pass;
	unit->blending = blending;
	unit->first    = cur_vert;  // count set later

	return local_verts + cur_vert;
}

//
// RGL_EndUnit
//
void RGL_EndUnit(int actual_vert)
{
	local_gl_unit_t *unit;

	SYS_ASSERT(actual_vert > 0);

	unit = local_units + cur_unit;

	unit->count = actual_vert;

	// adjust colors (for special effects)
	for (int i = 0; i < actual_vert; i++)
	{
		local_gl_vert_t *v = &local_verts[cur_vert + i];

		v->rgba[0] *= ren_red_mul;
		v->rgba[1] *= ren_grn_mul;
		v->rgba[2] *= ren_blu_mul;
	}

	cur_vert += actual_vert;
	cur_unit++;

	SYS_ASSERT(cur_vert <= MAX_L_VERT);
	SYS_ASSERT(cur_unit <= MAX_L_UNIT);
}


struct Compare_Unit_pred
{
	inline bool operator() (const local_gl_unit_t *A, const local_gl_unit_t *B) const
	{
		if (A->pass != B->pass)
			return A->pass < B->pass;

		if (A->tex[0] != B->tex[0])
			return A->tex[0] < B->tex[0];

		if (A->tex[1] != B->tex[1])
			return A->tex[1] < B->tex[1];

		if (A->env[0] != B->env[0])
			return A->env[0] < B->env[0];

		if (A->env[1] != B->env[1])
			return A->env[1] < B->env[1];

		return A->blending < B->blending;
	}
};

static void EnableCustomEnv(GLuint env, bool enable)
{
	switch (env)
	{
		case ENV_SKIP_RGB:
			if (enable)
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
			}
			else
			{
				/* no need to modify TEXTURE_ENV_MODE */
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
			}
			break;

		default:
			I_Error("INTERNAL ERROR: no such custom env: %08x\n", env);
	}
}

static inline void RGL_SendRawVector(const local_gl_vert_t *V)
{
	if (r_colormaterial.d || ! r_colorlighting.d)
		glColor4fv(V->rgba);
	else
	{
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, V->rgba);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, V->rgba);
	}

	myMultiTexCoord2f(GL_TEXTURE0, V->texc[0].x, V->texc[0].y);
	myMultiTexCoord2f(GL_TEXTURE1, V->texc[1].x, V->texc[1].y);

	glNormal3f(V->normal.x, V->normal.y, V->normal.z);
	glEdgeFlag(V->edge);

	// vertex must be last
	glVertex3f(V->pos.x, V->pos.y, V->pos.z);

	/*unit_texcoords0.push_back(V->texc[0].x);
	unit_texcoords0.push_back(V->texc[0].y);
	glClientActiveTexture(GL_TEXTURE0);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, unit_texcoords0.data());
	unit_texcoords1.push_back(V->texc[1].x);
	unit_texcoords1.push_back(V->texc[1].y);
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, unit_texcoords1.data());

	unit_normals.push_back(V->normal.x);
	unit_normals.push_back(V->normal.y);
	unit_normals.push_back(V->normal.z);

	unit_edgeflags.push_back(V->edge);

	unit_vertices.push_back(V->pos.x);
	unit_vertices.push_back(V->pos.y);
	unit_vertices.push_back(V->pos.z);*/
}

//
// RGL_DrawUnits
//
// Forces the set of current units to be drawn.  This call is
// optional (it never _needs_ to be called by client code).
//
void RGL_DrawUnits(void)
{
	if (cur_unit == 0)
		return;

	GLuint active_tex[2] = { 0, 0 };
	GLuint active_env[2] = { 0, 0 };

	int active_pass = 0;
	int active_blending = 0;

	for (int i=0; i < cur_unit; i++)
		local_unit_map[i] = & local_units[i];

	if (batch_sort)
	{
		std::sort(local_unit_map.begin(),
				  local_unit_map.begin() + cur_unit,
				  Compare_Unit_pred());
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	glAlphaFunc(GL_GREATER, 0);

	glPolygonOffset(0, 0);


	for (int j=0; j < cur_unit; j++)
	{
		local_gl_unit_t *unit = local_unit_map[j];

		SYS_ASSERT(unit->count > 0);

		// detect changes in texture/alpha/blending state

		if (active_pass != unit->pass)
		{
			active_pass = unit->pass;

			glPolygonOffset(0, -active_pass);
		}

		if ((active_blending ^ unit->blending) & (BL_Masked | BL_Less))
		{
			if (unit->blending & BL_Less)
			{
				// glAlphaFunc is updated below, because the alpha
				// value can change from unit to unit while the
				// BL_Less flag remains set.
				glEnable(GL_ALPHA_TEST);
			}
			else if (unit->blending & BL_Masked)
			{
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_GREATER, 0);
			}
			else
				glDisable(GL_ALPHA_TEST);
		}

		if ((active_blending ^ unit->blending) & (BL_Alpha | BL_Add))
		{
			if (unit->blending & BL_Add)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			}
			else if (unit->blending & BL_Alpha)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else
				glDisable(GL_BLEND);
		}

		if ((active_blending ^ unit->blending) & BL_CULL_BOTH)
		{
			if (unit->blending & BL_CULL_BOTH)
			{
				glEnable(GL_CULL_FACE);
				glCullFace((unit->blending & BL_CullFront) ? GL_FRONT : GL_BACK);
			}
			else
				glDisable(GL_CULL_FACE);
		}

		if ((active_blending ^ unit->blending) & BL_NoZBuf)
		{
			glDepthMask((unit->blending & BL_NoZBuf) ? GL_FALSE : GL_TRUE);
		}

		active_blending = unit->blending;

		if (active_blending & BL_Less)
		{
			// NOTE: assumes alpha is constant over whole polygon
			float a = local_verts[unit->first].rgba[3];

			glAlphaFunc(GL_GREATER, a * 0.66f);
		}

		for (int t=1; t >= 0; t--)
		{
			myActiveTexture(GL_TEXTURE0 + t);

			if (active_tex[t] != unit->tex[t])
			{
				if (unit->tex[t] == 0)
					glDisable(GL_TEXTURE_2D);
				else if (active_tex[t] == 0)
					glEnable(GL_TEXTURE_2D);

				if (unit->tex[t] != 0)
					glBindTexture(GL_TEXTURE_2D, unit->tex[t]);

				active_tex[t] = unit->tex[t];
			}

			if (active_env[t] != unit->env[t])
			{
				if (active_env[t] >= CUSTOM_ENV_BEGIN &&
					active_env[t] <= CUSTOM_ENV_END)
				{
					EnableCustomEnv(active_env[t], false);
				}

				if (unit->env[t] >= CUSTOM_ENV_BEGIN &&
					unit->env[t] <= CUSTOM_ENV_END)
				{
					EnableCustomEnv(unit->env[t], true);
				}
				else if (unit->env[t] != ENV_NONE)
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->env[t]);

				active_env[t] = unit->env[t];
			}
		}

		GLint old_clamp = DUMMY_CLAMP;

		if ((active_blending & BL_ClampY) && active_tex[0] != 0)
		{
			glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_clamp);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
				r_dumbclamp.d ? GL_CLAMP : GL_CLAMP_TO_EDGE);
		}
		glBegin(unit->shape);

		//glEnableClientState(GL_VERTEX_ARRAY);
		//glEnableClientState(GL_NORMAL_ARRAY);
		//glEnableClientState(GL_EDGE_FLAG_ARRAY);

		for (int v_idx=0; v_idx < unit->count; v_idx++)
		{
			RGL_SendRawVector(local_verts + unit->first + v_idx);
		}

		glEnd();
		/*glEdgeFlagPointer(0, unit_edgeflags.data());
		glNormalPointer(GL_FLOAT, 0, unit_normals.data());
		glVertexPointer(3, GL_FLOAT, 0, unit_vertices.data());
		glDrawArrays(unit->shape, 0, unit->shape < GL_TRIANGLE_FAN ? unit->count : (unit_vertices.size() / 3));

		glClientActiveTexture(GL_TEXTURE0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE1);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_EDGE_FLAG_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glClientActiveTexture(GL_TEXTURE0);

		unit_edgeflags.clear();
		unit_normals.clear();
		unit_vertices.clear();
		unit_texcoords0.clear();
		unit_texcoords1.clear();*/
		
		// restore the clamping mode
		if (old_clamp != DUMMY_CLAMP)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_clamp);
	}

	// all done
	cur_vert = cur_unit = 0;

	glPolygonOffset(0, 0);

	for (int t=1; t >=0; t--)
	{
		myActiveTexture(GL_TEXTURE0 + t);

		if (active_env[t] >= CUSTOM_ENV_BEGIN &&
			active_env[t] <= CUSTOM_ENV_END)
		{
			EnableCustomEnv(active_env[t], false);
		}
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
	}

	glDepthMask(GL_TRUE);
	glCullFace(GL_BACK);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glAlphaFunc(GL_GREATER, 0);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
