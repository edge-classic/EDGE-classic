//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2023  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
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
//------------------------------------------------------------------------

#include "bsp_local.h"
#include "bsp_raw_def.h"
#include "bsp_utility.h"
#include "bsp_wad.h"

// EPI
#include "endianess.h"
#include "math_crc.h"
#include "str_lexer.h"

#include "miniz.h"

#include <algorithm>

#define DEBUG_BLOCKMAP 0
#define DEBUG_REJECT   0

#define DEBUG_LOAD 0
#define DEBUG_BSP  0

// Startup Messages
extern void E_ProgressMessage(const char *message);

namespace ajbsp
{

enum UDMFTypes
{
    kUDMFThing   = 1,
    kUDMFVertex  = 2,
    kUDMFSector  = 3,
    kUDMFSidedef = 4,
    kUDMFLinedef = 5
};

WadFile *cur_wad;
WadFile *xwa_wad;

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------

// Note: ZDoom format support based on code (C) 2002,2003 Randy Heit

// per-level variables

const char *level_current_name;

int level_current_idx;
int level_current_start;

MapFormat level_format;

bool level_force_v5;
bool level_force_xnod;

bool level_long_name;
bool level_overflows;

// objects of loaded level, and stuff we've built
std::vector<Vertex *>  level_vertices;
std::vector<Linedef *> level_linedefs;
std::vector<Sidedef *> level_sidedefs;
std::vector<Sector *>  level_sectors;
std::vector<Thing *>   level_things;

std::vector<Seg *>     level_segs;
std::vector<Subsector *>  level_subsecs;
std::vector<Node *>    level_nodes;
std::vector<WallTip *> level_walltips;

int num_old_vert   = 0;
int num_new_vert   = 0;
int num_real_lines = 0;

/* ----- allocation routines ---------------------------- */

Vertex *NewVertex()
{
    Vertex *V = (Vertex *)UtilCalloc(sizeof(Vertex));
    V->index_    = (int)level_vertices.size();
    level_vertices.push_back(V);
    return V;
}

Linedef *NewLinedef()
{
    Linedef *L = (Linedef *)UtilCalloc(sizeof(Linedef));
    L->index     = (int)level_linedefs.size();
    level_linedefs.push_back(L);
    return L;
}

Sidedef *NewSidedef()
{
    Sidedef *S = (Sidedef *)UtilCalloc(sizeof(Sidedef));
    S->index     = (int)level_sidedefs.size();
    level_sidedefs.push_back(S);
    return S;
}

Sector *NewSector()
{
    Sector *S = (Sector *)UtilCalloc(sizeof(Sector));
    S->index    = (int)level_sectors.size();
    level_sectors.push_back(S);
    return S;
}

Thing *NewThing()
{
    Thing *T = (Thing *)UtilCalloc(sizeof(Thing));
    T->index   = (int)level_things.size();
    level_things.push_back(T);
    return T;
}

Seg *NewSeg()
{
    Seg *S = (Seg *)UtilCalloc(sizeof(Seg));
    level_segs.push_back(S);
    return S;
}

Subsector *NewSubsec()
{
    Subsector *S = (Subsector *)UtilCalloc(sizeof(Subsector));
    level_subsecs.push_back(S);
    return S;
}

Node *NewNode()
{
    Node *N = (Node *)UtilCalloc(sizeof(Node));
    level_nodes.push_back(N);
    return N;
}

WallTip *NewWallTip()
{
    WallTip *WT = (WallTip *)UtilCalloc(sizeof(WallTip));
    level_walltips.push_back(WT);
    return WT;
}

/* ----- free routines ---------------------------- */

void FreeVertices()
{
    for (unsigned int i = 0; i < level_vertices.size(); i++)
        UtilFree((void *)level_vertices[i]);

    level_vertices.clear();
}

void FreeLinedefs()
{
    for (unsigned int i = 0; i < level_linedefs.size(); i++)
        UtilFree((void *)level_linedefs[i]);

    level_linedefs.clear();
}

void FreeSidedefs()
{
    for (unsigned int i = 0; i < level_sidedefs.size(); i++)
        UtilFree((void *)level_sidedefs[i]);

    level_sidedefs.clear();
}

void FreeSectors()
{
    for (unsigned int i = 0; i < level_sectors.size(); i++)
        UtilFree((void *)level_sectors[i]);

    level_sectors.clear();
}

void FreeThings()
{
    for (unsigned int i = 0; i < level_things.size(); i++)
        UtilFree((void *)level_things[i]);

    level_things.clear();
}

void FreeSegs()
{
    for (unsigned int i = 0; i < level_segs.size(); i++)
        UtilFree((void *)level_segs[i]);

    level_segs.clear();
}

void FreeSubsecs()
{
    for (unsigned int i = 0; i < level_subsecs.size(); i++)
        UtilFree((void *)level_subsecs[i]);

    level_subsecs.clear();
}

void FreeNodes()
{
    for (unsigned int i = 0; i < level_nodes.size(); i++)
        UtilFree((void *)level_nodes[i]);

    level_nodes.clear();
}

void FreeWallTips()
{
    for (unsigned int i = 0; i < level_walltips.size(); i++)
        UtilFree((void *)level_walltips[i]);

    level_walltips.clear();
}

/* ----- reading routines ------------------------------ */

static Vertex *SafeLookupVertex(int num)
{
    if (num >= level_vertices.size())
        I_Error("AJBSP: illegal vertex number #%d\n", num);

    return level_vertices[num];
}

static Sector *SafeLookupSector(uint16_t num)
{
    if (num == 0xFFFF)
        return NULL;

    if (num >= level_sectors.size())
        I_Error("AJBSP: illegal sector number #%d\n", (int)num);

    return level_sectors[num];
}

static inline Sidedef *SafeLookupSidedef(uint16_t num)
{
    if (num == 0xFFFF)
        return NULL;

    // silently ignore illegal sidedef numbers
    if (num >= (unsigned int)level_sidedefs.size())
        return NULL;

    return level_sidedefs[num];
}

void GetVertices()
{
    int count = 0;

    Lump *lump = FindLevelLump("VERTEXES");

    if (lump)
        count = lump->Length() / (int)sizeof(RawVertex);

#if DEBUG_LOAD
    I_Debugf("GetVertices: num = %d\n", count);
#endif

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to vertices.\n");

    for (int i = 0; i < count; i++)
    {
        RawVertex raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading vertices.\n");

        Vertex *vert = NewVertex();

        vert->x_ = (double)AlignedLittleEndianS16(raw.x);
        vert->y_ = (double)AlignedLittleEndianS16(raw.y);
    }

    num_old_vert = level_vertices.size();
}

void GetSectors()
{
    int count = 0;

    Lump *lump = FindLevelLump("SECTORS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawSector);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to sectors.\n");

#if DEBUG_LOAD
    I_Debugf("GetSectors: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawSector raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading sectors.\n");

        Sector *sector = NewSector();

        (void)sector;
    }
}

void GetThings()
{
    int count = 0;

    Lump *lump = FindLevelLump("THINGS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawThing);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to things.\n");

#if DEBUG_LOAD
    I_Debugf("GetThings: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawThing raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading things.\n");

        Thing *thing = NewThing();

        thing->x    = AlignedLittleEndianS16(raw.x);
        thing->y    = AlignedLittleEndianS16(raw.y);
        thing->type = AlignedLittleEndianU16(raw.type);
    }
}

void GetThingsHexen()
{
    int count = 0;

    Lump *lump = FindLevelLump("THINGS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawHexenThing);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to things.\n");

#if DEBUG_LOAD
    I_Debugf("GetThingsHexen: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawHexenThing raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading things.\n");

        Thing *thing = NewThing();

        thing->x    = AlignedLittleEndianS16(raw.x);
        thing->y    = AlignedLittleEndianS16(raw.y);
        thing->type = AlignedLittleEndianU16(raw.type);
    }
}

