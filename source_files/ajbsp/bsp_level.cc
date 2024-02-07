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
#include "str_ename.h"
#include "str_lexer.h"
#include "str_util.h"

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

bool level_long_name;

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

static const char *GetLevelName(int level_index)
{
    SYS_ASSERT(cur_wad != NULL);

    int lump_idx = cur_wad->LevelHeader(level_index);

    return cur_wad->GetLump(lump_idx)->Name();
}

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
        line->zero_length = (fabs(start->x_ - end->x_) < kEpsilon) && (fabs(start->y_ - end->y_) < kEpsilon);

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
        line->zero_length = (fabs(start->x_ - end->x_) < kEpsilon) && (fabs(start->y_ - end->y_) < kEpsilon);

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

/* ----- UDMF reading routines ------------------------- */

void ParseThingField(Thing *thing, const int &key, const std::string &value)
{
    // Do we need more precision than an int for things? I think this would only be
    // an issue if/when polyobjects happen, as I think other thing types are ignored - Dasho

    if (key == epi::kENameX)
        thing->x = I_ROUND(epi::LexDouble(value));
    else if (key == epi::kENameY)
        thing->y = I_ROUND(epi::LexDouble(value));
    else if (key == epi::kENameType)
        thing->type = epi::LexInteger(value);
}

void ParseVertexField(Vertex *vertex, const int &key, const std::string &value)
{
    if (key == epi::kENameX)
        vertex->x_ = epi::LexDouble(value);
    else if (key == epi::kENameY)
        vertex->y_ = epi::LexDouble(value);
}

void ParseSidedefField(Sidedef *side, const int &key, const std::string &value)
{
    if (key == epi::kENameSector)
    {
        int num = epi::LexInteger(value);

        if (num < 0 || num >= level_sectors.size())
            I_Error("AJBSP: illegal sector number #%d\n", (int)num);

        side->sector = level_sectors[num];
    }
}

void ParseLinedefField(Linedef *line, const int &key, const std::string &value)
{
    switch(key)
    {
        case epi::kENameV1:
            line->start = SafeLookupVertex(epi::LexInteger(value));
            break;
        case epi::kENameV2:
            line->end = SafeLookupVertex(epi::LexInteger(value));
            break;
        case epi::kENameSpecial:
            line->type = epi::LexInteger(value);
            break;
        case epi::kENameTwosided:
            line->two_sided = epi::LexBoolean(value);
            break;
        case epi::kENameSidefront:
            {
                int num = epi::LexInteger(value);

                if (num < 0 || num >= (int)level_sidedefs.size())
                    line->right = NULL;
                else
                    line->right = level_sidedefs[num];
            }
            break;
        case epi::kENameSideback:
            {
                int num = epi::LexInteger(value);

                if (num < 0 || num >= (int)level_sidedefs.size())
                    line->left = NULL;
                else
                    line->left = level_sidedefs[num];
            }
            break;
        default:
            break;
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

        epi::EName key_ename(key, true);

        switch (cur_type)
        {
            case kUDMFVertex:
                ParseVertexField(vertex, key_ename.GetIndex(), value);
                break;
            case kUDMFThing:
                ParseThingField(thing, key_ename.GetIndex(), value);
                break;
            case kUDMFSidedef:
                ParseSidedefField(side, key_ename.GetIndex(), value);
                break;
            case kUDMFLinedef:
                ParseLinedefField(line, key_ename.GetIndex(), value);
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

        epi::EName section_ename(section, true);

        switch (section_ename.GetIndex())
        {
            case epi::kENameThing:
                if (pass == 1) cur_type = kUDMFThing;
                break;
            case epi::kENameVertex:
                if (pass == 1) cur_type = kUDMFVertex;
                break;
            case epi::kENameSector:
                if (pass == 1) cur_type = kUDMFSector;
                break;
            case epi::kENameSidedef:
                if (pass == 2) cur_type = kUDMFSidedef;
                break;
            case epi::kENameLinedef:
                if (pass == 3) cur_type = kUDMFLinedef;
                break;
            default:
                break;
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

static inline uint32_t VertexIndex_XNOD(const Vertex *v)
{
    if (v->is_new_)
        return (uint32_t)(num_old_vert + v->index_);

    return (uint32_t)v->index_;
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

static const uint8_t *level_XGL3_magic = (uint8_t *)"XGL3";
static const uint8_t *level_ZGL3_magic = (uint8_t *)"ZGL3";

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

static int node_cur_index;

static void PutOneZNode(Node *node)
{
    RawV5Node raw;

    if (node->r_.node)
        PutOneZNode(node->r_.node);

    if (node->l_.node)
        PutOneZNode(node->l_.node);

    node->index_ = node_cur_index++;

    uint32_t x  = AlignedLittleEndianS32(I_ROUND(node->x_ * 65536.0));
    uint32_t y  = AlignedLittleEndianS32(I_ROUND(node->y_ * 65536.0));
    uint32_t dx = AlignedLittleEndianS32(I_ROUND(node->dx_ * 65536.0));
    uint32_t dy = AlignedLittleEndianS32(I_ROUND(node->dy_ * 65536.0));

    ZLibAppendLump(&x, 4);
    ZLibAppendLump(&y, 4);
    ZLibAppendLump(&dx, 4);
    ZLibAppendLump(&dy, 4);

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

void PutZNodes(Node *root)
{
    uint32_t Rawnum = AlignedLittleEndianU32(level_nodes.size());
    ZLibAppendLump(&Rawnum, 4);

    node_cur_index = 0;

    if (root)
        PutOneZNode(root);

    if (node_cur_index != level_nodes.size())
        I_Error("AJBSP: PutZNodes miscounted (%d != %d)\n", node_cur_index, level_nodes.size());
}

void SaveXGL3Format(Lump *lump, Node *root_node)
{
    // WISH : compute a max_size

    if (current_build_info.compress_nodes)
        lump->Write(level_ZGL3_magic, 4);
    else
        lump->Write(level_XGL3_magic, 4);

    ZLibBeginLump(lump);

    PutZVertices();
    PutZSubsecs();
    PutXGL3Segs();
    PutZNodes(root_node);

    ZLibFinishLump();
}

/* ----- whole-level routines --------------------------- */

void LoadLevel()
{
    Lump *LEV = cur_wad->GetLump(level_current_start);

    level_current_name = LEV->Name();
    level_long_name    = false;

    E_ProgressMessage(epi::StringFormat("Building nodes for %s\n", level_current_name).c_str());

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

    if (!current_build_info.compress_nodes)
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
    if (!current_build_info.compress_nodes)
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
    if (!current_build_info.compress_nodes)
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

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

BuildInfo current_build_info;

void ResetInfo()
{
    current_build_info.total_minor_issues = 0;
    current_build_info.total_warnings = 0;
    current_build_info.compress_nodes = true;
    current_build_info.split_cost = kSplitCostDefault;
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

/* ----- build nodes for a single level ----- */

BuildResult BuildLevel(int level_index)
{
    Node   *root_node = NULL;
    Subsector *root_sub  = NULL;

    level_current_idx   = level_index;
    level_current_start = cur_wad->LevelHeader(level_index);
    level_format        = cur_wad->LevelFormat(level_index);

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
        else
            I_Error("AJBSP: Cannot save nodes to XWA file!\n");
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
