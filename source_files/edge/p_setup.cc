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
#include "ddf_colormap.h"
#include "ddf_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "epi_crc.h"
#include "epi_doomdefs.h"
#include "epi_ename.h"
#include "epi_endian.h"
#include "epi_lexer.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_math.h"
#include "m_misc.h"
#include "m_random.h"
#include "miniz.h" // ZGL3 nodes
#include "p_local.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_sky.h"
#include "rad_trig.h" // MUSINFO changers
#include "s_music.h"
#include "s_sound.h"
#include "sokol_color.h"
#include "sv_main.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"

#define EDGE_SEG_INVALID       ((Seg *)-3)
#define EDGE_SUBSECTOR_INVALID ((Subsector *)-3)

extern unsigned int root_node;

static bool level_active = false;

EDGE_DEFINE_CONSOLE_VARIABLE(udmf_strict_namespace, "0", kConsoleVariableFlagArchive)

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

int                 total_level_vertexes;
Vertex             *level_vertexes = nullptr;
static Vertex      *level_gl_vertexes;
int                 total_level_segs;
Seg                *level_segs;
int                 total_level_sectors;
Sector             *level_sectors;
int                 total_level_subsectors;
Subsector          *level_subsectors;
int                 total_level_extrafloors;
Extrafloor         *level_extrafloors;
int                 total_level_nodes;
BspNode            *level_nodes;
int                 total_level_lines;
Line               *level_lines;
int                 total_level_sides;
Side               *level_sides;
static int          total_level_vertical_gaps;
static VerticalGap *level_vertical_gaps;

VertexSectorList *level_vertex_sector_lists;

static Line **level_line_buffer = nullptr;

// bbox used
static float dummy_bounding_box[4];

epi::CRC32 map_sectors_crc;
epi::CRC32 map_lines_crc;
epi::CRC32 map_things_crc;

int total_map_things;

static bool        udmf_level;
static int         udmf_lump_number;
static std::string udmf_lump;

// a place to store sidedef numbers of the loaded linedefs.
// There is two values for every line: side0 and side1.
static int *temp_line_sides;

EDGE_DEFINE_CONSOLE_VARIABLE(goobers, "0", kConsoleVariableFlagNone)

// "MUSINFO" is used here to refer to the traditional MUSINFO lump
struct MUSINFOMapping
{
    std::unordered_map<int, int> mappings;
    bool                         processed = false;
};

// This is wonky, but essentially the idea is to not continually create
// duplicate RTS music changing scripts for the same level if warping back and
// forth, or using a hub or somesuch that happens to have music changers
static std::unordered_map<std::string, MUSINFOMapping> musinfo_tracks;

