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

#include "r_md2.h"

#include <stddef.h>

#include <vector>

#include "ddf_types.h"
#include "dm_state.h" // EDGE_IMAGE_IS_SKY
#include "epi.h"
#include "epi_endian.h"
#include "epi_str_compare.h"
#include "g_game.h" //current_map
#include "i_defs_gl.h"
#include "p_blockmap.h"
#include "r_colormap.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_mdcommon.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_shader.h"
#include "r_state.h"
#include "r_units.h"

extern float ApproximateDistance(float dx, float dy, float dz);

extern ConsoleVariable draw_culling;
extern ConsoleVariable cull_fog_color;
extern bool            need_to_draw_sky;

/*============== MD2 FORMAT DEFINITIONS ====================*/

// format uses float pointing values, but to allow for endianness
// conversions they are represented here as unsigned integers.

// struct member naming deviates from the style guide to reflect
// MD2 format documentation

static constexpr const char *kMD2Identifier = "IDP2";
static constexpr uint8_t     kMD2Version    = 8;

struct RawMD2Header
{
    char ident[4];

    int32_t version;

    int32_t skin_width;
    int32_t skin_height;

    int32_t frame_size;

    int32_t num_skins;
    int32_t num_vertices; // per frame
    int32_t num_st;
    int32_t total_triangles_;
    int32_t num_glcmds;
    int32_t num_frames;

    int32_t ofs_skins;
    int32_t ofs_st;
    int32_t ofs_triangles_;
    int32_t ofs_frames;
    int32_t ofs_glcmds;
    int32_t ofs_end;
};

struct RawMD2TextureCoordinate
{
    uint16_t s, t;
};

struct RawMD2Triangle
{
    uint16_t index_xyz[3];
    uint16_t index_st[3];
};

struct RawMD2Vertex
{
    uint8_t x, y, z;
    uint8_t light_normal;
};

struct RawMD2Frame
{
    uint32_t scale[3];
    uint32_t translate[3];

    char name[16];
};

struct RawMD2Skin
{
    char name[64];
};

/*============== MD3 FORMAT DEFINITIONS ====================*/

// format uses float pointing values, but to allow for endianness
// conversions they are represented here as unsigned integers.

// struct member naming deviates from the style guide to reflect
// MD3 format documentation

static constexpr const char *kMD3Identifier = "IDP3";
static constexpr uint8_t     kMD3Version    = 15;

struct RawMD3Header
{
    char    ident[4];
    int32_t version;

    char     name[64];
    uint32_t flags;

    int32_t num_frames;
    int32_t num_tags;
    int32_t num_meshes;
    int32_t num_skins;

    int32_t ofs_frames;
    int32_t ofs_tags;
    int32_t ofs_meshes;
    int32_t ofs_end;
};

struct RawMD3Mesh
{
    char ident[4];
    char name[64];

    uint32_t flags;

    int32_t num_frames;
    int32_t num_shaders;
    int32_t num_verts;
    int32_t total_triangles_;

    int32_t ofs_triangles_;
    int32_t ofs_shaders;
    int32_t ofs_texcoords; // one texcoord per vertex
    int32_t ofs_verts;
    int32_t ofs_next_mesh;
};

struct RawMD3TextureCoordinate
{
    uint32_t s, t;
};

struct RawMD3Triangle
{
    uint32_t index_xyz[3];
};

struct RawMD3Vertex
{
    int16_t x, y, z;

    uint8_t pitch, yaw;
};

struct RawMD3Frame
{
    uint32_t mins[3];
    uint32_t maxs[3];
    uint32_t origin[3];
    uint32_t radius;

    char name[16];
};

/*============== EDGE REPRESENTATION ====================*/

struct MD2Vertex
{
    float x, y, z;

    short normal_idx;
};

struct MD2Frame
{
    MD2Vertex *vertices;

    const char *name;

    // list of normals which are used.  Terminated by -1.
    short *used_normals_;
};

struct MD2Point
{
    float skin_s, skin_t;

    // index into frame's vertex array (md2_frame_c::verts)
    int vert_idx;
};

struct MD2Triangle
{
    // index to the first point (within md2_model_c::points).
    // All points for the strip are contiguous in that array.
    int first;
};

class MD2Model
{
  public:
    int total_frames_;
    int total_points_;
    int total_triangles_;

    MD2Frame    *frames_;
    MD2Point    *points_;
    MD2Triangle *triangles_;

    int vertices_per_frame_;

    GLuint vertex_buffer_object_;

    RendererVertex *gl_vertices_;

  public:
    MD2Model(int nframe, int npoint, int ntriangles_)
        : total_frames_(nframe), total_points_(npoint), total_triangles_(ntriangles_), vertices_per_frame_(0),
          vertex_buffer_object_(0), gl_vertices_(nullptr)
    {
        frames_      = new MD2Frame[total_frames_];
        points_      = new MD2Point[total_points_];
        triangles_   = new MD2Triangle[total_triangles_];
        gl_vertices_ = new RendererVertex[total_triangles_ * 3];
    }

