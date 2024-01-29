//----------------------------------------------------------------------------
//  MD2 Models
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2024 The EDGE Team.
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
//
//  Based on "qfiles.h" and "anorms.h" from the GPL'd quake 2 source
//  release.  Copyright (C) 1997-2001 Id Software, Inc.
//
//  Based on MD2 loading and rendering code (C) 2004 David Henry.
//
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "i_defs_gl.h"

#include "types.h"
#include "endianess.h"

#include "dm_state.h" // IS_SKY
#include "g_game.h"   //currmap
#include "r_mdcommon.h"
#include "r_md2.h"
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

#include <stddef.h>
#include <vector>

extern float P_ApproxDistance(float dx, float dy, float dz);

extern cvar_c r_culling;
extern cvar_c r_cullfog;
extern bool   need_to_draw_sky;

// #define DEBUG_MD2_LOAD  1

/*============== MD2 FORMAT DEFINITIONS ====================*/

// format uses float pointing values, but to allow for endianness
// conversions they are represented here as unsigned integers.
typedef uint32_t f32_t;

#define MD2_IDENTIFIER "IDP2"
#define MD2_VERSION    8

typedef struct
{
    char ident[4];

    int32_t version;

    int32_t skin_width;
    int32_t skin_height;

    int32_t frame_size;

    int32_t num_skins;
    int32_t num_vertices; // per frame
    int32_t num_st;
    int32_t num_tris;
    int32_t num_glcmds;
    int32_t num_frames;

    int32_t ofs_skins;
    int32_t ofs_st;
    int32_t ofs_tris;
    int32_t ofs_frames;
    int32_t ofs_glcmds;
    int32_t ofs_end;
} raw_md2_header_t;

typedef struct
{
    uint16_t s, t;
} raw_md2_texcoord_t;

typedef struct
{
    uint16_t index_xyz[3];
    uint16_t index_st[3];
} raw_md2_triangle_t;

typedef struct
{
    uint8_t x, y, z;
    uint8_t light_normal;
} raw_md2_vertex_t;

typedef struct
{
    f32_t scale[3];
    f32_t translate[3];

    char name[16];
} raw_md2_frame_t;

typedef struct
{
    char name[64];
} raw_md2_skin_t;

/*============== MD3 FORMAT DEFINITIONS ====================*/

// format uses float pointing values, but to allow for endianness
// conversions they are represented here as unsigned integers.

#define MD3_IDENTIFIER "IDP3"
#define MD3_VERSION    15

typedef struct
{
    char  ident[4];
    int32_t version;

    char  name[64];
    uint32_t flags;

    int32_t num_frames;
    int32_t num_tags;
    int32_t num_meshes;
    int32_t num_skins;

    int32_t ofs_frames;
    int32_t ofs_tags;
    int32_t ofs_meshes;
    int32_t ofs_end;
} raw_md3_header_t;

typedef struct
{
    char ident[4];
    char name[64];

    uint32_t flags;

    int32_t num_frames;
    int32_t num_shaders;
    int32_t num_verts;
    int32_t num_tris;

    int32_t ofs_tris;
    int32_t ofs_shaders;
    int32_t ofs_texcoords; // one texcoord per vertex
    int32_t ofs_verts;
    int32_t ofs_next_mesh;
} raw_md3_mesh_t;

typedef struct
{
    f32_t s, t;
} raw_md3_texcoord_t;

typedef struct
{
    uint32_t index_xyz[3];
} raw_md3_triangle_t;

typedef struct
{
    int16_t x, y, z;

    uint8_t pitch, yaw;
} raw_md3_vertex_t;

typedef struct
{
    f32_t mins[3];
    f32_t maxs[3];
    f32_t origin[3];
    f32_t radius;

    char name[16];
} raw_md3_frame_t;

/*============== EDGE REPRESENTATION ====================*/

struct md2_vertex_c
{
    float x, y, z;

    short normal_idx;
};

struct md2_frame_c
{
    md2_vertex_c *vertices;

    const char *name;

    // list of normals which are used.  Terminated by -1.
    short *used_normals;
};

struct md2_point_c
{
    float skin_s, skin_t;

    // index into frame's vertex array (md2_frame_c::verts)
    int vert_idx;
};

struct md2_triangle_c
{
    // index to the first point (within md2_model_c::points).
    // All points for the strip are contiguous in that array.
    int first;
};

class md2_model_c
{
  public:
    int num_frames;
    int num_points;
    int num_tris;

    md2_frame_c    *frames;
    md2_point_c    *points;
    md2_triangle_c *tris;

    int verts_per_frame;

    GLuint vbo;

    local_gl_vert_t *gl_verts;