static void GetMUSINFOTracksForLevel(void)
{
    if (musinfo_tracks.count(current_map->name_) && musinfo_tracks[current_map->name_].processed)
        return;
    int      raw_length  = 0;
    uint8_t *raw_musinfo = OpenPackOrLumpInMemory("MUSINFO", {".txt"}, &raw_length);
    if (!raw_musinfo)
        return;
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

        if (tok != epi::kTokenNumber && tok != epi::kTokenIdentifier)
            break;

        if (epi::StringCaseCompareASCII(section, current_map->name_) != 0)
            continue;

        // Parse "block" for current map
        int mus_number = -1;
        for (;;)
        {
            std::string    value;
            epi::TokenKind block_tok = lex.Next(value);

            if (block_tok != epi::kTokenNumber && block_tok != epi::kTokenIdentifier)
                return;

            // A valid map name should be the end of this block
            if (mapdefs.Lookup(value.c_str()))
                return;

            // This does have a bit of faith that the MUSINFO lump isn't
            // malformed
            if (mus_number == -1)
                mus_number = epi::LexInteger(value);
            else
            {
                // This mimics Lobo's ad-hoc playlist stuff for UMAPINFO
                int ddf_track = playlist.FindLast(value.c_str());
                if (ddf_track != -1) // Entry exists
                {
                    musinfo_tracks[current_map->name_].mappings.try_emplace(mus_number, ddf_track);
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
                    musinfo_tracks[current_map->name_].mappings.try_emplace(mus_number, dynamic_plentry->number_);
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

    static const uint8_t Y_key_data[] = {0x59, 0xf5, 0x48, 0xf8, 0, 0, 6, 0, 0x17, 0};

    static const int Y_key_offset = 0x125C;

    if (length < Y_key_offset + 10)
        return;

    data += Y_key_offset;

    if (memcmp(data, Y_key_data, 10) != 0)
        return;

    LogPrint("Detected TNT MAP31 bug, adding fix.\n");

    data[8] &= ~kThingNotSinglePlayer;
}

static void CheckDoom2Map05Bug(uint8_t *data, int length)
{
    // The IWAD for Doom2 has a bug in MAP05 where 2 sectors
    // are incorrectly tagged 9.  This fixes it.

    static const uint8_t sector_4_data[] = {0x60, 0,    0xc8, 0,    0x46, 0x4c, 0x41, 0x54, 0x31, 0, 0, 0, 0x46,
                                            0x4c, 0x41, 0x54, 0x31, 0x30, 0,    0,    0x70, 0,    0, 0, 9, 0};

    static const uint8_t sector_153_data[] = {0x98, 0,    0xe8, 0,    0x46, 0x4c, 0x41, 0x54, 0x31, 0, 0, 0, 0x46,
                                              0x4c, 0x41, 0x54, 0x31, 0x30, 0,    0,    0x70, 0,    9, 0, 9, 0};

    static const int sector_4_offset   = 0x68; // 104
    static const int sector_153_offset = 3978; // 0xf8a; //3978

    if (length < sector_4_offset + 26)
        return;

    if (length < sector_153_offset + 26)
        return;

    // Sector 4 first
    data += sector_4_offset;

    if (memcmp(data, sector_4_data, 26) != 0)
        return;

    if (data[24] == 9) // check just in case
        data[24] = 0;  // set tag to 0 instead of 9

    // now sector 153
    data += (sector_153_offset - sector_4_offset);

    if (memcmp(data, sector_153_data, 26) != 0)
        return;

    if (data[24] == 9) // check just in case
        data[24] = 0;  // set tag to 0 instead of 9

    LogPrint("Detected Doom2 MAP05 bug, adding fix.\n");
}

static void LoadVertexes(int lump)
{
    const uint8_t   *data;
    int              i;
    const RawVertex *ml;
    Vertex          *li;

    if (!VerifyLump(lump, "VERTEXES"))
        FatalError("Bad WAD: level %s missing VERTEXES.\n", current_map->lump_.c_str());

    // Determine number of lumps:
    //  total lump length / vertex record length.
    total_level_vertexes = GetLumpLength(lump) / sizeof(RawVertex);

    if (total_level_vertexes == 0)
        FatalError("Bad WAD: level %s contains 0 vertexes.\n", current_map->lump_.c_str());

    level_vertexes = new Vertex[total_level_vertexes];

    // Load data into cache.
    data = LoadLumpIntoMemory(lump);

    ml = (const RawVertex *)data;
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

static void SegCommonStuff(Seg *seg, int linedef_in)
{
    seg->front_sector = seg->back_sector = nullptr;

    if (linedef_in == -1)
    {
        seg->miniseg = true;
    }
    else
    {
        if (linedef_in >= total_level_lines) // sanity check
            FatalError("Bad GWA file: seg #%d has invalid linedef.\n", (int)(seg - level_segs));

        seg->miniseg = false;
        seg->linedef = &level_lines[linedef_in];

        float sx = seg->side ? seg->linedef->vertex_2->X : seg->linedef->vertex_1->X;
        float sy = seg->side ? seg->linedef->vertex_2->Y : seg->linedef->vertex_1->Y;

        seg->offset = RendererPointToDistance(sx, sy, seg->vertex_1->X, seg->vertex_1->Y);

        seg->sidedef = seg->linedef->side[seg->side];

        if (!seg->sidedef)
            FatalError("Bad GWA file: missing side for seg #%d\n", (int)(seg - level_segs));

        seg->front_sector = seg->sidedef->sector;

        if (seg->linedef->flags & kLineFlagTwoSided)
        {
            Side *other = seg->linedef->side[seg->side ^ 1];

            if (other)
                seg->back_sector = other->sector;
        }
    }
}

//
// GroupSectorTags
//
// Called during P_LoadSectors to set the tag_next & tag_previous fields of
// each sector_t, which keep all sectors with the same tag in a linked
// list for faster handling.
//
// -AJA- 1999/07/29: written.
//
static void GroupSectorTags(Sector *dest, Sector *seclist, int numsecs)
{
    // NOTE: `numsecs' does not include the current sector.

    dest->tag_next = dest->tag_previous = nullptr;

    for (; numsecs > 0; numsecs--)
    {
        Sector *src = &seclist[numsecs - 1];

        if (src->tag == dest->tag)
        {
            src->tag_next      = dest;
            dest->tag_previous = src;
            return;
        }
    }
}

static void LoadSectors(int lump)
{
    const uint8_t   *data;
    int              i;
    const RawSector *ms;
    Sector          *ss;

    if (!VerifyLump(lump, "SECTORS"))
    {
        // Check if SECTORS is immediately after
        // THINGS/LINEDEFS/SIDEDEFS/VERTEXES
        lump -= 3;
        if (!VerifyLump(lump, "SECTORS"))
            FatalError("Bad WAD: level %s missing SECTORS.\n", current_map->lump_.c_str());
    }

    total_level_sectors = GetLumpLength(lump) / sizeof(RawSector);

    if (total_level_sectors == 0)
        FatalError("Bad WAD: level %s contains 0 sectors.\n", current_map->lump_.c_str());

    level_sectors = new Sector[total_level_sectors];
    EPI_CLEAR_MEMORY(level_sectors, Sector, total_level_sectors);

    data = LoadLumpIntoMemory(lump);
    map_sectors_crc.AddBlock((const uint8_t *)data, GetLumpLength(lump));

    CheckDoom2Map05Bug((uint8_t *)data, GetLumpLength(lump)); // Lobo: 2023

    ms = (const RawSector *)data;
    ss = level_sectors;
    for (i = 0; i < total_level_sectors; i++, ss++, ms++)
    {
        char buffer[10];

        ss->floor_height   = AlignedLittleEndianS16(ms->floor_height);
        ss->ceiling_height = AlignedLittleEndianS16(ms->ceiling_height);

        // return to wolfenstein?
        if (goobers.d_)
        {
            ss->floor_height   = 0;
            ss->ceiling_height = (ms->floor_height == ms->ceiling_height) ? 0 : 128.0f;
        }

        ss->original_height = (ss->floor_height + ss->ceiling_height);

        ss->floor.translucency = 1.0f;
        ss->floor.x_matrix.X   = 1;
        ss->floor.x_matrix.Y   = 0;
        ss->floor.y_matrix.X   = 0;
        ss->floor.y_matrix.Y   = 1;

        ss->ceiling = ss->floor;

        epi::CStringCopyMax(buffer, ms->floor_texture, 8);
        ss->floor.image = ImageLookup(buffer, kImageNamespaceFlat);

        if (ss->floor.image)
        {
            FlatDefinition *current_flatdef = flatdefs.Find(ss->floor.image->name_.c_str());
            if (current_flatdef)
            {
                ss->bob_depth  = current_flatdef->bob_depth_;
                ss->sink_depth = current_flatdef->sink_depth_;
            }
        }

        epi::CStringCopyMax(buffer, ms->ceil_texture, 8);
        ss->ceiling.image = ImageLookup(buffer, kImageNamespaceFlat);

        if (!ss->floor.image)
        {
            LogWarning("Bad Level: sector #%d has missing floor texture.\n", i);
            ss->floor.image = ImageLookup("FLAT1", kImageNamespaceFlat);
        }
        if (!ss->ceiling.image)
        {
            LogWarning("Bad Level: sector #%d has missing ceiling texture.\n", i);
            ss->ceiling.image = ss->floor.image;
        }

        // convert negative tags to zero
        ss->tag = HMM_MAX(0, AlignedLittleEndianS16(ms->tag));

        ss->properties.light_level = AlignedLittleEndianS16(ms->light);

        int type = AlignedLittleEndianS16(ms->type);

        ss->properties.type    = HMM_MAX(0, type);
        ss->properties.special = LookupSectorType(ss->properties.type);

        ss->extrafloor_maximum = 0;

        ss->properties.colourmap = nullptr;

        ss->properties.gravity   = kGravityDefault;
        ss->properties.friction  = kFrictionDefault;
        ss->properties.viscosity = kViscosityDefault;
        ss->properties.drag      = kDragDefault;

        if (ss->properties.special && ss->properties.special->fog_color_ != kRGBANoValue)
        {
            ss->properties.fog_color   = ss->properties.special->fog_color_;
            ss->properties.fog_density = 0.01f * ss->properties.special->fog_density_;
        }
        else
        {
            ss->properties.fog_color   = kRGBANoValue;
            ss->properties.fog_density = 0;
        }

        ss->active_properties = &ss->properties;

        ss->sound_player = -1;

        // -AJA- 1999/07/29: Keep sectors with same tag in a list.
        GroupSectorTags(ss, level_sectors, i);
    }

    delete[] data;
}

static void SetupRootNode(void)
{
    if (total_level_nodes > 0)
    {
        root_node = total_level_nodes - 1;
    }
    else
    {
        root_node = kLeafSubsector | 0;

        // compute bbox for the single subsector
        BoundingBoxClear(dummy_bounding_box);

        int  i;
        Seg *seg;

        for (i = 0, seg = level_segs; i < total_level_segs; i++, seg++)
        {
            BoundingBoxAddPoint(dummy_bounding_box, seg->vertex_1->X, seg->vertex_1->Y);
            BoundingBoxAddPoint(dummy_bounding_box, seg->vertex_2->X, seg->vertex_2->Y);
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

static MapObject *SpawnMapThing(const MapObjectDefinition *info, float x, float y, float z, Sector *sec, BAMAngle angle,
                                int options, int tag)
{
    SpawnPoint point;

    point.x              = x;
    point.y              = y;
    point.z              = z;
    point.angle          = angle;
    point.vertical_angle = 0;
    point.info           = info;
    point.flags          = 0;
    point.tag            = tag;

    // -KM- 1999/01/31 Use playernum property.
    // count deathmatch start positions
    if (info->playernum_ < 0)
    {
        AddDeathmatchStart(point);
        return nullptr;
    }

    // check for players specially -jc-
    if (info->playernum_ > 0)
    {
        // -AJA- 2009/10/07: Hub support
        if (sec->properties.special && sec->properties.special->hub_)
        {
            if (sec->tag <= 0)
                LogWarning("HUB_START in sector without tag @ (%1.0f %1.0f)\n", x, y);

            point.tag = sec->tag;

            AddHubStart(point);
            return nullptr;
        }

        // -AJA- 2004/12/30: for duplicate players, the LAST one must
        //       be used (so levels with Voodoo dolls work properly).
        SpawnPoint *prev = FindCoopPlayer(info->playernum_);

        if (!prev)
            AddCoopStart(point);
        else
        {
            AddVoodooDoll(*prev);

            // overwrite one in the Coop list with new location
            memcpy(prev, &point, sizeof(point));
        }
        return nullptr;
    }

    // check for apropriate skill level
    // -ES- 1999/04/13 Implemented Kester's Bugfix.
    // -AJA- 1999/10/21: Reworked again.
    if (InSinglePlayerMatch() && (options & kThingNotSinglePlayer))
        return nullptr;

    // Disable deathmatch weapons for vanilla coop...should probably be in the
    // Gameplay Options menu - Dasho
    if (InCooperativeMatch() && (options & kThingNotSinglePlayer))
        return nullptr;

    // -AJA- 1999/09/22: Boom compatibility flags.
    if (InCooperativeMatch() && (options & kThingNotCooperative))
        return nullptr;

    if (InDeathmatch() && (options & kThingNotDeathmatch))
        return nullptr;

    int bit;

    if (game_skill == kSkillBaby)
        bit = 1;
    else if (game_skill == kSkillNightmare)
        bit = 4;
    else
        bit = 1 << (game_skill - 1);

    if ((options & bit) == 0)
        return nullptr;

    // don't spawn keycards in deathmatch
    if (InDeathmatch() && (info->flags_ & kMapObjectFlagNotDeathmatch))
        return nullptr;

    // don't spawn any monsters if -nomonsters
    if (level_flags.no_monsters && (info->extended_flags_ & kExtendedFlagMonster))
        return nullptr;

    // -AJA- 1999/10/07: don't spawn extra things if -noextra.
    if (!level_flags.have_extra && (info->extended_flags_ & kExtendedFlagExtra))
        return nullptr;

    // spawn it now !
    // Use MobjCreateObject -ACB- 1998/08/06
    MapObject *mo = CreateMapObject(x, y, z, info);

    mo->angle_      = angle;
    mo->spawnpoint_ = point;

    if (mo->state_ && mo->state_->tics > 1)
        mo->tics_ = 1 + (RandomByteDeterministic() % mo->state_->tics);

    if (options & kThingAmbush)
    {
        mo->flags_ |= kMapObjectFlagAmbush;
        mo->spawnpoint_.flags |= kMapObjectFlagAmbush;
    }

    // -AJA- 2000/09/22: MBF compatibility flag
    if (options & kThingFriend)
    {
        mo->side_ = 1; //~0;
        mo->hyper_flags_ |= kHyperFlagUltraLoyal;
        /*
        player_t *player;
        player = players[0];
        mo->SetSupportObj(player->map_object_);
        P_LookForPlayers(mo, mo->info_->sight_angle_);
        */
    }
    // Lobo 2022: added tagged mobj support ;)
    if (tag > 0)
        mo->tag_ = tag;

    return mo;
}

static void LoadThings(int lump)
{
    float    x, y, z;
    BAMAngle angle;
    int      options, typenum;
    int      i;

    const uint8_t             *data;
    const RawThing            *mt;
    const MapObjectDefinition *objtype;

    if (!VerifyLump(lump, "THINGS"))
        FatalError("Bad WAD: level %s missing THINGS.\n", current_map->lump_.c_str());

    total_map_things = GetLumpLength(lump) / sizeof(RawThing);

    if (total_map_things == 0)
        FatalError("Bad WAD: level %s contains 0 things.\n", current_map->lump_.c_str());

    data = LoadLumpIntoMemory(lump);
    map_things_crc.AddBlock((const uint8_t *)data, GetLumpLength(lump));

    CheckEvilutionBug((uint8_t *)data, GetLumpLength(lump));

    // -AJA- 2004/11/04: check the options in all things to see whether
    // we can use new option flags or not.  Same old wads put 1 bits in
    // unused locations (unusued for original Doom anyway).  The logic
    // here is based on PrBoom, but PrBoom checks each thing separately.

    bool limit_options = false;

    mt = (const RawThing *)data;

    for (i = 0; i < total_map_things; i++)
    {
        options = AlignedLittleEndianU16(mt[i].options);

        if (options & kThingReserved)
            limit_options = true;
    }

    for (i = 0; i < total_map_things; i++, mt++)
    {
        x       = (float)AlignedLittleEndianS16(mt->x);
        y       = (float)AlignedLittleEndianS16(mt->y);
        angle   = epi::BAMFromDegrees((float)AlignedLittleEndianS16(mt->angle));
        typenum = AlignedLittleEndianU16(mt->type);
        options = AlignedLittleEndianU16(mt->options);

        if (limit_options)
            options &= 0x001F;

        objtype = mobjtypes.Lookup(typenum);

        // MOBJTYPE not found, don't crash out: JDS Compliance.
        // -ACB- 1998/07/21
        if (objtype == nullptr)
        {
            UnknownThingWarning(typenum, x, y);
            continue;
        }

        Sector *sec = RendererPointInSubsector(x, y)->sector;

        if ((objtype->hyper_flags_ & kHyperFlagMusicChanger) && !musinfo_tracks[current_map->name_].processed)
        {
            // This really should only be used with the original DoomEd number
            // range
            if (objtype->number_ >= 14100 && objtype->number_ < 14165)
            {
                int mus_number = -1;

                if (objtype->number_ == 14100) // Default for level
                    mus_number = current_map->music_;
                else if (musinfo_tracks[current_map->name_].mappings.count(objtype->number_ - 14100))
                    mus_number = musinfo_tracks[current_map->name_].mappings[objtype->number_ - 14100];
                // Track found; make ad-hoc RTS script for music changing
                if (mus_number != -1)
                {
                    std::string mus_rts = "// MUSINFO SCRIPTS\n\n";
                    mus_rts.append(epi::StringFormat("START_MAP %s\n", current_map->name_.c_str()));
                    mus_rts.append(epi::StringFormat("  SECTOR_TRIGGER_INDEX %d\n", sec - level_sectors));
                    mus_rts.append("    TAGGED_INDEPENDENT\n");
                    mus_rts.append("    TAGGED_REPEATABLE\n");
                    mus_rts.append("    WAIT 30T\n");
                    mus_rts.append(epi::StringFormat("    CHANGE_MUSIC %d\n", mus_number));
                    mus_rts.append("    RETRIGGER\n");
                    mus_rts.append("  END_SECTOR_TRIGGER\n");
                    mus_rts.append("END_MAP\n\n");
                    ReadRADScript(mus_rts, "MUSINFO");
                }
            }
        }

        z = sec->floor_height;

        if (objtype->flags_ & kMapObjectFlagSpawnCeiling)
            z = sec->ceiling_height - objtype->height_;

        if ((options & kThingReserved) == 0 && (options & kExtrafloorMask))
        {
            int floor_num = (options & kExtrafloorMask) >> kExtrafloorBitShift;

            for (Extrafloor *ef = sec->bottom_extrafloor; ef; ef = ef->higher)
            {
                z = ef->top_height;

                floor_num--;
                if (floor_num == 0)
                    break;
            }
        }

        SpawnMapThing(objtype, x, y, z, sec, angle, options, 0);
    }

    // Mark MUSINFO for this level as done processing, even if it was empty,
    // so we can avoid re-checks
    musinfo_tracks[current_map->name_].processed = true;

    delete[] data;
}

static inline void ComputeLinedefData(Line *ld, int side0, int side1)
{
    Vertex *v1 = ld->vertex_1;
    Vertex *v2 = ld->vertex_2;

    ld->delta_x = v2->X - v1->X;
    ld->delta_y = v2->Y - v1->Y;

    if (AlmostEquals(ld->delta_x, 0.0f))
        ld->slope_type = kLineClipVertical;
    else if (AlmostEquals(ld->delta_y, 0.0f))
        ld->slope_type = kLineClipHorizontal;
    else if (ld->delta_y / ld->delta_x > 0)
        ld->slope_type = kLineClipPositive;
    else
        ld->slope_type = kLineClipNegative;

    ld->length = RendererPointToDistance(0, 0, ld->delta_x, ld->delta_y);

    if (v1->X < v2->X)
    {
        ld->bounding_box[kBoundingBoxLeft]  = v1->X;
        ld->bounding_box[kBoundingBoxRight] = v2->X;
    }
    else
    {
        ld->bounding_box[kBoundingBoxLeft]  = v2->X;
        ld->bounding_box[kBoundingBoxRight] = v1->X;
    }

    if (v1->Y < v2->Y)
    {
        ld->bounding_box[kBoundingBoxBottom] = v1->Y;
        ld->bounding_box[kBoundingBoxTop]    = v2->Y;
    }
    else
    {
        ld->bounding_box[kBoundingBoxBottom] = v2->Y;
        ld->bounding_box[kBoundingBoxTop]    = v1->Y;
    }

    if (!udmf_level && side0 == 0xFFFF)
        side0 = -1;
    if (!udmf_level && side1 == 0xFFFF)
        side1 = -1;

    // handle missing RIGHT sidedef (idea taken from MBF)
    if (side0 == -1)
    {
        LogWarning("Bad WAD: level %s linedef #%d is missing RIGHT side\n", current_map->lump_.c_str(),
                   (int)(ld - level_lines));
        side0 = 0;
    }

    if ((ld->flags & kLineFlagTwoSided) && ((side0 == -1) || (side1 == -1)))
    {
        LogWarning("Bad WAD: level %s has linedef #%d marked TWOSIDED, "
                   "but it has only one side.\n",
                   current_map->lump_.c_str(), (int)(ld - level_lines));

        ld->flags &= ~kLineFlagTwoSided;
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

    if (!VerifyLump(lump, "LINEDEFS"))
        FatalError("Bad WAD: level %s missing LINEDEFS.\n", current_map->lump_.c_str());

    total_level_lines = GetLumpLength(lump) / sizeof(RawLinedef);

    if (total_level_lines == 0)
        FatalError("Bad WAD: level %s contains 0 linedefs.\n", current_map->lump_.c_str());

    level_lines = new Line[total_level_lines];

    EPI_CLEAR_MEMORY(level_lines, Line, total_level_lines);

    temp_line_sides = new int[total_level_lines * 2];

    const uint8_t *data = LoadLumpIntoMemory(lump);
    map_lines_crc.AddBlock((const uint8_t *)data, GetLumpLength(lump));

    Line             *ld  = level_lines;
    const RawLinedef *mld = (const RawLinedef *)data;

    for (int i = 0; i < total_level_lines; i++, mld++, ld++)
    {
        ld->flags    = AlignedLittleEndianU16(mld->flags);
        ld->tag      = HMM_MAX(0, AlignedLittleEndianS16(mld->tag));
        ld->vertex_1 = &level_vertexes[AlignedLittleEndianU16(mld->start)];
        ld->vertex_2 = &level_vertexes[AlignedLittleEndianU16(mld->end)];

        // Check for BoomClear flag bit and clear applicable specials
        // (PassThru may still be intentionally added further down)
        if (ld->flags & kLineFlagClearBoomFlags)
            ld->flags &= ~(kLineFlagBoomPassThrough | kLineFlagBlockGroundedMonsters | kLineFlagBlockPlayers);

        ld->special = LookupLineType(HMM_MAX(0, AlignedLittleEndianS16(mld->type)));

        if (ld->special && ld->special->type_ == kLineTriggerWalkable)
            ld->flags |= kLineFlagBoomPassThrough;

        if (ld->special && ld->special->type_ == kLineTriggerNone &&
            (ld->special->s_xspeed_ || ld->special->s_yspeed_ || ld->special->scroll_type_ > BoomScrollerTypeNone ||
             ld->special->line_effect_ == kLineEffectTypeVectorScroll ||
             ld->special->line_effect_ == kLineEffectTypeOffsetScroll ||
             ld->special->line_effect_ == kLineEffectTypeTaggedOffsetScroll))
            ld->flags |= kLineFlagBoomPassThrough;

        if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailFloor)
            ld->flags |= kLineFlagBoomPassThrough;

        if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailCeiling)
            ld->flags |= kLineFlagBoomPassThrough;

        if (ld->special && ld->special == linetypes.Lookup(0)) // Add passthru to unknown/templated
            ld->flags |= kLineFlagBoomPassThrough;

        int side0 = AlignedLittleEndianU16(mld->right);
        int side1 = AlignedLittleEndianU16(mld->left);

        ComputeLinedefData(ld, side0, side1);

        // check for possible extrafloors, updating the extrafloor_maximum count
        // for the sectors in question.

        if (ld->tag && ld->special && ld->special->ef_.type_)
        {
            for (int j = 0; j < total_level_sectors; j++)
            {
                if (level_sectors[j].tag != ld->tag)
                    continue;

                level_sectors[j].extrafloor_maximum++;
                total_level_extrafloors++;
            }
        }
    }

    delete[] data;
}

static Sector *DetermineSubsectorSector(Subsector *ss, int pass)
{
    const Seg *seg;

    for (seg = ss->segs; seg != nullptr; seg = seg->subsector_next)
    {
        if (seg->miniseg)
            continue;

        // ignore self-referencing linedefs
        if (seg->front_sector == seg->back_sector)
            continue;

        return seg->front_sector;
    }

    for (seg = ss->segs; seg != nullptr; seg = seg->subsector_next)
    {
        if (seg->partner == nullptr)
            continue;

        // only do this for self-referencing linedefs if the original sector
        // isn't tagged, otherwise save it for the next pass
        if (seg->front_sector == seg->back_sector && seg->front_sector && seg->front_sector->tag == 0)
            return seg->front_sector;

        if (seg->front_sector != seg->back_sector && seg->partner->front_subsector->sector != nullptr)
            return seg->partner->front_subsector->sector;
    }

    if (pass == 1)
    {
        for (seg = ss->segs; seg != nullptr; seg = seg->subsector_next)
        {
            if (!seg->miniseg)
                return seg->front_sector;
        }
    }

    if (pass == 2)
        return &level_sectors[0];

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
        Subsector *ss = &level_subsectors[i];

        if (ss->sector == nullptr)
        {
            null_count += 1;

            ss->sector = DetermineSubsectorSector(ss, pass);

            if (ss->sector != nullptr)
            {
                progress = true;

                // link subsector into parent sector's list.
                // order is not important, so add it to the head of the list.
                ss->sector_next        = ss->sector->subsectors;
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

    while (AssignSubsectorsPass(0))
    {
    }

    while (AssignSubsectorsPass(1))
    {
    }

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

    xglen   = GetLumpLength(lumpnum);
    xgldata = (uint8_t *)LoadLumpIntoMemory(lumpnum);
    if (!xgldata)
        FatalError("LoadXGL3Nodes: Couldn't load lump\n");

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
            if (inflate_status == MZ_OK || inflate_status == MZ_BUF_ERROR) // Need to resize output buffer
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
    LogDebug("LoadXGL3Nodes: Orig Verts = %d, New Verts = %d, Map Verts = %d\n", oVerts, nVerts, total_level_vertexes);

    level_gl_vertexes = new Vertex[nVerts];

    // fill in new vertexes
    Vertex *vv = level_gl_vertexes;
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

    level_subsectors = new Subsector[total_level_subsectors];
    EPI_CLEAR_MEMORY(level_subsectors, Subsector, total_level_subsectors);

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

    level_segs = new Seg[total_level_segs];
    EPI_CLEAR_MEMORY(level_segs, Seg, total_level_segs);
    Seg *seg = level_segs;

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
            seg->vertex_1 = &level_vertexes[v1num];
        else
            seg->vertex_1 = &level_gl_vertexes[v1num - oVerts];

        seg->side = side ? 1 : 0;

        if (partner == -1)
            seg->partner = nullptr;
        else
        {
            EPI_ASSERT(partner < total_level_segs); // sanity check
            seg->partner = &level_segs[partner];
        }

        SegCommonStuff(seg, slinedef);

        // The following fields are filled out elsewhere:
        //     sub_next, front_sub, back_sub, frontsector, backsector.

        seg->subsector_next  = EDGE_SEG_INVALID;
        seg->front_subsector = seg->back_subsector = EDGE_SUBSECTOR_INVALID;
    }

    LogDebug("LoadXGL3Nodes: Post-process subsectors\n");
    // go back and fill in subsectors
    Subsector *ss = level_subsectors;
    xglSegs       = 0;
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
            seg->vertex_2 =
                j == (countsegs - 1) ? level_segs[firstseg].vertex_1 : level_segs[firstseg + j + 1].vertex_1;

            seg->angle = RendererPointToAngle(seg->vertex_1->X, seg->vertex_1->Y, seg->vertex_2->X, seg->vertex_2->Y);

            seg->length =
                RendererPointToDistance(seg->vertex_1->X, seg->vertex_1->Y, seg->vertex_2->X, seg->vertex_2->Y);
        }

        // -AJA- 1999/09/23: New linked list for the segs of a subsector
        //       (part of true bsp rendering).
        Seg **prevptr = &ss->segs;

        if (countsegs == 0)
            FatalError("LoadXGL3Nodes: level %s has invalid SSECTORS.\n", current_map->lump_.c_str());

        ss->sector     = nullptr;
        ss->thing_list = nullptr;

        // this is updated when the nodes are loaded
        ss->bounding_box = dummy_bounding_box;

        for (int j = 0; j < countsegs; j++)
        {
            Seg *cur = &level_segs[firstseg + j];

            *prevptr = cur;
            prevptr  = &cur->subsector_next;

            cur->front_subsector = ss;
            cur->back_subsector  = nullptr;

            // LogDebug("  ssec = %d, seg = %d\n", i, firstseg + j);
        }
        // LogDebug("LoadZNodes: ssec = %d, fseg = %d, cseg = %d\n", i,
        // firstseg, countsegs);

        *prevptr = nullptr;
    }
    delete[] ss_temp; // CA 9.30.18: allocated with new but released using
                      // delete, added [] between delete and ss_temp

    LogDebug("LoadXGL3Nodes: Read GL nodes\n");
    // finally, read the nodes
    // NOTE: no nodes is okay (a basic single sector map). -AJA-
    total_level_nodes = epi::UnalignedLittleEndianU32(td);
    td += 4;
    LogDebug("LoadXGL3Nodes: Num nodes = %d\n", total_level_nodes);

    level_nodes = new BspNode[total_level_nodes + 1];
    EPI_CLEAR_MEMORY(level_nodes, BspNode, total_level_nodes);
    BspNode *nd = level_nodes;

    for (i = 0; i < total_level_nodes; i++, nd++)
    {
        nd->divider.x = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->divider.y = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->divider.delta_x = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;
        nd->divider.delta_y = (float)epi::UnalignedLittleEndianS32(td) / 65536.0f;
        td += 4;

        nd->divider_length = RendererPointToDistance(0, 0, nd->divider.delta_x, nd->divider.delta_y);

        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 4; k++)
            {
                nd->bounding_boxes[j][k] = (float)epi::UnalignedLittleEndianS16(td);
                td += 2;
            }

        for (int j = 0; j < 2; j++)
        {
            nd->children[j] = epi::UnalignedLittleEndianU32(td);
            td += 4;

            // update bbox pointers in subsector
            if (nd->children[j] & kLeafSubsector)
            {
                Subsector *sss    = level_subsectors + (nd->children[j] & ~kLeafSubsector);
                sss->bounding_box = &nd->bounding_boxes[j][0];
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

        if (tok == epi::kTokenEOF)
            break;

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
                if (lex.Match("}"))
                    break;

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

                if (block_tok == epi::kTokenEOF || block_tok == epi::kTokenError || value == "}")
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
        else // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF)
                    break;
            }
        }
    }
    EPI_ASSERT(cur_vertex == total_level_vertexes);

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

        if (tok == epi::kTokenEOF)
            break;

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
                if (lex.Match("}"))
                    break;

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

                if (block_tok == epi::kTokenEOF || block_tok == epi::kTokenError || value == "}")
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
                    light_color = ((uint32_t)epi::LexInteger(value) << 8 | 0xFF);
                    break;
                case epi::kENameFadecolor:
                    fog_color = ((uint32_t)epi::LexInteger(value) << 8 | 0xFF);
                    break;
                case epi::kENameFogdensity:
                    fog_density = HMM_Clamp(0, epi::LexInteger(value), 1020);
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
            Sector *ss         = level_sectors + cur_sector;
            ss->floor_height   = fz;
            ss->ceiling_height = cz;

            // return to wolfenstein?
            if (goobers.d_)
            {
                ss->floor_height   = 0;
                ss->ceiling_height = (AlmostEquals(fz, cz)) ? 0 : 128.0f;
            }

            ss->original_height = (ss->floor_height + ss->ceiling_height);

            ss->floor.translucency = 1.0f;
            ss->floor.x_matrix.X   = 1;
            ss->floor.x_matrix.Y   = 0;
            ss->floor.y_matrix.X   = 0;
            ss->floor.y_matrix.Y   = 1;

            ss->ceiling = ss->floor;

            // granular offsets
            ss->floor.offset.X += fx;
            ss->floor.offset.Y += fy;
            ss->ceiling.offset.X += cx;
            ss->ceiling.offset.Y += cy;

            // rotations
            if (!AlmostEquals(rf, 0.0f))
                ss->floor.rotation = epi::BAMFromDegrees(rf);

            if (!AlmostEquals(rc, 0.0f))
                ss->ceiling.rotation = epi::BAMFromDegrees(rc);

            // granular scaling
            ss->floor.x_matrix.X   = fx_sc;
            ss->floor.y_matrix.Y   = fy_sc;
            ss->ceiling.x_matrix.X = cx_sc;
            ss->ceiling.y_matrix.Y = cy_sc;

            ss->floor.image = ImageLookup(floor_tex, kImageNamespaceFlat);

            if (ss->floor.image)
            {
                FlatDefinition *current_flatdef = flatdefs.Find(ss->floor.image->name_.c_str());
                if (current_flatdef)
                {
                    ss->bob_depth  = current_flatdef->bob_depth_;
                    ss->sink_depth = current_flatdef->sink_depth_;
                }
            }

            ss->ceiling.image = ImageLookup(ceil_tex, kImageNamespaceFlat);

            if (!ss->floor.image)
            {
                LogWarning("Bad Level: sector #%d has missing floor texture.\n", cur_sector);
                ss->floor.image = ImageLookup("FLAT1", kImageNamespaceFlat);
            }
            if (!ss->ceiling.image)
            {
                LogWarning("Bad Level: sector #%d has missing ceiling texture.\n", cur_sector);
                ss->ceiling.image = ss->floor.image;
            }

            // convert negative tags to zero
            ss->tag = HMM_MAX(0, tag);

            ss->properties.light_level = light;

            // convert negative types to zero
            ss->properties.type    = HMM_MAX(0, type);
            ss->properties.special = LookupSectorType(ss->properties.type);

            ss->extrafloor_maximum = 0;

            ss->properties.colourmap = nullptr;

            ss->properties.gravity   = kGravityDefault * gravfactor;
            ss->properties.friction  = kFrictionDefault;
            ss->properties.viscosity = kViscosityDefault;
            ss->properties.drag      = kDragDefault;

            // Allow UDMF sector light/fog information to override DDFSECT types
            if (fog_color != SG_BLACK_RGBA32) // All black is the established
                                              // UDMF "no fog" color
            {
                // Prevent UDMF-specified fog color from having our internal 'no
                // value'...uh...value
                if (fog_color == kRGBANoValue)
                    fog_color ^= 0x00010100;
                ss->properties.fog_color = fog_color;
                // Best-effort match for GZDoom's fogdensity values so that UDB,
                // etc give predictable results
                if (fog_density < 2)
                    ss->properties.fog_density = 0.002f;
                else
                    ss->properties.fog_density = 0.01f * ((float)fog_density / 1020.0f);
            }
            else if (ss->properties.special && ss->properties.special->fog_color_ != kRGBANoValue)
            {
                ss->properties.fog_color   = ss->properties.special->fog_color_;
                ss->properties.fog_density = 0.01f * ss->properties.special->fog_density_;
            }
            else
            {
                ss->properties.fog_color   = kRGBANoValue;
                ss->properties.fog_density = 0;
            }
            if (light_color != SG_WHITE_RGBA32)
            {
                if (light_color == kRGBANoValue)
                    light_color ^= 0x00010100;
                // Make colormap if necessary
                for (Colormap *cmap : colormaps)
                {
                    if (cmap->gl_color_ != kRGBANoValue && cmap->gl_color_ == light_color)
                    {
                        ss->properties.colourmap = cmap;
                        break;
                    }
                }
                if (!ss->properties.colourmap || ss->properties.colourmap->gl_color_ != light_color)
                {
                    Colormap *ad_hoc         = new Colormap;
                    ad_hoc->name_            = epi::StringFormat("UDMF_%d", light_color); // Internal
                    ad_hoc->gl_color_        = light_color;
                    ss->properties.colourmap = ad_hoc;
                    colormaps.push_back(ad_hoc);
                }
            }

            ss->active_properties = &ss->properties;

            ss->sound_player = -1;

            // -AJA- 1999/07/29: Keep sectors with same tag in a list.
            GroupSectorTags(ss, level_sectors, cur_sector);
            cur_sector++;
        }
        else // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF)
                    break;
            }
        }
    }
    EPI_ASSERT(cur_sector == total_level_sectors);

    LogDebug("LoadUDMFSectors: finished parsing TEXTMAP\n");
}

