//----------------------------------------------------------------------------
//  EDGE Level Loading/Setup Code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "p_setup.h"

#include <map>
#include <unordered_map>

#include "AlmostEquals.h"
#include "am_map.h"
#include "colormap.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dm_structs.h"
#include "e_main.h"
#include "endianess.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_math.h"
#include "m_misc.h"
#include "m_random.h"
#include "main.h"
#include "math_crc.h"
#include "miniz.h"  // ZGL3 nodes
#include "p_local.h"
#include "r_bsp.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_sky.h"
#include "rad_trig.h"  // MUSINFO changers
#include "s_music.h"
#include "s_sound.h"
#include "str_compare.h"
#include "str_ename.h"
#include "str_lexer.h"
#include "str_util.h"
#include "sv_main.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"

#define EDGE_SEG_INVALID       ((seg_t *)-3)
#define EDGE_SUBSECTOR_INVALID ((subsector_t *)-3)

static bool level_active = false;

EDGE_DEFINE_CONSOLE_VARIABLE(udmf_strict_namespace, "0",
                             kConsoleVariableFlagArchive)

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

int              total_level_vertexes;
vertex_t        *level_vertexes = nullptr;
static vertex_t *level_gl_vertexes;
int              total_level_segs;
seg_t           *level_segs;
int              total_level_sectors;
sector_t        *level_sectors;
int              total_level_subsectors;
subsector_t     *level_subsectors;
int              total_level_extrafloors;
extrafloor_t    *level_extrafloors;
int              total_level_nodes;
node_t          *level_nodes;
int              total_level_lines;
line_t          *level_lines;
int              total_level_sides;
side_t          *level_sides;
static int       total_level_vertical_gaps;
static vgap_t   *level_vertical_gaps;

vertex_seclist_t *level_vertex_sector_lists;

static line_t **level_line_buffer = nullptr;

// bbox used
static float dummy_bounding_box[4];

epi::CRC32 map_sectors_crc;
epi::CRC32 map_lines_crc;
epi::CRC32 map_things_crc;

int total_map_things;

static bool hexen_level;

static bool        udmf_level;
static int         udmf_lump_number;
static std::string udmf_lump;

// a place to store sidedef numbers of the loaded linedefs.
// There is two values for every line: side0 and side1.
static int *temp_line_sides;

EDGE_DEFINE_CONSOLE_VARIABLE(goobers, "0", kConsoleVariableFlagNone)

// "Musinfo" is used here to refer to the traditional MUSINFO lump
struct MusinfoMapping
{
    std::unordered_map<int, int> mappings;
    bool                         processed = false;
};

// This is wonky, but essentially the idea is to not continually create
// duplicate RTS music changing scripts for the same level if warping back and
// forth, or using a hub or somesuch that happens to have music changers
static std::unordered_map<std::string, MusinfoMapping> musinfo_tracks;

static void GetMusinfoTracksForLevel(void)
{
    if (musinfo_tracks.count(current_map->name_) &&
        musinfo_tracks[current_map->name_].processed)
        return;
    int      raw_length = 0;
    uint8_t *raw_musinfo =
        W_OpenPackOrLumpInMemory("MUSINFO", {".txt"}, &raw_length);
    if (!raw_musinfo) return;
    std::string musinfo;
    musinfo.resize(raw_length);
    memcpy(musinfo.data(), raw_musinfo, raw_length);
    delete[] raw_musinfo;
    epi::Lexer lex(musinfo);
    if (!musinfo_tracks.count(current_map->name_))
        musinfo_tracks.try_emplace({current_map->name_, {}});
    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok != epi::kTokenNumber && tok != epi::kTokenIdentifier) break;

        if (epi::StringCaseCompareASCII(section, current_map->name_) != 0)
            continue;

        // Parse "block" for current map
        int mus_number = -1;
        for (;;)
        {
            std::string    value;
            epi::TokenKind block_tok = lex.Next(value);

            if (block_tok != epi::kTokenNumber &&
                block_tok != epi::kTokenIdentifier)
                return;

            // A valid map name should be the end of this block
            if (mapdefs.Lookup(value.c_str())) return;

            // This does have a bit of faith that the MUSINFO lump isn't
            // malformed
            if (mus_number == -1)
                mus_number = epi::LexInteger(value);
            else
            {
                // This mimics Lobo's ad-hoc playlist stuff for UMAPINFO
                int ddf_track = playlist.FindLast(value.c_str());
                if (ddf_track != -1)  // Entry exists
                {
                    musinfo_tracks[current_map->name_].mappings.try_emplace(
                        mus_number, ddf_track);
                }
                else
                {
                    static PlaylistEntry *dynamic_plentry;
                    dynamic_plentry            = new PlaylistEntry;
                    dynamic_plentry->number_   = playlist.FindFree();
                    dynamic_plentry->info_     = value;
                    dynamic_plentry->type_     = kDDFMusicUnknown;
                    dynamic_plentry->infotype_ = kDDFMusicDataLump;
                    playlist.push_back(dynamic_plentry);
                    musinfo_tracks[current_map->name_].mappings.try_emplace(
                        mus_number, dynamic_plentry->number_);
                }
                mus_number = -1;
            }
        }
    }
}

static void CheckEvilutionBug(uint8_t *data, int length)
{
    // The IWAD for TNT Evilution has a bug in MAP31 which prevents
    // the yellow keycard from appearing (the "Multiplayer Only" flag
    // is set), and the level cannot be completed.  This fixes it.

    static const uint8_t Y_key_data[] = {0x59, 0xf5, 0x48, 0xf8, 0,
                                         0,    6,    0,    0x17, 0};

    static const int Y_key_offset = 0x125C;

    if (length < Y_key_offset + 10) return;

    data += Y_key_offset;

    if (memcmp(data, Y_key_data, 10) != 0) return;

    LogPrint("Detected TNT MAP31 bug, adding fix.\n");

    data[8] &= ~MTF_NOT_SINGLE;
}

static void CheckDoom2Map05Bug(uint8_t *data, int length)
{
    // The IWAD for Doom2 has a bug in MAP05 where 2 sectors
    // are incorrectly tagged 9.  This fixes it.

    static const uint8_t sector_4_data[] = {
        0x60, 0,    0xc8, 0,    0x46, 0x4c, 0x41, 0x54, 0x31, 0, 0, 0, 0x46,
        0x4c, 0x41, 0x54, 0x31, 0x30, 0,    0,    0x70, 0,    0, 0, 9, 0};

    static const uint8_t sector_153_data[] = {
        0x98, 0,    0xe8, 0,    0x46, 0x4c, 0x41, 0x54, 0x31, 0, 0, 0, 0x46,
        0x4c, 0x41, 0x54, 0x31, 0x30, 0,    0,    0x70, 0,    9, 0, 9, 0};

    static const int sector_4_offset   = 0x68;  // 104
    static const int sector_153_offset = 3978;  // 0xf8a; //3978

    if (length < sector_4_offset + 26) return;

    if (length < sector_153_offset + 26) return;

    // Sector 4 first
    data += sector_4_offset;

    if (memcmp(data, sector_4_data, 26) != 0) return;

    if (data[24] == 9)  // check just in case
        data[24] = 0;   // set tag to 0 instead of 9

    // now sector 153
    data += (sector_153_offset - sector_4_offset);

    if (memcmp(data, sector_153_data, 26) != 0) return;

    if (data[24] == 9)  // check just in case
        data[24] = 0;   // set tag to 0 instead of 9

    LogPrint("Detected Doom2 MAP05 bug, adding fix.\n");
}

static void LoadVertexes(int lump)
{
    const uint8_t      *data;
    int                 i;
    const raw_vertex_t *ml;
    vertex_t           *li;

    if (!W_VerifyLumpName(lump, "VERTEXES"))
        FatalError("Bad WAD: level %s missing VERTEXES.\n",
                   current_map->lump_.c_str());

    // Determine number of lumps:
    //  total lump length / vertex record length.
    total_level_vertexes = W_LumpLength(lump) / sizeof(raw_vertex_t);

    if (total_level_vertexes == 0)
        FatalError("Bad WAD: level %s contains 0 vertexes.\n",
                   current_map->lump_.c_str());

    level_vertexes = new vertex_t[total_level_vertexes];

    // Load data into cache.
    data = W_LoadLump(lump);

    ml = (const raw_vertex_t *)data;
    li = level_vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i = 0; i < total_level_vertexes; i++, li++, ml++)
    {
        li->X = AlignedLittleEndianS16(ml->x);
        li->Y = AlignedLittleEndianS16(ml->y);
        li->Z = -40000.0f;
        li->W = 40000.0f;
    }

    // Free buffer memory.
    delete[] data;
}

static void SegCommonStuff(seg_t *seg, int linedef_in)
{
    seg->frontsector = seg->backsector = nullptr;

    if (linedef_in == -1) { seg->miniseg = true; }
    else
    {
        if (linedef_in >= total_level_lines)  // sanity check
            FatalError("Bad GWA file: seg #%d has invalid linedef.\n",
                       (int)(seg - level_segs));

        seg->miniseg = false;
        seg->linedef = &level_lines[linedef_in];

        float sx = seg->side ? seg->linedef->v2->X : seg->linedef->v1->X;
        float sy = seg->side ? seg->linedef->v2->Y : seg->linedef->v1->Y;

        seg->offset = R_PointToDist(sx, sy, seg->v1->X, seg->v1->Y);

        seg->sidedef = seg->linedef->side[seg->side];

        if (!seg->sidedef)
            FatalError("Bad GWA file: missing side for seg #%d\n",
                       (int)(seg - level_segs));

        seg->frontsector = seg->sidedef->sector;

        if (seg->linedef->flags & MLF_TwoSided)
        {
            side_t *other = seg->linedef->side[seg->side ^ 1];

            if (other) seg->backsector = other->sector;
        }
    }
}

//
// GroupSectorTags
//
// Called during P_LoadSectors to set the tag_next & tag_prev fields of
// each sector_t, which keep all sectors with the same tag in a linked
// list for faster handling.
//
// -AJA- 1999/07/29: written.
//
static void GroupSectorTags(sector_t *dest, sector_t *seclist, int numsecs)
{
    // NOTE: `numsecs' does not include the current sector.

    dest->tag_next = dest->tag_prev = nullptr;

    for (; numsecs > 0; numsecs--)
    {
        sector_t *src = &seclist[numsecs - 1];

        if (src->tag == dest->tag)
        {
            src->tag_next  = dest;
            dest->tag_prev = src;
            return;
        }
    }
}

static void LoadSectors(int lump)
{
    const uint8_t      *data;
    int                 i;
    const raw_sector_t *ms;
    sector_t           *ss;

    if (!W_VerifyLumpName(lump, "SECTORS"))
    {
        // Check if SECTORS is immediately after
        // THINGS/LINEDEFS/SIDEDEFS/VERTEXES
        lump -= 3;
        if (!W_VerifyLumpName(lump, "SECTORS"))
            FatalError("Bad WAD: level %s missing SECTORS.\n",
                       current_map->lump_.c_str());
    }

    total_level_sectors = W_LumpLength(lump) / sizeof(raw_sector_t);

    if (total_level_sectors == 0)
        FatalError("Bad WAD: level %s contains 0 sectors.\n",
                   current_map->lump_.c_str());

    level_sectors = new sector_t[total_level_sectors];
    Z_Clear(level_sectors, sector_t, total_level_sectors);

    data = W_LoadLump(lump);
    map_sectors_crc.AddBlock((const uint8_t *)data, W_LumpLength(lump));

    CheckDoom2Map05Bug((uint8_t *)data, W_LumpLength(lump));  // Lobo: 2023

    ms = (const raw_sector_t *)data;
    ss = level_sectors;
    for (i = 0; i < total_level_sectors; i++, ss++, ms++)
    {
        char buffer[10];

        ss->f_h = AlignedLittleEndianS16(ms->floor_h);
        ss->c_h = AlignedLittleEndianS16(ms->ceil_h);

        // return to wolfenstein?
        if (goobers.d_)
        {
            ss->f_h = 0;
            ss->c_h = (ms->floor_h == ms->ceil_h) ? 0 : 128.0f;
        }

        ss->orig_height = (ss->f_h + ss->c_h);

        ss->floor.translucency = VISIBLE;
        ss->floor.x_mat.X      = 1;
        ss->floor.x_mat.Y      = 0;
        ss->floor.y_mat.X      = 0;
        ss->floor.y_mat.Y      = 1;

        ss->ceil = ss->floor;

        epi::CStringCopyMax(buffer, ms->floor_tex, 8);
        ss->floor.image = W_ImageLookup(buffer, kImageNamespaceFlat);

        if (ss->floor.image)
        {
            FlatDefinition *current_flatdef =
                flatdefs.Find(ss->floor.image->name.c_str());
            if (current_flatdef)
            {
                ss->bob_depth  = current_flatdef->bob_depth_;
                ss->sink_depth = current_flatdef->sink_depth_;
            }
        }

        epi::CStringCopyMax(buffer, ms->ceil_tex, 8);
        ss->ceil.image = W_ImageLookup(buffer, kImageNamespaceFlat);

        if (!ss->floor.image)
        {
            LogWarning("Bad Level: sector #%d has missing floor texture.\n", i);
            ss->floor.image = W_ImageLookup("FLAT1", kImageNamespaceFlat);
        }
        if (!ss->ceil.image)
        {
            LogWarning("Bad Level: sector #%d has missing ceiling texture.\n",
                       i);
            ss->ceil.image = ss->floor.image;
        }

        // convert negative tags to zero
        ss->tag = HMM_MAX(0, AlignedLittleEndianS16(ms->tag));

        ss->props.lightlevel = AlignedLittleEndianS16(ms->light);

        int type = AlignedLittleEndianS16(ms->special);

        ss->props.type    = HMM_MAX(0, type);
        ss->props.special = P_LookupSectorType(ss->props.type);

        ss->exfloor_max = 0;

        ss->props.colourmap = nullptr;

        ss->props.gravity   = kGravityDefault;
        ss->props.friction  = kFrictionDefault;
        ss->props.viscosity = kViscosityDefault;
        ss->props.drag      = kDragDefault;

        if (ss->props.special && ss->props.special->fog_color_ != kRGBANoValue)
        {
            ss->props.fog_color   = ss->props.special->fog_color_;
            ss->props.fog_density = 0.01f * ss->props.special->fog_density_;
        }
        else
        {
            ss->props.fog_color   = kRGBANoValue;
            ss->props.fog_density = 0;
        }

        ss->p = &ss->props;

        ss->sound_player = -1;

        // -AJA- 1999/07/29: Keep sectors with same tag in a list.
        GroupSectorTags(ss, level_sectors, i);
    }

    delete[] data;
}