void GetSidedefs()
{
    int count = 0;

    Lump *lump = FindLevelLump("SIDEDEFS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawSidedef);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to sidedefs.\n");

#if DEBUG_LOAD
    I_Debugf("GetSidedefs: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawSidedef raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading sidedefs.\n");

        Sidedef *side = NewSidedef();

        side->sector = SafeLookupSector(AlignedLittleEndianS16(raw.sector));
    }
}

void GetLinedefs()
{
    int count = 0;

    Lump *lump = FindLevelLump("LINEDEFS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawLinedef);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to linedefs.\n");

#if DEBUG_LOAD
    I_Debugf("GetLinedefs: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawLinedef raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading linedefs.\n");

        Linedef *line;

        Vertex *start = SafeLookupVertex(AlignedLittleEndianU16(raw.start));
        Vertex *end   = SafeLookupVertex(AlignedLittleEndianU16(raw.end));

        start->is_used_ = true;
        end->is_used_   = true;

        line = NewLinedef();

        line->start = start;
        line->end   = end;

        // check for zero-length line
        line->zero_length = (fabs(start->x_ - end->x_) < DIST_EPSILON) && (fabs(start->y_ - end->y_) < DIST_EPSILON);

        line->type  = AlignedLittleEndianU16(raw.type);
        uint16_t flags = AlignedLittleEndianU16(raw.flags);
        int16_t tag   = AlignedLittleEndianS16(raw.tag);

        line->two_sided   = (flags & kLineFlagTwoSided) != 0;
        line->is_precious = (tag >= 900 && tag < 1000); // Why is this the case? Need to investigate - Dasho

        line->right = SafeLookupSidedef(AlignedLittleEndianU16(raw.right));
        line->left  = SafeLookupSidedef(AlignedLittleEndianU16(raw.left));

        if (line->right || line->left)
            num_real_lines++;

        line->self_referencing = (line->left && line->right && (line->left->sector == line->right->sector));

        if (line->self_referencing)
            line->is_precious = true;
    }
}

void GetLinedefsHexen()
{
    int count = 0;

    Lump *lump = FindLevelLump("LINEDEFS");

    if (lump)
        count = lump->Length() / (int)sizeof(RawHexenLinedef);

    if (lump == NULL || count == 0)
        return;

    if (!lump->Seek(0))
        I_Error("AJBSP: Error seeking to linedefs.\n");

#if DEBUG_LOAD
    I_Debugf("GetLinedefsHexen: num = %d\n", count);
#endif

    for (int i = 0; i < count; i++)
    {
        RawHexenLinedef raw;

        if (!lump->Read(&raw, sizeof(raw)))
            I_Error("AJBSP: Error reading linedefs.\n");

        Linedef *line;

        Vertex *start = SafeLookupVertex(AlignedLittleEndianU16(raw.start));
        Vertex *end   = SafeLookupVertex(AlignedLittleEndianU16(raw.end));

        start->is_used_ = true;
        end->is_used_   = true;

        line = NewLinedef();

        line->start = start;
        line->end   = end;

        // check for zero-length line
        line->zero_length = (fabs(start->x_ - end->x_) < DIST_EPSILON) && (fabs(start->y_ - end->y_) < DIST_EPSILON);

        line->type  = (uint8_t)raw.type;
        uint16_t flags = AlignedLittleEndianU16(raw.flags);

        // -JL- Added missing twosided flag handling that caused a broken reject
        line->two_sided = (flags & kLineFlagTwoSided) != 0;

        line->right = SafeLookupSidedef(AlignedLittleEndianU16(raw.right));
        line->left  = SafeLookupSidedef(AlignedLittleEndianU16(raw.left));

        if (line->right || line->left)
            num_real_lines++;

        line->self_referencing = (line->left && line->right && (line->left->sector == line->right->sector));

        if (line->self_referencing)
            line->is_precious = true;
    }
}

static inline int VanillaSegDist(const Seg *seg)
{
    double lx = seg->side_ ? seg->linedef_->end->x_ : seg->linedef_->start->x_;
    double ly = seg->side_ ? seg->linedef_->end->y_ : seg->linedef_->start->y_;

    // use the "true" starting coord (as stored in the wad)
    double sx = round(seg->start_->x_);
    double sy = round(seg->start_->y_);

    return (int)floor(hypot(sx - lx, sy - ly) + 0.5);
}

static inline int VanillaSegAngle(const Seg *seg)
{
    // compute the "true" delta
    double dx = round(seg->end_->x_) - round(seg->start_->x_);
    double dy = round(seg->end_->y_) - round(seg->start_->y_);

    double angle = ComputeAngle(dx, dy);

    if (angle < 0)
        angle += 360.0;

    int result = (int)floor(angle * 65536.0 / 360.0 + 0.5);

    return (result & 0xFFFF);
}

/* ----- UDMF reading routines ------------------------- */

void ParseThingField(Thing *thing, const std::string &key, const std::string &value)
{
    // Do we need more precision than an int for things? I think this would only be
    // an issue if/when polyobjects happen, as I think other thing types are ignored - Dasho

    if (key == "x")
        thing->x = I_ROUND(epi::LexDouble(value));

    if (key == "y")
        thing->y = I_ROUND(epi::LexDouble(value));

    if (key == "type")
        thing->type = epi::LexInteger(value);
}

void ParseVertexField(Vertex *vertex, const std::string &key, const std::string &value)
{
    if (key == "x")
        vertex->x_ = epi::LexDouble(value);

    if (key == "y")
        vertex->y_ = epi::LexDouble(value);
}

void ParseSidedefField(Sidedef *side, const std::string &key, const std::string &value)
{
    if (key == "sector")
    {
        int num = epi::LexInteger(value);

        if (num < 0 || num >= level_sectors.size())
            I_Error("AJBSP: illegal sector number #%d\n", (int)num);

        side->sector = level_sectors[num];
    }
}

void ParseLinedefField(Linedef *line, const std::string &key, const std::string &value)
{
    if (key == "v1")
        line->start = SafeLookupVertex(epi::LexInteger(value));

    if (key == "v2")
        line->end = SafeLookupVertex(epi::LexInteger(value));

    if (key == "special")
        line->type = epi::LexInteger(value);

    if (key == "twosided")
        line->two_sided = epi::LexBoolean(value);

    if (key == "sidefront")
    {
        int num = epi::LexInteger(value);

        if (num < 0 || num >= (int)level_sidedefs.size())
            line->right = NULL;
        else
            line->right = level_sidedefs[num];
    }

    if (key == "sideback")
    {
        int num = epi::LexInteger(value);

        if (num < 0 || num >= (int)level_sidedefs.size())
            line->left = NULL;
        else
            line->left = level_sidedefs[num];
    }
}