  public:
    md2_model_c(int _nframe, int _npoint, int _ntris)
        : num_frames(_nframe), num_points(_npoint), num_tris(_ntris), verts_per_frame(0), vbo(0), gl_verts(nullptr)
    {
        frames   = new md2_frame_c[num_frames];
        points   = new md2_point_c[num_points];
        tris     = new md2_triangle_c[num_tris];
        gl_verts = new local_gl_vert_t[num_tris * 3];
    }

    ~md2_model_c()
    {
        delete[] frames;
        delete[] points;
        delete[] tris;
    }
};

/*============== LOADING CODE ====================*/

static const char *CopyFrameName(raw_md2_frame_t *frm)
{
    char *str = new char[20];

    memcpy(str, frm->name, 16);

    // ensure it is NUL terminated
    str[16] = 0;

    return str;
}

static const char *CopyFrameName(raw_md3_frame_t *frm)
{
    char *str = new char[20];

    memcpy(str, frm->name, 16);

    // ensure it is NUL terminated
    str[16] = 0;

    return str;
}

static short *CreateNormalList(uint8_t *which_normals)
{
    int count = 0;
    int i;

    for (i = 0; i < MD_NUM_NORMALS; i++)
        if (which_normals[i])
            count++;

    short *n_list = new short[count + 1];

    count = 0;

    for (i = 0; i < MD_NUM_NORMALS; i++)
        if (which_normals[i])
            n_list[count++] = i;

    n_list[count] = -1;

    return n_list;
}