    ~MD2Model()
    {
        delete[] frames_;
        delete[] points_;
        delete[] triangles_;
    }
};

/*============== LOADING CODE ====================*/

static const char *CopyFrameName(RawMD2Frame *frm)
{
    char *str = new char[20];

    memcpy(str, frm->name, 16);

    // ensure it is NUL terminated
    str[16] = 0;

    return str;
}

static const char *CopyFrameName(RawMD3Frame *frm)
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

    for (i = 0; i < kTotalMDFormatNormals; i++)
        if (which_normals[i])
            count++;

    short *n_list = new short[count + 1];

    count = 0;

    for (i = 0; i < kTotalMDFormatNormals; i++)
        if (which_normals[i])
            n_list[count++] = i;

    n_list[count] = -1;

    return n_list;
}

MD2Model *MD2Load(epi::File *f)
{
    int i;

    RawMD2Header header;

    /* read header */
    f->Read(&header, sizeof(RawMD2Header));

    int version = AlignedLittleEndianS32(header.version);

    LogDebug("MODEL IDENT: [%c%c%c%c] VERSION: %d", header.ident[0], header.ident[1], header.ident[2], header.ident[3],
             version);

    if (epi::StringPrefixCompare(header.ident, kMD2Identifier) != 0)
    {
        FatalError("MD2LoadModel: lump is not an MD2 model!");
        return nullptr; /* NOT REACHED */
    }

    if (version != kMD2Version)
    {
        FatalError("MD2LoadModel: strange version!");
        return nullptr; /* NOT REACHED */
    }

    int num_frames       = AlignedLittleEndianS32(header.num_frames);
    int total_triangles_ = AlignedLittleEndianS32(header.total_triangles_);
    int num_sts          = AlignedLittleEndianS32(header.num_st);
    int total_points_    = total_triangles_ * 3;

    /* PARSE TRIANGLES */

    RawMD2Triangle *md2_triangles_ = new RawMD2Triangle[total_triangles_];

    f->Seek(AlignedLittleEndianS32(header.ofs_triangles_), epi::File::kSeekpointStart);
    f->Read(md2_triangles_, total_triangles_ * sizeof(RawMD2Triangle));

    for (int tri = 0; tri < total_triangles_; tri++)
    {
        md2_triangles_[tri].index_xyz[0] = AlignedLittleEndianU16(md2_triangles_[tri].index_xyz[0]);
        md2_triangles_[tri].index_xyz[1] = AlignedLittleEndianU16(md2_triangles_[tri].index_xyz[1]);
        md2_triangles_[tri].index_xyz[2] = AlignedLittleEndianU16(md2_triangles_[tri].index_xyz[2]);
        md2_triangles_[tri].index_st[0]  = AlignedLittleEndianU16(md2_triangles_[tri].index_st[0]);
        md2_triangles_[tri].index_st[1]  = AlignedLittleEndianU16(md2_triangles_[tri].index_st[1]);
    }

    /* PARSE TEXCOORDS */

    RawMD2TextureCoordinate *md2_sts = new RawMD2TextureCoordinate[num_sts];

    f->Seek(AlignedLittleEndianS32(header.ofs_st), epi::File::kSeekpointStart);
    f->Read(md2_sts, num_sts * sizeof(RawMD2TextureCoordinate));

    for (int st = 0; st < num_sts; st++)
    {
        md2_sts[st].s = AlignedLittleEndianU16(md2_sts[st].s);
        md2_sts[st].t = AlignedLittleEndianU16(md2_sts[st].t);
    }

    LogDebug("  frames:%d  points:%d  triangles: %d\n", num_frames, total_triangles_ * 3, total_triangles_);

    MD2Model *md = new MD2Model(num_frames, total_points_, total_triangles_);

    md->vertices_per_frame_ = AlignedLittleEndianS32(header.num_vertices);

    LogDebug("  vertices_per_frame_:%d\n", md->vertices_per_frame_);

    // convert raw triangles
    MD2Triangle *tri   = md->triangles_;
    MD2Point    *point = md->points_;

    for (i = 0; i < total_triangles_; i++)
    {
        EPI_ASSERT(tri < md->triangles_ + md->total_triangles_);
        EPI_ASSERT(point < md->points_ + md->total_points_);

        tri->first = point - md->points_;

        tri++;

        for (int j = 0; j < 3; j++, point++)
        {
            RawMD2Triangle t = md2_triangles_[i];

            point->skin_s   = (float)md2_sts[t.index_st[j]].s / header.skin_width;
            point->skin_t   = 1.0f - ((float)md2_sts[t.index_st[j]].t / header.skin_height);
            point->vert_idx = t.index_xyz[j];

            EPI_ASSERT(point->vert_idx >= 0);
            EPI_ASSERT(point->vert_idx < md->vertices_per_frame_);
        }
    }

    EPI_ASSERT(tri == md->triangles_ + md->total_triangles_);
    EPI_ASSERT(point == md->points_ + md->total_points_);

    delete[] md2_triangles_;
    delete[] md2_sts;

    /* PARSE FRAMES */

    uint8_t which_normals[kTotalMDFormatNormals];

    RawMD2Vertex *raw_verts = new RawMD2Vertex[md->vertices_per_frame_];

    f->Seek(AlignedLittleEndianS32(header.ofs_frames), epi::File::kSeekpointStart);

    for (i = 0; i < num_frames; i++)
    {
        RawMD2Frame raw_frame;

        f->Read(&raw_frame, sizeof(raw_frame));

        for (int j = 0; j < 3; j++)
        {
            raw_frame.scale[j]     = AlignedLittleEndianU32(raw_frame.scale[j]);
            raw_frame.translate[j] = AlignedLittleEndianU32(raw_frame.translate[j]);
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

        md->frames_[i].name = CopyFrameName(&raw_frame);

#ifdef EDGE_DEBUG_MD2LOAD
        LogDebug("  __FRAME_%d__[%s]\n", i + 1, md->frames_[i].name);
        LogDebug("    scale: %1.2f, %1.2f, %1.2f\n", scale[0], scale[1], scale[2]);
        LogDebug("    translate: %1.2f, %1.2f, %1.2f\n", translate[0], translate[1], translate[2]);
#endif

        f->Read(raw_verts, md->vertices_per_frame_ * sizeof(RawMD2Vertex));

        md->frames_[i].vertices = new MD2Vertex[md->vertices_per_frame_];

        memset(which_normals, 0, sizeof(which_normals));

        for (int v = 0; v < md->vertices_per_frame_; v++)
        {
            RawMD2Vertex *raw_V  = raw_verts + v;
            MD2Vertex    *good_V = md->frames_[i].vertices + v;

            good_V->x = (int)raw_V->x * scale[0] + translate[0];
            good_V->y = (int)raw_V->y * scale[1] + translate[1];
            good_V->z = (int)raw_V->z * scale[2] + translate[2];

#ifdef EDGE_DEBUG_MD2LOAD
            LogDebug("    __VERT_%d__\n", v);
            LogDebug("      raw: %d,%d,%d\n", raw_V->x, raw_V->y, raw_V->z);
            LogDebug("      normal: %d\n", raw_V->light_normal);
            LogDebug("      good: %1.2f, %1.2f, %1.2f\n", good_V->x, good_V->y, good_V->z);
#endif
            good_V->normal_idx = raw_V->light_normal;

            EPI_ASSERT(good_V->normal_idx >= 0);
            // EPI_ASSERT(good_V->normal_idx < kTotalMDFormatNormals);
            //  Dasho: Maybe try to salvage bad MD2 models?
            if (good_V->normal_idx >= kTotalMDFormatNormals)
            {
                LogDebug("Vert %d of Frame %d has an invalid normal index: %d\n", v, i, good_V->normal_idx);
                good_V->normal_idx = (good_V->normal_idx % kTotalMDFormatNormals);
            }

            which_normals[good_V->normal_idx] = 1;
        }

        md->frames_[i].used_normals_ = CreateNormalList(which_normals);
    }

    delete[] raw_verts;

    glGenBuffers(1, &md->vertex_buffer_object_);
    if (md->vertex_buffer_object_ == 0)
        FatalError("MD2LoadModel: Failed to bind VBO!\n");
    glBindBuffer(GL_ARRAY_BUFFER, md->vertex_buffer_object_);
    glBufferData(GL_ARRAY_BUFFER, md->total_triangles_ * 3 * sizeof(RendererVertex), nullptr, GL_STREAM_DRAW);

    return md;
}

short MD2FindFrame(MD2Model *md, const char *name)
{
    EPI_ASSERT(strlen(name) > 0);

    for (int f = 0; f < md->total_frames_; f++)
    {
        MD2Frame *frame = &md->frames_[f];

        if (DDFCompareName(name, frame->name) == 0)
            return f;
    }

    return -1; // NOT FOUND
}

/*============== MD3 LOADING CODE ====================*/

static uint8_t md3_normal_to_md2[128][128];
static bool    md3_normal_map_built = false;

static uint8_t MD2FindNormal(float x, float y, float z)
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

static void MD3CreateNormalMap(void)
{
    // Create a table mapping MD3 normals to MD2 normals.
    // We discard the least significant bit of pitch and yaw
    // (for speed and memory saving).

    // build a sine table for even faster calcs
    float sintab[160];

    for (int i = 0; i < 160; i++)
        sintab[i] = sin(i * HMM_PI / 64.0);

    for (int pitch = 0; pitch < 128; pitch++)
    {
        uint8_t *dest = &md3_normal_to_md2[pitch][0];

        for (int yaw = 0; yaw < 128; yaw++)
        {
            float z = sintab[pitch + 32];
            float w = sintab[pitch];

            float x = w * sintab[yaw + 32];
            float y = w * sintab[yaw];

            *dest++ = MD2FindNormal(x, y, z);
        }
    }

    md3_normal_map_built = true;
}

MD2Model *MD3Load(epi::File *f)
{
    int    i;
    float *ff;

    if (!md3_normal_map_built)
        MD3CreateNormalMap();

    RawMD3Header header;

    /* read header */
    f->Read(&header, sizeof(RawMD3Header));

    int version = AlignedLittleEndianS32(header.version);

    LogDebug("MODEL IDENT: [%c%c%c%c] VERSION: %d", header.ident[0], header.ident[1], header.ident[2], header.ident[3],
             version);

    if (strncmp(header.ident, kMD3Identifier, 4) != 0)
    {
        FatalError("MD3LoadModel: lump is not an MD3 model!");
        return nullptr; /* NOT REACHED */
    }

    if (version != kMD3Version)
    {
        FatalError("MD3LoadModel: strange version!");
        return nullptr; /* NOT REACHED */
    }

    if (AlignedLittleEndianS32(header.num_meshes) > 1)
        LogWarning("Ignoring extra meshes in MD3 model.\n");

    /* LOAD MESH #1 */

    int mesh_base = AlignedLittleEndianS32(header.ofs_meshes);

    f->Seek(mesh_base, epi::File::kSeekpointStart);

    RawMD3Mesh mesh;

    f->Read(&mesh, sizeof(RawMD3Mesh));

    int num_frames       = AlignedLittleEndianS32(mesh.num_frames);
    int num_verts        = AlignedLittleEndianS32(mesh.num_verts);
    int total_triangles_ = AlignedLittleEndianS32(mesh.total_triangles_);

    LogDebug("  frames:%d  verts:%d  triangles: %d\n", num_frames, num_verts, total_triangles_);

    MD2Model *md = new MD2Model(num_frames, total_triangles_ * 3, total_triangles_);

    md->vertices_per_frame_ = num_verts;

    /* PARSE TEXCOORD */

    MD2Point *temp_TEXC = new MD2Point[num_verts];

    f->Seek(mesh_base + AlignedLittleEndianS32(mesh.ofs_texcoords), epi::File::kSeekpointStart);

    for (i = 0; i < num_verts; i++)
    {
        RawMD3TextureCoordinate texc;

        f->Read(&texc, sizeof(RawMD3TextureCoordinate));

        texc.s = AlignedLittleEndianU32(texc.s);
        texc.t = AlignedLittleEndianU32(texc.t);

        ff                  = (float *)&texc.s;
        temp_TEXC[i].skin_s = *ff;
        ff                  = (float *)&texc.t;
        temp_TEXC[i].skin_t = 1.0f - *ff;

        temp_TEXC[i].vert_idx = i;
    }

    /* PARSE TRIANGLES */

    f->Seek(mesh_base + AlignedLittleEndianS32(mesh.ofs_triangles_), epi::File::kSeekpointStart);

    for (i = 0; i < total_triangles_; i++)
    {
        RawMD3Triangle tri;

        f->Read(&tri, sizeof(RawMD3Triangle));

        int a = AlignedLittleEndianU32(tri.index_xyz[0]);
        int b = AlignedLittleEndianU32(tri.index_xyz[1]);
        int c = AlignedLittleEndianU32(tri.index_xyz[2]);

        EPI_ASSERT(a < num_verts);
        EPI_ASSERT(b < num_verts);
        EPI_ASSERT(c < num_verts);

        md->triangles_[i].first = i * 3;

        MD2Point *point = md->points_ + i * 3;

        point[0] = temp_TEXC[a];
        point[1] = temp_TEXC[b];
        point[2] = temp_TEXC[c];
    }

    delete[] temp_TEXC;

    /* PARSE VERTEX FRAMES */

    f->Seek(mesh_base + AlignedLittleEndianS32(mesh.ofs_verts), epi::File::kSeekpointStart);

    uint8_t which_normals[kTotalMDFormatNormals];

    for (i = 0; i < num_frames; i++)
    {
        md->frames_[i].vertices = new MD2Vertex[num_verts];

        memset(which_normals, 0, sizeof(which_normals));

        MD2Vertex *good_V = md->frames_[i].vertices;

        for (int v = 0; v < num_verts; v++, good_V++)
        {
            RawMD3Vertex vert;

            f->Read(&vert, sizeof(RawMD3Vertex));

            good_V->x = AlignedLittleEndianS16(vert.x) / 64.0;
            good_V->y = AlignedLittleEndianS16(vert.y) / 64.0;
            good_V->z = AlignedLittleEndianS16(vert.z) / 64.0;

            good_V->normal_idx = md3_normal_to_md2[vert.pitch >> 1][vert.yaw >> 1];

            which_normals[good_V->normal_idx] = 1;
        }

        md->frames_[i].used_normals_ = CreateNormalList(which_normals);
    }

    /* PARSE FRAME INFO */

    f->Seek(AlignedLittleEndianS32(header.ofs_frames), epi::File::kSeekpointStart);

    for (i = 0; i < num_frames; i++)
    {
        RawMD3Frame frame;

        f->Read(&frame, sizeof(RawMD3Frame));

        md->frames_[i].name = CopyFrameName(&frame);

        LogDebug("Frame %d = '%s'\n", i + 1, md->frames_[i].name);

        // TODO: load in bbox (for visibility checking)
    }
    glGenBuffers(1, &md->vertex_buffer_object_);
    if (md->vertex_buffer_object_ == 0)
        FatalError("MD3LoadModel: Failed to create VBO!\n");
    glBindBuffer(GL_ARRAY_BUFFER, md->vertex_buffer_object_);
    glBufferData(GL_ARRAY_BUFFER, md->total_triangles_ * 3 * sizeof(RendererVertex), nullptr, GL_STREAM_DRAW);
    return md;
}

/*============== MODEL RENDERING ====================*/

class MD2CoordinateData
{
  public:
    MapObject *map_object_;

    MD2Model *model_;

    const MD2Frame    *frame1_;
    const MD2Frame    *frame2_;
    const MD2Triangle *triangles_;

    float lerp_;
    float x_, y_, z_;

    bool is_weapon_;
    bool is_fuzzy_;

    // scaling
    float xy_scale_;
    float z_scale_;
    float bias_;

    // image size
    float image_right_;
    float image_top_;

    // fuzzy info
    float    fuzz_multiplier_;
    HMM_Vec2 fuzz_add_;

    // mlook vectors
    HMM_Vec2 mouselook_x_matrix_;
    HMM_Vec2 mouselook_z_matrix_;

    // rotation vectors
    HMM_Vec2 rotation_x_matrix_;
    HMM_Vec2 rotation_y_matrix_;

    ColorMixer normal_colors_[kTotalMDFormatNormals];

    short *used_normals_;

    bool is_additive_;

  public:
    void CalcPos(HMM_Vec3 *pos, float x1, float y1, float z1) const
    {
        x1 *= xy_scale_;
        y1 *= xy_scale_;
        z1 *= z_scale_;

        float x2 = x1 * mouselook_x_matrix_.X + z1 * mouselook_x_matrix_.Y;
        float z2 = x1 * mouselook_z_matrix_.X + z1 * mouselook_z_matrix_.Y;
        float y2 = y1;

        pos->X = x_ + x2 * rotation_x_matrix_.X + y2 * rotation_x_matrix_.Y;
        pos->Y = y_ + x2 * rotation_y_matrix_.X + y2 * rotation_y_matrix_.Y;
        pos->Z = z_ + z2;
    }

    void CalcNormal(HMM_Vec3 *normal, const MD2Vertex *vert) const
    {
        short n = vert->normal_idx;

        float nx1 = md_normals[n].X;
        float ny1 = md_normals[n].Y;
        float nz1 = md_normals[n].Z;

        float nx2 = nx1 * mouselook_x_matrix_.X + nz1 * mouselook_x_matrix_.Y;
        float nz2 = nx1 * mouselook_z_matrix_.X + nz1 * mouselook_z_matrix_.Y;
        float ny2 = ny1;

        normal->X = nx2 * rotation_x_matrix_.X + ny2 * rotation_x_matrix_.Y;
        normal->Y = nx2 * rotation_y_matrix_.X + ny2 * rotation_y_matrix_.Y;
        normal->Z = nz2;
    }
};

static void InitNormalColors(MD2CoordinateData *data)
{
    short *n_list = data->used_normals_;

    for (; *n_list >= 0; n_list++)
    {
        data->normal_colors_[*n_list].Clear();
    }
}

static void ShadeNormals(AbstractShader *shader, MD2CoordinateData *data, bool skip_calc)
{
    short *n_list = data->used_normals_;

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

            float nx2 = nx1 * data->mouselook_x_matrix_.X + nz1 * data->mouselook_x_matrix_.Y;
            float nz2 = nx1 * data->mouselook_z_matrix_.X + nz1 * data->mouselook_z_matrix_.Y;
            float ny2 = ny1;

            nx = nx2 * data->rotation_x_matrix_.X + ny2 * data->rotation_x_matrix_.Y;
            ny = nx2 * data->rotation_y_matrix_.X + ny2 * data->rotation_y_matrix_.Y;
            nz = nz2;
        }

        shader->Corner(data->normal_colors_ + n, nx, ny, nz, data->map_object_, data->is_weapon_);
    }
}