void ParseUDMF_Block(epi::Lexer &lex, int cur_type)
{
    Vertex  *vertex = NULL;
    Thing   *thing  = NULL;
    Sidedef *side   = NULL;
    Linedef *line   = NULL;

    switch (cur_type)
    {
    case kUDMFVertex:
        vertex = NewVertex();
        break;
    case kUDMFThing:
        thing = NewThing();
        break;
    case kUDMFSector:
        NewSector(); // We don't use the returned pointer in this function
        break;
    case kUDMFSidedef:
        side = NewSidedef();
        break;
    case kUDMFLinedef:
        line = NewLinedef();
        break;
    default:
        break;
    }

    for (;;)
    {
        if (lex.Match("}"))
            break;

        std::string key;
        std::string value;

        epi::TokenKind tok = lex.Next(key);

        if (tok == epi::kTokenEOF)
            I_Error("AJBSP: Malformed TEXTMAP lump: unclosed block\n");

        if (tok != epi::kTokenIdentifier)
            I_Error("AJBSP: Malformed TEXTMAP lump: missing key\n");

        if (!lex.Match("="))
            I_Error("AJBSP: Malformed TEXTMAP lump: missing '='\n");

        tok = lex.Next(value);

        if (tok == epi::kTokenEOF || tok == epi::kTokenError || value == "}")
            I_Error("AJBSP: Malformed TEXTMAP lump: missing value\n");

        if (!lex.Match(";"))
            I_Error("AJBSP: Malformed TEXTMAP lump: missing ';'\n");

        switch (cur_type)
        {
        case kUDMFVertex:
            ParseVertexField(vertex, key, value);
            break;
        case kUDMFThing:
            ParseThingField(thing, key, value);
            break;
        case kUDMFSidedef:
            ParseSidedefField(side, key, value);
            break;
        case kUDMFLinedef:
            ParseLinedefField(line, key, value);
            break;

        case kUDMFSector:
        default: /* just skip it */
            break;
        }
    }

    // validate stuff

    if (line != NULL)
    {
        if (line->start == NULL || line->end == NULL)
            I_Error("AJBSP: Linedef #%d is missing a vertex!\n", line->index);

        if (line->right || line->left)
            num_real_lines++;

        line->self_referencing = (line->left && line->right && (line->left->sector == line->right->sector));

        if (line->self_referencing)
            line->is_precious = true;
    }
}

void ParseUDMF_Pass(const std::string &data, int pass)
{
    // pass = 1 : vertices, sectors, things
    // pass = 2 : sidedefs
    // pass = 3 : linedefs

    epi::Lexer lex(data);

    for (;;)
    {
        std::string       section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF)
            return;

        if (tok != epi::kTokenIdentifier)
        {
            I_Error("AJBSP: Malformed TEXTMAP lump.\n");
            return;
        }

        // ignore top-level assignments
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                I_Error("AJBSP: Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            I_Error("AJBSP: Malformed TEXTMAP lump: missing '{'\n");

        int cur_type = 0;

        if (section == "thing")
        {
            if (pass == 1)
                cur_type = kUDMFThing;
        }
        else if (section == "vertex")
        {
            if (pass == 1)
                cur_type = kUDMFVertex;
        }
        else if (section == "sector")
        {
            if (pass == 1)
                cur_type = kUDMFSector;
        }
        else if (section == "sidedef")
        {
            if (pass == 2)
                cur_type = kUDMFSidedef;
        }
        else if (section == "linedef")
        {
            if (pass == 3)
                cur_type = kUDMFLinedef;
        }

        // process the block
        ParseUDMF_Block(lex, cur_type);
    }
}

void ParseUDMF()
{
    Lump *lump = FindLevelLump("TEXTMAP");

    if (lump == NULL || !lump->Seek(0))
        I_Error("AJBSP: Error finding TEXTMAP lump.\n");

    // load the lump into this string
    std::string data(lump->Length(), 0);
    if (!lump->Read(data.data(), lump->Length()))
        I_Error("AJBSP: Error reading TEXTMAP lump.\n");

    // now parse it...

    // the UDMF spec does not require objects to be in a dependency order.
    // for example: sidedefs may occur *after* the linedefs which refer to
    // them.  hence we perform multiple passes over the TEXTMAP data.

    ParseUDMF_Pass(data, 1);
    ParseUDMF_Pass(data, 2);
    ParseUDMF_Pass(data, 3);

    num_old_vert = level_vertices.size();
}

/* ----- writing routines ------------------------------ */

static const uint8_t *level_v2_magic = (uint8_t *)"gNd2";
static const uint8_t *level_v5_magic = (uint8_t *)"gNd5";

void MarkOverflow()
{
    level_overflows = true;
}

void PutVertices(const char *name, int do_gl)
{
    int count, i;

    // this size is worst-case scenario
    int size = level_vertices.size() * (int)sizeof(RawVertex);

    Lump *lump = CreateLevelLump(name, size);

    for (i = 0, count = 0; i < level_vertices.size(); i++)
    {
        RawVertex raw;

        const Vertex *vert = level_vertices[i];

        if ((do_gl ? 1 : 0) != (vert->is_new_ ? 1 : 0))
        {
            continue;
        }

        raw.x = AlignedLittleEndianS16(I_ROUND(vert->x_));
        raw.y = AlignedLittleEndianS16(I_ROUND(vert->y_));

        lump->Write(&raw, sizeof(raw));

        count++;
    }

    lump->Finish();

    if (count != (do_gl ? num_new_vert : num_old_vert))
        I_Error("AJBSP: PutVertices miscounted (%d != %d)\n", count, do_gl ? num_new_vert : num_old_vert);

    if (!do_gl && count > 65534)
    {
        I_Printf("Number of vertices has overflowed.\n");
        MarkOverflow();
    }
}

void PutGLVertices(int do_v5)
{
    int count, i;

    // this size is worst-case scenario
    int size = 4 + level_vertices.size() * (int)sizeof(RawV2Vertex);

    Lump *lump = CreateLevelLump("GL_VERT", size);

    if (do_v5)
        lump->Write(level_v5_magic, 4);
    else
        lump->Write(level_v2_magic, 4);

    for (i = 0, count = 0; i < level_vertices.size(); i++)
    {
        RawV2Vertex raw;

        const Vertex *vert = level_vertices[i];

        if (!vert->is_new_)
            continue;

        raw.x = AlignedLittleEndianS32(I_ROUND(vert->x_ * 65536.0));
        raw.y = AlignedLittleEndianS32(I_ROUND(vert->y_ * 65536.0));

        lump->Write(&raw, sizeof(raw));

        count++;
    }

    lump->Finish();

    if (count != num_new_vert)
        I_Error("AJBSP: PutGLVertices miscounted (%d != %d)\n", count, num_new_vert);
}

static inline uint16_t VertexIndex16Bit(const Vertex *v)
{
    if (v->is_new_)
        return (uint16_t)(v->index_ | 0x8000U);

    return (uint16_t)v->index_;
}

static inline uint32_t VertexIndex_V5(const Vertex *v)
{
    if (v->is_new_)
        return (uint32_t)(v->index_ | 0x80000000U);

    return (uint32_t)v->index_;
}

static inline uint32_t VertexIndex_XNOD(const Vertex *v)
{
    if (v->is_new_)
        return (uint32_t)(num_old_vert + v->index_);

    return (uint32_t)v->index_;
}

void PutSegs()
{
    // this size is worst-case scenario
    int size = level_segs.size() * (int)sizeof(RawSeg);

    Lump *lump = CreateLevelLump("SEGS", size);

    for (int i = 0; i < level_segs.size(); i++)
    {
        RawSeg raw;

        const Seg *seg = level_segs[i];

        raw.start   = AlignedLittleEndianU16(VertexIndex16Bit(seg->start_));
        raw.end     = AlignedLittleEndianU16(VertexIndex16Bit(seg->end_));
        raw.angle   = AlignedLittleEndianU16(VanillaSegAngle(seg));
        raw.linedef = AlignedLittleEndianU16(seg->linedef_->index);
        raw.flip    = AlignedLittleEndianU16(seg->side_);
        raw.dist    = AlignedLittleEndianU16(VanillaSegDist(seg));

        lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
        I_Debugf("PUT SEG: %04X  Vert %04X->%04X  Line %04X %s  "
                        "Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                        seg->index, AlignedLittleEndianU16(raw.start), AlignedLittleEndianU16(raw.end), AlignedLittleEndianU16(raw.linedef), seg->side ? "L" : "R",
                        AlignedLittleEndianU16(raw.angle), seg->start->x_, seg->start->y_, seg->end->x_, seg->end->y_);
#endif
    }

    lump->Finish();

    if (level_segs.size() > 65534)
    {
        I_Printf("Number of segs has overflowed.\n");
        MarkOverflow();
    }
}

