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
#define DDF_FIELD(name, base, field, parser)                                                                           \
    {                                                                                                                  \
        name, parser, ((char *)&base.field - (char *)&base), nullptr                                                   \
    }

#define DDF_SUB_LIST(name, base, field, subcomms)                                                                      \
    {                                                                                                                  \
        "*" name, nullptr, ((char *)&base.field - (char *)&base), subcomms                                             \
    }

#define DDF_STATE(name, redir, base, field)                                                                            \
    {                                                                                                                  \
        name, redir, ((char *)&base.field - (char *)&base)                                                             \
    }

//
// This structure passes the information needed to DdfMainReadFile, so that
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
    kDdfCheckFlagUnknown,

    // the flag should be set (i.e. forced on)
    kDdfCheckFlagPositive,

    // the flag should be cleared (i.e. forced off)
    kDdfCheckFlagNegative,

    // the flag should be made user-definable
    kDdfCheckFlagUser
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

// DdfMAIN Code (Reading all files, main init & generic functions).
void DdfMainReadFile(DDFReadInfo *readinfo, const std::string &data);

extern int         cur_ddf_line_num;
extern std::string cur_ddf_filename;
extern std::string cur_ddf_entryname;
extern std::string cur_ddf_linedata;

#ifdef __GNUC__
void DdfError(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DdfDebug(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DdfWarning(const char *err, ...) __attribute__((format(printf, 1, 2)));
void DdfWarnError(const char *err, ...) __attribute__((format(printf, 1, 2)));
#else
void DdfError(const char *err, ...);
void DdfDebug(const char *err, ...);
void DdfWarning(const char *err, ...);
void DdfWarnError(const char *err, ...);
#endif

void DdfMainGetPercent(const char *info, void *storage);
void DdfMainGetPercentAny(const char *info, void *storage);
void DdfMainGetBoolean(const char *info, void *storage);
void DdfMainGetFloat(const char *info, void *storage);
void DdfMainGetAngle(const char *info, void *storage);
void DdfMainGetSlope(const char *info, void *storage);
void DdfMainGetNumeric(const char *info, void *storage);
void DdfMainGetString(const char *info, void *storage);
void DdfMainGetLumpName(const char *info, void *storage);
void DdfMainGetTime(const char *info, void *storage);
void DdfMainGetColourmap(const char *info, void *storage);
void DdfMainGetRGB(const char *info, void *storage);
void DdfMainGetWhenAppear(const char *info, void *storage);
void DdfMainGetBitSet(const char *info, void *storage);

bool DdfMainParseField(const DDFCommandList *commands, const char *field, const char *contents, uint8_t *obj_base);
void DdfMainLookupSound(const char *info, void *storage);
void DdfMainRefAttack(const char *info, void *storage);

void DdfDummyFunction(const char *info, void *storage);

DDFCheckFlagResult DdfMainCheckSpecialFlag(const char *name, const DDFSpecialFlags *flag_set, int *flag_value,
                                            bool allow_prefixes, bool allow_user);

int DdfMainLookupDirector(const MapObjectDefinition *obj, const char *info);

// DdfANIM Code
void DdfAnimInit(void);
void DdfAnimCleanUp(void);

// DdfATK Code
void DdfAttackInit(void);
void DdfAttackCleanUp(void);

// DdfGAME Code
void DdfGameInit(void);
void DdfGameCleanUp(void);

// DdfLANG Code
void DdfLanguageCleanUp(void);

// DdfLEVL Code
void DdfLevelInit(void);
void DdfLevelCleanUp(void);

// DdfLINE Code
void DdfLinedefInit(void);
void DdfLinedefCleanUp(void);

constexpr const char *kEmptyColormapName   = "_NONE_";
constexpr int16_t     kEmptyColormapNumber = -777;

// DdfMOBJ Code  (Moving Objects)
void DdfMobjInit(void);
void DdfMobjCleanUp(void);
void DdfMobjGetExtra(const char *info, void *storage);
void DdfMobjGetItemType(const char *info, void *storage);
void DdfMobjGetBpAmmo(const char *info, void *storage);
void DdfMobjGetBpAmmoLimit(const char *info, void *storage);
void DdfMobjGetBpArmour(const char *info, void *storage);
void DdfMobjGetBpKeys(const char *info, void *storage);
void DdfMobjGetBpWeapon(const char *info, void *storage);
void DdfMobjGetPlayer(const char *info, void *storage);

void ThingParseField(const char *field, const char *contents, int index, bool is_last);

// DdfMUS Code
void DdfMusicPlaylistInit(void);
void DdfMusicPlaylistCleanUp(void);

// DdfSTAT Code
void DdfStateInit(void);
void DdfStateGetAttack(const char *arg, State *cur_state);
void DdfStateGetMobj(const char *arg, State *cur_state);
void DdfStateGetSound(const char *arg, State *cur_state);
void DdfStateGetInteger(const char *arg, State *cur_state);
void DdfStateGetIntPair(const char *arg, State *cur_state);
void DdfStateGetFloat(const char *arg, State *cur_state);
void DdfStateGetPercent(const char *arg, State *cur_state);
void DdfStateGetJump(const char *arg, State *cur_state);
void DdfStateGetBecome(const char *arg, State *cur_state);
void DdfStateGetMorph(const char *arg, State *cur_state);
void DdfStateGetBecomeWeapon(const char *arg, State *cur_state);
void DdfStateGetFrame(const char *arg, State *cur_state);
void DdfStateGetAngle(const char *arg, State *cur_state);
void DdfStateGetSlope(const char *arg, State *cur_state);
void DdfStateGetRGB(const char *arg, State *cur_state);

bool DdfMainParseState(uint8_t *object, std::vector<StateRange> &group, const char *field, const char *contents,
                        int index, bool is_last, bool is_weapon, const DDFStateStarter *starters,
                        const DDFActionCode *actions);

void DdfStateBeginRange(std::vector<StateRange> &group);
void DdfStateFinishRange(std::vector<StateRange> &group);
void DdfStateCleanUp(void);

// DdfSECT Code
void DdfSectorInit(void);
void DdfSectGetDestRef(const char *info, void *storage);
void DdfSectGetExit(const char *info, void *storage);
void DdfSectGetLighttype(const char *info, void *storage);
void DdfSectGetMType(const char *info, void *storage);
void DdfSectorCleanUp(void);

// DdfSFX Code
void DdfSFXInit(void);
void DdfSFXCleanUp(void);

// DdfSWTH Code
// -KM- 1998/07/31 Switch and Anim ddfs.
void DdfSwitchInit(void);
void DdfSwitchCleanUp(void);

// DdfWEAP Code
void                         DdfWeaponInit(void);
void                         DdfWeaponCleanUp(void);
extern const DDFSpecialFlags ammo_types[];

// DdfCOLM Code -AJA- 1999/07/09.
void DdfColmapInit(void);
void DdfColmapCleanUp(void);

// DdfFONT Code -AJA- 2004/11/13.
void DdfFontInit(void);
void DdfFontCleanUp(void);

// DdfSTYLE Code -AJA- 2004/11/14.
void DdfStyleInit(void);
void DdfStyleCleanUp(void);

// DdfFONT Code -AJA- 2004/11/18.
void DdfImageInit(void);
void DdfImageCleanUp(void);

// DdfFLAT Code 2022
void DdfFlatInit(void);
void DdfFlatCleanUp(void);

// WADFIXES Code 2022
void DdfFixInit(void);
void DdfFixCleanUp(void);

// MOVIES 2023
void DdfMovieInit(void);
void DdfMovieCleanUp(void);

// Miscellaneous stuff needed here & there
extern const DDFCommandList floor_commands[];
extern const DDFCommandList damage_commands[];

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