md2_model_c *MD2_LoadModel(epi::file_c *f)
{
    int i;

    raw_md2_header_t header;

    /* read header */
    f->Read(&header, sizeof(raw_md2_header_t));

    int version = EPI_LE_S32(header.version);

    I_Debugf("MODEL IDENT: [%c%c%c%c] VERSION: %d", header.ident[0], header.ident[1], header.ident[2], header.ident[3],
             version);

    if (epi::StringPrefixCompare(header.ident, MD2_IDENTIFIER) != 0)
    {
        I_Error("MD2_LoadModel: lump is not an MD2 model!");
        return NULL; /* NOT REACHED */
    }

    if (version != MD2_VERSION)
    {
        I_Error("MD2_LoadModel: strange version!");
        return NULL; /* NOT REACHED */
    }

    int num_frames = EPI_LE_S32(header.num_frames);
    int num_tris   = EPI_LE_S32(header.num_tris);
    int num_sts    = EPI_LE_S32(header.num_st);
    int num_points = num_tris * 3;

    /* PARSE TRIANGLES */

    raw_md2_triangle_t *md2_tris = new raw_md2_triangle_t[num_tris];

    f->Seek(EPI_LE_S32(header.ofs_tris), epi::file_c::SEEKPOINT_START);
    f->Read(md2_tris, num_tris * sizeof(raw_md2_triangle_t));

    for (int tri = 0; tri < num_tris; tri++)
    {
        md2_tris[tri].index_xyz[0] = EPI_LE_U16(md2_tris[tri].index_xyz[0]);
        md2_tris[tri].index_xyz[1] = EPI_LE_U16(md2_tris[tri].index_xyz[1]);
        md2_tris[tri].index_xyz[2] = EPI_LE_U16(md2_tris[tri].index_xyz[2]);
        md2_tris[tri].index_st[0]  = EPI_LE_U16(md2_tris[tri].index_st[0]);
        md2_tris[tri].index_st[1]  = EPI_LE_U16(md2_tris[tri].index_st[1]);
    }

    /* PARSE TEXCOORDS */

    raw_md2_texcoord_t *md2_sts = new raw_md2_texcoord_t[num_sts];

    f->Seek(EPI_LE_S32(header.ofs_st), epi::file_c::SEEKPOINT_START);
    f->Read(md2_sts, num_sts * sizeof(raw_md2_texcoord_t));

    for (int st = 0; st < num_sts; st++)
    {
        md2_sts[st].s = EPI_LE_U16(md2_sts[st].s);
        md2_sts[st].t = EPI_LE_U16(md2_sts[st].t);
    }

    I_Debugf("  frames:%d  points:%d  triangles: %d\n", num_frames, num_tris * 3, num_tris);

    md2_model_c *md = new md2_model_c(num_frames, num_points, num_tris);

    md->verts_per_frame = EPI_LE_S32(header.num_vertices);

    I_Debugf("  verts_per_frame:%d\n", md->verts_per_frame);

    // convert raw tris
    md2_triangle_c *tri   = md->tris;
    md2_point_c    *point = md->points;

    for (i = 0; i < num_tris; i++)
    {
        SYS_ASSERT(tri < md->tris + md->num_tris);
        SYS_ASSERT(point < md->points + md->num_points);

        tri->first = point - md->points;

        tri++;

        for (int j = 0; j < 3; j++, point++)
        {
            raw_md2_triangle_t t = md2_tris[i];

            point->skin_s   = (float)md2_sts[t.index_st[j]].s / header.skin_width;
            point->skin_t   = 1.0f - ((float)md2_sts[t.index_st[j]].t / header.skin_height);
            point->vert_idx = t.index_xyz[j];

            SYS_ASSERT(point->vert_idx >= 0);
            SYS_ASSERT(point->vert_idx < md->verts_per_frame);
        }
    }

    SYS_ASSERT(tri == md->tris + md->num_tris);
    SYS_ASSERT(point == md->points + md->num_points);

    delete[] md2_tris;
    delete[] md2_sts;

    /* PARSE FRAMES */

    uint8_t which_normals[MD_NUM_NORMALS];

    raw_md2_vertex_t *raw_verts = new raw_md2_vertex_t[md->verts_per_frame];

    f->Seek(EPI_LE_S32(header.ofs_frames), epi::file_c::SEEKPOINT_START);

    for (i = 0; i < num_frames; i++)
    {
        raw_md2_frame_t raw_frame;

        f->Read(&raw_frame, sizeof(raw_frame));

        for (int j = 0; j < 3; j++)
        {
            raw_frame.scale[j]     = EPI_LE_U32(raw_frame.scale[j]);
            raw_frame.translate[j] = EPI_LE_U32(raw_frame.translate[j]);
        }

        float *f_ptr = (float *)raw_frame.scale;

        float scale[3];
        float translate[3];

        scale[0] = f_ptr[0];
        scale[1] = f_ptr[1];
        scale[2] = f_ptr[2];

        translate[0] = f_ptr[3];
        translate[1] = f_ptr[4];
        translate[2] = f_ptr[5];

        md->frames[i].name = CopyFrameName(&raw_frame);

#ifdef DEBUG_MD2_LOAD
        I_Debugf("  __FRAME_%d__[%s]\n", i + 1, md->frames[i].name);
        I_Debugf("    scale: %1.2f, %1.2f, %1.2f\n", scale[0], scale[1], scale[2]);
        I_Debugf("    translate: %1.2f, %1.2f, %1.2f\n", translate[0], translate[1], translate[2]);
#endif

        f->Read(raw_verts, md->verts_per_frame * sizeof(raw_md2_vertex_t));

        md->frames[i].vertices = new md2_vertex_c[md->verts_per_frame];

        memset(which_normals, 0, sizeof(which_normals));

        for (int v = 0; v < md->verts_per_frame; v++)
        {
            raw_md2_vertex_t *raw_V  = raw_verts + v;
            md2_vertex_c     *good_V = md->frames[i].vertices + v;

            good_V->x = (int)raw_V->x * scale[0] + translate[0];
            good_V->y = (int)raw_V->y * scale[1] + translate[1];
            good_V->z = (int)raw_V->z * scale[2] + translate[2];

#ifdef DEBUG_MD2_LOAD
            I_Debugf("    __VERT_%d__\n", v);
            I_Debugf("      raw: %d,%d,%d\n", raw_V->x, raw_V->y, raw_V->z);
            I_Debugf("      normal: %d\n", raw_V->light_normal);
            I_Debugf("      good: %1.2f, %1.2f, %1.2f\n", good_V->x, good_V->y, good_V->z);
#endif
            good_V->normal_idx = raw_V->light_normal;

            SYS_ASSERT(good_V->normal_idx >= 0);
            // SYS_ASSERT(good_V->normal_idx < MD_NUM_NORMALS);
            //  Dasho: Maybe try to salvage bad MD2 models?
            if (good_V->normal_idx >= MD_NUM_NORMALS)
            {
                I_Debugf("Vert %d of Frame %d has an invalid normal index: %d\n", v, i, good_V->normal_idx);
                good_V->normal_idx = (good_V->normal_idx % MD_NUM_NORMALS);
            }

            which_normals[good_V->normal_idx] = 1;
        }

        md->frames[i].used_normals = CreateNormalList(which_normals);
    }

    delete[] raw_verts;

    glGenBuffers(1, &md->vbo);
    if (md->vbo == 0)
        I_Error("MD2_LoadModel: Failed to bind VBO!\n");
    glBindBuffer(GL_ARRAY_BUFFER, md->vbo);
    glBufferData(GL_ARRAY_BUFFER, md->num_tris * 3 * sizeof(local_gl_vert_t), NULL, GL_STREAM_DRAW);

    return md;
}

