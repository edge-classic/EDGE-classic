//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Local Header)
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

#pragma once

#include <stddef.h>

#include "epi.h"
#include "main.h"
#include "states.h"
#include "types.h"

//
// This structure forms the basis for the command checking, it hands back a code
// pointer and sometimes a pointer to a (int) numeric value. (used for info that
// gets its value directly from the file).
//
struct DDFCommandList
{
    // command name
    const char *name;

    // parse function.
    void (*parse_command)(const char *info, void *storage);

    ptrdiff_t offset;

    const struct DDFCommandList *sub_comms;
};

// NOTE: requires an instantiated base struct
#define DDF_FIELD(name, base, field, parser)                         \
    {                                                                \
        name, parser, ((char *)&base.field - (char *)&base), nullptr \
    }

#define DDF_SUB_LIST(name, base, field, subcomms)                          \
    {                                                                      \
        "*" name, nullptr, ((char *)&base.field - (char *)&base), subcomms \
    }

#define DDF_STATE(name, redir, base, field)                \
    {                                                      \
        name, redir, ((char *)&base.field - (char *)&base) \
    }

//
// This structure passes the information needed to DDF_MainReadFile, so that
// the reader uses the correct procedures when reading a file.
//
struct DDFReadInfo
{
    // name of the lump, for error messages
    const char *lumpname;

    // the file has to start with <tag>
    const char *tag;

    //
    //  FUNCTIONS
    //

    // create a new dynamic entry with the given name.  For number-only
    // ddf files (lines, sectors and playlist), it is a number.  For
    // things.ddf, it is a name with an optional ":####" number
    // appended.  For everything else it is just a normal name.
    //
    // this also instantiates the static entry's information (excluding
    // name and/or number) using the built-in defaults.
    //
    // if an entry with the given name/number already exists, re-use
    // that entry for the dynamic part, otherwise create a new dynamic
    // entry and add it to the list.  Note that only the name and/or
    // number need to be kept valid in the dynamic entry.  Returns true
    // if the name already existed, otherwise false.
    //
    // Note: for things.ddf, only the name is significant when checking
    // if the entry already exists.
    //
    void (*start_entry)(const char *name, bool extend);

    // parse a single field for the entry.  Usually it will just call
    // the ddf_main routine to handle the command list.  For
    // comma-separated fields (specials, states, etc), this routine will
    // be called multiple times, once for each element, and `index' is
    // used to indicate which element (starting at 0).
    //
    void (*parse_field)(const char *field, const char *contents, int index,
                        bool is_last);

    // when the entry has finished, this routine can perform any
    // necessary operations here (such as updating a number -> entry
    // lookup table).  In particular, it should copy the static buffer
    // part into the dynamic part.  It also should compute the CRC.
    //
    void (*finish_entry)(void);

    // this function is called when the #CLEARALL directive is used.
    // The entries should be deleted if it is safe (i.e. there are no
    // pointers to them), otherwise they should be marked `disabled' and
    // ignored in subsequent searches.  Note: The parser ensures that
    // this is never called in the middle of an entry.
    //
    void (*clear_all)(void);
};

//
// This structure forms the basis for referencing specials.
//
struct DDFSpecialFlags
{
    // name of special
    const char *name;

    // flag(s) or value of special
    int flags;

    // this is true if the DDF name (e.g. "GRAVITY") is opposite to the
    // code's flag name (e.g. MF_NoGravity).
    bool negative;
};

enum DDFCheckFlagResult
{
    // special flag is unknown
    kDDFCheckFlagUnknown,

    // the flag should be set (i.e. forced on)
    kDDFCheckFlagPositive,

    // the flag should be cleared (i.e. forced off)
    kDDFCheckFlagNegative,

    // the flag should be made user-definable
    kDDFCheckFlagUser
};

//
// This is a reference table, that determines what code pointer is placed in the
// states table entry.
//
struct DDFActionCode
{
    const char *actionname;
    void (*action)(MapObject *mo);

    // -AJA- 1999/08/09: This function handles the argument when brackets
    // are present (e.g. "WEAPON_SHOOT(FIREBALL)").  nullptr if unused.
    void (*handle_arg)(const char *arg, State *curstate);
};

// This structure is used for parsing states
struct DDFStateStarter
{
    // state label
    const char *label;

    // redirection label for last state
    const char *last_redir;

    // pointer to state_num storage
    ptrdiff_t offset;
};

// DDF_MAIN Code (Reading all files, main init & generic functions).
void DDF_MainReadFile(DDFReadInfo *readinfo, const std::string &data);

extern int         cur_ddf_line_num;
extern std::string cur_ddf_filename;
extern std::string cur_ddf_entryname;
extern std::string cur_ddf_linedata;