static void SetupRootNode(void)
{
    if (total_level_nodes > 0) { root_node = total_level_nodes - 1; }
    else
    {
        root_node = NF_V5_SUBSECTOR | 0;

        // compute bbox for the single subsector
        BoundingBoxClear(dummy_bounding_box);

        int    i;
        seg_t *seg;

        for (i = 0, seg = level_segs; i < total_level_segs; i++, seg++)
        {
            BoundingBoxAddPoint(dummy_bounding_box, seg->v1->X, seg->v1->Y);
            BoundingBoxAddPoint(dummy_bounding_box, seg->v2->X, seg->v2->Y);
        }
    }
}

static std::map<int, int> unknown_thing_map;

static void UnknownThingWarning(int type, float x, float y)
{
    int count = 0;

    if (unknown_thing_map.find(type) != unknown_thing_map.end())
        count = unknown_thing_map[type];

    if (count < 2)
        LogWarning("Unknown thing type %i at (%1.0f, %1.0f)\n", type, x, y);
    else if (count == 2)
        LogWarning("More unknown things of type %i found...\n", type);

    unknown_thing_map[type] = count + 1;
}

static MapObject *SpawnMapThing(const MapObjectDefinition *info, float x, float y,
                             float z, sector_t *sec, BAMAngle angle,
                             int options, int tag)
{
    SpawnPoint point;

    point.x         = x;
    point.y         = y;
    point.z         = z;
    point.angle     = angle;
    point.vertical_angle = 0;
    point.info      = info;
    point.flags     = 0;
    point.tag       = tag;

    // -KM- 1999/01/31 Use playernum property.
    // count deathmatch start positions
    if (info->playernum_ < 0)
    {
        GameAddDeathmatchStart(point);
        return nullptr;
    }

    // check for players specially -jc-
    if (info->playernum_ > 0)
    {
        // -AJA- 2009/10/07: Hub support
        if (sec->props.special && sec->props.special->hub_)
        {
            if (sec->tag <= 0)
                LogWarning("HUB_START in sector without tag @ (%1.0f %1.0f)\n",
                           x, y);

            point.tag = sec->tag;

            GameAddHubStart(point);
            return nullptr;
        }

        // -AJA- 2004/12/30: for duplicate players, the LAST one must
        //       be used (so levels with Voodoo dolls work properly).
        SpawnPoint *prev = GameFindCoopPlayer(info->playernum_);

        if (!prev)
            GameAddCoopStart(point);
        else
        {
            GameAddVoodooDoll(*prev);

            // overwrite one in the Coop list with new location
            memcpy(prev, &point, sizeof(point));
        }
        return nullptr;
    }

    // check for apropriate skill level
    // -ES- 1999/04/13 Implemented Kester's Bugfix.
    // -AJA- 1999/10/21: Reworked again.
    if (SP_MATCH() && (options & MTF_NOT_SINGLE)) return nullptr;

    // Disable deathmatch weapons for vanilla coop...should probably be in the
    // Gameplay Options menu - Dasho
    if (COOP_MATCH() && (options & MTF_NOT_SINGLE)) return nullptr;

    // -AJA- 1999/09/22: Boom compatibility flags.
    if (COOP_MATCH() && (options & MTF_NOT_COOP)) return nullptr;

    if (DEATHMATCH() && (options & MTF_NOT_DM)) return nullptr;

    int bit;

    if (game_skill == sk_baby)
        bit = 1;
    else if (game_skill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (game_skill - 1);

    if ((options & bit) == 0) return nullptr;

    // don't spawn keycards in deathmatch
    if (DEATHMATCH() && (info->flags_ & kMapObjectFlagNotDeathmatch))
        return nullptr;

    // don't spawn any monsters if -nomonsters
    if (level_flags.nomonsters && (info->extended_flags_ & kExtendedFlagMonster))
        return nullptr;

    // -AJA- 1999/10/07: don't spawn extra things if -noextra.
    if (!level_flags.have_extra && (info->extended_flags_ & kExtendedFlagExtra))
        return nullptr;

    // spawn it now !
    // Use MobjCreateObject -ACB- 1998/08/06
    MapObject *mo = P_MobjCreateObject(x, y, z, info);

    mo->angle_      = angle;
    mo->spawnpoint_ = point;

    if (mo->state_ && mo->state_->tics > 1)
        mo->tics_ = 1 + (RandomByteDeterministic() % mo->state_->tics);

    if (options & MTF_AMBUSH)
    {
        mo->flags_ |= kMapObjectFlagAmbush;
        mo->spawnpoint_.flags |= kMapObjectFlagAmbush;
    }

    // -AJA- 2000/09/22: MBF compatibility flag
    if (options & MTF_FRIEND)
    {
        mo->side_ = 1;  //~0;
        mo->hyper_flags_ |= kHyperFlagUltraLoyal;
        /*
        player_t *player;
        player = players[0];
        mo->SetSupportObj(player->mo);
        P_LookForPlayers(mo, mo->info_->sight_angle_);
        */
    }
    // Lobo 2022: added tagged mobj support ;)
    if (tag > 0) mo->tag_ = tag;

    return mo;
}

static void LoadThings(int lump)
{
    float    x, y, z;
    BAMAngle angle;
    int      options, typenum;
    int      i;

    const uint8_t             *data;
    const raw_thing_t         *mt;
    const MapObjectDefinition *objtype;

    if (!W_VerifyLumpName(lump, "THINGS"))
        FatalError("Bad WAD: level %s missing THINGS.\n",
                   current_map->lump_.c_str());

    total_map_things = W_LumpLength(lump) / sizeof(raw_thing_t);

    if (total_map_things == 0)
        FatalError("Bad WAD: level %s contains 0 things.\n",
                   current_map->lump_.c_str());

    data = W_LoadLump(lump);
    map_things_crc.AddBlock((const uint8_t *)data, W_LumpLength(lump));

    CheckEvilutionBug((uint8_t *)data, W_LumpLength(lump));

    // -AJA- 2004/11/04: check the options in all things to see whether
    // we can use new option flags or not.  Same old wads put 1 bits in
    // unused locations (unusued for original Doom anyway).  The logic
    // here is based on PrBoom, but PrBoom checks each thing separately.

    bool limit_options = false;

    mt = (const raw_thing_t *)data;

    for (i = 0; i < total_map_things; i++)
    {
        options = AlignedLittleEndianU16(mt[i].options);

        if (options & MTF_RESERVED) limit_options = true;
    }

    for (i = 0; i < total_map_things; i++, mt++)
    {
        x       = (float)AlignedLittleEndianS16(mt->x);
        y       = (float)AlignedLittleEndianS16(mt->y);
        angle   = epi::BAMFromDegrees((float)AlignedLittleEndianS16(mt->angle));
        typenum = AlignedLittleEndianU16(mt->type);
        options = AlignedLittleEndianU16(mt->options);

        if (limit_options) options &= 0x001F;

        objtype = mobjtypes.Lookup(typenum);

        // MOBJTYPE not found, don't crash out: JDS Compliance.
        // -ACB- 1998/07/21
        if (objtype == nullptr)
        {
            UnknownThingWarning(typenum, x, y);
            continue;
        }

        sector_t *sec = R_PointInSubsector(x, y)->sector;

        if ((objtype->hyper_flags_ & kHyperFlagMusicChanger) &&
            !musinfo_tracks[current_map->name_].processed)
        {
            // This really should only be used with the original DoomEd number
            // range
            if (objtype->number_ >= 14100 && objtype->number_ < 14165)
            {
                int mus_number = -1;

                if (objtype->number_ == 14100)  // Default for level
                    mus_number = current_map->music_;
                else if (musinfo_tracks[current_map->name_].mappings.count(
                             objtype->number_ - 14100))
                    mus_number = musinfo_tracks[current_map->name_]
                                     .mappings[objtype->number_ - 14100];
                // Track found; make ad-hoc RTS script for music changing
                if (mus_number != -1)
                {
                    std::string mus_rts = "// MUSINFO SCRIPTS\n\n";
                    mus_rts.append(epi::StringFormat(
                        "START_MAP %s\n", current_map->name_.c_str()));
                    mus_rts.append(epi::StringFormat(
                        "  SECTOR_TRIGGER_INDEX %d\n", sec - level_sectors));
                    mus_rts.append("    TAGGED_INDEPENDENT\n");
                    mus_rts.append("    TAGGED_REPEATABLE\n");
                    mus_rts.append("    WAIT 30T\n");
                    mus_rts.append(
                        epi::StringFormat("    CHANGE_MUSIC %d\n", mus_number));
                    mus_rts.append("    RETRIGGER\n");
                    mus_rts.append("  END_SECTOR_TRIGGER\n");
                    mus_rts.append("END_MAP\n\n");
                    RAD_ReadScript(mus_rts, "MUSINFO");
                }
            }
        }

        z = sec->f_h;

        if (objtype->flags_ & kMapObjectFlagSpawnCeiling)
            z = sec->c_h - objtype->height_;

        if ((options & MTF_RESERVED) == 0 && (options & MTF_EXFLOOR_MASK))
        {
            int floor_num = (options & MTF_EXFLOOR_MASK) >> MTF_EXFLOOR_SHIFT;

            for (extrafloor_t *ef = sec->bottom_ef; ef; ef = ef->higher)
            {
                z = ef->top_h;

                floor_num--;
                if (floor_num == 0) break;
            }
        }

        SpawnMapThing(objtype, x, y, z, sec, angle, options, 0);
    }

    // Mark MUSINFO for this level as done processing, even if it was empty,
    // so we can avoid re-checks
    musinfo_tracks[current_map->name_].processed = true;

    delete[] data;
}

static void LoadHexenThings(int lump)
{
    // -AJA- 2001/08/04: wrote this, based on the Hexen specs.

    float    x, y, z;
    BAMAngle angle;
    int      options, typenum;
    int      tag;
    int      i;

    const uint8_t             *data;
    const raw_hexen_thing_t   *mt;
    const MapObjectDefinition *objtype;

    if (!W_VerifyLumpName(lump, "THINGS"))
        FatalError("Bad WAD: level %s missing THINGS.\n",
                   current_map->lump_.c_str());

    total_map_things = W_LumpLength(lump) / sizeof(raw_hexen_thing_t);

    if (total_map_things == 0)
        FatalError("Bad WAD: level %s contains 0 things.\n",
                   current_map->lump_.c_str());

    data = W_LoadLump(lump);
    map_things_crc.AddBlock((const uint8_t *)data, W_LumpLength(lump));

    mt = (const raw_hexen_thing_t *)data;
    for (i = 0; i < total_map_things; i++, mt++)
    {
        x     = (float)AlignedLittleEndianS16(mt->x);
        y     = (float)AlignedLittleEndianS16(mt->y);
        z     = (float)AlignedLittleEndianS16(mt->height);
        angle = epi::BAMFromDegrees((float)AlignedLittleEndianS16(mt->angle));

        tag     = AlignedLittleEndianS16(mt->tid);
        typenum = AlignedLittleEndianU16(mt->type);
        options = AlignedLittleEndianU16(mt->options) & 0x000F;

        objtype = mobjtypes.Lookup(typenum);

        // MOBJTYPE not found, don't crash out: JDS Compliance.
        // -ACB- 1998/07/21
        if (objtype == nullptr)
        {
            UnknownThingWarning(typenum, x, y);
            continue;
        }

        sector_t *sec = R_PointInSubsector(x, y)->sector;

        z += sec->f_h;

        if (objtype->flags_ & kMapObjectFlagSpawnCeiling)
            z = sec->c_h - objtype->height_;

        SpawnMapThing(objtype, x, y, z, sec, angle, options, tag);
    }

    delete[] data;
}

static inline void ComputeLinedefData(line_t *ld, int side0, int side1)
{
    vertex_t *v1 = ld->v1;
    vertex_t *v2 = ld->v2;

    ld->dx = v2->X - v1->X;
    ld->dy = v2->Y - v1->Y;

    if (AlmostEquals(ld->dx, 0.0f))
        ld->slopetype = ST_VERTICAL;
    else if (AlmostEquals(ld->dy, 0.0f))
        ld->slopetype = ST_HORIZONTAL;
    else if (ld->dy / ld->dx > 0)
        ld->slopetype = ST_POSITIVE;
    else
        ld->slopetype = ST_NEGATIVE;

    ld->length = R_PointToDist(0, 0, ld->dx, ld->dy);

    if (v1->X < v2->X)
    {
        ld->bbox[kBoundingBoxLeft]  = v1->X;
        ld->bbox[kBoundingBoxRight] = v2->X;
    }
    else
    {
        ld->bbox[kBoundingBoxLeft]  = v2->X;
        ld->bbox[kBoundingBoxRight] = v1->X;
    }

    if (v1->Y < v2->Y)
    {
        ld->bbox[kBoundingBoxBottom] = v1->Y;
        ld->bbox[kBoundingBoxTop]    = v2->Y;
    }
    else
    {
        ld->bbox[kBoundingBoxBottom] = v2->Y;
        ld->bbox[kBoundingBoxTop]    = v1->Y;
    }

    if (!udmf_level && side0 == 0xFFFF) side0 = -1;
    if (!udmf_level && side1 == 0xFFFF) side1 = -1;

    // handle missing RIGHT sidedef (idea taken from MBF)
    if (side0 == -1)
    {
        LogWarning("Bad WAD: level %s linedef #%d is missing RIGHT side\n",
                   current_map->lump_.c_str(), (int)(ld - level_lines));
        side0 = 0;
    }

    if ((ld->flags & MLF_TwoSided) && ((side0 == -1) || (side1 == -1)))
    {
        LogWarning(
            "Bad WAD: level %s has linedef #%d marked TWOSIDED, "
            "but it has only one side.\n",
            current_map->lump_.c_str(), (int)(ld - level_lines));

        ld->flags &= ~MLF_TwoSided;
    }

    temp_line_sides[(ld - level_lines) * 2 + 0] = side0;
    temp_line_sides[(ld - level_lines) * 2 + 1] = side1;

    total_level_sides += (side1 == -1) ? 1 : 2;
}

static void LoadLineDefs(int lump)
{
    // -AJA- New handling for sidedefs.  Since sidedefs can be "packed" in
    //       a wad (i.e. shared by several linedefs) we need to unpack
    //       them.  This is to prevent potential problems with scrollers,
    //       the CHANGE_TEX command in RTS, etc, and also to implement
    //       "wall tiles" properly.

    if (!W_VerifyLumpName(lump, "LINEDEFS"))
        FatalError("Bad WAD: level %s missing LINEDEFS.\n",
                   current_map->lump_.c_str());

    total_level_lines = W_LumpLength(lump) / sizeof(raw_linedef_t);

    if (total_level_lines == 0)
        FatalError("Bad WAD: level %s contains 0 linedefs.\n",
                   current_map->lump_.c_str());

    level_lines = new line_t[total_level_lines];

    Z_Clear(level_lines, line_t, total_level_lines);

    temp_line_sides = new int[total_level_lines * 2];

    const uint8_t *data = W_LoadLump(lump);
    map_lines_crc.AddBlock((const uint8_t *)data, W_LumpLength(lump));

    line_t              *ld  = level_lines;
    const raw_linedef_t *mld = (const raw_linedef_t *)data;

    for (int i = 0; i < total_level_lines; i++, mld++, ld++)
    {
        ld->flags = AlignedLittleEndianU16(mld->flags);
        ld->tag   = HMM_MAX(0, AlignedLittleEndianS16(mld->tag));
        ld->v1    = &level_vertexes[AlignedLittleEndianU16(mld->start)];
        ld->v2    = &level_vertexes[AlignedLittleEndianU16(mld->end)];

        // Check for BoomClear flag bit and clear applicable specials
        // (PassThru may still be intentionally readded further down)
        if (ld->flags & MLF_ClearBoom)
            ld->flags &= ~(MLF_PassThru | MLF_BlockGrounded | MLF_BlockPlayers);

        ld->special =
            P_LookupLineType(HMM_MAX(0, AlignedLittleEndianS16(mld->special)));

        if (ld->special && ld->special->type_ == kLineTriggerWalkable)
            ld->flags |= MLF_PassThru;

        if (ld->special && ld->special->type_ == kLineTriggerNone &&
            (ld->special->s_xspeed_ || ld->special->s_yspeed_ ||
             ld->special->scroll_type_ > BoomScrollerTypeNone ||
             ld->special->line_effect_ == kLineEffectTypeVectorScroll ||
             ld->special->line_effect_ == kLineEffectTypeOffsetScroll ||
             ld->special->line_effect_ == kLineEffectTypeTaggedOffsetScroll))
            ld->flags |= MLF_PassThru;

        if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailFloor)
            ld->flags |= MLF_PassThru;

        if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailCeiling)
            ld->flags |= MLF_PassThru;

        if (ld->special &&
            ld->special ==
                linetypes.Lookup(0))  // Add passthru to unknown/templated
            ld->flags |= MLF_PassThru;

        int side0 = AlignedLittleEndianU16(mld->side_R);
        int side1 = AlignedLittleEndianU16(mld->side_L);

        ComputeLinedefData(ld, side0, side1);

        // check for possible extrafloors, updating the exfloor_max count
        // for the sectors in question.

        if (ld->tag && ld->special && ld->special->ef_.type_)
        {
            for (int j = 0; j < total_level_sectors; j++)
            {
                if (level_sectors[j].tag != ld->tag) continue;

                level_sectors[j].exfloor_max++;
                total_level_extrafloors++;
            }
        }
    }

    delete[] data;
}