static void LoadUDMFSideDefs()
{
    epi::Lexer lex(udmf_lump);

    LogDebug("LoadUDMFSectors: parsing TEXTMAP\n");

    level_sides = new Side[total_level_sides];
    EPI_CLEAR_MEMORY(level_sides, Side, total_level_sides);

    int nummapsides = 0;

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF)
            break;

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
                if (lex.Match("}"))
                    break;

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

                if (block_tok == epi::kTokenEOF || block_tok == epi::kTokenError || value == "}")
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
            EPI_ASSERT(nummapsides <= total_level_sides); // sanity check

            Side *sd = level_sides + nummapsides - 1;

            sd->top.translucency = 1.0f;
            sd->top.offset.X     = x;
            sd->top.offset.Y     = y;
            sd->top.x_matrix.X   = 1;
            sd->top.x_matrix.Y   = 0;
            sd->top.y_matrix.X   = 0;
            sd->top.y_matrix.Y   = 1;

            sd->middle = sd->top;
            sd->bottom = sd->top;

            sd->sector = &level_sectors[sec_num];

            sd->top.image = ImageLookup(top_tex, kImageNamespaceTexture, kImageLookupNull);

            if (sd->top.image == nullptr)
            {
                if (goobers.d_)
                    sd->top.image = ImageLookup(bottom_tex, kImageNamespaceTexture);
                else
                    sd->top.image = ImageLookup(top_tex, kImageNamespaceTexture);
            }

            sd->middle.image = ImageLookup(middle_tex, kImageNamespaceTexture);
            sd->bottom.image = ImageLookup(bottom_tex, kImageNamespaceTexture);

            // granular offsets
            sd->bottom.offset.X += lowx;
            sd->middle.offset.X += midx;
            sd->top.offset.X += highx;
            sd->bottom.offset.Y += lowy;
            sd->middle.offset.Y += midy;
            sd->top.offset.Y += highy;

            // granular scaling
            sd->bottom.x_matrix.X = low_scx;
            sd->middle.x_matrix.X = mid_scx;
            sd->top.x_matrix.X    = high_scx;
            sd->bottom.y_matrix.Y = low_scy;
            sd->middle.y_matrix.Y = mid_scy;
            sd->top.y_matrix.Y    = high_scy;

            // handle BOOM colormaps with [242] linetype
            sd->top.boom_colormap    = colormaps.Lookup(top_tex);
            sd->middle.boom_colormap = colormaps.Lookup(middle_tex);
            sd->bottom.boom_colormap = colormaps.Lookup(bottom_tex);

            if (sd->top.image && fabs(sd->top.offset.Y) > sd->top.image->ScaledHeightActual())
                sd->top.offset.Y = fmodf(sd->top.offset.Y, sd->top.image->ScaledHeightActual());

            if (sd->middle.image && fabs(sd->middle.offset.Y) > sd->middle.image->ScaledHeightActual())
                sd->middle.offset.Y = fmodf(sd->middle.offset.Y, sd->middle.image->ScaledHeightActual());

            if (sd->bottom.image && fabs(sd->bottom.offset.Y) > sd->bottom.image->ScaledHeightActual())
                sd->bottom.offset.Y = fmodf(sd->bottom.offset.Y, sd->bottom.image->ScaledHeightActual());
        }
        else // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF)
                    break;
            }
        }
    }

    LogDebug("LoadUDMFSideDefs: post-processing linedefs & sidedefs\n");

    // post-process linedefs & sidedefs

    EPI_ASSERT(temp_line_sides);

    Side *sd = level_sides;

    for (int i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        int side0 = temp_line_sides[i * 2 + 0];
        int side1 = temp_line_sides[i * 2 + 1];

        EPI_ASSERT(side0 != -1);

        if (side0 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n", current_map->lump_.c_str(), i);
            side0 = nummapsides - 1;
        }

        if (side1 != -1 && side1 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad LEFT side.\n", current_map->lump_.c_str(), i);
            side1 = nummapsides - 1;
        }

        ld->side[0] = sd;
        if (sd->middle.image && (side1 != -1))
        {
            sd->middle_mask_offset = sd->middle.offset.Y;
            sd->middle.offset.Y    = 0;
        }
        ld->front_sector = sd->sector;
        sd++;

        if (side1 != -1)
        {
            ld->side[1] = sd;
            if (sd->middle.image)
            {
                sd->middle_mask_offset = sd->middle.offset.Y;
                sd->middle.offset.Y    = 0;
            }
            ld->back_sector = sd->sector;
            sd++;
        }

        EPI_ASSERT(sd <= level_sides + total_level_sides);
    }

    EPI_ASSERT(sd == level_sides + total_level_sides);

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

        if (tok == epi::kTokenEOF)
            break;

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
                if (lex.Match("}"))
                    break;

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

                if (block_tok == epi::kTokenEOF || block_tok == epi::kTokenError || value == "}")
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
                    flags |= (epi::LexBoolean(value) ? kLineFlagBlocking : 0);
                    break;
                case epi::kENameBlockmonsters:
                    flags |= (epi::LexBoolean(value) ? kLineFlagBlockMonsters : 0);
                    break;
                case epi::kENameTwosided:
                    flags |= (epi::LexBoolean(value) ? kLineFlagTwoSided : 0);
                    break;
                case epi::kENameDontpegtop:
                    flags |= (epi::LexBoolean(value) ? kLineFlagUpperUnpegged : 0);
                    break;
                case epi::kENameDontpegbottom:
                    flags |= (epi::LexBoolean(value) ? kLineFlagLowerUnpegged : 0);
                    break;
                case epi::kENameSecret:
                    flags |= (epi::LexBoolean(value) ? kLineFlagSecret : 0);
                    break;
                case epi::kENameBlocksound:
                    flags |= (epi::LexBoolean(value) ? kLineFlagSoundBlock : 0);
                    break;
                case epi::kENameDontdraw:
                    flags |= (epi::LexBoolean(value) ? kLineFlagDontDraw : 0);
                    break;
                case epi::kENameMapped:
                    flags |= (epi::LexBoolean(value) ? kLineFlagMapped : 0);
                    break;
                case epi::kENamePassuse:
                    flags |= (epi::LexBoolean(value) ? kLineFlagBoomPassThrough : 0);
                    break;
                case epi::kENameBlockplayers:
                    flags |= (epi::LexBoolean(value) ? kLineFlagBlockPlayers : 0);
                    break;
                case epi::kENameBlocksight:
                    flags |= (epi::LexBoolean(value) ? kLineFlagSightBlock : 0);
                    break;
                default:
                    break;
                }
            }
            Line *ld = level_lines + cur_line;

            ld->flags    = flags;
            ld->tag      = HMM_MAX(0, tag);
            ld->vertex_1 = &level_vertexes[v1];
            ld->vertex_2 = &level_vertexes[v2];

            ld->special = LookupLineType(HMM_MAX(0, special));

            if (ld->special && ld->special->type_ == kLineTriggerWalkable)
                ld->flags |= kLineFlagBoomPassThrough;

            if (ld->special && ld->special->type_ == kLineTriggerNone &&
                (ld->special->s_xspeed_ || ld->special->s_yspeed_ || ld->special->scroll_type_ > BoomScrollerTypeNone ||
                 ld->special->line_effect_ == kLineEffectTypeVectorScroll ||
                 ld->special->line_effect_ == kLineEffectTypeOffsetScroll ||
                 ld->special->line_effect_ == kLineEffectTypeTaggedOffsetScroll))
                ld->flags |= kLineFlagBoomPassThrough;

            if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailFloor)
                ld->flags |= kLineFlagBoomPassThrough;

            if (ld->special && ld->special->slope_type_ & kSlopeTypeDetailCeiling)
                ld->flags |= kLineFlagBoomPassThrough;

            if (ld->special && ld->special == linetypes.Lookup(0)) // Add passthru to unknown/templated
                ld->flags |= kLineFlagBoomPassThrough;

            ComputeLinedefData(ld, side0, side1);

            if (ld->tag && ld->special && ld->special->ef_.type_)
            {
                for (int j = 0; j < total_level_sectors; j++)
                {
                    if (level_sectors[j].tag != ld->tag)
                        continue;

                    level_sectors[j].extrafloor_maximum++;
                    total_level_extrafloors++;
                }
            }
            cur_line++;
        }
        else // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF)
                    break;
            }
        }
    }
    EPI_ASSERT(cur_line == total_level_lines);

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

        if (tok == epi::kTokenEOF)
            break;

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
            float                      x = 0.0f, y = 0.0f, z = 0.0f;
            BAMAngle                   angle     = kBAMAngle0;
            int                        options   = kThingNotSinglePlayer | kThingNotDeathmatch | kThingNotCooperative;
            int                        typenum   = -1;
            int                        tag       = 0;
            float                      healthfac = 1.0f;
            float                      alpha     = 1.0f;
            float                      scale = 0.0f, scalex = 0.0f, scaley = 0.0f;
            const MapObjectDefinition *objtype;
            for (;;)
            {
                if (lex.Match("}"))
                    break;

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

                if (block_tok == epi::kTokenEOF || block_tok == epi::kTokenError || value == "}")
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
                    options |= (epi::LexBoolean(value) ? kThingEasy : 0);
                    break;
                case epi::kENameSkill2:
                    options |= (epi::LexBoolean(value) ? kThingEasy : 0);
                    break;
                case epi::kENameSkill3:
                    options |= (epi::LexBoolean(value) ? kThingMedium : 0);
                    break;
                case epi::kENameSkill4:
                    options |= (epi::LexBoolean(value) ? kThingHard : 0);
                    break;
                case epi::kENameSkill5:
                    options |= (epi::LexBoolean(value) ? kThingHard : 0);
                    break;
                case epi::kENameAmbush:
                    options |= (epi::LexBoolean(value) ? kThingAmbush : 0);
                    break;
                case epi::kENameSingle:
                    options &= (epi::LexBoolean(value) ? ~kThingNotSinglePlayer : options);
                    break;
                case epi::kENameDm:
                    options &= (epi::LexBoolean(value) ? ~kThingNotDeathmatch : options);
                    break;
                case epi::kENameCoop:
                    options &= (epi::LexBoolean(value) ? ~kThingNotCooperative : options);
                    break;
                case epi::kENameFriend:
                    options |= (epi::LexBoolean(value) ? kThingFriend : 0);
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

            Sector *sec = RendererPointInSubsector(x, y)->sector;

            if ((objtype->hyper_flags_ & kHyperFlagMusicChanger) && !musinfo_tracks[current_map->name_].processed)
            {
                // This really should only be used with the original DoomEd
                // number range
                if (objtype->number_ >= 14100 && objtype->number_ < 14165)
                {
                    int mus_number = -1;

                    if (objtype->number_ == 14100) // Default for level
                        mus_number = current_map->music_;
                    else if (musinfo_tracks[current_map->name_].mappings.count(objtype->number_ - 14100))
                    {
                        mus_number = musinfo_tracks[current_map->name_].mappings[objtype->number_ - 14100];
                    }
                    // Track found; make ad-hoc RTS script for music changing
                    if (mus_number != -1)
                    {
                        std::string mus_rts = "// MUSINFO SCRIPTS\n\n";
                        mus_rts.append(epi::StringFormat("START_MAP %s\n", current_map->name_.c_str()));
                        mus_rts.append(epi::StringFormat("  SECTOR_TRIGGER_INDEX %d\n", sec - level_sectors));
                        mus_rts.append("    TAGGED_INDEPENDENT\n");
                        mus_rts.append("    TAGGED_REPEATABLE\n");
                        mus_rts.append("    WAIT 30T\n");
                        mus_rts.append(epi::StringFormat("    CHANGE_MUSIC %d\n", mus_number));
                        mus_rts.append("    RETRIGGER\n");
                        mus_rts.append("  END_SECTOR_TRIGGER\n");
                        mus_rts.append("END_MAP\n\n");
                        ReadRADScript(mus_rts, "MUSINFO");
                    }
                }
            }

            if (objtype->flags_ & kMapObjectFlagSpawnCeiling)
                z += sec->ceiling_height - objtype->height_;
            else
                z += sec->floor_height;

            MapObject *udmf_thing = SpawnMapThing(objtype, x, y, z, sec, angle, options, tag);

            // check for UDMF-specific thing stuff
            if (udmf_thing)
            {
                udmf_thing->target_visibility_ = alpha;
                udmf_thing->alpha_             = alpha;
                if (!AlmostEquals(healthfac, 1.0f))
                {
                    if (healthfac < 0)
                    {
                        udmf_thing->spawn_health_ = fabs(healthfac);
                        udmf_thing->health_       = fabs(healthfac);
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
                else if (!AlmostEquals(scalex, 0.0f) || !AlmostEquals(scaley, 0.0f))
                {
                    float sx           = AlmostEquals(scalex, 0.0f) ? 1.0f : scalex;
                    float sy           = AlmostEquals(scaley, 0.0f) ? 1.0f : scaley;
                    udmf_thing->scale_ = udmf_thing->model_scale_ = sy;
                    udmf_thing->aspect_ = udmf_thing->model_aspect_ = (sx / sy);
                    udmf_thing->height_ *= sy;
                    udmf_thing->radius_ *= sx;
                }
            }

            total_map_things++;
        }
        else // consume other blocks
        {
            for (;;)
            {
                tok = lex.Next(section);
                if (lex.Match("}") || tok == epi::kTokenEOF)
                    break;
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

        if (tok == epi::kTokenEOF)
            break;

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed TEXTMAP lump.\n");

        // check namespace
        if (lex.Match("="))
        {
            lex.Next(section);

            if (udmf_strict_namespace.d_)
            {
                if (section != "doom" && section != "heretic" && section != "edge-classic" &&
                    section != "zdoomtranslated")
                {
                    LogWarning("UDMF: %s uses unsupported namespace "
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
            if (lex.Match("}") || tok == epi::kTokenEOF)
                break;
        }
    }

    // initialize arrays
    level_vertexes = new Vertex[total_level_vertexes];
    level_sectors  = new Sector[total_level_sectors];
    EPI_CLEAR_MEMORY(level_sectors, Sector, total_level_sectors);
    level_lines = new Line[total_level_lines];
    EPI_CLEAR_MEMORY(level_lines, Line, total_level_lines);
    temp_line_sides = new int[total_level_lines * 2];
}

static void TransferMapSideDef(const RawSidedef *msd, Side *sd, bool two_sided)
{
    char upper_tex[10];
    char middle_tex[10];
    char lower_tex[10];

    int sec_num = AlignedLittleEndianS16(msd->sector);

    sd->top.translucency = 1.0f;
    sd->top.offset.X     = AlignedLittleEndianS16(msd->x_offset);
    sd->top.offset.Y     = AlignedLittleEndianS16(msd->y_offset);
    sd->top.x_matrix.X   = 1;
    sd->top.x_matrix.Y   = 0;
    sd->top.y_matrix.X   = 0;
    sd->top.y_matrix.Y   = 1;

    sd->middle = sd->top;
    sd->bottom = sd->top;

    if (sec_num < 0)
    {
        LogWarning("Level %s has sidedef with bad sector ref (%d)\n", current_map->lump_.c_str(), sec_num);
        sec_num = 0;
    }
    sd->sector = &level_sectors[sec_num];

    epi::CStringCopyMax(upper_tex, msd->upper_texture, 8);
    epi::CStringCopyMax(middle_tex, msd->mid_texture, 8);
    epi::CStringCopyMax(lower_tex, msd->lower_texture, 8);

    sd->top.image = ImageLookup(upper_tex, kImageNamespaceTexture, kImageLookupNull);

    if (sd->top.image == nullptr)
    {
        if (goobers.d_)
            sd->top.image = ImageLookup(upper_tex, kImageNamespaceTexture);
        else
            sd->top.image = ImageLookup(upper_tex, kImageNamespaceTexture);
    }

    sd->middle.image = ImageLookup(middle_tex, kImageNamespaceTexture);
    sd->bottom.image = ImageLookup(lower_tex, kImageNamespaceTexture);

    // handle BOOM colormaps with [242] linetype
    sd->top.boom_colormap    = colormaps.Lookup(upper_tex);
    sd->middle.boom_colormap = colormaps.Lookup(middle_tex);
    sd->bottom.boom_colormap = colormaps.Lookup(lower_tex);

    if (sd->middle.image && two_sided)
    {
        sd->middle_mask_offset = sd->middle.offset.Y;
        sd->middle.offset.Y    = 0;
    }

    if (sd->top.image && fabs(sd->top.offset.Y) > sd->top.image->ScaledHeightActual())
        sd->top.offset.Y = fmodf(sd->top.offset.Y, sd->top.image->ScaledHeightActual());

    if (sd->middle.image && fabs(sd->middle.offset.Y) > sd->middle.image->ScaledHeightActual())
        sd->middle.offset.Y = fmodf(sd->middle.offset.Y, sd->middle.image->ScaledHeightActual());

    if (sd->bottom.image && fabs(sd->bottom.offset.Y) > sd->bottom.image->ScaledHeightActual())
        sd->bottom.offset.Y = fmodf(sd->bottom.offset.Y, sd->bottom.image->ScaledHeightActual());
}

static void LoadSideDefs(int lump)
{
    int               i;
    const uint8_t    *data;
    const RawSidedef *msd;
    Side             *sd;

    int nummapsides;

    if (!VerifyLump(lump, "SIDEDEFS"))
        FatalError("Bad WAD: level %s missing SIDEDEFS.\n", current_map->lump_.c_str());

    nummapsides = GetLumpLength(lump) / sizeof(RawSidedef);

    if (nummapsides == 0)
        FatalError("Bad WAD: level %s contains 0 sidedefs.\n", current_map->lump_.c_str());

    level_sides = new Side[total_level_sides];

    EPI_CLEAR_MEMORY(level_sides, Side, total_level_sides);

    data = LoadLumpIntoMemory(lump);
    msd  = (const RawSidedef *)data;

    sd = level_sides;

    EPI_ASSERT(temp_line_sides);

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        int side0 = temp_line_sides[i * 2 + 0];
        int side1 = temp_line_sides[i * 2 + 1];

        EPI_ASSERT(side0 != -1);

        if (side0 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n", current_map->lump_.c_str(), i);
            side0 = nummapsides - 1;
        }

        if (side1 != -1 && side1 >= nummapsides)
        {
            LogWarning("Bad WAD: level %s linedef #%d has bad LEFT side.\n", current_map->lump_.c_str(), i);
            side1 = nummapsides - 1;
        }

        ld->side[0] = sd;
        TransferMapSideDef(msd + side0, sd, (side1 != -1));
        ld->front_sector = sd->sector;
        sd++;

        if (side1 != -1)
        {
            ld->side[1] = sd;
            TransferMapSideDef(msd + side1, sd, true);
            ld->back_sector = sd->sector;
            sd++;
        }

        EPI_ASSERT(sd <= level_sides + total_level_sides);
    }

    EPI_ASSERT(sd == level_sides + total_level_sides);

    delete[] data;
}

//
// SetupExtrafloors
//
// This is done after loading sectors (which sets extrafloor_maximum to 0)
// and after loading linedefs (which increases it for each new
// extrafloor).  So now we know the maximum number of extrafloors
// that can ever be needed.
//
// Note: this routine doesn't create any extrafloors (this is done
// later when their linetypes are activated).
//
static void SetupExtrafloors(void)
{
    int     i, ef_index = 0;
    Sector *ss;

    if (total_level_extrafloors == 0)
        return;

    level_extrafloors = new Extrafloor[total_level_extrafloors];

    EPI_CLEAR_MEMORY(level_extrafloors, Extrafloor, total_level_extrafloors);

    for (i = 0, ss = level_sectors; i < total_level_sectors; i++, ss++)
    {
        ss->extrafloor_first = level_extrafloors + ef_index;

        ef_index += ss->extrafloor_maximum;

        EPI_ASSERT(ef_index <= total_level_extrafloors);
    }

    EPI_ASSERT(ef_index == total_level_extrafloors);
}

static void SetupSlidingDoors(void)
{
    for (int i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        if (!ld->special || ld->special->s_.type_ == kSlidingDoorTypeNone)
            continue;

        if (ld->tag == 0 || ld->special->type_ == kLineTriggerManual)
            ld->slide_door = ld->special;
        else
        {
            for (int k = 0; k < total_level_lines; k++)
            {
                Line *other = level_lines + k;

                if (other->tag != ld->tag || other == ld)
                    continue;

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

    VerticalGap *cur_gap;

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        ld->maximum_gaps = ld->back_sector ? 1 : 0;

        // factor in extrafloors
        ld->maximum_gaps += ld->front_sector->extrafloor_maximum;

        if (ld->back_sector)
            ld->maximum_gaps += ld->back_sector->extrafloor_maximum;

        line_gaps += ld->maximum_gaps;
    }

    for (i = 0; i < total_level_sectors; i++)
    {
        Sector *sec = level_sectors + i;

        sec->maximum_gaps = sec->extrafloor_maximum + 1;

        sect_sight_gaps += sec->maximum_gaps;
    }

    total_level_vertical_gaps = line_gaps + sect_sight_gaps;

    // LogPrint("%dK used for vert gaps.\n", (total_level_vertical_gaps *
    //    sizeof(vgap_t) + 1023) / 1024);

    // zero is now impossible
    EPI_ASSERT(total_level_vertical_gaps > 0);

    level_vertical_gaps = new VerticalGap[total_level_vertical_gaps];

    EPI_CLEAR_MEMORY(level_vertical_gaps, VerticalGap, total_level_vertical_gaps);

    for (i = 0, cur_gap = level_vertical_gaps; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        if (ld->maximum_gaps == 0)
            continue;

        ld->gaps = cur_gap;
        cur_gap += ld->maximum_gaps;
    }

    EPI_ASSERT(cur_gap == (level_vertical_gaps + line_gaps));

    for (i = 0; i < total_level_sectors; i++)
    {
        Sector *sec = level_sectors + i;

        if (sec->maximum_gaps == 0)
            continue;

        sec->sight_gaps = cur_gap;
        cur_gap += sec->maximum_gaps;
    }

    EPI_ASSERT(cur_gap == (level_vertical_gaps + total_level_vertical_gaps));
}

static void DetectDeepWaterTrick(void)
{
    uint8_t *self_subs = new uint8_t[total_level_subsectors];

    memset(self_subs, 0, total_level_subsectors);

    for (int i = 0; i < total_level_segs; i++)
    {
        const Seg *seg = level_segs + i;

        if (seg->miniseg)
            continue;

        EPI_ASSERT(seg->front_subsector);

        if (seg->linedef->back_sector && seg->linedef->front_sector == seg->linedef->back_sector)
        {
            self_subs[seg->front_subsector - level_subsectors] |= 1;
        }
        else
        {
            self_subs[seg->front_subsector - level_subsectors] |= 2;
        }
    }

    int count;
    int pass = 0;

    do
    {
        pass++;

        count = 0;

        for (int j = 0; j < total_level_subsectors; j++)
        {
            Subsector *sub = level_subsectors + j;
            const Seg *seg;

            if (self_subs[j] != 1)
                continue;

            const Seg *Xseg = 0;

            for (seg = sub->segs; seg; seg = seg->subsector_next)
            {
                EPI_ASSERT(seg->back_subsector);

                int k = seg->back_subsector - level_subsectors;

                if (self_subs[k] & 2)
                {
                    if (!Xseg)
                        Xseg = seg;
                }
            }

            if (Xseg)
            {
                sub->deep_water_reference = Xseg->back_subsector->deep_water_reference
                                                ? Xseg->back_subsector->deep_water_reference
                                                : Xseg->back_subsector->sector;
                self_subs[j]              = 3;

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
        Vertex *v = level_vertexes + i;

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
    int     i;
    int     j;
    int     total;
    Line   *li;
    Sector *sector;
    Seg    *seg;
    float   bbox[4];
    Line  **line_p;

    // setup remaining seg information
    for (i = 0, seg = level_segs; i < total_level_segs; i++, seg++)
    {
        if (seg->partner)
            seg->back_subsector = seg->partner->front_subsector;

        if (!seg->front_sector)
            seg->front_sector = seg->front_subsector->sector;

        if (!seg->back_sector && seg->back_subsector)
            seg->back_sector = seg->back_subsector->sector;
    }

    // count number of lines in each sector
    li    = level_lines;
    total = 0;
    for (i = 0; i < total_level_lines; i++, li++)
    {
        total++;
        li->front_sector->line_count++;

        if (li->back_sector && li->back_sector != li->front_sector)
        {
            total++;
            li->back_sector->line_count++;
        }
    }

    // build line tables for each sector
    level_line_buffer = new Line *[total];

    line_p = level_line_buffer;
    sector = level_sectors;

    for (i = 0; i < total_level_sectors; i++, sector++)
    {
        BoundingBoxClear(bbox);
        sector->lines = line_p;
        li            = level_lines;
        for (j = 0; j < total_level_lines; j++, li++)
        {
            if (li->front_sector == sector || li->back_sector == sector)
            {
                *line_p++ = li;
                BoundingBoxAddPoint(bbox, li->vertex_1->X, li->vertex_1->Y);
                BoundingBoxAddPoint(bbox, li->vertex_2->X, li->vertex_2->Y);
            }
        }
        if (line_p - sector->lines != sector->line_count)
            FatalError("GroupLines: miscounted");

        // Allow vertex slope if a triangular sector or a rectangular
        // sector in which two adjacent verts have an identical z-height
        // and the other two have it unset
        if (sector->line_count == 3 && udmf_level)
        {
            sector->floor_vertex_slope_high_low   = {{-40000, 40000}};
            sector->ceiling_vertex_slope_high_low = {{-40000, 40000}};
            for (j = 0; j < 3; j++)
            {
                Vertex *vert   = sector->lines[j]->vertex_1;
                bool    add_it = true;
                for (HMM_Vec3 v : sector->floor_z_vertices)
                    if (AlmostEquals(v.X, vert->X) && AlmostEquals(v.Y, vert->Y))
                        add_it = false;
                if (add_it)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_vertex_slope = true;
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vertex_slope_high_low.X)
                            sector->floor_vertex_slope_high_low.X = vert->Z;
                        if (vert->Z < sector->floor_vertex_slope_high_low.Y)
                            sector->floor_vertex_slope_high_low.Y = vert->Z;
                    }
                    else
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, sector->floor_height}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceiling_vertex_slope = true;
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceiling_vertex_slope_high_low.X)
                            sector->ceiling_vertex_slope_high_low.X = vert->W;
                        if (vert->W < sector->ceiling_vertex_slope_high_low.Y)
                            sector->ceiling_vertex_slope_high_low.Y = vert->W;
                    }
                    else
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, sector->ceiling_height}});
                }
                vert   = sector->lines[j]->vertex_2;
                add_it = true;
                for (HMM_Vec3 v : sector->floor_z_vertices)
                    if (AlmostEquals(v.X, vert->X) && AlmostEquals(v.Y, vert->Y))
                        add_it = false;
                if (add_it)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_vertex_slope = true;
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vertex_slope_high_low.X)
                            sector->floor_vertex_slope_high_low.X = vert->Z;
                        if (vert->Z < sector->floor_vertex_slope_high_low.Y)
                            sector->floor_vertex_slope_high_low.Y = vert->Z;
                    }
                    else
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, sector->floor_height}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceiling_vertex_slope = true;
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceiling_vertex_slope_high_low.X)
                            sector->ceiling_vertex_slope_high_low.X = vert->W;
                        if (vert->W < sector->ceiling_vertex_slope_high_low.Y)
                            sector->ceiling_vertex_slope_high_low.Y = vert->W;
                    }
                    else
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, sector->ceiling_height}});
                }
            }
            if (!sector->floor_vertex_slope)
                sector->floor_z_vertices.clear();
            else
            {
                sector->floor_vertex_slope_normal = TripleCrossProduct(
                    sector->floor_z_vertices[0], sector->floor_z_vertices[1], sector->floor_z_vertices[2]);
                if (sector->floor_height > sector->floor_vertex_slope_high_low.X)
                    sector->floor_vertex_slope_high_low.X = sector->floor_height;
                if (sector->floor_height < sector->floor_vertex_slope_high_low.Y)
                    sector->floor_vertex_slope_high_low.Y = sector->floor_height;
            }
            if (!sector->ceiling_vertex_slope)
                sector->ceiling_z_vertices.clear();
            else
            {
                sector->ceiling_vertex_slope_normal = TripleCrossProduct(
                    sector->ceiling_z_vertices[0], sector->ceiling_z_vertices[1], sector->ceiling_z_vertices[2]);
                if (sector->ceiling_height < sector->ceiling_vertex_slope_high_low.Y)
                    sector->ceiling_vertex_slope_high_low.Y = sector->ceiling_height;
                if (sector->ceiling_height > sector->ceiling_vertex_slope_high_low.X)
                    sector->ceiling_vertex_slope_high_low.X = sector->ceiling_height;
            }
        }
        if (sector->line_count == 4 && udmf_level)
        {
            int floor_z_lines                     = 0;
            int ceil_z_lines                      = 0;
            sector->floor_vertex_slope_high_low   = {{-40000, 40000}};
            sector->ceiling_vertex_slope_high_low = {{-40000, 40000}};
            for (j = 0; j < 4; j++)
            {
                Vertex *vert      = sector->lines[j]->vertex_1;
                Vertex *vert2     = sector->lines[j]->vertex_2;
                bool    add_it_v1 = true;
                bool    add_it_v2 = true;
                for (HMM_Vec3 v : sector->floor_z_vertices)
                    if (AlmostEquals(v.X, vert->X) && AlmostEquals(v.Y, vert->Y))
                        add_it_v1 = false;
                for (HMM_Vec3 v : sector->floor_z_vertices)
                    if (AlmostEquals(v.X, vert2->X) && AlmostEquals(v.Y, vert2->Y))
                        add_it_v2 = false;
                if (add_it_v1)
                {
                    if (vert->Z < 32767.0f && vert->Z > -32768.0f)
                    {
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, vert->Z}});
                        if (vert->Z > sector->floor_vertex_slope_high_low.X)
                            sector->floor_vertex_slope_high_low.X = vert->Z;
                        if (vert->Z < sector->floor_vertex_slope_high_low.Y)
                            sector->floor_vertex_slope_high_low.Y = vert->Z;
                    }
                    else
                        sector->floor_z_vertices.push_back({{vert->X, vert->Y, sector->floor_height}});
                    if (vert->W < 32767.0f && vert->W > -32768.0f)
                    {
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, vert->W}});
                        if (vert->W > sector->ceiling_vertex_slope_high_low.X)
                            sector->ceiling_vertex_slope_high_low.X = vert->W;
                        if (vert->W < sector->ceiling_vertex_slope_high_low.Y)
                            sector->ceiling_vertex_slope_high_low.Y = vert->W;
                    }
                    else
                        sector->ceiling_z_vertices.push_back({{vert->X, vert->Y, sector->ceiling_height}});
                }
                if (add_it_v2)
                {
                    if (vert2->Z < 32767.0f && vert2->Z > -32768.0f)
                    {
                        sector->floor_z_vertices.push_back({{vert2->X, vert2->Y, vert2->Z}});
                        if (vert2->Z > sector->floor_vertex_slope_high_low.X)
                            sector->floor_vertex_slope_high_low.X = vert2->Z;
                        if (vert2->Z < sector->floor_vertex_slope_high_low.Y)
                            sector->floor_vertex_slope_high_low.Y = vert2->Z;
                    }
                    else
                        sector->floor_z_vertices.push_back({{vert2->X, vert2->Y, sector->floor_height}});
                    if (vert2->W < 32767.0f && vert2->W > -32768.0f)
                    {
                        sector->ceiling_z_vertices.push_back({{vert2->X, vert2->Y, vert2->W}});
                        if (vert2->W > sector->ceiling_vertex_slope_high_low.X)
                            sector->ceiling_vertex_slope_high_low.X = vert2->W;
                        if (vert2->W < sector->ceiling_vertex_slope_high_low.Y)
                            sector->ceiling_vertex_slope_high_low.Y = vert2->W;
                    }
                    else
                        sector->ceiling_z_vertices.push_back({{vert2->X, vert2->Y, sector->ceiling_height}});
                }
                if ((vert->Z < 32767.0f && vert->Z > -32768.0f) && (vert2->Z < 32767.0f && vert2->Z > -32768.0f) &&
                    AlmostEquals(vert->Z, vert2->Z))
                {
                    floor_z_lines++;
                }
                if ((vert->W < 32767.0f && vert->W > -32768.0f) && (vert2->W < 32767.0f && vert2->W > -32768.0f) &&
                    AlmostEquals(vert->W, vert2->W))
                {
                    ceil_z_lines++;
                }
            }
            if (floor_z_lines == 1 && sector->floor_z_vertices.size() == 4)
            {
                sector->floor_vertex_slope        = true;
                sector->floor_vertex_slope_normal = TripleCrossProduct(
                    sector->floor_z_vertices[0], sector->floor_z_vertices[1], sector->floor_z_vertices[2]);
                if (sector->floor_height > sector->floor_vertex_slope_high_low.X)
                    sector->floor_vertex_slope_high_low.X = sector->floor_height;
                if (sector->floor_height < sector->floor_vertex_slope_high_low.Y)
                    sector->floor_vertex_slope_high_low.Y = sector->floor_height;
            }
            else
                sector->floor_z_vertices.clear();
            if (ceil_z_lines == 1 && sector->ceiling_z_vertices.size() == 4)
            {
                sector->ceiling_vertex_slope        = true;
                sector->ceiling_vertex_slope_normal = TripleCrossProduct(
                    sector->ceiling_z_vertices[0], sector->ceiling_z_vertices[1], sector->ceiling_z_vertices[2]);
                if (sector->ceiling_height < sector->ceiling_vertex_slope_high_low.Y)
                    sector->ceiling_vertex_slope_high_low.Y = sector->ceiling_height;
                if (sector->ceiling_height > sector->ceiling_vertex_slope_high_low.X)
                    sector->ceiling_vertex_slope_high_low.X = sector->ceiling_height;
            }
            else
                sector->ceiling_z_vertices.clear();
        }

        // set the degenmobj_t to the middle of the bounding box
        sector->sound_effects_origin.x = (bbox[kBoundingBoxRight] + bbox[kBoundingBoxLeft]) / 2;
        sector->sound_effects_origin.y = (bbox[kBoundingBoxTop] + bbox[kBoundingBoxBottom]) / 2;
        sector->sound_effects_origin.z = (sector->floor_height + sector->ceiling_height) / 2;
    }
}

