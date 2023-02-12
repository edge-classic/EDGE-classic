//----------------------------------------------------------------------------
//  MDL Models
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023  The EDGE Team.
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
//  Based on "qfiles.h" and "anorms.h" from the GPL'd quake 2 source
//  release.  Copyright (C) 1997-2001 Id Software, Inc.
//
//  Based on MDL loading and rendering code (C) 2004 David Henry.
//
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "i_defs_gl.h"

#include "types.h"
#include "endianess.h"
#include "image_data.h"

#include "r_mdcommon.h"
#include "r_mdl.h"
#include "r_gldefs.h"
#include "r_colormap.h"
#include "r_effects.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_state.h"
#include "r_shader.h"
#include "r_units.h"
#include "p_blockmap.h"
#include "r_texgl.h"

#include <vector>

extern float P_ApproxDistance(float dx, float dy, float dz);


/*============== MDL FORMAT DEFINITIONS ====================*/


// format uses float pointing values, but to allow for endianness
// conversions they are represented here as unsigned integers.
typedef u32_t f32_t;

#define MDL_IDENTIFIER  "IDPO"
#define MDL_VERSION     6

typedef struct
{
	char ident[4];

	s32_t version;

	f32_t scale_x;
	f32_t scale_y;
	f32_t scale_z;

	f32_t trans_x;
	f32_t trans_y;
	f32_t trans_z;

	f32_t boundingradius;

	f32_t eyepos_x;
	f32_t eyepos_y;
	f32_t eyepos_z;

	s32_t num_skins;

	s32_t skin_width;
	s32_t skin_height;

	s32_t num_vertices;  // per frame
	s32_t num_tris;
	s32_t num_frames;

	s32_t synctype;
	s32_t flags;
	f32_t size;
} 
raw_mdl_header_t;

typedef struct
{
	s32_t onseam;
	s32_t s;
	s32_t t;
} 
raw_mdl_texcoord_t;

typedef struct 
{
	s32_t facesfront;
	s32_t vertex[3];
} 
raw_mdl_triangle_t;

typedef struct
{
	u8_t x, y, z;
	u8_t light_normal;
} 
raw_mdl_vertex_t;

typedef struct
{
	raw_mdl_vertex_t bboxmin;
	raw_mdl_vertex_t bboxmax;
	char name[16];
	raw_mdl_vertex_t *verts;
} 
raw_mdl_simpleframe_t;

typedef struct
{
	s32_t type;
	raw_mdl_simpleframe_t frame;
}
raw_mdl_frame_t;


/*============== EDGE REPRESENTATION ====================*/

struct mdl_vertex_c
{
	float x, y, z;

	short normal_idx;
};

struct mdl_frame_c
{
	mdl_vertex_c *vertices;

	const char *name;

	// list of normals which are used.  Terminated by -1.
	short *used_normals;
};

struct mdl_point_c
{
	float skin_s, skin_t;

	// index into frame's vertex array (mdl_frame_c::verts)
	int vert_idx;
};

struct mdl_strip_c
{
	// either GL_TRIANGLE_STRIP or GL_TRIANGLE_FAN
	GLenum mode;

	// number of points in this strip / fan
	int count;

	// index to the first point (within mdl_model_c::points).
	// All points for the strip are contiguous in that array.
	int first;
};

class mdl_model_c
{
public:
	int num_frames;
	int num_points;
	int num_strips;
	int skin_width;
	int skin_height;

	mdl_frame_c *frames;
	mdl_point_c *points;
	mdl_strip_c *strips;

	int verts_per_frame;

	std::vector<u32_t> skin_ids;

public:
	mdl_model_c(int _nframe, int _npoint, int _nstrip, int _swidth, int _sheight) :
		num_frames(_nframe), num_points(_npoint),
		num_strips(_nstrip), skin_width(_swidth),
		skin_height(_sheight), verts_per_frame(0)
	{
		frames = new mdl_frame_c[num_frames];
		points = new mdl_point_c[num_points];
		strips = new mdl_strip_c[num_strips];
	}