static void LoadHexenLineDefs(int lump)
{
    // -AJA- 2001/08/04: wrote this, based on the Hexen specs.

    if (!W_VerifyLumpName(lump, "LINEDEFS"))
        FatalError("Bad WAD: level %s missing LINEDEFS.\n",
                   current_map->lump_.c_str());

    total_level_lines = W_LumpLength(lump) / sizeof(raw_hexen_linedef_t);

    if (total_level_lines == 0)
        FatalError("Bad WAD: level %s contains 0 linedefs.\n",
                   current_map->lump_.c_str());

    level_lines = new line_t[total_level_lines];

    Z_Clear(level_lines, line_t, total_level_lines);

    temp_line_sides = new int[total_level_lines * 2];

    const uint8_t *data = W_LoadLump(lump);
    map_lines_crc.AddBlock((const uint8_t *)data, W_LumpLength(lump));

    line_t                    *ld  = level_lines;
    const raw_hexen_linedef_t *mld = (const raw_hexen_linedef_t *)data;

    for (int i = 0; i < total_level_lines; i++, mld++, ld++)
    {
        ld->flags = AlignedLittleEndianU16(mld->flags) & 0x00FF;
        ld->tag   = 0;
        ld->v1    = &level_vertexes[AlignedLittleEndianU16(mld->start)];
        ld->v2    = &level_vertexes[AlignedLittleEndianU16(mld->end)];

        // this ignores the activation bits -- oh well
        ld->special = (mld->args[0] == 0)
                          ? nullptr
                          : linetypes.Lookup(1000 + mld->args[0]);

        int side0 = AlignedLittleEndianU16(mld->side_R);
        int side1 = AlignedLittleEndianU16(mld->side_L);

        ComputeLinedefData(ld, side0, side1);
    }

    delete[] data;
}

static sector_t *DetermineSubsectorSector(subsector_t *ss, int pass)
{
    const seg_t *seg;

    for (seg = ss->segs; seg != nullptr; seg = seg->sub_next)
    {
        if (seg->miniseg) continue;

        // ignore self-referencing linedefs
        if (seg->frontsector == seg->backsector) continue;

        return seg->frontsector;
    }

    for (seg = ss->segs; seg != nullptr; seg = seg->sub_next)
    {
        if (seg->partner == nullptr) continue;

        // only do this for self-referencing linedefs if the original sector
        // isn't tagged, otherwise save it for the next pass
        if (seg->frontsector == seg->backsector && seg->frontsector &&
            seg->frontsector->tag == 0)
            return seg->frontsector;

        if (seg->frontsector != seg->backsector &&
            seg->partner->front_sub->sector != nullptr)
            return seg->partner->front_sub->sector;
    }

    if (pass == 1)
    {
        for (seg = ss->segs; seg != nullptr; seg = seg->sub_next)
        {
            if (!seg->miniseg) return seg->frontsector;
        }
    }

    if (pass == 2) return &level_sectors[0];

    return nullptr;
}

static bool AssignSubsectorsPass(int pass)
{
    // pass 0 : ignore self-ref lines.
    // pass 1 : use them.
    // pass 2 : handle extreme brokenness.
    //
    // returns true if progress was made.

    int  null_count = 0;
    bool progress   = false;

    for (int i = 0; i < total_level_subsectors; i++)
    {
        subsector_t *ss = &level_subsectors[i];

        if (ss->sector == nullptr)
        {
            null_count += 1;

            ss->sector = DetermineSubsectorSector(ss, pass);

            if (ss->sector != nullptr)
            {
                progress = true;

                // link subsector into parent sector's list.
                // order is not important, so add it to the head of the list.
                ss->sec_next           = ss->sector->subsectors;
                ss->sector->subsectors = ss;
            }
        }
    }

    /* DEBUG
    fprintf(stderr, "** pass %d : %d : %d\n", pass, null_count, progress ? 1 :
    0);
    */

    return progress;
}

static void AssignSubsectorsToSectors()
{
    // AJA 2022: this attempts to improve handling of self-referencing lines
    //           (i.e. ones with the same sector on both sides).  Subsectors
    //           touching such lines should NOT be assigned to that line's
    //           sector, but rather to the "outer" sector.

    while (AssignSubsectorsPass(0)) {}

    while (AssignSubsectorsPass(1)) {}

    // the above *should* handle everything, so this pass is only needed
    // for extremely broken nodes or maps.
    AssignSubsectorsPass(2);
}

// Adapted from EDGE 2.X's ZNode loading routine; only handles XGL3/ZGL3 as that
// is all our built-in AJBSP produces now
static void LoadXGL3Nodes(int lumpnum)
{
    int                  i, xglen = 0;
    uint8_t             *xgldata = nullptr;
    std::vector<uint8_t> zgldata;
    uint8_t             *td = nullptr;

    LogDebug("LoadXGL3Nodes:\n");

    xglen   = W_LumpLength(lumpnum);
    xgldata = (uint8_t *)W_LoadLump(lumpnum);
    if (!xgldata) FatalError("LoadXGL3Nodes: Couldn't load lump\n");

    if (xglen < 12)
    {
        delete[] xgldata;
        FatalError("LoadXGL3Nodes: Lump too short\n");
    }

    if (!memcmp(xgldata, "XGL3", 4))
        LogDebug(" AJBSP uncompressed GL nodes v3\n");
    else if (!memcmp(xgldata, "ZGL3", 4))
    {
        LogDebug(" AJBSP compressed GL nodes v3\n");
        zgldata.resize(xglen);
        z_stream zgl_stream;
        memset(&zgl_stream, 0, sizeof(z_stream));
        zgl_stream.next_in   = &xgldata[4];
        zgl_stream.avail_in  = xglen - 4;
        zgl_stream.next_out  = zgldata.data();
        zgl_stream.avail_out = zgldata.size();
        inflateInit2(&zgl_stream, MZ_DEFAULT_WINDOW_BITS);
        int inflate_status;
        for (;;)
        {
            inflate_status = inflate(&zgl_stream, Z_NO_FLUSH);
            if (inflate_status == MZ_OK ||
                inflate_status == MZ_BUF_ERROR)  // Need to resize output buffer
            {
                zgldata.resize(zgldata.size() * 2);
                zgl_stream.next_out  = &zgldata[zgl_stream.total_out];
                zgl_stream.avail_out = zgldata.size() - zgl_stream.total_out;
            }
            else if (inflate_status == Z_STREAM_END)
            {
                inflateEnd(&zgl_stream);
                zgldata.resize(zgl_stream.total_out);
                zgldata.shrink_to_fit();
                break;
            }
            else
                FatalError("LoadXGL3Nodes: Failed to decompress ZGL3 nodes!\n");
        }
    }
    else
    {
        static char xgltemp[6];
        epi::CStringCopyMax(xgltemp, (char *)xgldata, 4);
        delete[] xgldata;
        FatalError("LoadXGL3Nodes: Unrecognized node type %s\n", xgltemp);
    }

    if (!zgldata.empty())
        td = zgldata.data();
    else
        td = &xgldata[4];

    // after signature, 1st u32 is number of original vertexes - should be <=
    // total_level_vertexes
    int oVerts = epi::UnalignedLittleEndianU32(td);
    td += 4;
    if (oVerts > total_level_vertexes)
    {
        delete[] xgldata;
        FatalError("LoadXGL3Nodes: Vertex/Node mismatch\n");
    }

    // 2nd u32 is the number of extra vertexes added by ajbsp
    int nVerts = epi::UnalignedLittleEndianU32(td);
    td += 4;
    LogDebug("LoadXGL3Nodes: Orig Verts = %d, New Verts = %d, Map Verts = %d\n",
             oVerts, nVerts, total_level_vertexes);

    level_gl_vertexes = new vertex_t[nVerts];

    // fill in new vertexes
    vertex_t *vv = level_gl_vertexes;
    for (i = 0; i < nVerts; i++, vv++)
    {
        // convert signed 16.16 fixed point to float
        vv->X = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        vv->Y = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        vv->Z = -40000.0f;
        vv->W = 40000.0f;
    }

    // new vertexes is followed by the subsectors
    total_level_subsectors = epi::UnalignedLittleEndianS32(td);
    td += 4;
    if (total_level_subsectors <= 0)
    {
        delete[] xgldata;
        FatalError("LoadXGL3Nodes: No subsectors\n");
    }
    LogDebug("LoadXGL3Nodes: Num SSECTORS = %d\n", total_level_subsectors);

    level_subsectors = new subsector_t[total_level_subsectors];
    Z_Clear(level_subsectors, subsector_t, total_level_subsectors);

    int *ss_temp = new int[total_level_subsectors];
    int  xglSegs = 0;
    for (i = 0; i < total_level_subsectors; i++)
    {
        int countsegs = epi::UnalignedLittleEndianS32(td);
        td += 4;
        ss_temp[i] = countsegs;
        xglSegs += countsegs;
    }

    // subsectors are followed by the segs
    total_level_segs = epi::UnalignedLittleEndianS32(td);
    td += 4;
    if (total_level_segs != xglSegs)
    {
        delete[] xgldata;
        FatalError("LoadXGL3Nodes: Incorrect number of segs in nodes\n");
    }
    LogDebug("LoadXGL3Nodes: Num SEGS = %d\n", total_level_segs);

    level_segs = new seg_t[total_level_segs];
    Z_Clear(level_segs, seg_t, total_level_segs);
    seg_t *seg = level_segs;

    for (i = 0; i < total_level_segs; i++, seg++)
    {
        unsigned int v1num;
        int          slinedef, partner, side;

        v1num = epi::UnalignedLittleEndianU32(td);
        td += 4;
        partner = epi::UnalignedLittleEndianS32(td);
        td += 4;
        slinedef = epi::UnalignedLittleEndianS32(td);
        td += 4;
        side = (int)(*td);
        td += 1;

        if (v1num < (uint32_t)oVerts)
            seg->v1 = &level_vertexes[v1num];
        else
            seg->v1 = &level_gl_vertexes[v1num - oVerts];

        seg->side = side ? 1 : 0;

        if (partner == -1)
            seg->partner = nullptr;
        else
        {
            SYS_ASSERT(partner < total_level_segs);  // sanity check
            seg->partner = &level_segs[partner];
        }

        SegCommonStuff(seg, slinedef);

        // The following fields are filled out elsewhere:
        //     sub_next, front_sub, back_sub, frontsector, backsector.

        seg->sub_next  = EDGE_SEG_INVALID;
        seg->front_sub = seg->back_sub = EDGE_SUBSECTOR_INVALID;
    }

    LogDebug("LoadXGL3Nodes: Post-process subsectors\n");
    // go back and fill in subsectors
    subsector_t *ss = level_subsectors;
    xglSegs         = 0;
    for (i = 0; i < total_level_subsectors; i++, ss++)
    {
        int countsegs = ss_temp[i];
        int firstseg  = xglSegs;
        xglSegs += countsegs;

        // go back and fill in v2 from v1 of next seg and do calcs that needed
        // both
        seg = &level_segs[firstseg];
        for (int j = 0; j < countsegs; j++, seg++)
        {
            seg->v2 = j == (countsegs - 1) ? level_segs[firstseg].v1
                                           : level_segs[firstseg + j + 1].v1;

            seg->angle =
                R_PointToAngle(seg->v1->X, seg->v1->Y, seg->v2->X, seg->v2->Y);

            seg->length =
                R_PointToDist(seg->v1->X, seg->v1->Y, seg->v2->X, seg->v2->Y);
        }

        // -AJA- 1999/09/23: New linked list for the segs of a subsector
        //       (part of true bsp rendering).
        seg_t **prevptr = &ss->segs;

        if (countsegs == 0)
            FatalError("LoadXGL3Nodes: level %s has invalid SSECTORS.\n",
                       current_map->lump_.c_str());

        ss->sector    = nullptr;
        ss->thinglist = nullptr;

        // this is updated when the nodes are loaded
        ss->bbox = dummy_bounding_box;

        for (int j = 0; j < countsegs; j++)
        {
            seg_t *cur = &level_segs[firstseg + j];

            *prevptr = cur;
            prevptr  = &cur->sub_next;

            cur->front_sub = ss;
            cur->back_sub  = nullptr;

            // LogDebug("  ssec = %d, seg = %d\n", i, firstseg + j);
        }
        // LogDebug("LoadZNodes: ssec = %d, fseg = %d, cseg = %d\n", i,
        // firstseg, countsegs);

        *prevptr = nullptr;
    }
    delete[] ss_temp;  // CA 9.30.18: allocated with new but released using
                       // delete, added [] between delete and ss_temp

    LogDebug("LoadXGL3Nodes: Read GL nodes\n");
    // finally, read the nodes
    // NOTE: no nodes is okay (a basic single sector map). -AJA-
    total_level_nodes = epi::UnalignedLittleEndianU32(td);
    td += 4;
    LogDebug("LoadXGL3Nodes: Num nodes = %d\n", total_level_nodes);

    level_nodes = new node_t[total_level_nodes + 1];
    Z_Clear(level_nodes, node_t, total_level_nodes);
    node_t *nd = level_nodes;

    for (i = 0; i < total_level_nodes; i++, nd++)
    {
        nd->div.x = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->div.y = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->div.dx = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->div.dy = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;

        nd->div_len = R_PointToDist(0, 0, nd->div.dx, nd->div.dy);

        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 4; k++)
            {
                nd->bbox[j][k] = (float)epi::UnalignedLittleEndianS16(td);
                td += 2;
            }

        for (int j = 0; j < 2; j++)
        {
            nd->children[j] = epi::UnalignedLittleEndianU32(td);
            td += 4;

            // update bbox pointers in subsector
            if (nd->children[j] & NF_V5_SUBSECTOR)
            {
                subsector_t *sss =
                    level_subsectors + (nd->children[j] & ~NF_V5_SUBSECTOR);
                sss->bbox = &nd->bbox[j][0];
            }
        }
    }

    AssignSubsectorsToSectors();

    LogDebug("LoadXGL3Nodes: Setup root node\n");
    SetupRootNode();

    LogDebug("LoadXGL3Nodes: Finished\n");
    delete[] xgldata;
    zgldata.clear();
}