static inline void AddSectorToVertices(int *branches, Line *ld, Sector *sec)
{
    if (!sec)
        return;

    unsigned short sec_idx = sec - level_sectors;

    for (int vert = 0; vert < 2; vert++)
    {
        int v_idx = (vert ? ld->vertex_2 : ld->vertex_1) - level_vertexes;

        EPI_ASSERT(0 <= v_idx && v_idx < total_level_vertexes);

        if (branches[v_idx] < 0)
            continue;

        VertexSectorList *L = level_vertex_sector_lists + branches[v_idx];

        if (L->total >= kVertexSectorListMaximum)
            continue;

        int pos;

        for (pos = 0; pos < L->total; pos++)
            if (L->sectors[pos] == sec_idx)
                break;

        if (pos < L->total)
            continue; // already in there

        L->sectors[L->total++] = sec_idx;
    }
}

static void CreateVertexSeclists(void)
{
    // step 1: determine number of linedef branches at each vertex
    int *branches = new int[total_level_vertexes];

    EPI_CLEAR_MEMORY(branches, int, total_level_vertexes);

    int i;

    for (i = 0; i < total_level_lines; i++)
    {
        int v1_idx = level_lines[i].vertex_1 - level_vertexes;
        int v2_idx = level_lines[i].vertex_2 - level_vertexes;

        EPI_ASSERT(0 <= v1_idx && v1_idx < total_level_vertexes);
        EPI_ASSERT(0 <= v2_idx && v2_idx < total_level_vertexes);

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
    level_vertex_sector_lists = new VertexSectorList[num_triples];

    EPI_CLEAR_MEMORY(level_vertex_sector_lists, VertexSectorList, num_triples);

    LogDebug("Created %d seclists from %d vertices (%1.1f%%)\n", num_triples, total_level_vertexes,
             num_triples * 100 / (float)total_level_vertexes);

    // multiple passes for each linedef:
    //   pass #1 takes care of normal sectors
    //   pass #2 handles any extrafloors
    //
    // Rationale: normal sectors are more important, hence they
    //            should be allocated to the limited slots first.

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        for (int side = 0; side < 2; side++)
        {
            Sector *sec = side ? ld->back_sector : ld->front_sector;

            AddSectorToVertices(branches, ld, sec);
        }
    }

    for (i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;

        for (int side = 0; side < 2; side++)
        {
            Sector *sec = side ? ld->back_sector : ld->front_sector;

            if (!sec)
                continue;

            Extrafloor *ef;

            for (ef = sec->bottom_extrafloor; ef; ef = ef->higher)
                AddSectorToVertices(branches, ld, ef->extrafloor_line->front_sector);

            for (ef = sec->bottom_liquid; ef; ef = ef->higher)
                AddSectorToVertices(branches, ld, ef->extrafloor_line->front_sector);
        }
    }

    // step 4: finally, update the segs that touch those vertices
    for (i = 0; i < total_level_segs; i++)
    {
        Seg *sg = level_segs + i;

        for (int vert = 0; vert < 2; vert++)
        {
            int v_idx = (vert ? sg->vertex_2 : sg->vertex_1) - level_vertexes;

            // skip GL vertices
            if (v_idx < 0 || v_idx >= total_level_vertexes)
                continue;

            if (branches[v_idx] < 0)
                continue;

            sg->vertex_sectors[vert] = level_vertex_sector_lists + branches[v_idx];
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
        StopSoundEffect(&level_sectors[i].sound_effects_origin);
    }
}

void ShutdownLevel(void)
{
    // Destroys everything on the level.

#ifdef DEVELOPERS
    if (!level_active)
        FatalError("ShutdownLevel: no level to shut down!");
#endif

    level_active = false;

    ClearRespawnQueue();

    P_RemoveSectorStuff();

    StopLevelSoundEffects();

    DestroyAllForces();
    DestroyAllLights();
    DestroyAllPlanes();
    DestroyAllSliders();
    DestroyAllAmbientSounds();

    DDFBoomClearGeneralizedTypes();

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
        Sector *sec = &level_sectors[i];
        if (sec->floor_slope)
        {
            delete sec->floor_slope;
            sec->floor_slope = nullptr;
        }
        if (sec->ceiling_slope)
        {
            delete sec->ceiling_slope;
            sec->ceiling_slope = nullptr;
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

    RemoveAllMapObjects(false);
}

void LevelSetup(void)
{
    // Sets up the current level using the skill passed and the
    // information in current_map.
    //
    // -ACB- 1998/08/09 Use current_map to ref lump and par time

    if (level_active)
        ShutdownLevel();

    // -ACB- 1998/08/27 nullptr the head pointers for the linked lists....
    respawn_queue_head   = nullptr;
    map_object_list_head = nullptr;
    seen_monsters.clear();

    // get lump for map header e.g. MAP01
    int lumpnum = CheckMapLumpNumberForName(current_map->lump_.c_str());
    if (lumpnum < 0)
        FatalError("No such level: %s\n", current_map->lump_.c_str());

    // get lump for XGL3 nodes from an XWA file
    int xgl_lump = CheckXGLLumpNumberForName(current_map->lump_.c_str());

    // ignore XGL nodes if it occurs _before_ the normal level marker.
    // [ something has gone horribly wrong if this happens! ]
    if (xgl_lump < lumpnum)
        xgl_lump = -1;

    // shouldn't happen (as during startup we checked for XWA files)
    if (xgl_lump < 0)
        FatalError("Internal error: missing XGL nodes.\n");

    // -CW- 2017/01/29: check for UDMF map lump
    if (VerifyLump(lumpnum + 1, "TEXTMAP"))
    {
        udmf_level          = true;
        udmf_lump_number    = lumpnum + 1;
        int      raw_length = 0;
        uint8_t *raw_udmf   = LoadLumpIntoMemory(udmf_lump_number, &raw_length);
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
        if (IsLumpIndexValid(lumpnum + kLumpBehavior) && VerifyLump(lumpnum + kLumpBehavior, "BEHAVIOR"))
        {
            FatalError("Level %s is Hexen format (not supported).\n", current_map->lump_.c_str());
        }
        LoadVertexes(lumpnum + kLumpVertexes);
        LoadSectors(lumpnum + kLumpSectors);
        LoadLineDefs(lumpnum + kLumpLinedefs);
        LoadSideDefs(lumpnum + kLumpSidedefs);
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

    ComputeSkyHeights();

    // compute sector and line gaps
    for (int j = 0; j < total_level_sectors; j++)
        RecomputeGapsAroundSector(level_sectors + j);

    ClearBodyQueue();

    // set up world state
    // (must be before loading things to create Extrafloors)
    SpawnMapSpecials1();

    // -AJA- 1999/10/21: Clear out player starts (ready to load).
    ClearPlayerStarts();

    unknown_thing_map.clear();

    // Must do before loading things
    GetMUSINFOTracksForLevel();

    if (!udmf_level)
        LoadThings(lumpnum + kLumpThings);
    else
        LoadUDMFThings();

        // OK, CRC values have now been computed
#ifdef DEVELOPERS
    LogDebug("MAP CRCS: S=%08x L=%08x T=%08x\n", map_sectors_crc.crc, map_lines_crc.crc, map_things_crc.crc);
#endif

    CreateVertexSeclists();

    SpawnMapSpecials2(current_map->autotag_);

    AutomapInitLevel();

    RendererUpdateSkyBoxTextures();

    // preload graphics
    if (precache)
        PrecacheLevelGraphics();

    // setup categories based on game mode (SP/COOP/DM)
    UpdateSoundCategoryLimits();

    // FIXME: cache sounds (esp. for player)

    ChangeMusic(current_map->music_, true); // start level music

    level_active = true;
}

void PlayerStateInit(void)
{
    StartupProgressMessage(language["PlayState"]);

    // There should not yet exist a player
    EPI_ASSERT(total_players == 0);

    ClearPlayerStarts();
}

LineType *LookupLineType(int num)
{
    if (num <= 0)
        return nullptr;

    LineType *def = linetypes.Lookup(num);

    // DDF types always override
    if (def)
        return def;

    if (DDFIsBoomLineType(num))
        return DDFBoomGetGeneralizedLine(num);

    LogWarning("P_LookupLineType(): Unknown linedef type %d\n", num);

    return linetypes.Lookup(0); // template line
}

SectorType *LookupSectorType(int num)
{
    if (num <= 0)
        return nullptr;

    SectorType *def = sectortypes.Lookup(num);

    // DDF types always override
    if (def)
        return def;

    if (DDFIsBoomSectorType(num))
        return DDFBoomGetGeneralizedSector(num);

    LogWarning("P_LookupSectorType(): Unknown sector type %d\n", num);

    return sectortypes.Lookup(0); // template sector
}

void LevelShutdown(void)
{
    if (level_active)
    {
        ShutdownLevel();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