	~mdl_model_c()
	{
		delete[] frames;
		delete[] points;
		delete[] strips;
	}
};


/*============== LOADING CODE ====================*/

static const char *CopyFrameName(raw_mdl_simpleframe_t *frm)
{
	char *str = new char[20];

	memcpy(str, frm->name, 16);

	// ensure it is NUL terminated
	str[16] = 0;

	return str;
}

static short *CreateNormalList(byte *which_normals)
{
	int count = 0;
	int i;
	
	for (i=0; i < MD_NUM_NORMALS; i++)
		if (which_normals[i])
			count++;

	short *n_list = new short[count+1];

	count = 0;

	for (i=0; i < MD_NUM_NORMALS; i++)
		if (which_normals[i])
			n_list[count++] = i;

	n_list[count] = -1;
	
	return n_list;
}


mdl_model_c *MDL_LoadModel(epi::file_c *f)
{
	int i;

	raw_mdl_header_t header;

	/* read header */
	f->Read(&header, sizeof (raw_mdl_header_t));

	int version = EPI_LE_S32(header.version);

	I_Debugf("MODEL IDENT: [%c%c%c%c] VERSION: %d",
			 header.ident[0], header.ident[1],
			 header.ident[2], header.ident[3], version);

	if (epi::prefix_cmp(header.ident, MDL_IDENTIFIER) != 0)
	{
		I_Error("MDL_LoadModel: lump is not an MDL model!");
		return NULL; /* NOT REACHED */
	}
			
	if (version != MDL_VERSION)
	{
		I_Error("MDL_LoadModel: strange version!");
		return NULL; /* NOT REACHED */
	}

	int num_frames = EPI_LE_S32(header.num_frames);
	int num_tris = EPI_LE_S32(header.num_tris);
	int num_verts = EPI_LE_S32(header.num_vertices);
	int swidth = EPI_LE_S32(header.skin_width);
	int sheight = EPI_LE_S32(header.skin_height);
	int num_points = num_tris * 3;
	int num_strips = num_tris;

	mdl_model_c *md = new mdl_model_c(num_frames, num_points, num_strips, swidth, sheight);

	/* PARSE SKINS */

	for (i=0; i < EPI_LE_S32(header.num_skins); i++)
	{
		int group = 0;
		u8_t *pixels = new u8_t[sheight * swidth];

		// Check for single vs. group skins; error if group skin found
		f->Read(&group, sizeof(int));
		if (EPI_LE_S32(group))
		{
			I_Error("MDL_LoadModel: Group skins unsupported!\n");
			return nullptr; // Not reached
		}

		f->Read(pixels, sheight * swidth * sizeof(u8_t));
		epi::image_data_c *tmp_img = new epi::image_data_c(swidth, sheight, 3);
		// Expand 8 bits paletted image to RGB
		for (int i = 0; i < swidth * sheight; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				tmp_img->pixels[(i * 3) + j] = md_colormap[pixels[i]][j];
			}
		}
		delete[] pixels;
		md->skin_ids.push_back(R_UploadTexture(tmp_img, UPL_MipMap | UPL_Smooth));
		delete tmp_img;
	}

	/* PARSE TEXCOORDS */
	raw_mdl_texcoord_t *texcoords = new raw_mdl_texcoord_t[num_verts];
	f->Read(texcoords, num_verts * sizeof(raw_mdl_texcoord_t));

	/* PARSE TRIANGLES */

	raw_mdl_triangle_t *tris = new raw_mdl_triangle_t[num_tris];
	f->Read(tris, num_tris * sizeof(raw_mdl_triangle_t));

	/* PARSE FRAMES */

	raw_mdl_frame_t *frames = new raw_mdl_frame_t[num_frames];

	for (int fr = 0; fr < num_frames; fr++)
	{
		frames[fr].frame.verts = new raw_mdl_vertex_t[num_verts];
		f->Read(&frames[fr].type, sizeof(int));
		f->Read(&frames[fr].frame.bboxmin, sizeof(raw_mdl_vertex_t));
		f->Read(&frames[fr].frame.bboxmax, sizeof(raw_mdl_vertex_t));
		f->Read(frames[fr].frame.name, 16 * sizeof(char));
		f->Read(frames[fr].frame.verts, num_verts * sizeof(raw_mdl_vertex_t));
	}

	I_Debugf("  frames:%d  points:%d  tris: %d\n",
			num_frames, num_tris * 3, num_tris);

	md->verts_per_frame = num_verts;

	I_Debugf("  verts_per_frame:%d\n", md->verts_per_frame);

	// convert glcmds into strips and points
	mdl_strip_c *strip = md->strips;
	mdl_point_c *point = md->points;

	for (i = 0; i < num_tris; i++)
	{
		SYS_ASSERT(strip < md->strips + md->num_strips);
		SYS_ASSERT(point < md->points + md->num_points);

		strip->mode = GL_TRIANGLES;

		strip->count = 3;
		strip->first = point - md->points;

		strip++;

		for (int j=0; j < 3; j++, point++)
		{
			raw_mdl_triangle_t tri = tris[i];
			point->vert_idx = EPI_LE_S32(tri.vertex[j]);
			float s = (float)EPI_LE_S16(texcoords[point->vert_idx].s);
			float t = (float)EPI_LE_S16(texcoords[point->vert_idx].t);
			if (!EPI_LE_S32(tri.facesfront) && EPI_LE_S32(texcoords[point->vert_idx].onseam))
				s += (float)swidth * 0.5f;
			point->skin_s   = (s + 0.5f) / (float)swidth;
			point->skin_t   = (t + 0.5f) / (float)sheight;
			SYS_ASSERT(point->vert_idx >= 0);
			SYS_ASSERT(point->vert_idx < md->verts_per_frame);
		}
	}

	SYS_ASSERT(strip == md->strips + md->num_strips);
	SYS_ASSERT(point == md->points + md->num_points);

	/* PARSE FRAMES */

	byte which_normals[MD_NUM_NORMALS];

	u32_t raw_scale[3];
	u32_t raw_translate[3];

	raw_scale[0] = EPI_LE_U32(header.scale_x);
	raw_scale[1] = EPI_LE_U32(header.scale_y);
	raw_scale[2] = EPI_LE_U32(header.scale_z);
	raw_translate[0] = EPI_LE_U32(header.trans_x);
	raw_translate[1] = EPI_LE_U32(header.trans_y);
	raw_translate[2] = EPI_LE_U32(header.trans_z);

	float *f_ptr = (float *) raw_scale;
	float scale[3];
	float translate[3];

	scale[0] = f_ptr[0];
	scale[1] = f_ptr[1];
	scale[2] = f_ptr[2];

	f_ptr = (float *) raw_translate;
	translate[0] = f_ptr[0];
	translate[1] = f_ptr[1];
	translate[2] = f_ptr[2];

	for (i = 0; i < num_frames; i++)
	{
		raw_mdl_frame_t raw_frame = frames[i];

		md->frames[i].name = CopyFrameName(&raw_frame.frame);

		raw_mdl_vertex_t *raw_verts = frames[i].frame.verts;

		md->frames[i].vertices = new mdl_vertex_c[md->verts_per_frame];

		memset(which_normals, 0, sizeof(which_normals));

		for (int v = 0; v < md->verts_per_frame; v++)
		{
			raw_mdl_vertex_t *raw_V  = raw_verts + v;
			mdl_vertex_c     *good_V = md->frames[i].vertices + v;

			good_V->x = (int)raw_V->x * scale[0] + translate[0];
			good_V->y = (int)raw_V->y * scale[1] + translate[1];
			good_V->z = (int)raw_V->z * scale[2] + translate[2];

			good_V->normal_idx = raw_V->light_normal;

			SYS_ASSERT(good_V->normal_idx >= 0);
			SYS_ASSERT(good_V->normal_idx < MD_NUM_NORMALS);

			which_normals[good_V->normal_idx] = 1;
		}

		md->frames[i].used_normals = CreateNormalList(which_normals);
	}

	delete[] texcoords;
	delete[] tris;
	delete[] frames;
	
	return md;
}