static void LoadUDMFVertexes()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFVertexes: parsing TEXTMAP\n");
    int cur_vertex = 0;

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        if (section == "vertex")
        {
            float x = 0.0f, y = 0.0f;
            float zf = -40000.0f, zc = 40000.0f;
            for (;;)
            {
                if (lex.Match("}")) break;

                std::string    key;
                std::string    value;
                epi::TokenKind block_tok = lex.Next(key);

                if (block_tok == epi::kTokenEOF)
                    FatalError("Malformed TEXTMAP lump: unclosed block\n");

                if (block_tok != epi::kTokenIdentifier)
                    FatalError("Malformed TEXTMAP lump: missing key\n");

                if (!lex.Match("="))
                    FatalError("Malformed TEXTMAP lump: missing '='\n");

                block_tok = lex.Next(value);

                if (block_tok == epi::kTokenEOF ||
                    block_tok == epi::kTokenError || value == "}")
                    FatalError("Malformed TEXTMAP lump: missing value\n");

                if (!lex.Match(";"))
                    FatalError("Malformed TEXTMAP lump: missing ';'\n");

                epi::EName key_ename(key, true);

                switch (key_ename.GetIndex())
                {
                    case epi::kENameX:
                        x = epi::LexDouble(value);
                        break;
                    case epi::kENameY:
                        y = epi::LexDouble(value);
                        break;
                    case epi::kENameZfloor:
                        zf = epi::LexDouble(value);
                        break;
                    case epi::kENameZceiling:
                        zc = epi::LexDouble(value);
                        break;
                    default:
                        break;
                }
            }
            level_vertexes[cur_vertex] = {{{{{x, y, zf}}}, zc}};
            cur_vertex++;
        }
        else  // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF) break;
            }
        }
    }
    SYS_ASSERT(cur_vertex == total_level_vertexes);

    LogDebug("LoadUDMFVertexes: finished parsing TEXTMAP\n");
}

static void LoadUDMFSectors()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFSectors: parsing TEXTMAP\n");
    int cur_sector = 0;

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        if (section == "sector")
        {
            int       cz = 0, fz = 0;
            float     fx = 0.0f, fy = 0.0f, cx = 0.0f, cy = 0.0f;
            float     fx_sc = 1.0f, fy_sc = 1.0f, cx_sc = 1.0f, cy_sc = 1.0f;
            float     rf = 0.0f, rc = 0.0f;
            float     gravfactor = 1.0f;
            int       light = 160, type = 0, tag = 0;
            RGBAColor fog_color   = SG_BLACK_RGBA32;
            RGBAColor light_color = SG_WHITE_RGBA32;
            int       fog_density = 0;
            char      floor_tex[10];
            char      ceil_tex[10];
            strcpy(floor_tex, "-");
            strcpy(ceil_tex, "-");
            for (;;)
            {
                if (lex.Match("}")) break;

                std::string    key;
                std::string    value;
                epi::TokenKind block_tok = lex.Next(key);

                if (block_tok == epi::kTokenEOF)
                    FatalError("Malformed TEXTMAP lump: unclosed block\n");

                if (block_tok != epi::kTokenIdentifier)
                    FatalError("Malformed TEXTMAP lump: missing key\n");

                if (!lex.Match("="))
                    FatalError("Malformed TEXTMAP lump: missing '='\n");

                block_tok = lex.Next(value);

                if (block_tok == epi::kTokenEOF ||
                    block_tok == epi::kTokenError || value == "}")
                    FatalError("Malformed TEXTMAP lump: missing value\n");

                if (!lex.Match(";"))
                    FatalError("Malformed TEXTMAP lump: missing ';'\n");

                epi::EName key_ename(key, true);

                switch (key_ename.GetIndex())
                {
                    case epi::kENameHeightfloor:
                        fz = epi::LexInteger(value);
                        break;
                    case epi::kENameHeightceiling:
                        cz = epi::LexInteger(value);
                        break;
                    case epi::kENameTexturefloor:
                        epi::CStringCopyMax(floor_tex, value.c_str(), 8);
                        break;
                    case epi::kENameTextureceiling:
                        epi::CStringCopyMax(ceil_tex, value.c_str(), 8);
                        break;
                    case epi::kENameLightlevel:
                        light = epi::LexInteger(value);
                        break;
                    case epi::kENameSpecial:
                        type = epi::LexInteger(value);
                        break;
                    case epi::kENameId:
                        tag = epi::LexInteger(value);
                        break;
                    case epi::kENameLightcolor:
                        light_color =
                            ((uint32_t)epi::LexInteger(value) << 8 | 0xFF);
                        break;
                    case epi::kENameFadecolor:
                        fog_color =
                            ((uint32_t)epi::LexInteger(value) << 8 | 0xFF);
                        break;
                    case epi::kENameFogdensity:
                        fog_density =
                            HMM_Clamp(0, epi::LexInteger(value), 1020);
                        break;
                    case epi::kENameXpanningfloor:
                        fx = epi::LexDouble(value);
                        break;
                    case epi::kENameYpanningfloor:
                        fy = epi::LexDouble(value);
                        break;
                    case epi::kENameXpanningceiling:
                        cx = epi::LexDouble(value);
                        break;
                    case epi::kENameYpanningceiling:
                        cy = epi::LexDouble(value);
                        break;
                    case epi::kENameXscalefloor:
                        fx_sc = epi::LexDouble(value);
                        break;
                    case epi::kENameYscalefloor:
                        fy_sc = epi::LexDouble(value);
                        break;
                    case epi::kENameXscaleceiling:
                        cx_sc = epi::LexDouble(value);
                        break;
                    case epi::kENameYscaleceiling:
                        cy_sc = epi::LexDouble(value);
                        break;
                    case epi::kENameRotationfloor:
                        rf = epi::LexDouble(value);
                        break;
                    case epi::kENameRotationceiling:
                        rc = epi::LexDouble(value);
                        break;
                    case epi::kENameGravity:
                        gravfactor = epi::LexDouble(value);
                        break;
                    default:
                        break;
                }
            }
            sector_t *ss = level_sectors + cur_sector;
            ss->f_h      = fz;
            ss->c_h      = cz;

            // return to wolfenstein?
            if (goobers.d_)
            {
                ss->f_h = 0;
                ss->c_h = (AlmostEquals(fz, cz)) ? 0 : 128.0f;
            }

            ss->orig_height = (ss->f_h + ss->c_h);

            ss->floor.translucency = VISIBLE;
            ss->floor.x_mat.X      = 1;
            ss->floor.x_mat.Y      = 0;
            ss->floor.y_mat.X      = 0;
            ss->floor.y_mat.Y      = 1;

            ss->ceil = ss->floor;

            // granular offsets
            ss->floor.offset.X += fx;
            ss->floor.offset.Y += fy;
            ss->ceil.offset.X += cx;
            ss->ceil.offset.Y += cy;

            // rotations
            if (!AlmostEquals(rf, 0.0f))
                ss->floor.rotation = epi::BAMFromDegrees(rf);

            if (!AlmostEquals(rc, 0.0f))
                ss->ceil.rotation = epi::BAMFromDegrees(rc);

            // granular scaling
            ss->floor.x_mat.X = fx_sc;
            ss->floor.y_mat.Y = fy_sc;
            ss->ceil.x_mat.X  = cx_sc;
            ss->ceil.y_mat.Y  = cy_sc;

            ss->floor.image = W_ImageLookup(floor_tex, kImageNamespaceFlat);

            if (ss->floor.image)
            {
                FlatDefinition *current_flatdef =
                    flatdefs.Find(ss->floor.image->name.c_str());
                if (current_flatdef)
                {
                    ss->bob_depth  = current_flatdef->bob_depth_;
                    ss->sink_depth = current_flatdef->sink_depth_;
                }
            }

            ss->ceil.image = W_ImageLookup(ceil_tex, kImageNamespaceFlat);

            if (!ss->floor.image)
            {
                LogWarning("Bad Level: sector #%d has missing floor texture.\n",
                           cur_sector);
                ss->floor.image = W_ImageLookup("FLAT1", kImageNamespaceFlat);
            }
            if (!ss->ceil.image)
            {
                LogWarning(
                    "Bad Level: sector #%d has missing ceiling texture.\n",
                    cur_sector);
                ss->ceil.image = ss->floor.image;
            }

            // convert negative tags to zero
            ss->tag = HMM_MAX(0, tag);

            ss->props.lightlevel = light;

            // convert negative types to zero
            ss->props.type    = HMM_MAX(0, type);
            ss->props.special = P_LookupSectorType(ss->props.type);

            ss->exfloor_max = 0;

            ss->props.colourmap = nullptr;

            ss->props.gravity   = kGravityDefault * gravfactor;
            ss->props.friction  = kFrictionDefault;
            ss->props.viscosity = kViscosityDefault;
            ss->props.drag      = kDragDefault;

            // Allow UDMF sector light/fog information to override DDFSECT types
            if (fog_color != SG_BLACK_RGBA32)  // All black is the established
                                               // UDMF "no fog" color
            {
                // Prevent UDMF-specified fog color from having our internal 'no
                // value'...uh...value
                if (fog_color == kRGBANoValue) fog_color ^= 0x00010100;
                ss->props.fog_color = fog_color;
                // Best-effort match for GZDoom's fogdensity values so that UDB,
                // etc give predictable results
                if (fog_density < 2)
                    ss->props.fog_density = 0.002f;
                else
                    ss->props.fog_density =
                        0.01f * ((float)fog_density / 1020.0f);
            }
            else if (ss->props.special &&
                     ss->props.special->fog_color_ != kRGBANoValue)
            {
                ss->props.fog_color   = ss->props.special->fog_color_;
                ss->props.fog_density = 0.01f * ss->props.special->fog_density_;
            }
            else
            {
                ss->props.fog_color   = kRGBANoValue;
                ss->props.fog_density = 0;
            }
            if (light_color != SG_WHITE_RGBA32)
            {
                if (light_color == kRGBANoValue) light_color ^= 0x00010100;
                // Make colormap if necessary
                for (Colormap *cmap : colormaps)
                {
                    if (cmap->gl_color_ != kRGBANoValue &&
                        cmap->gl_color_ == light_color)
                    {
                        ss->props.colourmap = cmap;
                        break;
                    }
                }
                if (!ss->props.colourmap ||
                    ss->props.colourmap->gl_color_ != light_color)
                {
                    Colormap *ad_hoc = new Colormap;
                    ad_hoc->name_ =
                        epi::StringFormat("UDMF_%d", light_color);  // Internal
                    ad_hoc->gl_color_   = light_color;
                    ss->props.colourmap = ad_hoc;
                    colormaps.push_back(ad_hoc);
                }
            }

            ss->p = &ss->props;

            ss->sound_player = -1;

            // -AJA- 1999/07/29: Keep sectors with same tag in a list.
            GroupSectorTags(ss, level_sectors, cur_sector);
            cur_sector++;
        }
        else  // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF) break;
            }
        }
    }
    SYS_ASSERT(cur_sector == total_level_sectors);

    LogDebug("LoadUDMFSectors: finished parsing TEXTMAP\n");
}