static void DLIT_Model(MapObject *mo, void *dataptr)
{
    MD2CoordinateData *data = (MD2CoordinateData *)dataptr;

    // dynamic lights do not light themselves up!
    if (mo == data->map_object_)
        return;

    EPI_ASSERT(mo->dynamic_light_.shader);

    ShadeNormals(mo->dynamic_light_.shader, data, false);
}

static int MD2MulticolMaxRGB(MD2CoordinateData *data, bool additive)
{
    int result = 0;

    short *n_list = data->used_normals_;

    for (; *n_list >= 0; n_list++)
    {
        ColorMixer *col = &data->normal_colors_[*n_list];

        int mx = additive ? col->add_MAX() : col->mod_MAX();

        result = HMM_MAX(result, mx);
    }

    return result;
}

static void UpdateMulticols(MD2CoordinateData *data)
{
    short *n_list = data->used_normals_;

    for (; *n_list >= 0; n_list++)
    {
        ColorMixer *col = &data->normal_colors_[*n_list];

        col->modulate_red_ -= 256;
        col->modulate_green_ -= 256;
        col->modulate_blue_ -= 256;
    }
}

static inline float LerpIt(float v1, float v2, float lerp)
{
    return v1 * (1.0f - lerp) + v2 * lerp;
}

static inline void ModelCoordFunc(MD2CoordinateData *data, int v_idx, HMM_Vec3 *pos, float *rgb, HMM_Vec2 *texc,
                                  HMM_Vec3 *normal)
{
    const MD2Model *md = data->model_;

    const MD2Frame    *frame1 = data->frame1_;
    const MD2Frame    *frame2 = data->frame2_;
    const MD2Triangle *tri    = data->triangles_;

    EPI_ASSERT(tri->first + v_idx >= 0);
    EPI_ASSERT(tri->first + v_idx < md->total_points_);

    const MD2Point *point = &md->points_[tri->first + v_idx];

    const MD2Vertex *vert1 = &frame1->vertices[point->vert_idx];
    const MD2Vertex *vert2 = &frame2->vertices[point->vert_idx];

    float x1 = LerpIt(vert1->x, vert2->x, data->lerp_);
    float y1 = LerpIt(vert1->y, vert2->y, data->lerp_);
    float z1 = LerpIt(vert1->z, vert2->z, data->lerp_) + data->bias_;

    if (MirrorReflective())
        y1 = -y1;

    data->CalcPos(pos, x1, y1, z1);

    const MD2Vertex *n_vert = (data->lerp_ < 0.5) ? vert1 : vert2;

    data->CalcNormal(normal, n_vert);

    if (data->is_fuzzy_)
    {
        texc->X = point->skin_s * data->fuzz_multiplier_ + data->fuzz_add_.X;
        texc->Y = point->skin_t * data->fuzz_multiplier_ + data->fuzz_add_.Y;

        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    *texc = {{point->skin_s * data->image_right_, point->skin_t * data->image_top_}};

    ColorMixer *col = &data->normal_colors_[n_vert->normal_idx];

    if (!data->is_additive_)
    {
        rgb[0] = col->modulate_red_ / 255.0;
        rgb[1] = col->modulate_green_ / 255.0;
        rgb[2] = col->modulate_blue_ / 255.0;
    }
    else
    {
        rgb[0] = col->add_red_ / 255.0;
        rgb[1] = col->add_green_ / 255.0;
        rgb[2] = col->add_blue_ / 255.0;
    }

    rgb[0] *= render_view_red_multiplier;
    rgb[1] *= render_view_green_multiplier;
    rgb[2] *= render_view_blue_multiplier;
}

void MD2RenderModel(MD2Model *md, const Image *skin_img, bool is_weapon_, int frame1, int frame2, float lerp, float x,
                    float y, float z, MapObject *mo, RegionProperties *props, float scale, float aspect, float bias_,
                    int rotation)
{
    // check if frames are valid
    if (frame1 < 0 || frame1 >= md->total_frames_)
    {
        LogDebug("Render model: bad frame %d\n", frame1);
        return;
    }
    if (frame2 < 0 || frame2 >= md->total_frames_)
    {
        LogDebug("Render model: bad frame %d\n", frame1);
        return;
    }

    MD2CoordinateData data;

    data.is_fuzzy_ = (mo->flags_ & kMapObjectFlagFuzzy) ? true : false;

    float trans = mo->visibility_;

    if (trans <= 0)
        return;

    int blending;

    if (trans >= 0.99f && skin_img->opacity_ == kOpacitySolid)
        blending = kBlendingNone;
    else if (trans < 0.11f || skin_img->opacity_ == kOpacityComplex)
        blending = kBlendingMasked;
    else
        blending = kBlendingLess;

    if (trans < 0.99f || skin_img->opacity_ == kOpacityComplex)
        blending |= kBlendingAlpha;

    if (mo->hyper_flags_ & kHyperFlagNoZBufferUpdate)
        blending |= kBlendingNoZBuffer;

    if (MirrorReflective())
        blending |= kBlendingCullFront;
    else
        blending |= kBlendingCullBack;

    data.map_object_ = mo;
    data.model_      = md;

    data.frame1_ = &md->frames_[frame1];
    data.frame2_ = &md->frames_[frame2];

    data.lerp_ = lerp;

    data.x_ = x;
    data.y_ = y;
    data.z_ = z;

    data.is_weapon_ = is_weapon_;

    data.xy_scale_ = scale * aspect * MirrorXYScale();
    data.z_scale_  = scale * MirrorZScale();
    data.bias_     = bias_;

    bool tilt = is_weapon_ || (mo->flags_ & kMapObjectFlagMissile) || (mo->hyper_flags_ & kHyperFlagForceModelTilt);

    BAMAngleToMatrix(tilt ? ~mo->vertical_angle_ : 0, &data.mouselook_x_matrix_, &data.mouselook_z_matrix_);

    BAMAngle ang = mo->angle_ + rotation;

    MirrorAngle(ang);

    BAMAngleToMatrix(~ang, &data.rotation_x_matrix_, &data.rotation_y_matrix_);

    data.used_normals_ = (lerp < 0.5) ? data.frame1_->used_normals_ : data.frame2_->used_normals_;

    InitNormalColors(&data);

    GLuint skin_tex = 0;

    if (data.is_fuzzy_)
    {
        skin_tex = ImageCache(fuzz_image, false);

        data.fuzz_multiplier_ = 0.8;
        data.fuzz_add_        = {{0, 0}};

        data.image_right_ = 1.0;
        data.image_top_   = 1.0;

        if (!data.is_weapon_ && !view_is_zoomed)
        {
            float dist = ApproximateDistance(mo->x - view_x, mo->y - view_y, mo->z - view_z);

            data.fuzz_multiplier_ = 70.0 / HMM_Clamp(35, dist, 700);
        }

        FuzzAdjust(&data.fuzz_add_, mo);

        trans = 1.0f;

        blending |= kBlendingAlpha | kBlendingMasked;
        blending &= ~kBlendingLess;
    }
    else /* (! data.is_fuzzy_) */
    {
        skin_tex = ImageCache(skin_img, false,
                              render_view_effect_colormap ? render_view_effect_colormap
                              : is_weapon_                ? nullptr
                                                          : mo->info_->palremap_);

        data.image_right_ = skin_img->Right();
        data.image_top_   = skin_img->Top();

        AbstractShader *shader = GetColormapShader(props, mo->state_->bright, mo->subsector_->sector);

        ShadeNormals(shader, &data, true);

        if (use_dynamic_lights && render_view_extra_light < 250)
        {
            float r = mo->radius_;

            DynamicLightIterator(mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r, mo->z + mo->height_, DLIT_Model,
                                 &data);

            SectorGlowIterator(mo->subsector_->sector, mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r,
                               mo->z + mo->height_, DLIT_Model, &data);
        }
    }

    /* draw the model */

    int num_pass = data.is_fuzzy_ ? 1 : (detail_level > 0 ? 4 : 3);

    RGBAColor fc_to_use = mo->subsector_->sector->properties.fog_color;
    float     fd_to_use = mo->subsector_->sector->properties.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (EDGE_IMAGE_IS_SKY(mo->subsector_->sector->ceiling))
        {
            fc_to_use = current_map->outdoor_fog_color_;
            fd_to_use = 0.01f * current_map->outdoor_fog_density_;
        }
        else
        {
            fc_to_use = current_map->indoor_fog_color_;
            fd_to_use = 0.01f * current_map->indoor_fog_density_;
        }
    }

    if (!draw_culling.d_ && fc_to_use != kRGBANoValue)
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
    else if (draw_culling.d_)
    {
        sg_color fogColor;
        if (need_to_draw_sky)
        {
            switch (cull_fog_color.d_)
            {
            case 0:
                fogColor = culling_fog_color;
                break;
            case 1:
                // Not pure white, but 1.0f felt like a little much - Dasho
                fogColor = sg_silver;
                break;
            case 2:
                fogColor = {0.25f, 0.25f, 0.25f, 1.0f};
                break;
            case 3:
                fogColor = sg_black;
                break;
            default:
                fogColor = culling_fog_color;
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
        glFogf(GL_FOG_START, renderer_far_clip.f_ - 750.0f);
        glFogf(GL_FOG_END, renderer_far_clip.f_ - 250.0f);
        glEnable(GL_FOG);
    }
    else
        glDisable(GL_FOG);

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~kBlendingAlpha;
            blending |= kBlendingAdd;
            glDisable(GL_FOG);
        }

        data.is_additive_ = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            UpdateMulticols(&data);
            if (MD2MulticolMaxRGB(&data, false) <= 0)
                continue;
        }
        else if (data.is_additive_)
        {
            if (MD2MulticolMaxRGB(&data, true) <= 0)
                continue;
        }

        glPolygonOffset(0, -pass);

        if (blending & (kBlendingMasked | kBlendingLess))
        {
            if (blending & kBlendingLess)
            {
                glEnable(GL_ALPHA_TEST);
            }
            else if (blending & kBlendingMasked)
            {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0);
            }
            else
                glDisable(GL_ALPHA_TEST);
        }

        if (blending & (kBlendingAlpha | kBlendingAdd))
        {
            if (blending & kBlendingAdd)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            else if (blending & kBlendingAlpha)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
                glDisable(GL_BLEND);
        }

        if (blending & (kBlendingCullBack | kBlendingCullFront))
        {
            if (blending & (kBlendingCullBack | kBlendingCullFront))
            {
                glEnable(GL_CULL_FACE);
                glCullFace((blending & kBlendingCullFront) ? GL_FRONT : GL_BACK);
            }
            else
                glDisable(GL_CULL_FACE);
        }

        if (blending & kBlendingNoZBuffer)
        {
            glDepthMask((blending & kBlendingNoZBuffer) ? GL_FALSE : GL_TRUE);
        }

        if (blending & kBlendingLess)
        {
            // NOTE: assumes alpha is constant over whole model
            glAlphaFunc(GL_GREATER, trans * 0.66f);
        }

        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, skin_tex);

        if (data.is_additive_)
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

        if (blending & kBlendingClampY)
        {
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_clamp);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE);
        }

        RendererVertex *start = md->gl_vertices_;

        for (int i = 0; i < md->total_triangles_; i++)
        {
            data.triangles_ = &md->triangles_[i];

            for (int v_idx = 0; v_idx < 3; v_idx++)
            {
                RendererVertex *dest = start + (i * 3) + v_idx;

                ModelCoordFunc(&data, v_idx, &dest->position, dest->rgba_color, &dest->texture_coordinates[0],
                               &dest->normal);

                dest->rgba_color[3] = trans;
            }
        }

        // setup client state
        glBindBuffer(GL_ARRAY_BUFFER, md->vertex_buffer_object_);
        glBufferData(GL_ARRAY_BUFFER, md->total_triangles_ * 3 * sizeof(RendererVertex), md->gl_vertices_,
                     GL_STREAM_DRAW);
        glVertexPointer(3, GL_FLOAT, sizeof(RendererVertex), (void *)(offsetof(RendererVertex, position.X)));
        glColorPointer(4, GL_FLOAT, sizeof(RendererVertex), (void *)(offsetof(RendererVertex, rgba_color)));
        glNormalPointer(GL_FLOAT, sizeof(RendererVertex), (void *)(offsetof(RendererVertex, normal.Y)));
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(RendererVertex),
                          (void *)(offsetof(RendererVertex, texture_coordinates[0])));

        glDrawArrays(GL_TRIANGLES, 0, md->total_triangles_ * 3);

        // restore the clamping mode
        if (old_clamp != 789)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_clamp);
    }

    RenderState *state = GetRenderState();
    state->SetDefaultStateFull();
}