short MDL_FindFrame(mdl_model_c *md, const char *name)
{
	SYS_ASSERT(strlen(name) > 0);

 	for (int f = 0; f < md->num_frames; f++)
	{
		mdl_frame_c *frame = &md->frames[f];

		if (DDF_CompareName(name, frame->name) == 0)
			return f;
	}

	return -1; // NOT FOUND
}

/*============== MODEL RENDERING ====================*/


typedef struct
{
	mobj_t *mo;

	mdl_model_c *model;

	const mdl_frame_c *frame1;
	const mdl_frame_c *frame2;
	const mdl_strip_c *strip;

	float lerp;
	float x, y, z;

	bool is_weapon;
	bool is_fuzzy;

	// scaling
	float xy_scale;
	float  z_scale;
	float bias;

	// image size
	float im_right;
	float im_top;

	// fuzzy info
	float  fuzz_mul;
	vec2_t fuzz_add;

	// mlook vectors
	vec2_t kx_mat;
	vec2_t kz_mat;

	// rotation vectors
	vec2_t rx_mat;
	vec2_t ry_mat;

	multi_color_c nm_colors[MD_NUM_NORMALS];

	short * used_normals;

	bool is_additive;

public:
	void CalcPos(vec3_t *pos, float x1, float y1, float z1) const
	{
		x1 *= xy_scale;
		y1 *= xy_scale;
		z1 *=  z_scale;

		float x2 = x1 * kx_mat.x + z1 * kx_mat.y;
		float z2 = x1 * kz_mat.x + z1 * kz_mat.y;
		float y2 = y1;

		pos->x = x + x2 * rx_mat.x + y2 * rx_mat.y;
		pos->y = y + x2 * ry_mat.x + y2 * ry_mat.y;
		pos->z = z + z2;
	}

	void CalcNormal(vec3_t *normal, const mdl_vertex_c *vert) const
	{
		short n = vert->normal_idx;

		float nx1 = md_normals[n].x;
		float ny1 = md_normals[n].y;
		float nz1 = md_normals[n].z;

		float nx2 = nx1 * kx_mat.x + nz1 * kx_mat.y;
		float nz2 = nx1 * kz_mat.x + nz1 * kz_mat.y;
		float ny2 = ny1;

		normal->x = nx2 * rx_mat.x + ny2 * rx_mat.y;
		normal->y = nx2 * ry_mat.x + ny2 * ry_mat.y;
		normal->z = nz2;
	}
}
model_coord_data_t;