static void LoadUDMFSideDefs()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFSectors: parsing TEXTMAP\n");

    level_sides = new side_t[total_level_sides];
    Z_Clear(level_sides, side_t, total_level_sides);

    int nummapsides = 0;

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        if (section == "sidedef")
        {
            nummapsides++;
            int   x = 0, y = 0;
            float lowx = 0.0f, midx = 0.0f, highx = 0.0f;
            float lowy = 0.0f, midy = 0.0f, highy = 0.0f;
            float low_scx = 1.0f, mid_scx = 1.0f, high_scx = 1.0f;
            float low_scy = 1.0f, mid_scy = 1.0f, high_scy = 1.0f;
            int   sec_num = 0;
            char  top_tex[10];
            char  bottom_tex[10];
            char  middle_tex[10];
            strcpy(top_tex, "-");
            strcpy(bottom_tex, "-");
            strcpy(middle_tex, "-");
            for (;;)
            {
                if (lex.Match("}")) break;

                std::string    key;
                std::string    value;
                epi::TokenKind block_tok = lex.Next(key);

                if (block_tok == epi::kTokenEOF)
                    FatalError("Malformed TEXTMAP lump: unclosed block\n");

                if (block_tok != epi::kTokenIdentifier)
                    FatalError("Malformed TEXTMAP lump: missing key\n");

                if (!lex.Match("="))
                    FatalError("Malformed TEXTMAP lump: missing '='\n");

                block_tok = lex.Next(value);

                if (block_tok == epi::kTokenEOF ||
                    block_tok == epi::kTokenError || value == "}")
                    FatalError("Malformed TEXTMAP lump: missing value\n");

                if (!lex.Match(";"))
                    FatalError("Malformed TEXTMAP lump: missing ';'\n");

                epi::EName key_ename(key, true);

                switch (key_ename.GetIndex())
                {
                    case epi::kENameOffsetx:
                        x = epi::LexInteger(value);
                        break;
                    case epi::kENameOffsety:
                        y = epi::LexInteger(value);
                        break;
                    case epi::kENameOffsetx_bottom:
                        lowx = epi::LexDouble(value);
                        break;
                    case epi::kENameOffsetx_mid:
                        midx = epi::LexDouble(value);
                        break;
                    case epi::kENameOffsetx_top:
                        highx = epi::LexDouble(value);
                        break;
                    case epi::kENameOffsety_bottom:
                        lowy = epi::LexDouble(value);
                        break;
                    case epi::kENameOffsety_mid:
                        midy = epi::LexDouble(value);
                        break;
                    case epi::kENameOffsety_top:
                        highy = epi::LexDouble(value);
                        break;
                    case epi::kENameScalex_bottom:
                        low_scx = epi::LexDouble(value);
                        break;
                    case epi::kENameScalex_mid:
                        mid_scx = epi::LexDouble(value);
                        break;
                    case epi::kENameScalex_top:
                        high_scx = epi::LexDouble(value);
                        break;
                    case epi::kENameScaley_bottom:
                        low_scy = epi::LexDouble(value);
                        break;
                    case epi::kENameScaley_mid:
                        mid_scy = epi::LexDouble(value);
                        break;
                    case epi::kENameScaley_top:
                        high_scy = epi::LexDouble(value);
                        break;
                    case epi::kENameTexturetop:
                        epi::CStringCopyMax(top_tex, value.c_str(), 8);
                        break;
                    case epi::kENameTexturebottom:
                        epi::CStringCopyMax(bottom_tex, value.c_str(), 8);
                        break;
                    case epi::kENameTexturemiddle:
                        epi::CStringCopyMax(middle_tex, value.c_str(), 8);
                        break;
                    case epi::kENameSector:
                        sec_num = epi::LexInteger(value);
                        break;
                    default:
                        break;
                }
            }
            SYS_ASSERT(nummapsides <= total_level_sides);  // sanity check

            side_t *sd = level_sides + nummapsides - 1;

            sd->top.translucency = VISIBLE;
            sd->top.offset.X     = x;
            sd->top.offset.Y     = y;
            sd->top.x_mat.X      = 1;
            sd->top.x_mat.Y      = 0;
            sd->top.y_mat.X      = 0;
            sd->top.y_mat.Y      = 1;

            sd->middle = sd->top;
            sd->bottom = sd->top;

            sd->sector = &level_sectors[sec_num];

            sd->top.image =
                W_ImageLookup(top_tex, kImageNamespaceTexture, ILF_Null);

            if (sd->top.image == nullptr)
            {
                if (goobers.d_)
                    sd->top.image =
                        W_ImageLookup(bottom_tex, kImageNamespaceTexture);
                else
                    sd->top.image =
                        W_ImageLookup(top_tex, kImageNamespaceTexture);
            }

            sd->middle.image =
                W_ImageLookup(middle_tex, kImageNamespaceTexture);
            sd->bottom.image =
                W_ImageLookup(bottom_tex, kImageNamespaceTexture);

            // granular offsets
            sd->bottom.offset.X += lowx;
            sd->middle.offset.X += midx;
            sd->top.offset.X += highx;
            sd->bottom.offset.Y += lowy;
            sd->middle.offset.Y += midy;
            sd->top.offset.Y += highy;

            // granular scaling
            sd->bottom.x_mat.X = low_scx;
            sd->middle.x_mat.X = mid_scx;
            sd->top.x_mat.X    = high_scx;
            sd->bottom.y_mat.Y = low_scy;
            sd->middle.y_mat.Y = mid_scy;
            sd->top.y_mat.Y    = high_scy;

            // handle BOOM colormaps with [242] linetype
            sd->top.boom_colmap    = colormaps.Lookup(top_tex);
            sd->middle.boom_colmap = colormaps.Lookup(middle_tex);
            sd->bottom.boom_colmap = colormaps.Lookup(bottom_tex);

            if (sd->top.image &&
                fabs(sd->top.offset.Y) > IM_HEIGHT(sd->top.image))
                sd->top.offset.Y =
                    fmodf(sd->top.offset.Y, IM_HEIGHT(sd->top.image));

            if (sd->middle.image &&
                fabs(sd->middle.offset.Y) > IM_HEIGHT(sd->middle.image))
                sd->middle.offset.Y =
                    fmodf(sd->middle.offset.Y, IM_HEIGHT(sd->middle.image));

            if (sd->bottom.image &&
                fabs(sd->bottom.offset.Y) > IM_HEIGHT(sd->bottom.image))
                sd->bottom.offset.Y =
                    fmodf(sd->bottom.offset.Y, IM_HEIGHT(sd->bottom.image));
        }
        else  // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF) break;
            }
        }
    }

    LogDebug("LoadUDMFSideDefs: post-processing linedefs & sidedefs\n");

    // post-process linedefs & sidedefs

    SYS_ASSERT(temp_line_sides);

    side_t *sd = level_sides;

    for (int i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        int side0 = temp_line_sides[i * 2 + 0];
        int side1 = temp_line_sides[i * 2 + 1];

        SYS_ASSERT(side0 != -1);

        if (side0 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n",
                       current_map->lump_.c_str(), i);
            side0 = nummapsides - 1;
        }

        if (side1 != -1 && side1 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad LEFT side.\n",
                       current_map->lump_.c_str(), i);
            side1 = nummapsides - 1;
        }

        ld->side[0] = sd;
        if (sd->middle.image && (side1 != -1))
        {
            sd->midmask_offset  = sd->middle.offset.Y;
            sd->middle.offset.Y = 0;
        }
        ld->frontsector = sd->sector;
        sd++;

        if (side1 != -1)
        {
            ld->side[1] = sd;
            if (sd->middle.image)
            {
                sd->midmask_offset  = sd->middle.offset.Y;
                sd->middle.offset.Y = 0;
            }
            ld->backsector = sd->sector;
            sd++;
        }

        SYS_ASSERT(sd <= level_sides + total_level_sides);
    }

    SYS_ASSERT(sd == level_sides + total_level_sides);

    LogDebug("LoadUDMFSideDefs: finished parsing TEXTMAP\n");
}

static void LoadUDMFLineDefs()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFLineDefs: parsing TEXTMAP\n");

    int cur_line = 0;

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        if (section == "linedef")
        {
            int flags = 0, v1 = 0, v2 = 0;
            int side0 = -1, side1 = -1, tag = -1;
            int special = 0;
            for (;;)
            {
                if (lex.Match("}")) break;

                std::string    key;
                std::string    value;
                epi::TokenKind block_tok = lex.Next(key);

                if (block_tok == epi::kTokenEOF)
                    FatalError("Malformed TEXTMAP lump: unclosed block\n");

                if (block_tok != epi::kTokenIdentifier)
                    FatalError("Malformed TEXTMAP lump: missing key\n");

                if (!lex.Match("="))
                    FatalError("Malformed TEXTMAP lump: missing '='\n");

                block_tok = lex.Next(value);

                if (block_tok == epi::kTokenEOF ||
                    block_tok == epi::kTokenError || value == "}")
                    FatalError("Malformed TEXTMAP lump: missing value\n");

                if (!lex.Match(";"))
                    FatalError("Malformed TEXTMAP lump: missing ';'\n");

                epi::EName key_ename(key, true);

                switch (key_ename.GetIndex())
                {
                    case epi::kENameId:
                        tag = epi::LexInteger(value);
                        break;
                    case epi::kENameV1:
                        v1 = epi::LexInteger(value);
                        break;
                    case epi::kENameV2:
                        v2 = epi::LexInteger(value);
                        break;
                    case epi::kENameSpecial:
                        special = epi::LexInteger(value);
                        break;
                    case epi::kENameSidefront:
                        side0 = epi::LexInteger(value);
                        break;
                    case epi::kENameSideback:
                        side1 = epi::LexInteger(value);
                        break;
                    case epi::kENameBlocking:
                        flags |= (epi::LexBoolean(value) ? MLF_Blocking : 0);
                        break;
                    case epi::kENameBlockmonsters:
                        flags |=
                            (epi::LexBoolean(value) ? MLF_BlockMonsters : 0);
                        break;
                    case epi::kENameTwosided:
                        flags |= (epi::LexBoolean(value) ? MLF_TwoSided : 0);
                        break;
                    case epi::kENameDontpegtop:
                        flags |=
                            (epi::LexBoolean(value) ? MLF_UpperUnpegged : 0);
                        break;
                    case epi::kENameDontpegbottom:
                        flags |=
                            (epi::LexBoolean(value) ? MLF_LowerUnpegged : 0);
                        break;
                    case epi::kENameSecret:
                        flags |= (epi::LexBoolean(value) ? MLF_Secret : 0);
                        break;
                    case epi::kENameBlocksound:
                        flags |= (epi::LexBoolean(value) ? MLF_SoundBlock : 0);
                        break;
                    case epi::kENameDontdraw:
                        flags |= (epi::LexBoolean(value) ? MLF_DontDraw : 0);
                        break;
                    case epi::kENameMapped:
                        flags |= (epi::LexBoolean(value) ? MLF_Mapped : 0);
                        break;
                    case epi::kENamePassuse:
                        flags |= (epi::LexBoolean(value) ? MLF_PassThru : 0);
                        break;
                    case epi::kENameBlockplayers:
                        flags |=
                            (epi::LexBoolean(value) ? MLF_BlockPlayers : 0);
                        break;
                    case epi::kENameBlocksight:
                        flags |= (epi::LexBoolean(value) ? MLF_SightBlock : 0);
                        break;
                    default:
                        break;
                }
            }
            line_t *ld = level_lines + cur_line;

            ld->flags = flags;
            ld->tag   = HMM_MAX(0, tag);
            ld->v1    = &level_vertexes[v1];
            ld->v2    = &level_vertexes[v2];

            ld->special = P_LookupLineType(HMM_MAX(0, special));

            if (ld->special && ld->special->type_ == kLineTriggerWalkable)
                ld->flags |= MLF_PassThru;

            if (ld->special && ld->special->type_ == kLineTriggerNone &&
                (ld->special->s_xspeed_ || ld->special->s_yspeed_ ||
                 ld->special->scroll_type_ > BoomScrollerTypeNone ||
                 ld->special->line_effect_ == kLineEffectTypeVectorScroll ||
                 ld->special->line_effect_ == kLineEffectTypeOffsetScroll ||
                 ld->special->line_effect_ ==
                     kLineEffectTypeTaggedOffsetScroll))
                ld->flags |= MLF_PassThru;

            if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailFloor)
                ld->flags |= MLF_PassThru;

            if (ld->special &&
                ld->special->slope_type_ & kSlopeTypeDetailCeiling)
                ld->flags |= MLF_PassThru;

            if (ld->special &&
                ld->special ==
                    linetypes.Lookup(0))  // Add passthru to unknown/templated
                ld->flags |= MLF_PassThru;

            ComputeLinedefData(ld, side0, side1);

            if (ld->tag && ld->special && ld->special->ef_.type_)
            {
                for (int j = 0; j < total_level_sectors; j++)
                {
                    if (level_sectors[j].tag != ld->tag) continue;

                    level_sectors[j].exfloor_max++;
                    total_level_extrafloors++;
                }
            }
            cur_line++;
        }
        else  // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF) break;
            }
        }
    }
    SYS_ASSERT(cur_line == total_level_lines);

    LogDebug("LoadUDMFLineDefs: finished parsing TEXTMAP\n");
}