void PutGLSegs_V2()
{
    // should not happen (we should have upgraded to V5)
    SYS_ASSERT(level_segs.size() <= 65534);

    // this size is worst-case scenario
    int size = level_segs.size() * (int)sizeof(RawGLSeg);

    Lump *lump = CreateLevelLump("GL_SEGS", size);

    for (int i = 0; i < level_segs.size(); i++)
    {
        RawGLSeg raw;

        const Seg *seg = level_segs[i];

        raw.start = AlignedLittleEndianU16(VertexIndex16Bit(seg->start_));
        raw.end   = AlignedLittleEndianU16(VertexIndex16Bit(seg->end_));
        raw.side  = AlignedLittleEndianU16(seg->side_);

        if (seg->linedef_ != NULL)
            raw.linedef = AlignedLittleEndianU16(seg->linedef_->index);
        else
            raw.linedef = AlignedLittleEndianU16(0xFFFF);

        if (seg->partner_ != NULL)
            raw.partner = AlignedLittleEndianU16(seg->partner_->index_);
        else
            raw.partner = AlignedLittleEndianU16(0xFFFF);

        lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
        I_Debugf("PUT GL SEG: %04X  Line %04X %s  Partner %04X  "
                        "(%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                        seg->index, AlignedLittleEndianU16(raw.linedef), seg->side ? "L" : "R", AlignedLittleEndianU16(raw.partner), seg->start->x_,
                        seg->start->y_, seg->end->x_, seg->end->y_);
#endif
    }

    lump->Finish();
}

void PutGLSegs_V5()
{
    // this size is worst-case scenario
    int size = level_segs.size() * (int)sizeof(RawV5Seg);

    Lump *lump = CreateLevelLump("GL_SEGS", size);

    for (int i = 0; i < level_segs.size(); i++)
    {
        RawV5Seg raw;

        const Seg *seg = level_segs[i];

        raw.start = AlignedLittleEndianU32(VertexIndex_V5(seg->start_));
        raw.end   = AlignedLittleEndianU32(VertexIndex_V5(seg->end_));
        raw.side  = AlignedLittleEndianU16(seg->side_);

        if (seg->linedef_ != NULL)
            raw.linedef = AlignedLittleEndianU16(seg->linedef_->index);
        else
            raw.linedef = AlignedLittleEndianU16(0xFFFF);

        if (seg->partner_ != NULL)
            raw.partner = AlignedLittleEndianU32(seg->partner_->index_);
        else
            raw.partner = AlignedLittleEndianU32(0xFFFFFFFF);

        lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
        I_Debugf("PUT V3 SEG: %06X  Line %04X %s  Partner %06X  "
                        "(%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                        seg->index, AlignedLittleEndianU16(raw.linedef), seg->side ? "L" : "R", AlignedLittleEndianU32(raw.partner), seg->start->x_,
                        seg->start->y_, seg->end->x_, seg->end->y_);
#endif
    }

    lump->Finish();
}

void PutSubsecs(const char *name, int do_gl)
{
    int size = level_subsecs.size() * (int)sizeof(RawSubsector);

    Lump *lump = CreateLevelLump(name, size);

    for (int i = 0; i < level_subsecs.size(); i++)
    {
        RawSubsector raw;

        const Subsector *sub = level_subsecs[i];

        raw.first = AlignedLittleEndianU16(sub->seg_list_->index_);
        raw.num   = AlignedLittleEndianU16(sub->seg_count_);

        lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
        I_Debugf("PUT SUBSEC %04X  First %04X  Num %04X\n", sub->index, AlignedLittleEndianU16(raw.first), AlignedLittleEndianU16(raw.num));
#endif
    }

    if (level_subsecs.size() > 32767)
    {
        I_Printf("Number of %s has overflowed.\n", do_gl ? "GL subsectors" : "subsectors");
        MarkOverflow();
    }

    lump->Finish();
}

void PutGLSubsecs_V5()
{
    int size = level_subsecs.size() * (int)sizeof(RawV5Subsector);

    Lump *lump = CreateLevelLump("GL_SSECT", size);

    for (int i = 0; i < level_subsecs.size(); i++)
    {
        RawV5Subsector raw;

        const Subsector *sub = level_subsecs[i];

        raw.first = AlignedLittleEndianU32(sub->seg_list_->index_);
        raw.num   = AlignedLittleEndianU32(sub->seg_count_);

        lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
        I_Debugf("PUT V3 SUBSEC %06X  First %06X  Num %06X\n", sub->index, AlignedLittleEndianU32(raw.first), AlignedLittleEndianU32(raw.num));
#endif
    }

    lump->Finish();
}

static int node_cur_index;

static void PutOneNode(Node *node, Lump *lump)
{
    if (node->r_.node)
        PutOneNode(node->r_.node, lump);

    if (node->l_.node)
        PutOneNode(node->l_.node, lump);

    node->index_ = node_cur_index++;

    RawNode raw;

    // note that x/y/dx/dy are always integral in non-UDMF maps
    raw.x  = AlignedLittleEndianS16(I_ROUND(node->x_));
    raw.y  = AlignedLittleEndianS16(I_ROUND(node->y_));
    raw.dx = AlignedLittleEndianS16(I_ROUND(node->dx_));
    raw.dy = AlignedLittleEndianS16(I_ROUND(node->dy_));

    raw.b1.minx = AlignedLittleEndianS16(node->r_.bounds.minx);
    raw.b1.miny = AlignedLittleEndianS16(node->r_.bounds.miny);
    raw.b1.maxx = AlignedLittleEndianS16(node->r_.bounds.maxx);
    raw.b1.maxy = AlignedLittleEndianS16(node->r_.bounds.maxy);

    raw.b2.minx = AlignedLittleEndianS16(node->l_.bounds.minx);
    raw.b2.miny = AlignedLittleEndianS16(node->l_.bounds.miny);
    raw.b2.maxx = AlignedLittleEndianS16(node->l_.bounds.maxx);
    raw.b2.maxy = AlignedLittleEndianS16(node->l_.bounds.maxy);

    if (node->r_.node)
        raw.right = AlignedLittleEndianU16(node->r_.node->index_);
    else if (node->r_.subsec)
        raw.right = AlignedLittleEndianU16(node->r_.subsec->index_ | 0x8000);
    else
        I_Error("AJBSP: Bad right child in node %d\n", node->index_);

    if (node->l_.node)
        raw.left = AlignedLittleEndianU16(node->l_.node->index_);
    else if (node->l_.subsec)
        raw.left = AlignedLittleEndianU16(node->l_.subsec->index_ | 0x8000);
    else
        I_Error("AJBSP: Bad left child in node %d\n", node->index_);

    lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
    I_Debugf("PUT NODE %04X  Left %04X  Right %04X  "
                    "(%d,%d) -> (%d,%d)\n",
                    node->index, AlignedLittleEndianU16(raw.left), AlignedLittleEndianU16(raw.right), node->x_, node->y_, node->x_ + node->dx_,
                    node->y_ + node->dy_);
#endif
}