static void InitNormalColors(model_coord_data_t *data)
{
	short *n_list = data->used_normals;

	for (; *n_list >= 0; n_list++)
	{
		data->nm_colors[*n_list].Clear();
	}
}

static void ShadeNormals(abstract_shader_c *shader,
		 model_coord_data_t *data, bool skip_calc)
{
	short *n_list = data->used_normals;

	for (; *n_list >= 0; n_list++)
	{
		short n = *n_list;
		float nx, ny, nz;

		if (!skip_calc)
		{
			float nx1 = md_normals[n].x;
			float ny1 = md_normals[n].y;
			float nz1 = md_normals[n].z;

			float nx2 = nx1 * data->kx_mat.x + nz1 * data->kx_mat.y;
			float nz2 = nx1 * data->kz_mat.x + nz1 * data->kz_mat.y;
			float ny2 = ny1;

			nx = nx2 * data->rx_mat.x + ny2 * data->rx_mat.y;
			ny = nx2 * data->ry_mat.x + ny2 * data->ry_mat.y;
			nz = nz2;
		}

		shader->Corner(data->nm_colors + n, nx, ny, nz, data->mo, data->is_weapon);
	}
}

static void DLIT_Model(mobj_t *mo, void *dataptr)
{
	model_coord_data_t *data = (model_coord_data_t *)dataptr;

	// dynamic lights do not light themselves up!
	if (mo == data->mo)
		return;

	SYS_ASSERT(mo->dlight.shader);

	ShadeNormals(mo->dlight.shader, data, false);
}