static void LoadUDMFThings()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFThings: parsing TEXTMAP\n");
    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);
            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        if (section == "thing")
        {
            float    x = 0.0f, y = 0.0f, z = 0.0f;
            BAMAngle angle     = kBAMAngle0;
            int      options   = MTF_NOT_SINGLE | MTF_NOT_DM | MTF_NOT_COOP;
            int      typenum   = -1;
            int      tag       = 0;
            float    healthfac = 1.0f;
            float    alpha     = 1.0f;
            float    scale = 0.0f, scalex = 0.0f, scaley = 0.0f;
            const MapObjectDefinition *objtype;
            for (;;)
            {
                if (lex.Match("}")) break;

                std::string    key;
                std::string    value;
                epi::TokenKind block_tok = lex.Next(key);

                if (block_tok == epi::kTokenEOF)
                    FatalError("Malformed TEXTMAP lump: unclosed block\n");

                if (block_tok != epi::kTokenIdentifier)
                    FatalError("Malformed TEXTMAP lump: missing key\n");

                if (!lex.Match("="))
                    FatalError("Malformed TEXTMAP lump: missing '='\n");

                block_tok = lex.Next(value);

                if (block_tok == epi::kTokenEOF ||
                    block_tok == epi::kTokenError || value == "}")
                    FatalError("Malformed TEXTMAP lump: missing value\n");

                if (!lex.Match(";"))
                    FatalError("Malformed TEXTMAP lump: missing ';'\n");

                epi::EName key_ename(key, true);

                switch (key_ename.GetIndex())
                {
                    case epi::kENameId:
                        tag = epi::LexInteger(value);
                        break;
                    case epi::kENameX:
                        x = epi::LexDouble(value);
                        break;
                    case epi::kENameY:
                        y = epi::LexDouble(value);
                        break;
                    case epi::kENameHeight:
                        z = epi::LexDouble(value);
                        break;
                    case epi::kENameAngle:
                        angle = epi::BAMFromDegrees(epi::LexInteger(value));
                        break;
                    case epi::kENameType:
                        typenum = epi::LexInteger(value);
                        break;
                    case epi::kENameSkill1:
                        options |= (epi::LexBoolean(value) ? MTF_EASY : 0);
                        break;
                    case epi::kENameSkill2:
                        options |= (epi::LexBoolean(value) ? MTF_EASY : 0);
                        break;
                    case epi::kENameSkill3:
                        options |= (epi::LexBoolean(value) ? MTF_NORMAL : 0);
                        break;
                    case epi::kENameSkill4:
                        options |= (epi::LexBoolean(value) ? MTF_HARD : 0);
                        break;
                    case epi::kENameSkill5:
                        options |= (epi::LexBoolean(value) ? MTF_HARD : 0);
                        break;
                    case epi::kENameAmbush:
                        options |= (epi::LexBoolean(value) ? MTF_AMBUSH : 0);
                        break;
                    case epi::kENameSingle:
                        options &= (epi::LexBoolean(value) ? ~MTF_NOT_SINGLE
                                                           : options);
                        break;
                    case epi::kENameDm:
                        options &=
                            (epi::LexBoolean(value) ? ~MTF_NOT_DM : options);
                        break;
                    case epi::kENameCoop:
                        options &=
                            (epi::LexBoolean(value) ? ~MTF_NOT_COOP : options);
                        break;
                    case epi::kENameFriend:
                        options |= (epi::LexBoolean(value) ? MTF_FRIEND : 0);
                        break;
                    case epi::kENameHealth:
                        healthfac = epi::LexDouble(value);
                        break;
                    case epi::kENameAlpha:
                        alpha = epi::LexDouble(value);
                        break;
                    case epi::kENameScale:
                        scale = epi::LexDouble(value);
                        break;
                    case epi::kENameScalex:
                        scalex = epi::LexDouble(value);
                        break;
                    case epi::kENameScaley:
                        scaley = epi::LexDouble(value);
                        break;
                    default:
                        break;
                }
            }
            objtype = mobjtypes.Lookup(typenum);

            // MOBJTYPE not found, don't crash out: JDS Compliance.
            // -ACB- 1998/07/21
            if (objtype == nullptr)
            {
                UnknownThingWarning(typenum, x, y);
                continue;
            }

            sector_t *sec = R_PointInSubsector(x, y)->sector;

            if ((objtype->hyper_flags_ & kHyperFlagMusicChanger) &&
                !musinfo_tracks[current_map->name_].processed)
            {
                // This really should only be used with the original DoomEd
                // number range
                if (objtype->number_ >= 14100 && objtype->number_ < 14165)
                {
                    int mus_number = -1;

                    if (objtype->number_ == 14100)  // Default for level
                        mus_number = current_map->music_;
                    else if (musinfo_tracks[current_map->name_].mappings.count(
                                 objtype->number_ - 14100))
                    {
                        mus_number = musinfo_tracks[current_map->name_]
                                         .mappings[objtype->number_ - 14100];
                    }
                    // Track found; make ad-hoc RTS script for music changing
                    if (mus_number != -1)
                    {
                        std::string mus_rts = "// MUSINFO SCRIPTS\n\n";
                        mus_rts.append(epi::StringFormat(
                            "START_MAP %s\n", current_map->name_.c_str()));
                        mus_rts.append(
                            epi::StringFormat("  SECTOR_TRIGGER_INDEX %d\n",
                                              sec - level_sectors));
                        mus_rts.append("    TAGGED_INDEPENDENT\n");
                        mus_rts.append("    TAGGED_REPEATABLE\n");
                        mus_rts.append("    WAIT 30T\n");
                        mus_rts.append(epi::StringFormat(
                            "    CHANGE_MUSIC %d\n", mus_number));
                        mus_rts.append("    RETRIGGER\n");
                        mus_rts.append("  END_SECTOR_TRIGGER\n");
                        mus_rts.append("END_MAP\n\n");
                        RAD_ReadScript(mus_rts, "MUSINFO");
                    }
                }
            }

            if (objtype->flags_ & kMapObjectFlagSpawnCeiling)
                z += sec->c_h - objtype->height_;
            else
                z += sec->f_h;

            MapObject *udmf_thing =
                SpawnMapThing(objtype, x, y, z, sec, angle, options, tag);

            // check for UDMF-specific thing stuff
            if (udmf_thing)
            {
                udmf_thing->target_visibility_ = alpha;
                udmf_thing->alpha_      = alpha;
                if (!AlmostEquals(healthfac, 1.0f))
                {
                    if (healthfac < 0)
                    {
                        udmf_thing->spawn_health_ = fabs(healthfac);
                        udmf_thing->health_      = fabs(healthfac);
                    }
                    else
                    {
                        udmf_thing->spawn_health_ *= healthfac;
                        udmf_thing->health_ *= healthfac;
                    }
                }
                // Treat 'scale' and 'scalex/scaley' as one or the other; don't
                // try to juggle both
                if (!AlmostEquals(scale, 0.0f))
                {
                    udmf_thing->scale_ = udmf_thing->model_scale_ = scale;
                    udmf_thing->height_ *= scale;
                    udmf_thing->radius_ *= scale;
                }
                else if (!AlmostEquals(scalex, 0.0f) ||
                         !AlmostEquals(scaley, 0.0f))
                {
                    float sx = AlmostEquals(scalex, 0.0f) ? 1.0f : scalex;
                    float sy = AlmostEquals(scaley, 0.0f) ? 1.0f : scaley;
                    udmf_thing->scale_ = udmf_thing->model_scale_ = sy;
                    udmf_thing->aspect_ = udmf_thing->model_aspect_ = (sx / sy);
                    udmf_thing->height_ *= sy;
                    udmf_thing->radius_ *= sx;
                }
            }

            total_map_things++;
        }
        else  // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF) break;
            }
        }
    }

    // Mark MUSINFO for this level as done processing, even if it was empty,
    // so we can avoid re-checks
    musinfo_tracks[current_map->name_].processed = true;

    LogDebug("LoadUDMFThings: finished parsing TEXTMAP\n");
}

static void LoadUDMFCounts()
{
    epi::Lexer lex(udmf_lump);

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);

            if (udmf_strict_namespace.d_)
            {
                if (section != "doom" && section != "heretic" &&
                    section != "edge-classic" && section != "zdoomtranslated")
                {
                    LogWarning(
                        "UDMF: %s uses unsupported namespace "
                        "\"%s\"!\nSupported namespaces are \"doom\", "
                        "\"heretic\", \"edge-classic\", or "
                        "\"zdoomtranslated\"!\n",
                        current_map->lump_.c_str(), section.c_str());
                }
            }

            if (!lex.Match(";"))
                FatalError("Malformed TEXTMAP lump: missing ';'\n");
            continue;
        }

        if (!lex.Match("{"))
            FatalError("Malformed TEXTMAP lump: missing '{'\n");

        epi::EName section_ename(section, true);

        // side counts are computed during linedef loading
        switch (section_ename.GetIndex())
        {
            case epi::kENameThing:
                total_map_things++;
                break;
            case epi::kENameVertex:
                total_level_vertexes++;
                break;
            case epi::kENameSector:
                total_level_sectors++;
                break;
            case epi::kENameLinedef:
                total_level_lines++;
                break;
            default:
                break;
        }

        // ignore block contents
        for (;;)
        {
            tok = lex.Next(section);
            if (lex.Match("}") || tok == epi::kTokenEOF) break;
        }
    }

    // initialize arrays
    level_vertexes = new vertex_t[total_level_vertexes];
    level_sectors  = new sector_t[total_level_sectors];
    Z_Clear(level_sectors, sector_t, total_level_sectors);
    level_lines = new line_t[total_level_lines];
    Z_Clear(level_lines, line_t, total_level_lines);
    temp_line_sides = new int[total_level_lines * 2];
}

static void TransferMapSideDef(const raw_sidedef_t *msd, side_t *sd,
                               bool two_sided)
{
    char upper_tex[10];
    char middle_tex[10];
    char lower_tex[10];

    int sec_num = AlignedLittleEndianS16(msd->sector);

    sd->top.translucency = VISIBLE;
    sd->top.offset.X     = AlignedLittleEndianS16(msd->x_offset);
    sd->top.offset.Y     = AlignedLittleEndianS16(msd->y_offset);
    sd->top.x_mat.X      = 1;
    sd->top.x_mat.Y      = 0;
    sd->top.y_mat.X      = 0;
    sd->top.y_mat.Y      = 1;

    sd->middle = sd->top;
    sd->bottom = sd->top;

    if (sec_num < 0)
    {
        LogWarning("Level %s has sidedef with bad sector ref (%d)\n",
                   current_map->lump_.c_str(), sec_num);
        sec_num = 0;
    }
    sd->sector = &level_sectors[sec_num];

    epi::CStringCopyMax(upper_tex, msd->upper_tex, 8);
    epi::CStringCopyMax(middle_tex, msd->mid_tex, 8);
    epi::CStringCopyMax(lower_tex, msd->lower_tex, 8);

    sd->top.image = W_ImageLookup(upper_tex, kImageNamespaceTexture, ILF_Null);

    if (sd->top.image == nullptr)
    {
        if (goobers.d_)
            sd->top.image = W_ImageLookup(upper_tex, kImageNamespaceTexture);
        else
            sd->top.image = W_ImageLookup(upper_tex, kImageNamespaceTexture);
    }

    sd->middle.image = W_ImageLookup(middle_tex, kImageNamespaceTexture);
    sd->bottom.image = W_ImageLookup(lower_tex, kImageNamespaceTexture);

    // handle BOOM colormaps with [242] linetype
    sd->top.boom_colmap    = colormaps.Lookup(upper_tex);
    sd->middle.boom_colmap = colormaps.Lookup(middle_tex);
    sd->bottom.boom_colmap = colormaps.Lookup(lower_tex);

    if (sd->middle.image && two_sided)
    {
        sd->midmask_offset  = sd->middle.offset.Y;
        sd->middle.offset.Y = 0;
    }

    if (sd->top.image && fabs(sd->top.offset.Y) > IM_HEIGHT(sd->top.image))
        sd->top.offset.Y = fmodf(sd->top.offset.Y, IM_HEIGHT(sd->top.image));

    if (sd->middle.image &&
        fabs(sd->middle.offset.Y) > IM_HEIGHT(sd->middle.image))
        sd->middle.offset.Y =
            fmodf(sd->middle.offset.Y, IM_HEIGHT(sd->middle.image));

    if (sd->bottom.image &&
        fabs(sd->bottom.offset.Y) > IM_HEIGHT(sd->bottom.image))
        sd->bottom.offset.Y =
            fmodf(sd->bottom.offset.Y, IM_HEIGHT(sd->bottom.image));
}

static void LoadSideDefs(int lump)
{
    int                  i;
    const uint8_t       *data;
    const raw_sidedef_t *msd;
    side_t              *sd;

    int nummapsides;

    if (!W_VerifyLumpName(lump, "SIDEDEFS"))
        FatalError("Bad WAD: level %s missing SIDEDEFS.\n",
                   current_map->lump_.c_str());

    nummapsides = W_LumpLength(lump) / sizeof(raw_sidedef_t);

    if (nummapsides == 0)
        FatalError("Bad WAD: level %s contains 0 sidedefs.\n",
                   current_map->lump_.c_str());

    level_sides = new side_t[total_level_sides];

    Z_Clear(level_sides, side_t, total_level_sides);

    data = W_LoadLump(lump);
    msd  = (const raw_sidedef_t *)data;

    sd = level_sides;

    SYS_ASSERT(temp_line_sides);

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        int side0 = temp_line_sides[i * 2 + 0];
        int side1 = temp_line_sides[i * 2 + 1];

        SYS_ASSERT(side0 != -1);

        if (side0 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n",
                       current_map->lump_.c_str(), i);
            side0 = nummapsides - 1;
        }

        if (side1 != -1 && side1 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad LEFT side.\n",
                       current_map->lump_.c_str(), i);
            side1 = nummapsides - 1;
        }

        ld->side[0] = sd;
        TransferMapSideDef(msd + side0, sd, (side1 != -1));
        ld->frontsector = sd->sector;
        sd++;

        if (side1 != -1)
        {
            ld->side[1] = sd;
            TransferMapSideDef(msd + side1, sd, true);
            ld->backsector = sd->sector;
            sd++;
        }

        SYS_ASSERT(sd <= level_sides + total_level_sides);
    }

    SYS_ASSERT(sd == level_sides + total_level_sides);

    delete[] data;
}

