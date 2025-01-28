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

#include "ddf_main.h"
#include "ddf_states.h"
#include "ddf_types.h"
#include "epi.h"

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
#define DDF_FIELD(name, base, field, parser) {name, parser, ((char *)&base.field - (char *)&base), nullptr}

#define DDF_SUB_LIST(name, base, field, subcomms) {"*" name, nullptr, ((char *)&base.field - (char *)&base), subcomms}

#define DDF_STATE(name, redir, base, field) {name, redir, ((char *)&base.field - (char *)&base)}

//
// This structure passes the information needed to DDFMainReadFile, so that
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
    void (*parse_field)(const char *field, const char *contents, int index, bool is_last);

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

// DDFMAIN Code (Reading all files, main init & generic functions).
void DDFMainReadFile(DDFReadInfo *readinfo, const std::string &data);

extern int         cur_ddf_line_num;
extern std::string cur_ddf_filename;
extern std::string cur_ddf_entryname;
extern std::string cur_ddf_linedata;

#ifdef __GNUC__
[[noreturn]] void DDFError(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDFDebug(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDFWarning(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DDFWarnError(const char *err, ...) __attribute__((format(printf, 1, 2)));
#else
[[noreturn]] void DDFError(const char *err, ...);
void DDFDebug(const char *err, ...);
void DDFWarning(const char *err, ...);
void DDFWarnError(const char *err, ...);
#endif

void DDFMainGetPercent(const char *info, void *storage);
void DDFMainGetPercentAny(const char *info, void *storage);
void DDFMainGetBoolean(const char *info, void *storage);
void DDFMainGetFloat(const char *info, void *storage);
void DDFMainGetAngle(const char *info, void *storage);
void DDFMainGetSlope(const char *info, void *storage);
void DDFMainGetNumeric(const char *info, void *storage);
void DDFMainGetString(const char *info, void *storage);
void DDFMainGetLumpName(const char *info, void *storage);
void DDFMainGetTime(const char *info, void *storage);
void DDFMainGetColourmap(const char *info, void *storage);
void DDFMainGetRGB(const char *info, void *storage);
void DDFMainGetWhenAppear(const char *info, void *storage);
void DDFMainGetBitSet(const char *info, void *storage);

bool DDFMainParseField(const DDFCommandList *commands, const char *field, const char *contents, uint8_t *obj_base);
void DDFMainLookupSound(const char *info, void *storage);
void DDFMainRefAttack(const char *info, void *storage);

void DDFDummyFunction(const char *info, void *storage);

DDFCheckFlagResult DDFMainCheckSpecialFlag(const char *name, const DDFSpecialFlags *flag_set, int *flag_value,
                                           bool allow_prefixes, bool allow_user);

int DDFMainLookupDirector(const MapObjectDefinition *obj, const char *info);

// DDFANIM Code
void DDFAnimInit(void);
void DDFAnimCleanUp(void);

// DDFATK Code
void DDFAttackInit(void);
void DDFAttackCleanUp(void);

// DDFGAME Code
void DDFGameInit(void);
void DDFGameCleanUp(void);

// DDFLANG Code
void DDFLanguageCleanUp(void);

// DDFLEVL Code
void DDFLevelInit(void);
void DDFLevelCleanUp(void);

// DDFLINE Code
void DDFLinedefInit(void);
void DDFLinedefCleanUp(void);

constexpr const char *kEmptyColormapName   = "_NONE_";
constexpr int16_t     kEmptyColormapNumber = -777;

// DDFMOBJ Code  (Moving Objects)
void DDFMobjInit(void);
void DDFMobjCleanUp(void);
void DDFMobjGetExtra(const char *info, void *storage);
void DDFMobjGetItemType(const char *info, void *storage);
void DDFMobjGetBpAmmo(const char *info, void *storage);
void DDFMobjGetBpAmmoLimit(const char *info, void *storage);
void DDFMobjGetBpArmour(const char *info, void *storage);
void DDFMobjGetBpKeys(const char *info, void *storage);
void DDFMobjGetBpWeapon(const char *info, void *storage);
void DDFMobjGetPlayer(const char *info, void *storage);

void ThingParseField(const char *field, const char *contents, int index, bool is_last);

// DDFMUS Code
void DDFMusicPlaylistInit(void);
void DDFMusicPlaylistCleanUp(void);

// DDFSTAT Code
void DDFStateInit(void);
void DDFStateGetAttack(const char *arg, State *cur_state);
void DDFStateGetMobj(const char *arg, State *cur_state);
void DDFStateGetSound(const char *arg, State *cur_state);
void DDFStateGetInteger(const char *arg, State *cur_state);
void DDFStateGetIntPair(const char *arg, State *cur_state);
void DDFStateGetFloat(const char *arg, State *cur_state);
void DDFStateGetPercent(const char *arg, State *cur_state);
void DDFStateGetJump(const char *arg, State *cur_state);
void DDFStateGetJumpInt(const char *arg, State *cur_state);
void DDFStateGetJumpIntPair(const char *arg, State *cur_state);
void DDFStateGetBecome(const char *arg, State *cur_state);
void DDFStateGetMorph(const char *arg, State *cur_state);
void DDFStateGetBecomeWeapon(const char *arg, State *cur_state);
void DDFStateGetFrame(const char *arg, State *cur_state);
void DDFStateGetAngle(const char *arg, State *cur_state);
void DDFStateGetSlope(const char *arg, State *cur_state);
void DDFStateGetRGB(const char *arg, State *cur_state);
// For MBF21 mostly, but in theory backwards compatible with the DEH parser rework;
// grab up to 8 integer arguments separated by commas
void DDFStateGetDEHParams(const char *arg, State *cur_state);

bool DDFMainParseState(uint8_t *object, std::vector<StateRange> &group, const char *field, const char *contents,
                       int index, bool is_last, bool is_weapon, const DDFStateStarter *starters,
                       const DDFActionCode *actions);

void DDFStateBeginRange(std::vector<StateRange> &group);
void DDFStateFinishRange(std::vector<StateRange> &group);
void DDFStateCleanUp(void);

// DDFSECT Code
void DDFSectorInit(void);
void DDFSectGetDestRef(const char *info, void *storage);
void DDFSectGetExit(const char *info, void *storage);
void DDFSectGetLighttype(const char *info, void *storage);
void DDFSectGetMType(const char *info, void *storage);
void DDFSectorCleanUp(void);

// DDFSFX Code
void DDFSFXInit(void);
void DDFSFXCleanUp(void);

// DDFSWTH Code
// -KM- 1998/07/31 Switch and Anim ddfs.
void DDFSwitchInit(void);
void DDFSwitchCleanUp(void);

// DDFWEAP Code
void                         DDFWeaponInit(void);
void                         DDFWeaponCleanUp(void);
extern const DDFSpecialFlags ammo_types[];

// DDFCOLM Code -AJA- 1999/07/09.
void DDFColmapInit(void);
void DDFColmapCleanUp(void);

// DDFFONT Code -AJA- 2004/11/13.
void DDFFontInit(void);
void DDFFontCleanUp(void);

// DDFSTYLE Code -AJA- 2004/11/14.
void DDFStyleInit(void);
void DDFStyleCleanUp(void);

// DDFFONT Code -AJA- 2004/11/18.
void DDFImageInit(void);
void DDFImageCleanUp(void);

// DDFFLAT Code 2022
void DDFFlatInit(void);
void DDFFlatCleanUp(void);

// WADFIXES Code 2022
void DDFFixInit(void);
void DDFFixCleanUp(void);

// MOVIES 2023
void DDFMovieInit(void);
void DDFMovieCleanUp(void);

// Miscellaneous stuff needed here & there
extern const DDFCommandList floor_commands[];
extern const DDFCommandList damage_commands[];

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