static int MDL_MulticolMaxRGB(model_coord_data_t *data, bool additive)
{
	int result = 0;

	short *n_list = data->used_normals;

	for (; *n_list >= 0; n_list++)
	{
		multi_color_c *col = &data->nm_colors[*n_list];

		int mx = additive ? col->add_MAX() : col->mod_MAX();

		result = MAX(result, mx);
	}

	return result;
}

static void UpdateMulticols(model_coord_data_t *data)
{
	short *n_list = data->used_normals;

	for (; *n_list >= 0; n_list++)
	{
		multi_color_c *col = &data->nm_colors[*n_list];

		col->mod_R -= 256;
		col->mod_G -= 256;
		col->mod_B -= 256;
	}
}

static inline float LerpIt(float v1, float v2, float lerp)
{
	return v1 * (1.0f - lerp) + v2 * lerp;
}


static inline void ModelCoordFunc(model_coord_data_t *data,
					 int v_idx, vec3_t *pos,
					 float *rgb, vec2_t *texc, vec3_t *normal)
{
	const mdl_model_c *md = data->model;

	const mdl_frame_c *frame1 = data->frame1;
	const mdl_frame_c *frame2 = data->frame2;
	const mdl_strip_c *strip  = data->strip;

	SYS_ASSERT(strip->first + v_idx >= 0);
	SYS_ASSERT(strip->first + v_idx < md->num_points);

	const mdl_point_c *point = &md->points[strip->first + v_idx];

	const mdl_vertex_c *vert1 = &frame1->vertices[point->vert_idx];
	const mdl_vertex_c *vert2 = &frame2->vertices[point->vert_idx];

	float x1 = LerpIt(vert1->x, vert2->x, data->lerp);
	float y1 = LerpIt(vert1->y, vert2->y, data->lerp);
	float z1 = LerpIt(vert1->z, vert2->z, data->lerp) + data->bias;

	if (MIR_Reflective())
		y1 = -y1;

	data->CalcPos(pos, x1, y1, z1);

	const mdl_vertex_c *n_vert = (data->lerp < 0.5) ? vert1 : vert2;

	data->CalcNormal(normal, n_vert);

	if (data->is_fuzzy)
	{
		texc->x = point->skin_s * data->fuzz_mul + data->fuzz_add.x;
		texc->y = point->skin_t * data->fuzz_mul + data->fuzz_add.y;

		rgb[0] = rgb[1] = rgb[2] = 0;
		return;
	}

	texc->Set(point->skin_s, point->skin_t);


	multi_color_c *col = &data->nm_colors[n_vert->normal_idx];

	if (! data->is_additive)
	{
		rgb[0] = col->mod_R / 255.0;
		rgb[1] = col->mod_G / 255.0;
		rgb[2] = col->mod_B / 255.0;
	}
	else
	{
		rgb[0] = col->add_R / 255.0;
		rgb[1] = col->add_G / 255.0;
		rgb[2] = col->add_B / 255.0;
	}
}