short MD2_FindFrame(md2_model_c *md, const char *name)
{
    SYS_ASSERT(strlen(name) > 0);

    for (int f = 0; f < md->num_frames; f++)
    {
        md2_frame_c *frame = &md->frames[f];

        if (DDF_CompareName(name, frame->name) == 0)
            return f;
    }

    return -1; // NOT FOUND
}

/*============== MD3 LOADING CODE ====================*/

static uint8_t md3_normal_to_md2[128][128];
static bool md3_normal_map_built = false;

static uint8_t MD2_FindNormal(float x, float y, float z)
{
    // -AJA- we make the search around SIX times faster by only
    // considering the first quadrant (where x, y, z are >= 0).

    int quadrant = 0;

    if (x < 0)
    {
        x = -x;
        quadrant |= 4;
    }
    if (y < 0)
    {
        y = -y;
        quadrant |= 2;
    }
    if (z < 0)
    {
        z = -z;
        quadrant |= 1;
    }

    int   best_g   = 0;
    float best_dot = -1;

    for (int i = 0; i < 27; i++)
    {
        int n = md_normal_groups[i][0];

        float nx = md_normals[n].X;
        float ny = md_normals[n].Y;
        float nz = md_normals[n].Z;

        float dot = (x * nx + y * ny + z * nz);

        if (dot > best_dot)
        {
            best_g   = i;
            best_dot = dot;
        }
    }

    return md_normal_groups[best_g][quadrant];
}

static void MD3_CreateNormalMap(void)
{
    // Create a table mapping MD3 normals to MD2 normals.
    // We discard the least significant bit of pitch and yaw
    // (for speed and memory saving).

    // build a sine table for even faster calcs
    float sintab[160];

    for (int i = 0; i < 160; i++)
        sintab[i] = sin(i * M_PI / 64.0);

    for (int pitch = 0; pitch < 128; pitch++)
    {
        uint8_t *dest = &md3_normal_to_md2[pitch][0];

        for (int yaw = 0; yaw < 128; yaw++)
        {
            float z = sintab[pitch + 32];
            float w = sintab[pitch];

            float x = w * sintab[yaw + 32];
            float y = w * sintab[yaw];

            *dest++ = MD2_FindNormal(x, y, z);
        }
    }

    md3_normal_map_built = true;
}