static void PutOneNode_V5(Node *node, Lump *lump)
{
    if (node->r_.node)
        PutOneNode_V5(node->r_.node, lump);

    if (node->l_.node)
        PutOneNode_V5(node->l_.node, lump);

    node->index_ = node_cur_index++;

    RawV5Node raw;

    raw.x  = AlignedLittleEndianS16(I_ROUND(node->x_));
    raw.y  = AlignedLittleEndianS16(I_ROUND(node->y_));
    raw.dx = AlignedLittleEndianS16(I_ROUND(node->dx_));
    raw.dy = AlignedLittleEndianS16(I_ROUND(node->dy_));

    raw.b1.minx = AlignedLittleEndianS16(node->r_.bounds.minx);
    raw.b1.miny = AlignedLittleEndianS16(node->r_.bounds.miny);
    raw.b1.maxx = AlignedLittleEndianS16(node->r_.bounds.maxx);
    raw.b1.maxy = AlignedLittleEndianS16(node->r_.bounds.maxy);

    raw.b2.minx = AlignedLittleEndianS16(node->l_.bounds.minx);
    raw.b2.miny = AlignedLittleEndianS16(node->l_.bounds.miny);
    raw.b2.maxx = AlignedLittleEndianS16(node->l_.bounds.maxx);
    raw.b2.maxy = AlignedLittleEndianS16(node->l_.bounds.maxy);

    if (node->r_.node)
        raw.right = AlignedLittleEndianU32(node->r_.node->index_);
    else if (node->r_.subsec)
        raw.right = AlignedLittleEndianU32(node->r_.subsec->index_ | 0x80000000U);
    else
        I_Error("AJBSP: Bad right child in V5 node %d\n", node->index_);

    if (node->l_.node)
        raw.left = AlignedLittleEndianU32(node->l_.node->index_);
    else if (node->l_.subsec)
        raw.left = AlignedLittleEndianU32(node->l_.subsec->index_ | 0x80000000U);
    else
        I_Error("AJBSP: Bad left child in V5 node %d\n", node->index_);

    lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
    I_Debugf("PUT V5 NODE %08X  Left %08X  Right %08X  "
                    "(%d,%d) -> (%d,%d)\n",
                    node->index, AlignedLittleEndianU32(raw.left), AlignedLittleEndianU32(raw.right), node->x_, node->y_, node->x_ + node->dx_,
                    node->y_ + node->dy_);
#endif
}

void PutNodes(const char *name, int do_v5, Node *root)
{
    int struct_size = do_v5 ? (int)sizeof(RawV5Node) : (int)sizeof(RawNode);

    // this can be bigger than the actual size, but never smaller
    int max_size = (level_nodes.size() + 1) * struct_size;

    Lump *lump = CreateLevelLump(name, max_size);

    node_cur_index = 0;

    if (root != NULL)
    {
        if (do_v5)
            PutOneNode_V5(root, lump);
        else
            PutOneNode(root, lump);
    }

    lump->Finish();

    if (node_cur_index != level_nodes.size())
        I_Error("AJBSP: PutNodes miscounted (%d != %d)\n", node_cur_index, level_nodes.size());

    if (!do_v5 && node_cur_index > 32767)
    {
        I_Printf("Number of nodes has overflowed.\n");
        MarkOverflow();
    }
}

void CheckLimits()
{
    // this could potentially be 65536, since there are no reserved values
    // for sectors, but there may be source ports or tools treating 0xFFFF
    // as a special value, so we are extra cautious here (and in some of
    // the other checks below, like the vertex counts).
    if (level_sectors.size() > 65535)
    {
        I_Printf("Map has too many sectors.\n");
        MarkOverflow();
    }
    // the sidedef 0xFFFF is reserved to mean "no side" in DOOM map format
    if (level_sidedefs.size() > 65535)
    {
        I_Printf("Map has too many sidedefs.\n");
        MarkOverflow();
    }
    // the linedef 0xFFFF is reserved for minisegs in GL nodes
    if (level_linedefs.size() > 65535)
    {
        I_Printf("Map has too many linedefs.\n");
        MarkOverflow();
    }

    if (current_build_info.gl_nodes && !current_build_info.force_v5)
    {
        if (num_old_vert > 32767 || num_new_vert > 32767 || level_segs.size() > 65535 || level_nodes.size() > 32767)
        {
            I_Printf("Forcing V5 of GL-Nodes due to overflows.\n");
            current_build_info.total_warnings++;
            level_force_v5 = true;
        }
    }

    if (!current_build_info.force_xnod)
    {
        if (num_old_vert > 32767 || num_new_vert > 32767 || level_segs.size() > 32767 || level_nodes.size() > 32767)
        {
            I_Printf("Forcing XNOD format nodes due to overflows.\n");
            current_build_info.total_warnings++;
            level_force_xnod = true;
        }
    }
}

struct CompareSegPredicate
{
    inline bool operator()(const Seg *A, const Seg *B) const
    {
        return A->index_ < B->index_;
    }
};

void SortSegs()
{
    // do a sanity check
    for (int i = 0; i < level_segs.size(); i++)
        if (level_segs[i]->index_ < 0)
            I_Error("AJBSP: Seg %p never reached a subsector!\n", i);

    // sort segs into ascending index
    std::sort(level_segs.begin(), level_segs.end(), CompareSegPredicate());

    // remove unwanted segs
    while (level_segs.size() > 0 && level_segs.back()->index_ == kSegIsGarbage)
    {
        UtilFree((void *)level_segs.back());
        level_segs.pop_back();
    }
}

/* ----- ZDoom format writing --------------------------- */

static const uint8_t *level_XNOD_magic = (uint8_t *)"XNOD";
static const uint8_t *level_XGL3_magic = (uint8_t *)"XGL3";
static const uint8_t *level_ZGL3_magic = (uint8_t *)"ZGL3";
static const uint8_t *level_ZNOD_magic = (uint8_t *)"ZNOD";

void PutZVertices()
{
    int count, i;

    uint32_t orgverts = AlignedLittleEndianU32(num_old_vert);
    uint32_t newverts = AlignedLittleEndianU32(num_new_vert);

    ZLibAppendLump(&orgverts, 4);
    ZLibAppendLump(&newverts, 4);

    for (i = 0, count = 0; i < level_vertices.size(); i++)
    {
        RawV2Vertex raw;

        const Vertex *vert = level_vertices[i];

        if (!vert->is_new_)
            continue;

        raw.x = AlignedLittleEndianS32(I_ROUND(vert->x_ * 65536.0));
        raw.y = AlignedLittleEndianS32(I_ROUND(vert->y_ * 65536.0));

        ZLibAppendLump(&raw, sizeof(raw));

        count++;
    }

    if (count != num_new_vert)
        I_Error("AJBSP: PutZVertices miscounted (%d != %d)\n", count, num_new_vert);
}

void PutZSubsecs()
{
    uint32_t Rawnum = AlignedLittleEndianU32(level_subsecs.size());
    ZLibAppendLump(&Rawnum, 4);

    int cur_seg_index = 0;

    for (int i = 0; i < level_subsecs.size(); i++)
    {
        const Subsector *sub = level_subsecs[i];

        Rawnum = AlignedLittleEndianU32(sub->seg_count_);
        ZLibAppendLump(&Rawnum, 4);

        // sanity check the seg index values
        int count = 0;
        for (const Seg *seg = sub->seg_list_; seg; seg = seg->next_, cur_seg_index++)
        {
            if (cur_seg_index != seg->index_)
                I_Error("AJBSP: PutZSubsecs: seg index mismatch in sub %d (%d != %d)\n", i, cur_seg_index, seg->index_);

            count++;
        }

        if (count != sub->seg_count_)
            I_Error("AJBSP: PutZSubsecs: miscounted segs in sub %d (%d != %d)\n", i, count, sub->seg_count_);
    }

    if (cur_seg_index != level_segs.size())
        I_Error("AJBSP: PutZSubsecs miscounted segs (%d != %d)\n", cur_seg_index, level_segs.size());
}