void MDL_RenderModel(mdl_model_c *md, const image_c *skin_img, bool is_weapon,
		             int frame1, int frame2, float lerp,
		             float x, float y, float z, mobj_t *mo,
					 region_properties_t *props,
					 float scale, float aspect, float bias, int rotation)
{
	// check if frames are valid
	if (frame1 < 0 || frame1 >= md->num_frames)
	{
I_Debugf("Render model: bad frame %d\n", frame1);
		return;
	}
	if (frame2 < 0 || frame2 >= md->num_frames)
	{
I_Debugf("Render model: bad frame %d\n", frame1);
		return;
	}

	model_coord_data_t data;

	data.is_fuzzy = (mo->flags & MF_FUZZY) ? true : false;

	float trans = mo->visibility;

	if (trans <= 0)
		return;

	int blending = BL_NONE;

	if (mo->hyperflags & HF_NOZBUFFER)
		blending |= BL_NoZBuf;

	if (MIR_Reflective())
		blending |= BL_CullFront;
	else
		blending |= BL_CullBack;

	data.mo = mo;
	data.model = md;

	data.frame1 = & md->frames[frame1];
	data.frame2 = & md->frames[frame2];

	data.lerp = lerp;

	data.x = x;
	data.y = y;
	data.z = z;

	data.is_weapon = is_weapon;

	data.xy_scale = scale * aspect * MIR_XYScale();
	data. z_scale = scale * MIR_ZScale();
	data.bias = bias;

	bool tilt = is_weapon || (mo->flags & MF_MISSILE) || (mo->hyperflags & HF_TILT);

	M_Angle2Matrix(tilt ? ~mo->vertangle : 0, &data.kx_mat, &data.kz_mat);

	angle_t ang = mo->angle + rotation;

	MIR_Angle(ang);

	M_Angle2Matrix(~ ang, &data.rx_mat, &data.ry_mat);

	data.used_normals = (lerp < 0.5) ? data.frame1->used_normals : data.frame2->used_normals;

	InitNormalColors(&data);

	GLuint skin_tex = 0;

	if (data.is_fuzzy)
	{
		skin_tex = W_ImageCache(fuzz_image, false);

		data.fuzz_mul = 0.8;
		data.fuzz_add.Set(0, 0);

		data.im_right = 1.0;
		data.im_top   = 1.0;

		if (! data.is_weapon && ! viewiszoomed)
		{
			float dist = P_ApproxDistance(mo->x - viewx, mo->y - viewy, mo->z - viewz);

			data.fuzz_mul = 70.0 / CLAMP(35, dist, 700);
		}

		FUZZ_Adjust(&data.fuzz_add, mo);

		trans = 1.0f;

		blending |=  BL_Alpha | BL_Masked;
		blending &= ~BL_Less;
	}
	else /* (! data.is_fuzzy) */
	{
		int mdlSkin = 0;

		if (is_weapon == true)
			mdlSkin = mo->player->weapons[mo->player->ready_wp].model_skin;
		else
			mdlSkin = mo->model_skin;

		mdlSkin --; //ddf MODEL_SKIN starts at 1 not 0

		if (mdlSkin > -1)
			skin_tex = md->skin_ids[mdlSkin];
		else
			skin_tex = md->skin_ids[0]; // Just use skin 0?

		if (skin_tex == 0)
			I_Error("MDL Frame %s missing skins?\n", md->frames[frame1].name);

		data.im_right = (float)md->skin_width / (float)W_MakeValidSize(md->skin_width);
		data.im_top   = (float)md->skin_height / (float)W_MakeValidSize(md->skin_height);

		abstract_shader_c *shader = R_GetColormapShader(props, mo->state->bright);

		ShadeNormals(shader, &data, true);

		if (use_dlights && ren_extralight < 250)
		{
			float r = mo->radius;
			
			P_DynamicLightIterator(mo->x - r, mo->y - r, mo->z,
					               mo->x + r, mo->y + r, mo->z + mo->height,
								   DLIT_Model, &data);

			P_SectorGlowIterator(mo->subsector->sector,
					             mo->x - r, mo->y - r, mo->z,
					             mo->x + r, mo->y + r, mo->z + mo->height,
								 DLIT_Model, &data);
		}
	}


	/* draw the model */

	int num_pass = data.is_fuzzy  ? 1 :
		           data.is_weapon ? (3 + detail_level) :
					                (2 + detail_level*2);

	for (int pass = 0; pass < num_pass; pass++)
	{
		if (pass == 1)
		{
			blending &= ~BL_Alpha;
			blending |=  BL_Add;
		}

		data.is_additive = (pass > 0 && pass == num_pass-1);

		if (pass > 0 && pass < num_pass-1)
		{
			UpdateMulticols(&data);
			if (MDL_MulticolMaxRGB(&data, false) <= 0)
				continue;
		}
		else if (data.is_additive)
		{
			if (MDL_MulticolMaxRGB(&data, true) <= 0)
				continue;
		}

		local_gl_vert_t * glvert = RGL_BeginUnit(
			 GL_TRIANGLES, md->num_strips * 3,
			 data.is_additive ? ENV_SKIP_RGB : GL_MODULATE, skin_tex,
			 ENV_NONE, 0, pass, blending);

		for (int i = 0; i < md->num_strips; i++)
		{
			data.strip = & md->strips[i];

			for (int v_idx=0; v_idx < md->strips[i].count; v_idx++)
			{
				local_gl_vert_t *dest = glvert + (i*3) + v_idx;

				ModelCoordFunc(&data, v_idx, &dest->pos, dest->rgba,
						&dest->texc[0], &dest->normal);

				dest->rgba[3] = trans;
			}
		}

		RGL_EndUnit(md->num_strips * 3);
	}
}