#ifdef __GNUC__
void DDF_Error(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDF_Debug(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDF_Warning(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDF_WarnError(const char *err, ...) __attribute__((format(printf, 1, 2)));
#else
void DDF_Error(const char *err, ...);
void DDF_Debug(const char *err, ...);
void DDF_Warning(const char *err, ...);
void DDF_WarnError(const char *err, ...);
#endif

void DDF_MainGetPercent(const char *info, void *storage);
void DDF_MainGetPercentAny(const char *info, void *storage);
void DDF_MainGetBoolean(const char *info, void *storage);
void DDF_MainGetFloat(const char *info, void *storage);
void DDF_MainGetAngle(const char *info, void *storage);
void DDF_MainGetSlope(const char *info, void *storage);
void DDF_MainGetNumeric(const char *info, void *storage);
void DDF_MainGetString(const char *info, void *storage);
void DDF_MainGetLumpName(const char *info, void *storage);
void DDF_MainGetTime(const char *info, void *storage);
void DDF_MainGetColourmap(const char *info, void *storage);
void DDF_MainGetRGB(const char *info, void *storage);
void DDF_MainGetWhenAppear(const char *info, void *storage);
void DDF_MainGetBitSet(const char *info, void *storage);

bool DDF_MainParseField(const DDFCommandList *commands, const char *field,
                        const char *contents, uint8_t *obj_base);
void DDF_MainLookupSound(const char *info, void *storage);
void DDF_MainRefAttack(const char *info, void *storage);

void DDF_DummyFunction(const char *info, void *storage);

DDFCheckFlagResult DDF_MainCheckSpecialFlag(const char            *name,
                                            const DDFSpecialFlags *flag_set,
                                            int                   *flag_value,
                                            bool allow_prefixes,
                                            bool allow_user);

int DDF_MainLookupDirector(const MapObjectDefinition *obj, const char *info);

// DDF_ANIM Code
void DDF_AnimInit(void);
void DDF_AnimCleanUp(void);

// DDF_ATK Code
void DDF_AttackInit(void);
void DDF_AttackCleanUp(void);

// DDF_GAME Code
void DDF_GameInit(void);
void DDF_GameCleanUp(void);

// DDF_LANG Code
void DDF_LanguageCleanUp(void);

// DDF_LEVL Code
void DDF_LevelInit(void);
void DDF_LevelCleanUp(void);

// DDF_LINE Code
void DDF_LinedefInit(void);
void DDF_LinedefCleanUp(void);

constexpr const char *kEmptyColormapName   = "_NONE_";
constexpr int16_t     kEmptyColormapNumber = -777;

// DDF_MOBJ Code  (Moving Objects)
void DDF_MobjInit(void);
void DDF_MobjCleanUp(void);
void DDF_MobjGetExtra(const char *info, void *storage);
void DDF_MobjGetItemType(const char *info, void *storage);
void DDF_MobjGetBpAmmo(const char *info, void *storage);
void DDF_MobjGetBpAmmoLimit(const char *info, void *storage);
void DDF_MobjGetBpArmour(const char *info, void *storage);
void DDF_MobjGetBpKeys(const char *info, void *storage);
void DDF_MobjGetBpWeapon(const char *info, void *storage);
void DDF_MobjGetPlayer(const char *info, void *storage);

void ThingParseField(const char *field, const char *contents, int index,
                     bool is_last);

// DDF_MUS Code
void DDF_MusicPlaylistInit(void);
void DDF_MusicPlaylistCleanUp(void);

// DDF_STAT Code
void DDF_StateInit(void);
void DDF_StateGetAttack(const char *arg, State *cur_state);
void DDF_StateGetMobj(const char *arg, State *cur_state);
void DDF_StateGetSound(const char *arg, State *cur_state);
void DDF_StateGetInteger(const char *arg, State *cur_state);
void DDF_StateGetIntPair(const char *arg, State *cur_state);
void DDF_StateGetFloat(const char *arg, State *cur_state);
void DDF_StateGetPercent(const char *arg, State *cur_state);
void DDF_StateGetJump(const char *arg, State *cur_state);
void DDF_StateGetBecome(const char *arg, State *cur_state);
void DDF_StateGetMorph(const char *arg, State *cur_state);
void DDF_StateGetBecomeWeapon(const char *arg, State *cur_state);
void DDF_StateGetFrame(const char *arg, State *cur_state);
void DDF_StateGetAngle(const char *arg, State *cur_state);
void DDF_StateGetSlope(const char *arg, State *cur_state);
void DDF_StateGetRGB(const char *arg, State *cur_state);

bool DDF_MainParseState(uint8_t *object, std::vector<StateRange> &group,
                        const char *field, const char *contents, int index,
                        bool is_last, bool is_weapon,
                        const DDFStateStarter *starters,
                        const DDFActionCode   *actions);

void DDF_StateBeginRange(std::vector<StateRange> &group);
void DDF_StateFinishRange(std::vector<StateRange> &group);
void DDF_StateCleanUp(void);

// DDF_SECT Code
void DDF_SectorInit(void);
void DDF_SectGetDestRef(const char *info, void *storage);
void DDF_SectGetExit(const char *info, void *storage);
void DDF_SectGetLighttype(const char *info, void *storage);
void DDF_SectGetMType(const char *info, void *storage);
void DDF_SectorCleanUp(void);

// DDF_SFX Code
void DDF_SFXInit(void);
void DDF_SFXCleanUp(void);

// DDF_SWTH Code
// -KM- 1998/07/31 Switch and Anim ddfs.
void DDF_SwitchInit(void);
void DDF_SwitchCleanUp(void);

// DDF_WEAP Code
void                         DDF_WeaponInit(void);
void                         DDF_WeaponCleanUp(void);
extern const DDFSpecialFlags ammo_types[];

// DDF_COLM Code -AJA- 1999/07/09.
void DDF_ColmapInit(void);
void DDF_ColmapCleanUp(void);

// DDF_FONT Code -AJA- 2004/11/13.
void DDF_FontInit(void);
void DDF_FontCleanUp(void);

// DDF_STYLE Code -AJA- 2004/11/14.
void DDF_StyleInit(void);
void DDF_StyleCleanUp(void);

// DDF_FONT Code -AJA- 2004/11/18.
void DDF_ImageInit(void);
void DDF_ImageCleanUp(void);

// DDF_FLAT Code 2022
void DDF_FlatInit(void);
void DDF_FlatCleanUp(void);

// WADFIXES Code 2022
void DDF_FixInit(void);
void DDF_FixCleanUp(void);

// MOVIES 2023
void DDF_MovieInit(void);
void DDF_MovieCleanUp(void);

// Miscellaneous stuff needed here & there
extern const DDFCommandList floor_commands[];
extern const DDFCommandList damage_commands[];

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