void PutZSegs()
{
    uint32_t Rawnum = AlignedLittleEndianU32(level_segs.size());
    ZLibAppendLump(&Rawnum, 4);

    for (int i = 0; i < level_segs.size(); i++)
    {
        const Seg *seg = level_segs[i];

        if (seg->index_ != i)
            I_Error("AJBSP: PutZSegs: seg index mismatch (%d != %d)\n", seg->index_, i);

        uint32_t v1 = AlignedLittleEndianU32(VertexIndex_XNOD(seg->start_));
        uint32_t v2 = AlignedLittleEndianU32(VertexIndex_XNOD(seg->end_));

        uint16_t line = AlignedLittleEndianU16(seg->linedef_->index);
        uint8_t  side = (uint8_t)seg->side_;

        ZLibAppendLump(&v1, 4);
        ZLibAppendLump(&v2, 4);
        ZLibAppendLump(&line, 2);
        ZLibAppendLump(&side, 1);
    }
}

void PutXGL3Segs()
{
    uint32_t Rawnum = AlignedLittleEndianU32(level_segs.size());
    ZLibAppendLump(&Rawnum, 4);

    for (int i = 0; i < level_segs.size(); i++)
    {
        const Seg *seg = level_segs[i];

        if (seg->index_ != i)
            I_Error("AJBSP: PutXGL3Segs: seg index mismatch (%d != %d)\n", seg->index_, i);

        uint32_t v1      = AlignedLittleEndianU32(VertexIndex_XNOD(seg->start_));
        uint32_t partner = AlignedLittleEndianU32(seg->partner_ ? seg->partner_->index_ : -1);
        uint32_t line    = AlignedLittleEndianU32(seg->linedef_ ? seg->linedef_->index : -1);
        uint8_t  side    = (uint8_t)seg->side_;

        ZLibAppendLump(&v1, 4);
        ZLibAppendLump(&partner, 4);
        ZLibAppendLump(&line, 4);
        ZLibAppendLump(&side, 1);

#if DEBUG_BSP
        fprintf(stderr, "SEG[%d] v1=%d partner=%d line=%d side=%d\n", i, v1, partner, line, side);
#endif
    }
}

static void PutOneZNode(Node *node, bool do_xgl3)
{
    RawV5Node raw;

    if (node->r_.node)
        PutOneZNode(node->r_.node, do_xgl3);

    if (node->l_.node)
        PutOneZNode(node->l_.node, do_xgl3);

    node->index_ = node_cur_index++;

    if (do_xgl3)
    {
        uint32_t x  = AlignedLittleEndianS32(I_ROUND(node->x_ * 65536.0));
        uint32_t y  = AlignedLittleEndianS32(I_ROUND(node->y_ * 65536.0));
        uint32_t dx = AlignedLittleEndianS32(I_ROUND(node->dx_ * 65536.0));
        uint32_t dy = AlignedLittleEndianS32(I_ROUND(node->dy_ * 65536.0));

        ZLibAppendLump(&x, 4);
        ZLibAppendLump(&y, 4);
        ZLibAppendLump(&dx, 4);
        ZLibAppendLump(&dy, 4);
    }
    else
    {
        raw.x  = AlignedLittleEndianS16(I_ROUND(node->x_));
        raw.y  = AlignedLittleEndianS16(I_ROUND(node->y_));
        raw.dx = AlignedLittleEndianS16(I_ROUND(node->dx_));
        raw.dy = AlignedLittleEndianS16(I_ROUND(node->dy_));

        ZLibAppendLump(&raw.x, 2);
        ZLibAppendLump(&raw.y, 2);
        ZLibAppendLump(&raw.dx, 2);
        ZLibAppendLump(&raw.dy, 2);
    }

    raw.b1.minx = AlignedLittleEndianS16(node->r_.bounds.minx);
    raw.b1.miny = AlignedLittleEndianS16(node->r_.bounds.miny);
    raw.b1.maxx = AlignedLittleEndianS16(node->r_.bounds.maxx);
    raw.b1.maxy = AlignedLittleEndianS16(node->r_.bounds.maxy);

    raw.b2.minx = AlignedLittleEndianS16(node->l_.bounds.minx);
    raw.b2.miny = AlignedLittleEndianS16(node->l_.bounds.miny);
    raw.b2.maxx = AlignedLittleEndianS16(node->l_.bounds.maxx);
    raw.b2.maxy = AlignedLittleEndianS16(node->l_.bounds.maxy);

    ZLibAppendLump(&raw.b1, sizeof(raw.b1));
    ZLibAppendLump(&raw.b2, sizeof(raw.b2));

    if (node->r_.node)
        raw.right = AlignedLittleEndianU32(node->r_.node->index_);
    else if (node->r_.subsec)
        raw.right = AlignedLittleEndianU32(node->r_.subsec->index_ | 0x80000000U);
    else
        I_Error("AJBSP: Bad right child in V5 node %d\n", node->index_);

    if (node->l_.node)
        raw.left = AlignedLittleEndianU32(node->l_.node->index_);
    else if (node->l_.subsec)
        raw.left = AlignedLittleEndianU32(node->l_.subsec->index_ | 0x80000000U);
    else
        I_Error("AJBSP: Bad left child in V5 node %d\n", node->index_);

    ZLibAppendLump(&raw.right, 4);
    ZLibAppendLump(&raw.left, 4);

#if DEBUG_BSP
    I_Debugf("PUT Z NODE %08X  Left %08X  Right %08X  "
                    "(%d,%d) -> (%d,%d)\n",
                    node->index, AlignedLittleEndianU32(raw.left), AlignedLittleEndianU32(raw.right), node->x_, node->y_, node->x_ + node->dx_,
                    node->y_ + node->dy_);
#endif
}

void PutZNodes(Node *root, bool do_xgl3)
{
    uint32_t Rawnum = AlignedLittleEndianU32(level_nodes.size());
    ZLibAppendLump(&Rawnum, 4);

    node_cur_index = 0;

    if (root)
        PutOneZNode(root, do_xgl3);

    if (node_cur_index != level_nodes.size())
        I_Error("AJBSP: PutZNodes miscounted (%d != %d)\n", node_cur_index, level_nodes.size());
}

static int CalcZDoomNodesSize()
{
    // compute size of the ZDoom format nodes.
    // it does not need to be exact, but it *does* need to be bigger
    // (or equal) to the actual size of the lump.

    int size = 32; // header + a bit extra

    size += 8 + level_vertices.size() * 8;
    size += 4 + level_subsecs.size() * 4;
    size += 4 + level_segs.size() * 11;
    size += 4 + level_nodes.size() * sizeof(RawV5Node);

    if (current_build_info.force_compress)
    {
        // according to RFC1951, the zlib compression worst-case
        // scenario is 5 extra bytes per 32KB (0.015% increase).
        // we are significantly more conservative!

        size += ((size + 255) >> 5);
    }

    return size;
}

void SaveZDFormat(Node *root_node)
{
    // leave SEGS and SSECTORS empty
    CreateLevelLump("SEGS")->Finish();
    CreateLevelLump("SSECTORS")->Finish();

    int max_size = CalcZDoomNodesSize();

    Lump *lump = CreateLevelLump("NODES", max_size);

    if (current_build_info.force_compress)
        lump->Write(level_ZNOD_magic, 4);
    else
        lump->Write(level_XNOD_magic, 4);

    // the ZLibXXX functions do no compression for XNOD format
    ZLibBeginLump(lump);

    PutZVertices();
    PutZSubsecs();
    PutZSegs();
    PutZNodes(root_node, false);

    ZLibFinishLump();
}