//
// SetupExtrafloors
//
// This is done after loading sectors (which sets exfloor_max to 0)
// and after loading linedefs (which increases it for each new
// extrafloor).  So now we know the maximum number of extrafloors
// that can ever be needed.
//
// Note: this routine doesn't create any extrafloors (this is done
// later when their linetypes are activated).
//
static void SetupExtrafloors(void)
{
    int       i, ef_index = 0;
    sector_t *ss;

    if (total_level_extrafloors == 0) return;

    level_extrafloors = new extrafloor_t[total_level_extrafloors];

    Z_Clear(level_extrafloors, extrafloor_t, total_level_extrafloors);

    for (i = 0, ss = level_sectors; i < total_level_sectors; i++, ss++)
    {
        ss->exfloor_first = level_extrafloors + ef_index;

        ef_index += ss->exfloor_max;

        SYS_ASSERT(ef_index <= total_level_extrafloors);
    }

    SYS_ASSERT(ef_index == total_level_extrafloors);
}

static void SetupSlidingDoors(void)
{
    for (int i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        if (!ld->special || ld->special->s_.type_ == kSlidingDoorTypeNone)
            continue;

        if (ld->tag == 0 || ld->special->type_ == kLineTriggerManual)
            ld->slide_door = ld->special;
        else
        {
            for (int k = 0; k < total_level_lines; k++)
            {
                line_t *other = level_lines + k;

                if (other->tag != ld->tag || other == ld) continue;

                other->slide_door = ld->special;
            }
        }
    }
}

//
// SetupVertGaps
//
// Computes how many vertical gaps we'll need.
//
static void SetupVertGaps(void)
{
    int i;
    int line_gaps       = 0;
    int sect_sight_gaps = 0;

    vgap_t *cur_gap;

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        ld->max_gaps = ld->backsector ? 1 : 0;

        // factor in extrafloors
        ld->max_gaps += ld->frontsector->exfloor_max;

        if (ld->backsector) ld->max_gaps += ld->backsector->exfloor_max;

        line_gaps += ld->max_gaps;
    }

    for (i = 0; i < total_level_sectors; i++)
    {
        sector_t *sec = level_sectors + i;

        sec->max_gaps = sec->exfloor_max + 1;

        sect_sight_gaps += sec->max_gaps;
    }

    total_level_vertical_gaps = line_gaps + sect_sight_gaps;

    // LogPrint("%dK used for vert gaps.\n", (total_level_vertical_gaps *
    //    sizeof(vgap_t) + 1023) / 1024);

    // zero is now impossible
    SYS_ASSERT(total_level_vertical_gaps > 0);

    level_vertical_gaps = new vgap_t[total_level_vertical_gaps];

    Z_Clear(level_vertical_gaps, vgap_t, total_level_vertical_gaps);

    for (i = 0, cur_gap = level_vertical_gaps; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        if (ld->max_gaps == 0) continue;

        ld->gaps = cur_gap;
        cur_gap += ld->max_gaps;
    }

    SYS_ASSERT(cur_gap == (level_vertical_gaps + line_gaps));

    for (i = 0; i < total_level_sectors; i++)
    {
        sector_t *sec = level_sectors + i;

        if (sec->max_gaps == 0) continue;

        sec->sight_gaps = cur_gap;
        cur_gap += sec->max_gaps;
    }

    SYS_ASSERT(cur_gap == (level_vertical_gaps + total_level_vertical_gaps));
}

static void DetectDeepWaterTrick(void)
{
    uint8_t *self_subs = new uint8_t[total_level_subsectors];

    memset(self_subs, 0, total_level_subsectors);

    for (int i = 0; i < total_level_segs; i++)
    {
        const seg_t *seg = level_segs + i;

        if (seg->miniseg) continue;

        SYS_ASSERT(seg->front_sub);

        if (seg->linedef->backsector &&
            seg->linedef->frontsector == seg->linedef->backsector)
        {
            self_subs[seg->front_sub - level_subsectors] |= 1;
        }
        else { self_subs[seg->front_sub - level_subsectors] |= 2; }
    }

    int count;
    int pass = 0;

    do {
        pass++;

        count = 0;

        for (int j = 0; j < total_level_subsectors; j++)
        {
            subsector_t *sub = level_subsectors + j;
            const seg_t *seg;

            if (self_subs[j] != 1) continue;
#if 0
			LogDebug("Subsector [%d] @ (%1.0f,%1.0f) sec %d --> %d\n", j,
				(sub->bbox[kBoundingBoxLeft] + sub->bbox[kBoundingBoxRight]) / 2.0,
				(sub->bbox[kBoundingBoxBottom] + sub->bbox[kBoundingBoxTop]) / 2.0,
				sub->sector - sectors, self_subs[j]);
#endif
            const seg_t *Xseg = 0;

            for (seg = sub->segs; seg; seg = seg->sub_next)
            {
                SYS_ASSERT(seg->back_sub);

                int k = seg->back_sub - level_subsectors;
#if 0
				LogDebug("  Seg [%d] back_sub %d (back_sect %d)\n", seg - segs, k,
					seg->back_sub->sector - sectors);
#endif
                if (self_subs[k] & 2)
                {
                    if (!Xseg) Xseg = seg;
                }
            }

            if (Xseg)
            {
                sub->deep_ref = Xseg->back_sub->deep_ref
                                    ? Xseg->back_sub->deep_ref
                                    : Xseg->back_sub->sector;
#if 0
				LogDebug("  Updating (from seg %d) --> SEC %d\n", Xseg - segs,
					sub->deep_ref - sectors);
#endif
                self_subs[j] = 3;

                count++;
            }
        }
    } while (count > 0 && pass < 100);

    delete[] self_subs;
}

static void DoBlockMap()
{
    int min_x = (int)level_vertexes[0].X;
    int min_y = (int)level_vertexes[0].Y;

    int max_x = (int)level_vertexes[0].X;
    int max_y = (int)level_vertexes[0].Y;

    for (int i = 1; i < total_level_vertexes; i++)
    {
        vertex_t *v = level_vertexes + i;

        min_x = HMM_MIN((int)v->X, min_x);
        min_y = HMM_MIN((int)v->Y, min_y);
        max_x = HMM_MAX((int)v->X, max_x);
        max_y = HMM_MAX((int)v->Y, max_y);
    }

    GenerateBlockmap(min_x, min_y, max_x, max_y);

    CreateThingBlockmap();
}

//
// GroupLines
//
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void GroupLines(void)
{
    int       i;
    int       j;
    int       total;
    line_t   *li;
    sector_t *sector;
    seg_t    *seg;
    float     bbox[4];
    line_t  **line_p;

    // setup remaining seg information
    for (i = 0, seg = level_segs; i < total_level_segs; i++, seg++)
    {
        if (seg->partner) seg->back_sub = seg->partner->front_sub;

        if (!seg->frontsector) seg->frontsector = seg->front_sub->sector;

        if (!seg->backsector && seg->back_sub)
            seg->backsector = seg->back_sub->sector;
    }

    // count number of lines in each sector
    li    = level_lines;
    total = 0;
    for (i = 0; i < total_level_lines; i++, li++)
    {
        total++;
        li->frontsector->linecount++;

        if (li->backsector && li->backsector != li->frontsector)
        {
            total++;
            li->backsector->linecount++;
        }
    }

    // build line tables for each sector
    level_line_buffer = new line_t *[total];

    line_p = level_line_buffer;
    sector = level_sectors;

    for (i = 0; i < total_level_sectors; i++, sector++)
    {
        BoundingBoxClear(bbox);
        sector->lines = line_p;
        li            = level_lines;
        for (j = 0; j < total_level_lines; j++, li++)
        {
            if (li->frontsector == sector || li->backsector == sector)
            {
                *line_p++ = li;
                BoundingBoxAddPoint(bbox, li->v1->X, li->v1->Y);
                BoundingBoxAddPoint(bbox, li->v2->X, li->v2->Y);
            }
        }
        if (line_p - sector->lines != sector->linecount)
            FatalError("GroupLines: miscounted");

        // Allow vertex slope if a triangular sector or a rectangular
        // sector in which two adjacent verts have an identical z-height
        // and the other two have it unset
        if (sector->linecount == 3 && udmf_level)
        {
            sector->floor_vs_hilo = {{-40000, 40000}};
            sector->ceil_vs_hilo  = {{-40000, 40000}};
            for (j = 0; j < 3; j++)
            {
                vertex_t *vert   = sector->lines[j]->v1;
                bool      add_it = true;
                for (HMM_Vec3 v : sector->floor_z_verts)
                    if (AlmostEquals(v.X, vert->X) &&
                        AlmostEquals(v.Y, vert->Y))
                        add_it = false;
                if (add_it)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_vertex_slope = true;
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vs_hilo.X)
                            sector->floor_vs_hilo.X = vert->Z;
                        if (vert->Z < sector->floor_vs_hilo.Y)
                            sector->floor_vs_hilo.Y = vert->Z;
                    }
                    else
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, sector->f_h}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceil_vertex_slope = true;
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceil_vs_hilo.X)
                            sector->ceil_vs_hilo.X = vert->W;
                        if (vert->W < sector->ceil_vs_hilo.Y)
                            sector->ceil_vs_hilo.Y = vert->W;
                    }
                    else
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, sector->c_h}});
                }
                vert   = sector->lines[j]->v2;
                add_it = true;
                for (HMM_Vec3 v : sector->floor_z_verts)
                    if (AlmostEquals(v.X, vert->X) &&
                        AlmostEquals(v.Y, vert->Y))
                        add_it = false;
                if (add_it)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_vertex_slope = true;
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vs_hilo.X)
                            sector->floor_vs_hilo.X = vert->Z;
                        if (vert->Z < sector->floor_vs_hilo.Y)
                            sector->floor_vs_hilo.Y = vert->Z;
                    }
                    else
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, sector->f_h}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceil_vertex_slope = true;
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceil_vs_hilo.X)
                            sector->ceil_vs_hilo.X = vert->W;
                        if (vert->W < sector->ceil_vs_hilo.Y)
                            sector->ceil_vs_hilo.Y = vert->W;
                    }
                    else
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, sector->c_h}});
                }
            }
            if (!sector->floor_vertex_slope)
                sector->floor_z_verts.clear();
            else
            {
                sector->floor_vs_normal = MathTripleCrossProduct(
                    sector->floor_z_verts[0], sector->floor_z_verts[1],
                    sector->floor_z_verts[2]);
                if (sector->f_h > sector->floor_vs_hilo.X)
                    sector->floor_vs_hilo.X = sector->f_h;
                if (sector->f_h < sector->floor_vs_hilo.Y)
                    sector->floor_vs_hilo.Y = sector->f_h;
            }
            if (!sector->ceil_vertex_slope)
                sector->ceil_z_verts.clear();
            else
            {
                sector->ceil_vs_normal = MathTripleCrossProduct(
                    sector->ceil_z_verts[0], sector->ceil_z_verts[1],
                    sector->ceil_z_verts[2]);
                if (sector->c_h < sector->ceil_vs_hilo.Y)
                    sector->ceil_vs_hilo.Y = sector->c_h;
                if (sector->c_h > sector->ceil_vs_hilo.X)
                    sector->ceil_vs_hilo.X = sector->c_h;
            }
        }
        if (sector->linecount == 4 && udmf_level)
        {
            int floor_z_lines     = 0;
            int ceil_z_lines      = 0;
            sector->floor_vs_hilo = {{-40000, 40000}};
            sector->ceil_vs_hilo  = {{-40000, 40000}};
            for (j = 0; j < 4; j++)
            {
                vertex_t *vert      = sector->lines[j]->v1;
                vertex_t *vert2     = sector->lines[j]->v2;
                bool      add_it_v1 = true;
                bool      add_it_v2 = true;
                for (HMM_Vec3 v : sector->floor_z_verts)
                    if (AlmostEquals(v.X, vert->X) &&
                        AlmostEquals(v.Y, vert->Y))
                        add_it_v1 = false;
                for (HMM_Vec3 v : sector->floor_z_verts)
                    if (AlmostEquals(v.X, vert2->X) &&
                        AlmostEquals(v.Y, vert2->Y))
                        add_it_v2 = false;
                if (add_it_v1)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vs_hilo.X)
                            sector->floor_vs_hilo.X = vert->Z;
                        if (vert->Z < sector->floor_vs_hilo.Y)
                            sector->floor_vs_hilo.Y = vert->Z;
                    }
                    else
                        sector->floor_z_verts.push_back(
                            {{vert->X, vert->Y, sector->f_h}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceil_vs_hilo.X)
                            sector->ceil_vs_hilo.X = vert->W;
                        if (vert->W < sector->ceil_vs_hilo.Y)
                            sector->ceil_vs_hilo.Y = vert->W;
                    }
                    else
                        sector->ceil_z_verts.push_back(
                            {{vert->X, vert->Y, sector->c_h}});
                }
                if (add_it_v2)
                {
                    if (vert2->Z < 32767.0f && vert2->Z > -32768.0f)
                    {
                        sector->floor_z_verts.push_back(
                            {{vert2->X, vert2->Y, vert2->Z}});
                        if (vert2->Z > sector->floor_vs_hilo.X)
                            sector->floor_vs_hilo.X = vert2->Z;
                        if (vert2->Z < sector->floor_vs_hilo.Y)
                            sector->floor_vs_hilo.Y = vert2->Z;
                    }
                    else
                        sector->floor_z_verts.push_back(
                            {{vert2->X, vert2->Y, sector->f_h}});
                    if (vert2->W < 32767.0f && vert2->W > -32768.0f)
                    {
                        sector->ceil_z_verts.push_back(
                            {{vert2->X, vert2->Y, vert2->W}});
                        if (vert2->W > sector->ceil_vs_hilo.X)
                            sector->ceil_vs_hilo.X = vert2->W;
                        if (vert2->W < sector->ceil_vs_hilo.Y)
                            sector->ceil_vs_hilo.Y = vert2->W;
                    }
                    else
                        sector->ceil_z_verts.push_back(
                            {{vert2->X, vert2->Y, sector->c_h}});
                }
                if ((vert->Z < 32767.0f && vert->Z > -32768.0f) &&
                    (vert2->Z < 32767.0f && vert2->Z > -32768.0f) &&
                    AlmostEquals(vert->Z, vert2->Z))
                {
                    floor_z_lines++;
                }
                if ((vert->W < 32767.0f && vert->W > -32768.0f) &&
                    (vert2->W < 32767.0f && vert2->W > -32768.0f) &&
                    AlmostEquals(vert->W, vert2->W))
                {
                    ceil_z_lines++;
                }
            }
            if (floor_z_lines == 1 && sector->floor_z_verts.size() == 4)
            {
                sector->floor_vertex_slope = true;
                sector->floor_vs_normal    = MathTripleCrossProduct(
                    sector->floor_z_verts[0], sector->floor_z_verts[1],
                    sector->floor_z_verts[2]);
                if (sector->f_h > sector->floor_vs_hilo.X)
                    sector->floor_vs_hilo.X = sector->f_h;
                if (sector->f_h < sector->floor_vs_hilo.Y)
                    sector->floor_vs_hilo.Y = sector->f_h;
            }
            else
                sector->floor_z_verts.clear();
            if (ceil_z_lines == 1 && sector->ceil_z_verts.size() == 4)
            {
                sector->ceil_vertex_slope = true;
                sector->ceil_vs_normal    = MathTripleCrossProduct(
                    sector->ceil_z_verts[0], sector->ceil_z_verts[1],
                    sector->ceil_z_verts[2]);
                if (sector->c_h < sector->ceil_vs_hilo.Y)
                    sector->ceil_vs_hilo.Y = sector->c_h;
                if (sector->c_h > sector->ceil_vs_hilo.X)
                    sector->ceil_vs_hilo.X = sector->c_h;
            }
            else
                sector->ceil_z_verts.clear();
        }

        // set the degenmobj_t to the middle of the bounding box
        sector->sfx_origin.x =
            (bbox[kBoundingBoxRight] + bbox[kBoundingBoxLeft]) / 2;
        sector->sfx_origin.y =
            (bbox[kBoundingBoxTop] + bbox[kBoundingBoxBottom]) / 2;
        sector->sfx_origin.z = (sector->f_h + sector->c_h) / 2;
    }
}