md2_model_c *MD3_LoadModel(epi::file_c *f)
{
    int    i;
    float *ff;

    if (!md3_normal_map_built)
        MD3_CreateNormalMap();

    raw_md3_header_t header;

    /* read header */
    f->Read(&header, sizeof(raw_md3_header_t));

    int version = EPI_LE_S32(header.version);

    I_Debugf("MODEL IDENT: [%c%c%c%c] VERSION: %d", header.ident[0], header.ident[1], header.ident[2], header.ident[3],
             version);

    if (strncmp(header.ident, MD3_IDENTIFIER, 4) != 0)
    {
        I_Error("MD3_LoadModel: lump is not an MD3 model!");
        return NULL; /* NOT REACHED */
    }

    if (version != MD3_VERSION)
    {
        I_Error("MD3_LoadModel: strange version!");
        return NULL; /* NOT REACHED */
    }

    if (EPI_LE_S32(header.num_meshes) > 1)
        I_Warning("Ignoring extra meshes in MD3 model.\n");

    /* LOAD MESH #1 */

    int mesh_base = EPI_LE_S32(header.ofs_meshes);

    f->Seek(mesh_base, epi::file_c::SEEKPOINT_START);

    raw_md3_mesh_t mesh;

    f->Read(&mesh, sizeof(raw_md3_mesh_t));

    int num_frames = EPI_LE_S32(mesh.num_frames);
    int num_verts  = EPI_LE_S32(mesh.num_verts);
    int num_tris   = EPI_LE_S32(mesh.num_tris);

    I_Debugf("  frames:%d  verts:%d  triangles: %d\n", num_frames, num_verts, num_tris);

    md2_model_c *md = new md2_model_c(num_frames, num_tris * 3, num_tris);

    md->verts_per_frame = num_verts;

    /* PARSE TEXCOORD */

    md2_point_c *temp_TEXC = new md2_point_c[num_verts];

    f->Seek(mesh_base + EPI_LE_S32(mesh.ofs_texcoords), epi::file_c::SEEKPOINT_START);

    for (i = 0; i < num_verts; i++)
    {
        raw_md3_texcoord_t texc;

        f->Read(&texc, sizeof(raw_md3_texcoord_t));

        texc.s = EPI_LE_U32(texc.s);
        texc.t = EPI_LE_U32(texc.t);

        ff                  = (float *)&texc.s;
        temp_TEXC[i].skin_s = *ff;
        ff                  = (float *)&texc.t;
        temp_TEXC[i].skin_t = 1.0f - *ff;

        temp_TEXC[i].vert_idx = i;
    }

    /* PARSE TRIANGLES */

    f->Seek(mesh_base + EPI_LE_S32(mesh.ofs_tris), epi::file_c::SEEKPOINT_START);

    for (i = 0; i < num_tris; i++)
    {
        raw_md3_triangle_t tri;

        f->Read(&tri, sizeof(raw_md3_triangle_t));

        int a = EPI_LE_U32(tri.index_xyz[0]);
        int b = EPI_LE_U32(tri.index_xyz[1]);
        int c = EPI_LE_U32(tri.index_xyz[2]);

        SYS_ASSERT(a < num_verts);
        SYS_ASSERT(b < num_verts);
        SYS_ASSERT(c < num_verts);

        md->tris[i].first = i * 3;

        md2_point_c *point = md->points + i * 3;

        point[0] = temp_TEXC[a];
        point[1] = temp_TEXC[b];
        point[2] = temp_TEXC[c];
    }

    delete[] temp_TEXC;

    /* PARSE VERTEX FRAMES */

    f->Seek(mesh_base + EPI_LE_S32(mesh.ofs_verts), epi::file_c::SEEKPOINT_START);

    uint8_t which_normals[MD_NUM_NORMALS];

    for (i = 0; i < num_frames; i++)
    {
        md->frames[i].vertices = new md2_vertex_c[num_verts];

        memset(which_normals, 0, sizeof(which_normals));

        md2_vertex_c *good_V = md->frames[i].vertices;

        for (int v = 0; v < num_verts; v++, good_V++)
        {
            raw_md3_vertex_t vert;

            f->Read(&vert, sizeof(raw_md3_vertex_t));

            good_V->x = EPI_LE_S16(vert.x) / 64.0;
            good_V->y = EPI_LE_S16(vert.y) / 64.0;
            good_V->z = EPI_LE_S16(vert.z) / 64.0;

            good_V->normal_idx = md3_normal_to_md2[vert.pitch >> 1][vert.yaw >> 1];

            which_normals[good_V->normal_idx] = 1;
        }

        md->frames[i].used_normals = CreateNormalList(which_normals);
    }

    /* PARSE FRAME INFO */

    f->Seek(EPI_LE_S32(header.ofs_frames), epi::file_c::SEEKPOINT_START);

    for (i = 0; i < num_frames; i++)
    {
        raw_md3_frame_t frame;

        f->Read(&frame, sizeof(raw_md3_frame_t));

        md->frames[i].name = CopyFrameName(&frame);

        I_Debugf("Frame %d = '%s'\n", i + 1, md->frames[i].name);

        // TODO: load in bbox (for visibility checking)
    }
    glGenBuffers(1, &md->vbo);
    if (md->vbo == 0)
        I_Error("MD3_LoadModel: Failed to create VBO!\n");
    glBindBuffer(GL_ARRAY_BUFFER, md->vbo);
    glBufferData(GL_ARRAY_BUFFER, md->num_tris * 3 * sizeof(local_gl_vert_t), NULL, GL_STREAM_DRAW);
    return md;
}

/*============== MODEL RENDERING ====================*/

typedef struct model_coord_data_s
{
    mobj_t *mo;

    md2_model_c *model;

    const md2_frame_c    *frame1;
    const md2_frame_c    *frame2;
    const md2_triangle_c *tri;

    float lerp;
    float x, y, z;

    bool is_weapon;
    bool is_fuzzy;

    // scaling
    float xy_scale;
    float z_scale;
    float bias;

    // image size
    float im_right;
    float im_top;

    // fuzzy info
    float  fuzz_mul;
    HMM_Vec2 fuzz_add;

    // mlook vectors
    HMM_Vec2 kx_mat;
    HMM_Vec2 kz_mat;

    // rotation vectors
    HMM_Vec2 rx_mat;
    HMM_Vec2 ry_mat;

    multi_color_c nm_colors[MD_NUM_NORMALS];

    short *used_normals;

    bool is_additive;

  public:
    void CalcPos(HMM_Vec3 *pos, float x1, float y1, float z1) const
    {
        x1 *= xy_scale;
        y1 *= xy_scale;
        z1 *= z_scale;

        float x2 = x1 * kx_mat.X + z1 * kx_mat.Y;
        float z2 = x1 * kz_mat.X + z1 * kz_mat.Y;
        float y2 = y1;

        pos->X = x + x2 * rx_mat.X + y2 * rx_mat.Y;
        pos->Y = y + x2 * ry_mat.X + y2 * ry_mat.Y;
        pos->Z = z + z2;
    }

    void CalcNormal(HMM_Vec3 *normal, const md2_vertex_c *vert) const
    {
        short n = vert->normal_idx;

        float nx1 = md_normals[n].X;
        float ny1 = md_normals[n].Y;
        float nz1 = md_normals[n].Z;

        float nx2 = nx1 * kx_mat.X + nz1 * kx_mat.Y;
        float nz2 = nx1 * kz_mat.X + nz1 * kz_mat.Y;
        float ny2 = ny1;

        normal->X = nx2 * rx_mat.X + ny2 * rx_mat.Y;
        normal->Y = nx2 * ry_mat.X + ny2 * ry_mat.Y;
        normal->Z = nz2;
    }
} model_coord_data_t;