void SaveXGL3Format(Lump *lump, Node *root_node)
{
    // WISH : compute a max_size

    if (current_build_info.force_compress)
        lump->Write(level_ZGL3_magic, 4);
    else
        lump->Write(level_XGL3_magic, 4);

    ZLibBeginLump(lump);

    PutZVertices();
    PutZSubsecs();
    PutXGL3Segs();
    PutZNodes(root_node, true /* do_xgl3 */);

    ZLibFinishLump();
}

/* ----- whole-level routines --------------------------- */

void LoadLevel()
{
    Lump *LEV = cur_wad->GetLump(level_current_start);

    level_current_name = LEV->Name();
    level_long_name    = false;
    level_overflows    = false;

    E_ProgressMessage(StringPrintf("Building nodes for %s\n", level_current_name));

    num_new_vert   = 0;
    num_real_lines = 0;

    if (level_format == kMapFormatUDMF)
    {
        ParseUDMF();
    }
    else
    {
        GetVertices();
        GetSectors();
        GetSidedefs();

        if (level_format == kMapFormatHexen)
        {
            GetLinedefsHexen();
            GetThingsHexen();
        }
        else
        {
            GetLinedefs();
            GetThings();
        }

        // always prune vertices at end of lump, otherwise all the
        // unused vertices from seg splits would keep accumulating.
        PruneVerticesAtEnd();
    }

    I_Debugf("    Loaded %d vertices, %d sectors, %d sides, %d lines, %d things\n", level_vertices.size(), level_sectors.size(),
                    level_sidedefs.size(), level_linedefs.size(), level_things.size());

    DetectOverlappingVertices();
    DetectOverlappingLines();

    CalculateWallTips();

    // -JL- Find sectors containing polyobjs
    switch (level_format)
    {
    case kMapFormatHexen:
        DetectPolyobjSectors(false);
        break;
    case kMapFormatUDMF:
        DetectPolyobjSectors(true);
        break;
    default:
        break;
    }
}

void FreeLevel()
{
    FreeVertices();
    FreeSidedefs();
    FreeLinedefs();
    FreeSectors();
    FreeThings();
    FreeSegs();
    FreeSubsecs();
    FreeNodes();
    FreeWallTips();
    FreeIntersections();
}

static uint32_t CalcGLChecksum(void)
{
    epi::CRC32 crc;

    Lump *lump = FindLevelLump("VERTEXES");

    if (lump && lump->Length() > 0)
    {
        uint8_t *data = new uint8_t[lump->Length()];

        if (!lump->Seek(0) || !lump->Read(data, lump->Length()))
            I_Error("AJBSP: Error reading vertices (for checksum).\n");

        crc.AddBlock(data, lump->Length());
        delete[] data;
    }

    lump = FindLevelLump("LINEDEFS");

    if (lump && lump->Length() > 0)
    {
        uint8_t *data = new uint8_t[lump->Length()];

        if (!lump->Seek(0) || !lump->Read(data, lump->Length()))
            I_Error("AJBSP: Error reading linedefs (for checksum).\n");

        crc.AddBlock(data, lump->Length());
        delete[] data;
    }

    return crc.GetCRC();
}

void UpdateGLMarker(Lump *marker)
{
    // this is very conservative, around 4 times the actual size
    const int max_size = 512;

    // we *must* compute the checksum BEFORE (re)creating the lump
    // [ otherwise we write data into the wrong part of the file ]
    uint32_t crc = CalcGLChecksum();

    cur_wad->RecreateLump(marker, max_size);

    if (level_long_name)
    {
        marker->Printf("LEVEL=%s\n", level_current_name);
    }

    marker->Printf("BUILDER=AJBSP %s\n", kAJBSPVersion);
    marker->Printf("CHECKSUM=0x%08x\n", crc);

    marker->Finish();
}

static void AddMissingLump(const char *name, const char *after)
{
    if (cur_wad->LevelLookupLump(level_current_idx, name) >= 0)
        return;

    int exist = cur_wad->LevelLookupLump(level_current_idx, after);

    // if this happens, the level structure is very broken
    if (exist < 0)
    {
        I_Printf("Missing %s lump -- level structure is broken\n", after);
        current_build_info.total_warnings++;
        exist = cur_wad->LevelLastLump(level_current_idx);
    }

    cur_wad->InsertPoint(exist + 1);

    cur_wad->AddLump(name)->Finish();
}

BuildResult SaveLevel(Node *root_node)
{
    // Note: root_node may be NULL

    cur_wad->BeginWrite();

    // remove any existing GL-Nodes
    cur_wad->RemoveGLNodes(level_current_idx);

    // ensure all necessary level lumps are present
    AddMissingLump("SEGS", "VERTEXES");
    AddMissingLump("SSECTORS", "SEGS");
    AddMissingLump("NODES", "SSECTORS");
    AddMissingLump("REJECT", "SECTORS");
    AddMissingLump("BLOCKMAP", "REJECT");

    // user preferences
    level_force_v5   = current_build_info.force_v5;
    level_force_xnod = current_build_info.force_xnod;

    // check for overflows...
    // this sets the force_xxx vars if certain limits are breached
    CheckLimits();

    /* --- GL Nodes --- */

    Lump *gl_marker = NULL;

    if (current_build_info.gl_nodes && num_real_lines > 0)
    {
        // this also removes minisegs and degenerate segs
        SortSegs();

        // create empty marker now, flesh it out later
        gl_marker = CreateGLMarker();

        PutGLVertices(level_force_v5);

        if (level_force_v5)
            PutGLSegs_V5();
        else
            PutGLSegs_V2();

        if (level_force_v5)
            PutGLSubsecs_V5();
        else
            PutSubsecs("GL_SSECT", true);

        PutNodes("GL_NODES", level_force_v5, root_node);

        // -JL- Add empty PVS lump
        CreateLevelLump("GL_PVS")->Finish();
    }

    /* --- Normal nodes --- */

    // remove all the mini-segs from subsectors
    NormaliseBspTree();

    if (level_force_xnod && num_real_lines > 0)
    {
        SortSegs();
        SaveZDFormat(root_node);
    }
    else
    {
        // reduce vertex precision for classic DOOM nodes.
        // some segs can become "degenerate" after this, and these
        // are removed from subsectors.
        RoundOffBspTree();

        SortSegs();

        PutVertices("VERTEXES", false);

        PutSegs();
        PutSubsecs("SSECTORS", false);
        PutNodes("NODES", false, root_node);
    }

    // keyword support (v5.0 of the specs).
    // must be done *after* doing normal nodes, for proper checksum.
    if (gl_marker)
    {
        UpdateGLMarker(gl_marker);
    }

    cur_wad->EndWrite();

    if (level_overflows)
    {
        // no message here
        // [ in verbose mode, each overflow already printed a message ]
        // [ in normal mode, we don't want any messages at all ]

        return kBuildLumpOverflow;
    }

    return kBuildOK;
}

BuildResult SaveUDMF(Node *root_node)
{
    cur_wad->BeginWrite();

    // remove any existing ZNODES lump
    cur_wad->RemoveZNodes(level_current_idx);

    Lump *lump = CreateLevelLump("ZNODES", -1);

    if (num_real_lines == 0)
    {
        lump->Finish();
    }
    else
    {
        SortSegs();
        SaveXGL3Format(lump, root_node);
    }

    cur_wad->EndWrite();

    return kBuildOK;
}

BuildResult SaveXWA(Node *root_node)
{
    xwa_wad->BeginWrite();

    const char *level_name = GetLevelName(level_current_idx);
    Lump     *lump     = xwa_wad->AddLump(level_name);

    if (num_real_lines == 0)
    {
        lump->Finish();
    }
    else
    {
        SortSegs();
        SaveXGL3Format(lump, root_node);
    }

    xwa_wad->EndWrite();

    return kBuildOK;
}