void MDL_RenderModel_2D(mdl_model_c *md, const image_c *skin_img, int frame,
		                float x, float y, float xscale, float yscale,
		                const mobjtype_c *info)
{
	// check if frame is valid
	if (frame < 0 || frame >= md->num_frames)
		return;

	GLuint skin_tex = md->skin_ids[0]; // Just use skin 0?

	if (skin_tex == 0)
		I_Error("MDL Frame %s missing skins?\n", md->frames[frame].name);

	float im_right = (float)md->skin_width / (float)W_MakeValidSize(md->skin_width);
	float im_top   = (float)md->skin_height / (float)W_MakeValidSize(md->skin_height);

	xscale = yscale * info->model_scale * info->model_aspect;
	yscale = yscale * info->model_scale;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, skin_tex);
 
	glEnable(GL_BLEND);
	glEnable(GL_CULL_FACE);

	if (info->flags & MF_FUZZY)
		glColor4f(0, 0, 0, 0.5f);
	else
		glColor4f(1, 1, 1, 1.0f);

	for (int i = 0; i < md->num_strips; i++)
	{
		const mdl_strip_c *strip = & md->strips[i];

		glBegin(strip->mode);

		for (int v_idx=0; v_idx < md->strips[i].count; v_idx++)
		{
			const mdl_frame_c *frame_ptr = & md->frames[frame];

			SYS_ASSERT(strip->first + v_idx >= 0);
			SYS_ASSERT(strip->first + v_idx < md->num_points);

			const mdl_point_c *point = &md->points[strip->first + v_idx];
			const mdl_vertex_c *vert = &frame_ptr->vertices[point->vert_idx];

			glTexCoord2f(point->skin_s, point->skin_t);
		
			short n = vert->normal_idx;

			float norm_x = md_normals[n].x;
			float norm_y = md_normals[n].y;
			float norm_z = md_normals[n].z;

			glNormal3f(norm_y, norm_z, norm_x);

			float dx = vert->x * xscale;
			float dy = vert->y * xscale;
			float dz = (vert->z + info->model_bias) * yscale;

			glVertex3f(x + dy, y + dz, dx / 256.0f);
		}

		glEnd();
	}

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