void MD2RenderModel2D(MD2Model *md, const Image *skin_img, int frame, float x, float y, float xscale, float yscale,
                      const MapObjectDefinition *info)
{
    // check if frame is valid
    if (frame < 0 || frame >= md->total_frames_)
        return;

    GLuint skin_tex = ImageCache(skin_img, false, info->palremap_);

    float im_right = skin_img->Right();
    float im_top   = skin_img->Top();

    xscale = yscale * info->model_scale_ * info->model_aspect_;
    yscale = yscale * info->model_scale_;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, skin_tex);

    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    if (info->flags_ & kMapObjectFlagFuzzy)
        glColor4f(0, 0, 0, 0.5f);
    else
        glColor4f(1, 1, 1, 1.0f);

    for (int i = 0; i < md->total_triangles_; i++)
    {
        const MD2Triangle *tri = &md->triangles_[i];

        glBegin(GL_TRIANGLES);

        for (int v_idx = 0; v_idx < 3; v_idx++)
        {
            const MD2Frame *frame_ptr = &md->frames_[frame];

            EPI_ASSERT(tri->first + v_idx >= 0);
            EPI_ASSERT(tri->first + v_idx < md->total_points_);

            const MD2Point  *point = &md->points_[tri->first + v_idx];
            const MD2Vertex *vert  = &frame_ptr->vertices[point->vert_idx];

            glTexCoord2f(point->skin_s * im_right, point->skin_t * im_top);

            short n = vert->normal_idx;

            float norm_x = md_normals[n].X;
            float norm_y = md_normals[n].Y;
            float norm_z = md_normals[n].Z;

            glNormal3f(norm_y, norm_z, norm_x);

            float dx = vert->x * xscale;
            float dy = vert->y * xscale;
            float dz = (vert->z + info->model_bias_) * yscale;

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