//----------------------------------------------------------------------

static Lump *zout_lump;

static z_stream zout_stream;
static Bytef    zout_buffer[1024];

void ZLibBeginLump(Lump *lump)
{
    zout_lump = lump;

    if (!current_build_info.force_compress)
        return;

    zout_stream.zalloc = (alloc_func)0;
    zout_stream.zfree  = (free_func)0;
    zout_stream.opaque = (voidpf)0;

    if (Z_OK != deflateInit(&zout_stream, Z_DEFAULT_COMPRESSION))
        I_Error("AJBSP: Trouble setting up zlib compression\n");

    zout_stream.next_out  = zout_buffer;
    zout_stream.avail_out = sizeof(zout_buffer);
}

void ZLibAppendLump(const void *data, int length)
{
    // ASSERT(zout_lump)
    // ASSERT(length > 0)

    if (!current_build_info.force_compress)
    {
        zout_lump->Write(data, length);
        return;
    }

    zout_stream.next_in  = (Bytef *)data; // const override
    zout_stream.avail_in = length;

    while (zout_stream.avail_in > 0)
    {
        int err = deflate(&zout_stream, Z_NO_FLUSH);

        if (err != Z_OK)
            I_Error("AJBSP: Trouble compressing %d bytes (zlib)\n", length);

        if (zout_stream.avail_out == 0)
        {
            zout_lump->Write(zout_buffer, sizeof(zout_buffer));

            zout_stream.next_out  = zout_buffer;
            zout_stream.avail_out = sizeof(zout_buffer);
        }
    }
}

void ZLibFinishLump(void)
{
    if (!current_build_info.force_compress)
    {
        zout_lump->Finish();
        zout_lump = NULL;
        return;
    }

    int left_over;

    // ASSERT(zout_stream.avail_out > 0)

    zout_stream.next_in  = Z_NULL;
    zout_stream.avail_in = 0;

    for (;;)
    {
        int err = deflate(&zout_stream, Z_FINISH);

        if (err == Z_STREAM_END)
            break;

        if (err != Z_OK)
            I_Error("AJBSP: Trouble finishing compression (zlib)\n");

        if (zout_stream.avail_out == 0)
        {
            zout_lump->Write(zout_buffer, sizeof(zout_buffer));

            zout_stream.next_out  = zout_buffer;
            zout_stream.avail_out = sizeof(zout_buffer);
        }
    }

    left_over = sizeof(zout_buffer) - zout_stream.avail_out;

    if (left_over > 0)
        zout_lump->Write(zout_buffer, left_over);

    deflateEnd(&zout_stream);

    zout_lump->Finish();
    zout_lump = NULL;
}

/* ---------------------------------------------------------------- */

Lump *FindLevelLump(const char *name)
{
    int idx = cur_wad->LevelLookupLump(level_current_idx, name);

    if (idx < 0)
        return NULL;

    return cur_wad->GetLump(idx);
}

Lump *CreateLevelLump(const char *name, int max_size)
{
    // look for existing one
    Lump *lump = FindLevelLump(name);

    if (lump)
    {
        cur_wad->RecreateLump(lump, max_size);
    }
    else
    {
        int last_idx = cur_wad->LevelLastLump(level_current_idx);

        // in UDMF maps, insert before the ENDMAP lump, otherwise insert
        // after the last known lump of the level.
        if (level_format != kMapFormatUDMF)
            last_idx += 1;

        cur_wad->InsertPoint(last_idx);

        lump = cur_wad->AddLump(name, max_size);
    }

    return lump;
}

Lump *CreateGLMarker()
{
    char name_buf[64];

    if (strlen(level_current_name) <= 5)
    {
        sprintf(name_buf, "GL_%s", level_current_name);

        level_long_name = false;
    }
    else
    {
        // support for level names longer than 5 letters
        strcpy(name_buf, "GL_LEVEL");

        level_long_name = true;
    }

    int last_idx = cur_wad->LevelLastLump(level_current_idx);

    cur_wad->InsertPoint(last_idx + 1);

    Lump *marker = cur_wad->AddLump(name_buf);

    marker->Finish();

    return marker;
}

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

BuildInfo current_build_info;

void ResetInfo()
{
    current_build_info.total_minor_issues = 0;
    current_build_info.total_warnings = 0;
    current_build_info.fast = true;
    current_build_info.gl_nodes = true;
    current_build_info.force_v5 = false;
    current_build_info.force_xnod = false;
    current_build_info.force_compress = true;
    current_build_info.split_cost = kSplitCostDefault;
    current_build_info.verbosity = 0;
}

void OpenWad(std::string filename)
{
    cur_wad = WadFile::Open(filename, 'r');
    if (cur_wad == NULL)
        I_Error("AJBSP: Cannot open file: %s\n", filename.c_str());
}

void OpenMem(std::string filename, uint8_t *Rawdata, int Rawlength)
{
    cur_wad = WadFile::OpenMem(filename, Rawdata, Rawlength);
    if (cur_wad == NULL)
        I_Error("AJBSP: Cannot open file from memory: %s\n", filename.c_str());
}

void CreateXWA(std::string filename)
{
    xwa_wad = WadFile::Open(filename, 'w');
    if (xwa_wad == NULL)
        I_Error("AJBSP: Cannot create file: %s\n", filename.c_str());

    xwa_wad->BeginWrite();
    xwa_wad->AddLump("XG_START")->Finish();
    xwa_wad->EndWrite();
}

void FinishXWA()
{
    xwa_wad->BeginWrite();
    xwa_wad->AddLump("XG_END")->Finish();
    xwa_wad->EndWrite();
}

void CloseWad()
{
    if (cur_wad != NULL)
    {
        // this closes the file
        delete cur_wad;
        cur_wad = NULL;
    }

    if (xwa_wad != NULL)
    {
        delete xwa_wad;
        xwa_wad = NULL;
    }
}

int LevelsInWad()
{
    if (cur_wad == NULL)
        return 0;

    return cur_wad->LevelCount();
}

const char *GetLevelName(int level_idx)
{
    SYS_ASSERT(cur_wad != NULL);

    int lump_idx = cur_wad->LevelHeader(level_idx);

    return cur_wad->GetLump(lump_idx)->Name();
}

/* ----- build nodes for a single level ----- */

BuildResult BuildLevel(int level_idx)
{
    Node   *root_node = NULL;
    Subsector *root_sub  = NULL;

    level_current_idx   = level_idx;
    level_current_start = cur_wad->LevelHeader(level_idx);
    level_format        = cur_wad->LevelFormat(level_idx);

    LoadLevel();

    BuildResult ret = kBuildOK;

    if (num_real_lines > 0)
    {
        BoundingBox dummy;

        // create initial segs
        Seg *list = CreateSegs();

        // recursively create nodes
        ret = BuildNodes(list, 0, &dummy, &root_node, &root_sub);
    }

    if (ret == kBuildOK)
    {
        I_Debugf("    Built %d NODES, %d SSECTORS, %d SEGS, %d VERTEXES\n", level_nodes.size(), level_subsecs.size(), level_segs.size(),
                        num_old_vert + num_new_vert);

        if (root_node != NULL)
        {
            I_Debugf("    Heights of subtrees: %d / %d\n", ComputeBspHeight(root_node->r_.node),
                            ComputeBspHeight(root_node->l_.node));
        }

        ClockwiseBspTree();

        if (xwa_wad != NULL)
            ret = SaveXWA(root_node);
        else if (level_format == kMapFormatUDMF)
            ret = SaveUDMF(root_node);
        else
            ret = SaveLevel(root_node);
    }
    else
    {
        /* build was Cancelled by the user */
    }

    FreeLevel();

    return ret;
}

} // namespace ajbsp

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