static void InitNormalColors(model_coord_data_t *data)
{
    short *n_list = data->used_normals;

    for (; *n_list >= 0; n_list++)
    {
        data->nm_colors[*n_list].Clear();
    }
}

static void ShadeNormals(abstract_shader_c *shader, model_coord_data_t *data, bool skip_calc)
{
    short *n_list = data->used_normals;

    for (; *n_list >= 0; n_list++)
    {
        short n  = *n_list;
        float nx = 0;
        float ny = 0;
        float nz = 0;

        if (!skip_calc)
        {
            float nx1 = md_normals[n].X;
            float ny1 = md_normals[n].Y;
            float nz1 = md_normals[n].Z;

            float nx2 = nx1 * data->kx_mat.X + nz1 * data->kx_mat.Y;
            float nz2 = nx1 * data->kz_mat.X + nz1 * data->kz_mat.Y;
            float ny2 = ny1;

            nx = nx2 * data->rx_mat.X + ny2 * data->rx_mat.Y;
            ny = nx2 * data->ry_mat.X + ny2 * data->ry_mat.Y;
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

static int MD2_MulticolMaxRGB(model_coord_data_t *data, bool additive)
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

static inline void ModelCoordFunc(model_coord_data_t *data, int v_idx, HMM_Vec3 *pos, float *rgb, HMM_Vec2 *texc,
                                  HMM_Vec3 *normal)
{
    const md2_model_c *md = data->model;

    const md2_frame_c    *frame1 = data->frame1;
    const md2_frame_c    *frame2 = data->frame2;
    const md2_triangle_c *tri    = data->tri;

    SYS_ASSERT(tri->first + v_idx >= 0);
    SYS_ASSERT(tri->first + v_idx < md->num_points);

    const md2_point_c *point = &md->points[tri->first + v_idx];

    const md2_vertex_c *vert1 = &frame1->vertices[point->vert_idx];
    const md2_vertex_c *vert2 = &frame2->vertices[point->vert_idx];

    float x1 = LerpIt(vert1->x, vert2->x, data->lerp);
    float y1 = LerpIt(vert1->y, vert2->y, data->lerp);
    float z1 = LerpIt(vert1->z, vert2->z, data->lerp) + data->bias;

    if (MIR_Reflective())
        y1 = -y1;

    data->CalcPos(pos, x1, y1, z1);

    const md2_vertex_c *n_vert = (data->lerp < 0.5) ? vert1 : vert2;

    data->CalcNormal(normal, n_vert);

    if (data->is_fuzzy)
    {
        texc->X = point->skin_s * data->fuzz_mul + data->fuzz_add.X;
        texc->Y = point->skin_t * data->fuzz_mul + data->fuzz_add.Y;

        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    *texc = {{point->skin_s * data->im_right, point->skin_t * data->im_top}};

    multi_color_c *col = &data->nm_colors[n_vert->normal_idx];

    if (!data->is_additive)
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

    rgb[0] *= ren_red_mul;
    rgb[1] *= ren_grn_mul;
    rgb[2] *= ren_blu_mul;
}

void MD2_RenderModel(md2_model_c *md, const image_c *skin_img, bool is_weapon, int frame1, int frame2, float lerp,
                     float x, float y, float z, mobj_t *mo, region_properties_t *props, float scale, float aspect,
                     float bias, int rotation)
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

    int blending;

    if (trans >= 0.99f && skin_img->opacity == OPAC_Solid)
        blending = BL_NONE;
    else if (trans < 0.11f || skin_img->opacity == OPAC_Complex)
        blending = BL_Masked;
    else
        blending = BL_Less;

    if (trans < 0.99f || skin_img->opacity == OPAC_Complex)
        blending |= BL_Alpha;

    if (mo->hyperflags & HF_NOZBUFFER)
        blending |= BL_NoZBuf;

    if (MIR_Reflective())
        blending |= BL_CullFront;
    else
        blending |= BL_CullBack;

    data.mo    = mo;
    data.model = md;

    data.frame1 = &md->frames[frame1];
    data.frame2 = &md->frames[frame2];

    data.lerp = lerp;

    data.x = x;
    data.y = y;
    data.z = z;

    data.is_weapon = is_weapon;

    data.xy_scale = scale * aspect * MIR_XYScale();
    data.z_scale  = scale * MIR_ZScale();
    data.bias     = bias;

    bool tilt = is_weapon || (mo->flags & MF_MISSILE) || (mo->hyperflags & HF_TILT);

    M_Angle2Matrix(tilt ? ~mo->vertangle : 0, &data.kx_mat, &data.kz_mat);

    BAMAngle ang = mo->angle + rotation;

    MIR_Angle(ang);

    M_Angle2Matrix(~ang, &data.rx_mat, &data.ry_mat);

    data.used_normals = (lerp < 0.5) ? data.frame1->used_normals : data.frame2->used_normals;

    InitNormalColors(&data);

    GLuint skin_tex = 0;

    if (data.is_fuzzy)
    {
        skin_tex = W_ImageCache(fuzz_image, false);

        data.fuzz_mul = 0.8;
        data.fuzz_add = {{0, 0}};

        data.im_right = 1.0;
        data.im_top   = 1.0;

        if (!data.is_weapon && !viewiszoomed)
        {
            float dist = P_ApproxDistance(mo->x - viewx, mo->y - viewy, mo->z - viewz);

            data.fuzz_mul = 70.0 / CLAMP(35, dist, 700);
        }

        FUZZ_Adjust(&data.fuzz_add, mo);

        trans = 1.0f;

        blending |= BL_Alpha | BL_Masked;
        blending &= ~BL_Less;
    }
    else /* (! data.is_fuzzy) */
    {
        skin_tex = W_ImageCache(skin_img, false, ren_fx_colmap ? ren_fx_colmap : is_weapon ? NULL : mo->info->palremap);

        data.im_right = IM_RIGHT(skin_img);
        data.im_top   = IM_TOP(skin_img);

        abstract_shader_c *shader = R_GetColormapShader(props, mo->state->bright, mo->subsector->sector);

        ShadeNormals(shader, &data, true);

        if (use_dlights && ren_extralight < 250)
        {
            float r = mo->radius;

            P_DynamicLightIterator(mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r, mo->z + mo->height, DLIT_Model,
                                   &data);

            P_SectorGlowIterator(mo->subsector->sector, mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r,
                                 mo->z + mo->height, DLIT_Model, &data);
        }
    }

    /* draw the model */

    int num_pass = data.is_fuzzy ? 1 : (detail_level > 0 ? 4 : 3);

    RGBAColor fc_to_use = mo->subsector->sector->props.fog_color;
    float    fd_to_use = mo->subsector->sector->props.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (IS_SKY(mo->subsector->sector->ceil))
        {
            fc_to_use = currmap->outdoor_fog_color;
            fd_to_use = 0.01f * currmap->outdoor_fog_density;
        }
        else
        {
            fc_to_use = currmap->indoor_fog_color;
            fd_to_use = 0.01f * currmap->indoor_fog_density;
        }
    }

    if (!r_culling.d && fc_to_use != kRGBANoValue)
    {
        GLfloat fc[4];
        fc[0] = (float)epi::GetRGBARed(fc_to_use) / 255.0f;
        fc[1] = (float)epi::GetRGBAGreen(fc_to_use) / 255.0f;
        fc[2] = (float)epi::GetRGBABlue(fc_to_use) / 255.0f;
        fc[3] = 1.0f;
        glClearColor(fc[0], fc[1], fc[2], 1.0f);
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogfv(GL_FOG_COLOR, fc);
        glFogf(GL_FOG_DENSITY, std::log1p(fd_to_use));
        glEnable(GL_FOG);
    }
    else if (r_culling.d)
    {
        sg_color fogColor;
        if (need_to_draw_sky)
        {
            switch (r_cullfog.d)
            {
            case 0:
                fogColor = cull_fog_color;
                break;
            case 1:
                // Not pure white, but 1.0f felt like a little much - Dasho
                fogColor = sg_silver;
                break;
            case 2:
                fogColor = { 0.25f, 0.25f, 0.25f, 1.0f };
                break;
            case 3:
                fogColor = sg_black;
                break;
            default:
                fogColor = cull_fog_color;
                break;
            }
        }
        else
        {
            fogColor = sg_black;
        }
        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, &fogColor.r);
        glFogf(GL_FOG_START, r_farclip.f - 750.0f);
        glFogf(GL_FOG_END, r_farclip.f - 250.0f);
        glEnable(GL_FOG);
    }
    else
        glDisable(GL_FOG);

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~BL_Alpha;
            blending |= BL_Add;
            glDisable(GL_FOG);
        }

        data.is_additive = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            UpdateMulticols(&data);
            if (MD2_MulticolMaxRGB(&data, false) <= 0)
                continue;
        }
        else if (data.is_additive)
        {
            if (MD2_MulticolMaxRGB(&data, true) <= 0)
                continue;
        }

        glPolygonOffset(0, -pass);

        if (blending & (BL_Masked | BL_Less))
        {
            if (blending & BL_Less)
            {
                glEnable(GL_ALPHA_TEST);
            }
            else if (blending & BL_Masked)
            {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0);
            }
            else
                glDisable(GL_ALPHA_TEST);
        }

        if (blending & (BL_Alpha | BL_Add))
        {
            if (blending & BL_Add)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            else if (blending & BL_Alpha)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
                glDisable(GL_BLEND);
        }

        if (blending & BL_CULL_BOTH)
        {
            if (blending & BL_CULL_BOTH)
            {
                glEnable(GL_CULL_FACE);
                glCullFace((blending & BL_CullFront) ? GL_FRONT : GL_BACK);
            }
            else
                glDisable(GL_CULL_FACE);
        }

        if (blending & BL_NoZBuf)
        {
            glDepthMask((blending & BL_NoZBuf) ? GL_FALSE : GL_TRUE);
        }

        if (blending & BL_Less)
        {
            // NOTE: assumes alpha is constant over whole model
            glAlphaFunc(GL_GREATER, trans * 0.66f);
        }

        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, skin_tex);

        if (data.is_additive)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        }
        else
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        }

        GLint old_clamp = 789;

        if (blending & BL_ClampY)
        {
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_clamp);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, r_dumbclamp.d ? GL_CLAMP : GL_CLAMP_TO_EDGE);
        }

        local_gl_vert_t *start = md->gl_verts;

        for (int i = 0; i < md->num_tris; i++)
        {
            data.tri = &md->tris[i];

            for (int v_idx = 0; v_idx < 3; v_idx++)
            {
                local_gl_vert_t *dest = start + (i * 3) + v_idx;

                ModelCoordFunc(&data, v_idx, &dest->pos, dest->rgba, &dest->texc[0], &dest->normal);

                dest->rgba[3] = trans;
            }
        }

        // setup client state
        glBindBuffer(GL_ARRAY_BUFFER, md->vbo);
        glBufferData(GL_ARRAY_BUFFER, md->num_tris * 3 * sizeof(local_gl_vert_t), md->gl_verts, GL_STREAM_DRAW);
        glVertexPointer(3, GL_FLOAT, sizeof(local_gl_vert_t), BUFFER_OFFSET(offsetof(local_gl_vert_t, pos.X)));
        glColorPointer(4, GL_FLOAT, sizeof(local_gl_vert_t), BUFFER_OFFSET(offsetof(local_gl_vert_t, rgba)));
        glNormalPointer(GL_FLOAT, sizeof(local_gl_vert_t), BUFFER_OFFSET(offsetof(local_gl_vert_t, normal.Y)));
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(local_gl_vert_t), BUFFER_OFFSET(offsetof(local_gl_vert_t, texc[0])));

        glDrawArrays(GL_TRIANGLES, 0, md->num_tris * 3);

        // restore the clamping mode
        if (old_clamp != 789)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_clamp);
    }

    gl_state_c *state = RGL_GetState();
    state->setDefaultStateFull();
}