static inline void AddSectorToVertices(int *branches, line_t *ld, sector_t *sec)
{
    if (!sec) return;

    unsigned short sec_idx = sec - level_sectors;

    for (int vert = 0; vert < 2; vert++)
    {
        int v_idx = (vert ? ld->v2 : ld->v1) - level_vertexes;

        SYS_ASSERT(0 <= v_idx && v_idx < total_level_vertexes);

        if (branches[v_idx] < 0) continue;

        vertex_seclist_t *L = level_vertex_sector_lists + branches[v_idx];

        if (L->num >= SECLIST_MAX) continue;

        int pos;

        for (pos = 0; pos < L->num; pos++)
            if (L->sec[pos] == sec_idx) break;

        if (pos < L->num) continue;  // already in there

        L->sec[L->num++] = sec_idx;
    }
}

static void CreateVertexSeclists(void)
{
    // step 1: determine number of linedef branches at each vertex
    int *branches = new int[total_level_vertexes];

    Z_Clear(branches, int, total_level_vertexes);

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        int v1_idx = level_lines[i].v1 - level_vertexes;
        int v2_idx = level_lines[i].v2 - level_vertexes;

        SYS_ASSERT(0 <= v1_idx && v1_idx < total_level_vertexes);
        SYS_ASSERT(0 <= v2_idx && v2_idx < total_level_vertexes);

        branches[v1_idx] += 1;
        branches[v2_idx] += 1;
    }

    // step 2: count how many vertices have 3 or more branches,
    //         and simultaneously give them index numbers.
    int num_triples = 0;

    for (i = 0; i < total_level_vertexes; i++)
    {
        if (branches[i] < 3)
            branches[i] = -1;
        else
            branches[i] = num_triples++;
    }

    if (num_triples == 0)
    {
        delete[] branches;

        level_vertex_sector_lists = nullptr;
        return;
    }

    // step 3: create a vertex_seclist for those multi-branches
    level_vertex_sector_lists = new vertex_seclist_t[num_triples];

    Z_Clear(level_vertex_sector_lists, vertex_seclist_t, num_triples);

    LogDebug("Created %d seclists from %d vertices (%1.1f%%)\n", num_triples,
             total_level_vertexes,
             num_triples * 100 / (float)total_level_vertexes);

    // multiple passes for each linedef:
    //   pass #1 takes care of normal sectors
    //   pass #2 handles any extrafloors
    //
    // Rationale: normal sectors are more important, hence they
    //            should be allocated to the limited slots first.

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        for (int side = 0; side < 2; side++)
        {
            sector_t *sec = side ? ld->backsector : ld->frontsector;

            AddSectorToVertices(branches, ld, sec);
        }
    }

    for (i = 0; i < total_level_lines; i++)
    {
        line_t *ld = level_lines + i;

        for (int side = 0; side < 2; side++)
        {
            sector_t *sec = side ? ld->backsector : ld->frontsector;

            if (!sec) continue;

            extrafloor_t *ef;

            for (ef = sec->bottom_ef; ef; ef = ef->higher)
                AddSectorToVertices(branches, ld, ef->ef_line->frontsector);

            for (ef = sec->bottom_liq; ef; ef = ef->higher)
                AddSectorToVertices(branches, ld, ef->ef_line->frontsector);
        }
    }

    // step 4: finally, update the segs that touch those vertices
    for (i = 0; i < total_level_segs; i++)
    {
        seg_t *sg = level_segs + i;

        for (int vert = 0; vert < 2; vert++)
        {
            int v_idx = (vert ? sg->v2 : sg->v1) - level_vertexes;

            // skip GL vertices
            if (v_idx < 0 || v_idx >= total_level_vertexes) continue;

            if (branches[v_idx] < 0) continue;

            sg->nb_sec[vert] = level_vertex_sector_lists + branches[v_idx];
        }
    }

    delete[] branches;
}

static void P_RemoveSectorStuff(void)
{
    int i;

    for (i = 0; i < total_level_sectors; i++)
    {
        FreeSectorTouchNodes(level_sectors + i);

        // Might still be playing a sound.
        S_StopFX(&level_sectors[i].sfx_origin);
    }
}

void ShutdownLevel(void)
{
    // Destroys everything on the level.

#ifdef DEVELOPERS
    if (!level_active) FatalError("ShutdownLevel: no level to shut down!");
#endif

    level_active = false;

    P_RemoveItemsInQue();

    P_RemoveSectorStuff();

    S_StopLevelFX();

    DestroyAllForces();
    DestroyAllLights();
    DestroyAllPlanes();
    DestroyAllSliders();
    DestroyAllAmbientSounds();

    DDF_BoomClearGenTypes();

    delete[] level_segs;
    level_segs = nullptr;
    delete[] level_nodes;
    level_nodes = nullptr;
    delete[] level_vertexes;
    level_vertexes = nullptr;
    delete[] level_sides;
    level_sides = nullptr;
    delete[] level_lines;
    level_lines = nullptr;
    for (int i = 0; i < total_level_sectors; i++)
    {
        sector_t *sec = &level_sectors[i];
        if (sec->f_slope)
        {
            delete sec->f_slope;
            sec->f_slope = nullptr;
        }
        if (sec->c_slope)
        {
            delete sec->c_slope;
            sec->c_slope = nullptr;
        }
    }
    delete[] level_sectors;
    level_sectors = nullptr;
    delete[] level_subsectors;
    level_subsectors = nullptr;

    delete[] level_gl_vertexes;
    level_gl_vertexes = nullptr;
    delete[] level_extrafloors;
    level_extrafloors = nullptr;
    delete[] level_vertical_gaps;
    level_vertical_gaps = nullptr;
    delete[] level_line_buffer;
    level_line_buffer = nullptr;
    delete[] level_vertex_sector_lists;
    level_vertex_sector_lists = nullptr;

    DestroyBlockmap();

    P_RemoveAllMobjs(false);
}

void LevelSetup(void)
{
    // Sets up the current level using the skill passed and the
    // information in current_map.
    //
    // -ACB- 1998/08/09 Use current_map to ref lump and par time

    if (level_active) ShutdownLevel();

    // -ACB- 1998/08/27 nullptr the head pointers for the linked lists....
    respawn_queue_head  = nullptr;
    map_object_list_head = nullptr;
    seen_monsters.clear();

    // get lump for map header e.g. MAP01
    int lumpnum = W_CheckNumForName_MAP(current_map->lump_.c_str());
    if (lumpnum < 0)
        FatalError("No such level: %s\n", current_map->lump_.c_str());

    // get lump for XGL3 nodes from an XWA file
    int xgl_lump = W_CheckNumForName_XGL(current_map->lump_.c_str());

    // ignore XGL nodes if it occurs _before_ the normal level marker.
    // [ something has gone horribly wrong if this happens! ]
    if (xgl_lump < lumpnum) xgl_lump = -1;

    // shouldn't happen (as during startup we checked for XWA files)
    if (xgl_lump < 0) FatalError("Internal error: missing XGL nodes.\n");

    // -CW- 2017/01/29: check for UDMF map lump
    if (W_VerifyLumpName(lumpnum + 1, "TEXTMAP"))
    {
        udmf_level          = true;
        udmf_lump_number    = lumpnum + 1;
        int      raw_length = 0;
        uint8_t *raw_udmf   = W_LoadLump(udmf_lump_number, &raw_length);
        udmf_lump.clear();
        udmf_lump.resize(raw_length);
        memcpy(udmf_lump.data(), raw_udmf, raw_length);
        if (udmf_lump.empty())
            FatalError("Internal error: can't load UDMF lump.\n");
        delete[] raw_udmf;
    }
    else
    {
        udmf_level       = false;
        udmf_lump_number = -1;
    }

    // clear CRC values
    map_sectors_crc.Reset();
    map_lines_crc.Reset();
    map_things_crc.Reset();

    // note: most of this ordering is important
    // 23-6-98 KM, eg, Sectors must be loaded before sidedefs,
    // Vertexes must be loaded before LineDefs,
    // LineDefs + Vertexes must be loaded before BlockMap,
    // Sectors must be loaded before Segs

    total_level_sides         = 0;
    total_level_extrafloors   = 0;
    total_level_vertical_gaps = 0;
    total_map_things          = 0;
    total_level_vertexes      = 0;
    total_level_sectors       = 0;
    total_level_lines         = 0;

    if (!udmf_level)
    {
        // check if the level is for Hexen
        hexen_level = false;

        if (W_VerifyLump(lumpnum + ML_BEHAVIOR) &&
            W_VerifyLumpName(lumpnum + ML_BEHAVIOR, "BEHAVIOR"))
        {
            LogDebug("Detected Hexen level.\n");
            hexen_level = true;
        }

        LoadVertexes(lumpnum + ML_VERTEXES);
        LoadSectors(lumpnum + ML_SECTORS);

        if (hexen_level)
            LoadHexenLineDefs(lumpnum + ML_LINEDEFS);
        else
            LoadLineDefs(lumpnum + ML_LINEDEFS);

        LoadSideDefs(lumpnum + ML_SIDEDEFS);
    }
    else
    {
        LoadUDMFCounts();
        LoadUDMFVertexes();
        LoadUDMFSectors();
        LoadUDMFLineDefs();
        LoadUDMFSideDefs();
    }

    SetupExtrafloors();
    SetupSlidingDoors();
    SetupVertGaps();

    delete[] temp_line_sides;

    LoadXGL3Nodes(xgl_lump);

    // REJECT is ignored, and we generate our own BLOCKMAP

    DoBlockMap();

    GroupLines();

    DetectDeepWaterTrick();

    R_ComputeSkyHeights();

    // compute sector and line gaps
    for (int j = 0; j < total_level_sectors; j++)
        P_RecomputeGapsAroundSector(level_sectors + j);

    GameClearBodyQueue();

    // set up world state
    // (must be before loading things to create Extrafloors)
    SpawnMapSpecials1();

    // -AJA- 1999/10/21: Clear out player starts (ready to load).
    GameClearPlayerStarts();

    unknown_thing_map.clear();

    // Must do before loading things
    GetMusinfoTracksForLevel();

    if (!udmf_level)
    {
        if (hexen_level)
            LoadHexenThings(lumpnum + ML_THINGS);
        else
            LoadThings(lumpnum + ML_THINGS);
    }
    else
        LoadUDMFThings();

        // OK, CRC values have now been computed
#ifdef DEVELOPERS
    LogDebug("MAP CRCS: S=%08x L=%08x T=%08x\n", map_sectors_crc.crc,
             map_lines_crc.crc, map_things_crc.crc);
#endif

    CreateVertexSeclists();

    SpawnMapSpecials2(current_map->autotag_);

    AutomapInitLevel();

    RGL_UpdateSkyBoxTextures();

    // preload graphics
    if (precache) W_PrecacheLevel();

    // setup categories based on game mode (SP/COOP/DM)
    S_ChangeChannelNum();

    // FIXME: cache sounds (esp. for player)

    S_ChangeMusic(current_map->music_, true);  // start level music

    level_active = true;
}

void PlayerStateInit(void)
{
    E_ProgressMessage(language["PlayState"]);

    // There should not yet exist a player
    SYS_ASSERT(numplayers == 0);

    GameClearPlayerStarts();
}

LineType *P_LookupLineType(int num)
{
    if (num <= 0) return nullptr;

    LineType *def = linetypes.Lookup(num);

    // DDF types always override
    if (def) return def;

    if (DDF_IsBoomLineType(num)) return DDF_BoomGetGenLine(num);

    LogWarning("P_LookupLineType(): Unknown linedef type %d\n", num);

    return linetypes.Lookup(0);  // template line
}

SectorType *P_LookupSectorType(int num)
{
    if (num <= 0) return nullptr;

    SectorType *def = sectortypes.Lookup(num);

    // DDF types always override
    if (def) return def;

    if (DDF_IsBoomSectorType(num)) return DDF_BoomGetGenSector(num);

    LogWarning("P_LookupSectorType(): Unknown sector type %d\n", num);

    return sectortypes.Lookup(0);  // template sector
}

void LevelShutdown(void)
{
    if (level_active) { ShutdownLevel(); }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