void MD2_RenderModel_2D(md2_model_c *md, const image_c *skin_img, int frame, float x, float y, float xscale,
                        float yscale, const mobjtype_c *info)
{
    // check if frame is valid
    if (frame < 0 || frame >= md->num_frames)
        return;

    GLuint skin_tex = W_ImageCache(skin_img, false, info->palremap);

    float im_right = IM_RIGHT(skin_img);
    float im_top   = IM_TOP(skin_img);

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

    for (int i = 0; i < md->num_tris; i++)
    {
        const md2_triangle_c *tri = &md->tris[i];

        glBegin(GL_TRIANGLES);

        for (int v_idx = 0; v_idx < 3; v_idx++)
        {
            const md2_frame_c *frame_ptr = &md->frames[frame];

            SYS_ASSERT(tri->first + v_idx >= 0);
            SYS_ASSERT(tri->first + v_idx < md->num_points);

            const md2_point_c  *point = &md->points[tri->first + v_idx];
            const md2_vertex_c *vert  = &frame_ptr->vertices[point->vert_idx];

            glTexCoord2f(point->skin_s * im_right, point->skin_t * im_top);

            short n = vert->normal_idx;

            float norm_x = md_normals[n].X;
            float norm_y = md_normals[n].Y;
            float norm_z = md_normals[n].Z;

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
