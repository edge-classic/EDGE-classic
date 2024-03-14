//------------------------------------------------------------------------
//  PATCH Loading
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_patch.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_music.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_things.h"
#include "deh_weapons.h"
#include "epi.h"
#include "str_compare.h"
#include "str_util.h"
namespace dehacked
{

static constexpr uint16_t kMaximumLineLength =
    768;  // Lobo 2023: seeing lots of truncated wads lately so bumped up from
          // 512
static constexpr uint16_t kMaximumTextStringLength = 1200;

static constexpr uint8_t kPrettyLength = 28;

// Some version 1.2 constants
static constexpr uint8_t  kV12Things  = 103;
static constexpr uint16_t kV12Frames  = 512;
static constexpr uint8_t  kV12Sprites = 105;
static constexpr uint8_t  kV12Sounds  = 63;
static constexpr uint16_t kV16Texts   = 1053;

// Thing conversion array from 1.2 to 1.666
static constexpr int16_t thing_v12_to_v166[kV12Things] = {
    0,   11,  1,   2,   12,  13,  14,  18,  15,  19,  21,  30,  31,  32,  16,
    33,  34,  35,  37,  38,  39,  41,  42,  43,  44,  45,  46,  47,  48,  49,
    50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  63,  64,  65,
    66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  81,  82,  83,
    84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,
    99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126};

// Frame conversion array from 1.2 to 1.666
static constexpr int16_t frame_v12_to_v166[kV12Frames] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 49, 50, 51, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
    95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 522, 523, 524, 525,
    526, 107, 108, 109, 110, 111,
    /* 100 */
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
    127, 128, 129, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
    191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234,
    /* 200 */
    235, 442, 443, 444, 445, 446, 447, 448, 449, 450, 451, 452, 453, 454, 455,
    456, 457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 475,
    476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490,
    491, 492, 493, 494, 495, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511,
    512, 513, 514, 515, 527, 528, 529, 530, 531, 532, 533, 534, 535, 536, 537,
    538, 539, 540, 541, 542, 543, 544, 545, 546, 547, 548, 585, 586, 587, 588,
    589, 590, 591, 592, 593, 594, 595, 596, 597, 598,
    /* 300 */
    599, 600, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611, 612, 613,
    614, 615, 616, 617, 618, 619, 620, 621, 622, 623, 624, 625, 626, 627, 628,
    629, 630, 631, 674, 675, 676, 677, 678, 679, 680, 681, 682, 683, 684, 685,
    686, 687, 688, 689, 690, 691, 692, 693, 694, 695, 696, 697, 698, 699, 700,
    130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 802, 803, 804,
    805, 806, 807, 808, 809, 810, 811, 812, 816, 817, 818, 819, 820, 821, 822,
    823, 824, 825, 826, 827, 828, 829, 830, 831, 832,
    /* 400 */
    833, 834, 835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846, 847,
    848, 849, 850, 851, 852, 853, 854, 855, 856, 861, 862, 863, 864, 865, 866,
    867, 868, 869, 870, 871, 872, 873, 874, 875, 876, 877, 878, 879, 880, 881,
    882, 883, 884, 886, 887, 888, 889, 890, 891, 892, 893, 894, 895, 896, 897,
    898, 899, 900, 901, 902, 903, 904, 905, 906, 907, 908, 909, 910, 911, 912,
    913, 914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927,
    928, 929, 930, 931, 932, 933, 934, 935, 936, 937,
    /* 500 */
    938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949};

// Sound conversion array from 1.2 to 1.666
static constexpr int16_t sound_v12_to_v166[kV12Sounds] = {
    0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 51, 52, 55, 57, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 75, 76, 77, 81, 82, 83, 84, 85, 86};

// Sprite conversion array from 1.2 to 1.666
static constexpr int16_t sprite_v12_to_v166[kV12Sprites] = {
    0,   1,   2,   3,   4,   5,   7,   8,   9,   10,  11,  12,  13,  14,  15,
    16,  17,  18,  19,  41,  20,  21,  22,  23,  24,  25,  28,  29,  30,  39,
    40,  42,  44,  45,  49,  26,  55,  56,  57,  58,  60,  61,  62,  63,  64,
    65,  66,  67,  68,  69,  70,  71,  72,  73,  75,  76,  77,  78,  79,  80,
    81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  94,  95,  96,
    97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126};

namespace patch
{
InputBuffer *pat_buf;

bool file_error;

int patch_fmt; /* 1 to 6 */
int dhe_ver;   /* 12, 13, 20-24, 30-31 */
int doom_ver;  /* 12, 16 to 21 */

enum ObjectKind
{
    kObjectKindMobj,
    kObjectKindAmmo,
    kObjectKindWeapon,
    kObjectKindFrame,
    kObjectKindSound,
    kObjectKindSprite
};

void DetectMsg(const char *kind)
{
    LogPrint("Detected %s patch file from DEHACKED v%d.%d\n", kind,
             dhe_ver / 10, dhe_ver % 10);
}

void VersionMsg(void)
{
    LogPrint("Patch format %d, for DOOM EXE %d.%d%s\n", patch_fmt,
             doom_ver / 10, doom_ver % 10, (doom_ver == 16) ? "66" : "");
}

int GetRawInt(void)
{
    if (pat_buf->EndOfFile() || pat_buf->Error()) file_error = true;

    if (file_error) return -1;

    unsigned char raw[4];

    pat_buf->Read(raw, 4);

    return ((int)raw[0]) + ((int)raw[1] << 8) + ((int)raw[2] << 16) +
           ((int)(char)raw[3] << 24);
}

void GetRawString(char *buf, int max_len)
{
    // luckily for us, DeHackEd ensured that replacement strings
    // were never truncated short (i.e. the NUL byte appearing
    // in an earlier 32-bit word).  Hence we don't need to know
    // the length of the original strings in order to read in
    // modified strings.

    int len = 0;

    for (;;)
    {
        int ch = pat_buf->GetCharacter();

        if (ch == 0) break;

        if (ch == EOF || pat_buf->Error()) file_error = true;

        if (file_error) break;

        buf[len++] = ch;

        if (len >= max_len)
        {
            FatalError(
                "Dehacked: Error - Text string exceeds internal buffer "
                "length.\n"
                "[> %d characters, from binary patch file]\n",
                kMaximumTextStringLength);
        }
    }

    buf[len] = 0;

    // strings are aligned to 4 byte boundaries
    for (; (len % 4) != 3; len++) pat_buf->GetCharacter();
}

const char *ObjectName(int o_kind)
{
    switch (o_kind)
    {
        case kObjectKindMobj:
            return "thing";
        case kObjectKindAmmo:
            return "ammo";
        case kObjectKindWeapon:
            return "weapon";
        case kObjectKindFrame:
            return "frame";
        case kObjectKindSound:
            return "sound";
        case kObjectKindSprite:
            return "sprite";

        default:
            FatalError("Dehacked: Error - Illegal object kind: %d\n", o_kind);
    }

    return nullptr;  // not reached
}

void MarkObject(int o_kind, int o_num)
{
    LogPrint("[%d] MODIFIED\n", o_num);

    switch (o_kind)
    {
        case kObjectKindMobj:
            things::MarkThing(o_num);
            break;
        case kObjectKindAmmo:
            ammo::MarkAmmo(o_num);
            break;
        case kObjectKindWeapon:
            weapons::MarkWeapon(o_num);
            break;
        case kObjectKindFrame:
            frames::MarkState(o_num);
            break;
        case kObjectKindSound:
            sounds::MarkSound(o_num);
            break;

        case kObjectKindSprite:  // not needed
            break;

        default:
            FatalError("Dehacked: Error - Illegal object kind: %d\n", o_kind);
    }
}

void GetInt(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("Int: %d\n", temp);

    if (*dest == temp) return;

    MarkObject(o_kind, o_num);

    *dest = temp;
}

void GetFlags(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("Flags: 0x%08x\n", temp);

    // prevent the BOOM/MBF specific flags from being set
    // from binary patch files.
    temp &= ~ALL_BEX_FLAGS;

    if (*dest == temp) return;

    MarkObject(o_kind, o_num);

    *dest = temp;
}

void GetFrame(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("Frame: %d\n", temp);

    if (doom_ver == 12)
    {
        if (temp < 0 || temp >= kV12Frames)
        {
            LogDebug(
                "Dehacked: Warning - Found illegal V1.2 frame number: %d\n",
                temp);
            return;
        }

        temp = frame_v12_to_v166[temp];
    }

    if (temp < 0 || temp >= kTotalStates)
    {
        LogDebug("Dehacked: Warning - Found illegal frame number: %d\n", temp);
        return;
    }

    // no need to MarkObject, already done (e.g. in ReadBinaryThing)

    *dest = temp;
}

void GetSprite(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("Sprite: %d\n", temp);

    if (doom_ver == 12)
    {
        if (temp < 0 || temp >= kV12Sprites)
        {
            LogDebug(
                "Dehacked: Warning - Found illegal V1.2 sprite number: %d\n",
                temp);
            return;
        }

        temp = sprite_v12_to_v166[temp];
    }

    if (temp < 0 || temp >= kTotalSprites)
    {
        LogDebug("Dehacked: Warning - Found illegal sprite number: %d\n", temp);
        return;
    }

    *dest = temp;
}

void GetSound(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("Sound: %d\n", temp);

    if (doom_ver == 12)
    {
        if (temp < 0 || temp >= kV12Sounds)
        {
            LogDebug(
                "Dehacked: Warning - Found illegal V1.2 sound number: %d\n",
                temp);
            return;
        }

        temp = sound_v12_to_v166[temp];
    }

    if (temp < 0 || temp >= kTotalSoundEffects)
    {
        LogDebug("Dehacked: Warning - Found illegal sound number: %d\n", temp);
        return;
    }

    // no need to MarkObject, already done (e.g. in ReadBinaryThing)

    *dest = temp;
}

void GetAmmoType(int o_kind, int o_num, int *dest)
{
    int temp = GetRawInt();

    LogPrint("AmmoType: %d\n", temp);

    if (temp < 0 || temp > 5)
    {
        LogDebug("Dehacked: Warning - Found illegal ammo type: %d\n", temp);
        return;
    }

    if (temp == 4) temp = kAmmoTypeNoAmmo;

    // no need to MarkObject, already done (in ReadBinaryWeapon)

    *dest = temp;
}

const char *PrettyTextString(const char *t)
{
    static char buf[kPrettyLength * 2 + 10];

    while (epi::IsSpaceASCII(*t)) t++;

    if (!*t) return "<<EMPTY>>";

    int len = 0;

    for (; *t && len < kPrettyLength; t++)
    {
        if (t[0] == t[1] && t[1] == t[2]) continue;

        if (*t == '"') { buf[len++] = '\''; }
        else if (*t == '\n')
        {
            buf[len++] = '\\';
            buf[len++] = 'n';
        }
        else if ((unsigned char)*t < 32 || (unsigned char)*t >= 127)
        {
            buf[len++] = '?';
        }
        else
            buf[len++] = *t;
    }

    if (*t)
    {
        buf[len++] = '.';
        buf[len++] = '.';
        buf[len++] = '.';
    }

    buf[len] = 0;

    return buf;
}

void ReadBinaryThing(int mt_num)
{
    LogPrint("\n--- ReadBinaryThing %d ---\n", mt_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary thing table.\n");

    DehackedMapObjectDefinition *mobj = things::GetModifiedMobj(mt_num);

    GetInt(kObjectKindMobj, mt_num, &mobj->doomednum);
    GetFrame(kObjectKindMobj, mt_num, &mobj->spawnstate);
    GetInt(kObjectKindMobj, mt_num, &mobj->spawnhealth);
    GetFrame(kObjectKindMobj, mt_num, &mobj->seestate);
    GetSound(kObjectKindMobj, mt_num, &mobj->seesound);
    GetInt(kObjectKindMobj, mt_num, &mobj->reactiontime);

    GetSound(kObjectKindMobj, mt_num, &mobj->attacksound);
    GetFrame(kObjectKindMobj, mt_num, &mobj->painstate);
    GetInt(kObjectKindMobj, mt_num, &mobj->painchance);
    GetSound(kObjectKindMobj, mt_num, &mobj->painsound);
    GetFrame(kObjectKindMobj, mt_num, &mobj->meleestate);
    GetFrame(kObjectKindMobj, mt_num, &mobj->missilestate);
    GetFrame(kObjectKindMobj, mt_num, &mobj->deathstate);
    GetFrame(kObjectKindMobj, mt_num, &mobj->xdeathstate);
    GetSound(kObjectKindMobj, mt_num, &mobj->deathsound);

    GetInt(kObjectKindMobj, mt_num, &mobj->speed);
    GetInt(kObjectKindMobj, mt_num, &mobj->radius);
    GetInt(kObjectKindMobj, mt_num, &mobj->height);
    GetInt(kObjectKindMobj, mt_num, &mobj->mass);
    GetInt(kObjectKindMobj, mt_num, &mobj->damage);
    GetSound(kObjectKindMobj, mt_num, &mobj->activesound);
    GetFlags(kObjectKindMobj, mt_num, &mobj->flags);

    if (doom_ver != 12) GetFrame(kObjectKindMobj, mt_num, &mobj->raisestate);
}

void ReadBinaryAmmo(void)
{
    LogPrint("\n--- ReadBinaryAmmo ---\n");

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary ammo table.\n");

    GetInt(kObjectKindAmmo, 0, ammo::player_max + 0);
    GetInt(kObjectKindAmmo, 1, ammo::player_max + 1);
    GetInt(kObjectKindAmmo, 2, ammo::player_max + 2);
    GetInt(kObjectKindAmmo, 3, ammo::player_max + 3);

    GetInt(kObjectKindAmmo, 0, ammo::pickups + 0);
    GetInt(kObjectKindAmmo, 1, ammo::pickups + 1);
    GetInt(kObjectKindAmmo, 2, ammo::pickups + 2);
    GetInt(kObjectKindAmmo, 3, ammo::pickups + 3);
}

void ReadBinaryWeapon(int wp_num)
{
    LogPrint("\n--- ReadBinaryWeapon %d ---\n", wp_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary weapon table.\n");

    WeaponInfo *weap = weapon_info + wp_num;

    GetAmmoType(kObjectKindWeapon, wp_num, &weap->ammo);

    GetFrame(kObjectKindWeapon, wp_num, &weap->upstate);
    GetFrame(kObjectKindWeapon, wp_num, &weap->downstate);
    GetFrame(kObjectKindWeapon, wp_num, &weap->readystate);
    GetFrame(kObjectKindWeapon, wp_num, &weap->atkstate);
    GetFrame(kObjectKindWeapon, wp_num, &weap->flashstate);
}

void ReadBinaryFrame(int st_num)
{
    LogPrint("\n--- ReadBinaryFrame %d ---\n", st_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary frame table.\n");

    State *state = frames::GetModifiedState(st_num);

    GetSprite(kObjectKindFrame, st_num, &state->sprite);
    GetInt(kObjectKindFrame, st_num, &state->frame);
    GetInt(kObjectKindFrame, st_num, &state->tics);

    GetRawInt();  // ignore code-pointer

    GetFrame(kObjectKindFrame, st_num, &state->next_state);

    GetRawInt();  // ignore misc1/misc2 fields
    GetRawInt();
}

void ReadBinarySound(int s_num)
{
    LogPrint("\n--- ReadBinarySound %d ---\n", s_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary sound table.\n");

    GetRawInt();  // ignore sound name pointer
    GetRawInt();  // ignore singularity
    GetRawInt();  // ignore priority

    GetRawInt();  // ignore link pointer
    GetRawInt();  // ignore link pitch
    GetRawInt();  // ignore link volume

    GetRawInt();  //
    GetRawInt();  // unused stuff
    GetRawInt();  //
}

void ReadBinarySprite(int spr_num)
{
    LogPrint("\n--- ReadBinarySprite %d ---\n", spr_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary sprite table.\n");

    GetRawInt();  // ignore sprite name pointer
}

void ReadBinaryText(int tx_num)
{
    LogPrint("\n--- ReadBinaryText %d ---\n", tx_num);

    if (file_error)
        FatalError("Dehacked: Error - File error reading binary text table.\n");

    static char text_buf[kMaximumTextStringLength + 8];

    GetRawString(text_buf, kMaximumTextStringLength);

    // LogPrint("\"%s\"\n", PrettyTextString(text_buf));

    text_strings::ReplaceBinaryString(tx_num, text_buf);
}

DehackedResult LoadReallyOld(void)
{
    char tempfmt = 0;

    pat_buf->Read(&tempfmt, 1);

    if (tempfmt < 1 || tempfmt > 2)
    {
        SetErrorMsg(
            "Bad format byte in DeHackEd patch file.\n"
            "[Really old patch, format byte %d]\n",
            tempfmt);
        return kDehackedConversionParseError;
    }

    patch_fmt = (int)tempfmt;
    doom_ver  = 12;
    dhe_ver   = 11 + patch_fmt;

    DetectMsg("really old");
    VersionMsg();

    int j;

    for (j = 0; j < kV12Things; j++) ReadBinaryThing(thing_v12_to_v166[j]);

    ReadBinaryAmmo();

    for (j = 0; j < 8; j++) ReadBinaryWeapon(j);  // no need to convert

    if (patch_fmt == 2)
    {
        for (j = 0; j < kV12Frames; j++) ReadBinaryFrame(frame_v12_to_v166[j]);
    }

    return kDehackedConversionOK;
}

DehackedResult LoadBinary(void)
{
    char tempdoom = 0;
    char tempfmt  = 0;

    pat_buf->Read(&tempdoom, 1);
    pat_buf->Read(&tempfmt, 1);

    if (tempfmt == 3)
    {
        SetErrorMsg("Doom 1.6 beta patches are not supported.\n");
        return kDehackedConversionParseError;
    }
    else if (tempfmt != 4)
    {
        SetErrorMsg(
            "Bad format byte in DeHackEd patch file.\n"
            "[Binary patch, format byte %d]\n",
            tempfmt);
        return kDehackedConversionParseError;
    }

    patch_fmt = 4;

    if (tempdoom != 12 && !(tempdoom >= 16 && tempdoom <= 21))
    {
        SetErrorMsg(
            "Bad Doom release number in patch file !\n"
            "[Binary patch, release number %d]\n",
            tempdoom);
        return kDehackedConversionParseError;
    }

    doom_ver = (int)tempdoom;

    DetectMsg("binary");
    VersionMsg();

    int j;

    if (doom_ver == 12)
    {
        for (j = 0; j < kV12Things; j++) ReadBinaryThing(thing_v12_to_v166[j]);
    }
    else
    {
        for (j = 0; j < kTotalDehackedMapObjectTypes; j++) ReadBinaryThing(j);
    }

    ReadBinaryAmmo();

    int num_weap = (doom_ver == 12) ? 8 : 9;

    for (j = 0; j < num_weap; j++) ReadBinaryWeapon(j);

    if (doom_ver == 12)
    {
        for (j = 0; j < kV12Frames; j++) ReadBinaryFrame(frame_v12_to_v166[j]);
    }
    else
    {
        /* -AJA- NOTE WELL: the "- 1" here.  Testing confirms that the
         * DeHackEd code omits the very last frame from the V1.666+
         * binary format.  The V1.2 binary format is fine though.
         */
        for (j = 0; j < kTotalStates - 1; j++) ReadBinaryFrame(j);
    }

    if (doom_ver == 12)
    {
        // Note: this V1.2 sound/sprite handling UNTESTED.  I'm not even
        // sure that there exists any such DEH patch files.

        for (j = 1; j < kV12Sounds; j++) ReadBinarySound(sound_v12_to_v166[j]);

        for (j = 0; j < kV12Sprites; j++)
            ReadBinarySprite(sprite_v12_to_v166[j]);
    }
    else
    {
        /* -AJA- NOTE WELL: we start at one, as DEH patches don't
         * include the dummy entry.  More important: the "- 1" here,
         * the very last sound is "DSRADIO" which is omitted from the
         * patch file.  Confirmed through testing.
         */
        for (j = 1; j < kTotalSoundEffects - 1; j++) ReadBinarySound(j);

        for (j = 0; j < kTotalSprites; j++) ReadBinarySprite(j);
    }

    if (doom_ver == 16 || doom_ver == 17)
    {
        // -AJA- starts at one simply to match v166_index
        for (j = 1; j <= kV16Texts; j++) ReadBinaryText(j);
    }

    return kDehackedConversionOK;
}

//------------------------------------------------------------------------

// This diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum SectionKind
{
    // patch format 5:
    kDEH_THING,
    kDEH_SOUND,
    kDEH_FRAME,
    kDEH_SPRITE,
    kDEH_AMMO,
    kDEH_WEAPON,
    /* DEH_TEXT handled specially */

    // patch format 6:
    kDEH_PTR,
    kDEH_CHEAT,
    kDEH_MISC,

    // boom extensions:
    BEX_HELPER,
    BEX_STRINGS,
    BEX_PARS,
    BEX_CODEPTR,
    BEX_SPRITES,
    BEX_SOUNDS,
    BEX_MUSIC,

    kTotalSections
};

const char *section_name[] = {"Thing", "Sound", "Frame", "Sprite", "Ammo",
                              "Weapon", "Pointer", "Cheat", "Misc",

                              // Boom extensions:
                              "[HELPER]", "[STRINGS]", "[PARS]", "[CODEPTR]",
                              "[SPRITES]", "[SOUNDS]", "[MUSIC]"};

char line_buf[kMaximumLineLength + 4];
int  line_num;

char *equal_pos;

int active_section = -1;
int active_obj     = -1;

const char *cur_txt_ptr;
bool        syncing;

void GetNextLine(void)
{
    int len   = 0;
    equal_pos = nullptr;

    for (;;)
    {
        int ch = pat_buf->GetCharacter();

        if (ch == EOF)
        {
            if (pat_buf->Error())
                LogDebug("Dehacked: Warning - Read error on input file.\n");

            break;
        }

        // end-of-line detection.  We support the following conventions:
        //    1. CR LF    (MSDOS/Windows)
        //    2. LF only  (Unix)
        //    3. CR only  (Macintosh)

        if (ch == '\n') break;

        if (ch == '\r')
        {
            ch = pat_buf->GetCharacter();

            if (ch != EOF && ch != '\n') pat_buf->UngetCharacter(ch);

            break;
        }

        if (len >= kMaximumLineLength)  // truncation mode
            continue;

        if (!equal_pos && (char)ch == '=') equal_pos = line_buf + len;

        line_buf[len++] = (char)ch;

        if (len == kMaximumLineLength)
            LogDebug("Dehacked: Warning - Truncating very long line (#%d).\n",
                     line_num);
    }

    line_buf[len] = 0;
    line_num++;
}

void StripTrailingSpace(void)
{
    int len = strlen(line_buf);

    while (len > 0 && epi::IsSpaceASCII(line_buf[len - 1])) len--;

    line_buf[len] = 0;
}

bool ValidateObject(void)
{
    int min_obj = 0;
    int max_obj = 0;

    if (active_section == kDEH_MISC || active_section == kDEH_CHEAT ||
        active_section == kDEH_SPRITE)
    {
        return true; /* don't care */
    }

    if (patch_fmt <= 5)
    {
        switch (active_section)
        {
            case kDEH_THING:
                max_obj = kTotalDehackedMapObjectTypes;
                min_obj = 1;
                break;

            case kDEH_SOUND:
                max_obj = kTotalSoundEffects - 1;
                break;
            case kDEH_FRAME:
                max_obj = kTotalStates - 1;
                break;
            case kDEH_AMMO:
                max_obj = kTotalAmmoTypes - 1;
                break;
            case kDEH_WEAPON:
                max_obj = kTotalWeapons - 1;
                break;
            case kDEH_PTR:
                max_obj = kTotalStates - 1;
                break;

            default:
                FatalError("Dehacked: Error - Bad active_section value %d\n",
                        active_section);
        }
    }
    else /* patch_fmt == 6, allow BOOM/MBF stuff */
    {
        switch (active_section)
        {
            case kDEH_AMMO:
                max_obj = kTotalAmmoTypes - 1;
                break;
            case kDEH_WEAPON:
                max_obj = kTotalWeapons - 1;
                break;

            // for DSDehacked, allow very high values
            case kDEH_FRAME:
                max_obj = 32767;
                break;
            case kDEH_PTR:
                max_obj = 32767;
                break;
            case kDEH_SOUND:
                max_obj = 32767;
                break;
            case kDEH_THING:
                max_obj = 32767;
                min_obj = 1;
                break;

            default:
                FatalError("Dehacked: Error - Bad active_section value %d\n",
                        active_section);
        }
    }

    if (active_obj < min_obj || active_obj > max_obj)
    {
        LogDebug("Dehacked: Warning - Line %d: Illegal %s number: %d.\n",
                 line_num, section_name[active_section], active_obj);

        syncing = true;
        return false;
    }

    return true;
}

bool CheckNewSection(void)
{
    int i;
    int obj_num;

    for (i = 0; i < kTotalSections; i++)
    {
        if (epi::StringPrefixCaseCompareASCII(line_buf, section_name[i]) != 0)
            continue;

        // make sure no '=' appears (to prevent a mismatch with
        // DEH ^Frame sections and BEX CODEPTR ^FRAME lines).
        for (const char *pos = line_buf; *pos && *pos != '('; pos++)
            if (*pos == '=') return false;

        if (line_buf[0] == '[')
        {
            active_section = i;
            active_obj     = -1;  // unused

            if (active_section == BEX_PARS || active_section == BEX_HELPER)
            {
                LogDebug("Dehacked: Warning - Ignoring BEX %s section.\n",
                         section_name[i]);
            }

            return true;
        }

        int sec_len = strlen(section_name[i]);

        if (!epi::IsSpaceASCII(line_buf[sec_len])) continue;

        // for the "Pointer" section, MBF and other source ports don't use
        // the immediately following number, but the state number in `()`
        // parentheses.  support that idiom here.
        if (i == kDEH_PTR)
        {
            if (sscanf(line_buf + sec_len, " %*i ( %*s %i )", &obj_num) != 1)
                continue;
        }
        else
        {
            if (sscanf(line_buf + sec_len, " %i ", &obj_num) != 1) continue;
        }

        active_section = i;
        active_obj     = obj_num;

        return ValidateObject();
    }

    return false;
}

void ReadTextString(char *dest, int len)
{
    EPI_ASSERT(cur_txt_ptr);

    char *begin = dest;

    int start_line = line_num;

    while (len > 0)
    {
        if ((dest - begin) >= kMaximumTextStringLength)
        {
            FatalError(
                "Dehacked: Error - Text string exceeds internal buffer "
                "length.\n"
                "[> %d characters, starting on line %d]\n",
                kMaximumTextStringLength, start_line);
        }

        if (*cur_txt_ptr)
        {
            *dest++ = *cur_txt_ptr++;
            len--;
            continue;
        }

        if (pat_buf->EndOfFile())
            FatalError(
                "Dehacked: Error - End of file while reading Text "
                "replacement.\n");

        GetNextLine();
        cur_txt_ptr = line_buf;

        *dest++ = '\n';
        len--;
    }

    *dest = 0;
}

void ProcessTextSection(int len1, int len2)
{
    LogPrint("TEXT REPLACE: %d %d\n", len1, len2);

    static char text_1[kMaximumTextStringLength + 8];
    static char text_2[kMaximumTextStringLength + 8];

    GetNextLine();

    cur_txt_ptr = line_buf;

    ReadTextString(text_1, len1);
    ReadTextString(text_2, len2);

    LogPrint("- Before <%s>\n", text_1);
    LogPrint("- After  <%s>\n", text_2);

    if (len1 == 4 && len2 == 4)
        if (sprites::ReplaceSprite(text_1, text_2)) return;

    if (len1 <= 6 && len2 <= 6)
    {
        if (sounds::ReplaceSound(text_1, text_2)) return;

        if (music::ReplaceMusic(text_1, text_2)) return;
    }

    if (text_strings::ReplaceString(text_1, text_2)) return;

    LogDebug("Dehacked: Warning - Cannot match text: \"%s\"\n",
             PrettyTextString(text_1));
}

void ReadBexTextString(char *dest)  // upto kMaximumTextStringLength chars
{
    EPI_ASSERT(cur_txt_ptr);

    char *begin = dest;

    int start_line = line_num;

    for (;;)
    {
        if ((dest - begin) >= kMaximumTextStringLength)
        {
            FatalError(
                "Dehacked: Error - Bex String exceeds internal buffer length.\n"
                "[> %d characters, starting on line %d]\n",
                kMaximumTextStringLength, start_line);
        }

        if (*cur_txt_ptr == 0) break;

        // handle the newline sequence
        if (cur_txt_ptr[0] == '\\' && epi::ToLowerASCII(cur_txt_ptr[1]) == 'n')
        {
            cur_txt_ptr += 2;
            *dest++ = '\n';
            continue;
        }

        if (cur_txt_ptr[0] == '\\' && cur_txt_ptr[1] == 0)
        {
            do  // need a loop to ignore comment lines
            {
                if (pat_buf->EndOfFile())
                    FatalError(
                        "Dehacked: Error - End of file while reading Bex "
                        "String replacement.\n");

                GetNextLine();
                StripTrailingSpace();
            } while (line_buf[0] == '#');

            cur_txt_ptr = line_buf;

            // strip leading whitespace from continuing lines
            while (epi::IsSpaceASCII(*cur_txt_ptr)) cur_txt_ptr++;

            continue;
        }

        *dest++ = *cur_txt_ptr++;
    }

    *dest = 0;
}

void ProcessBexString(void)
{
    LogPrint("BEX STRING REPLACE: %s\n", line_buf);

    if (strlen(line_buf) >= 100)
        FatalError("Dehacked: Error - Bex string name too long !\nLine %d: %s\n",
                line_num, line_buf);

    char bex_field[104];

    strcpy(bex_field, line_buf);

    static char text_buf[kMaximumTextStringLength + 8];

    cur_txt_ptr = equal_pos;

    ReadBexTextString(text_buf);

    LogPrint("- Replacement <%s>\n", text_buf);

    if (!text_strings::ReplaceBexString(bex_field, text_buf))
        LogDebug("Dehacked: Warning - Line %d: unknown BEX string name: %s\n",
                 line_num, bex_field);
}

void ProcessLine(void)
{
    EPI_ASSERT(active_section >= 0);

    LogPrint("Section %d Object %d : <%s>\n", active_section, active_obj,
             line_buf);

    if (active_section == BEX_PARS || active_section == BEX_HELPER) return;

    if (!equal_pos)
    {
        LogDebug("Dehacked: Warning - Ignoring line: %s\n", line_buf);
        return;
    }

    if (active_section == BEX_STRINGS)
    {
        // this is needed for compatible handling of trailing '\'
        StripTrailingSpace();
    }

    // remove whitespace around '=' sign

    char *final = equal_pos;

    while (final > line_buf && epi::IsSpaceASCII(final[-1])) final--;

    *final = 0;

    equal_pos++;

    while (*equal_pos && epi::IsSpaceASCII(*equal_pos)) equal_pos++;

    if (line_buf[0] == 0)
    {
        LogDebug(
            "Dehacked: Warning - Line %d: No field name before equal sign.\n",
            line_num);
        return;
    }
    else if (equal_pos[0] == 0)
    {
        LogDebug("Dehacked: Warning - Line %d: No value after equal sign.\n",
                 line_num);
        return;
    }

    if (patch_fmt >= 6 && active_section == kDEH_THING &&
        epi::StringCaseCompareASCII(line_buf, "Bits") == 0)
    {
        things::AlterBexBits(equal_pos);
        return;
    }

    if (patch_fmt >= 6 && active_section == kDEH_THING &&
        epi::StringCaseCompareASCII(line_buf, "MBF21 Bits") == 0)
    {
        things::AlterMBF21Bits(equal_pos);
        return;
    }

    int num_value = 0;

    if (active_section != kDEH_CHEAT && active_section <= BEX_HELPER)
    {
        if (sscanf(equal_pos, " %i ", &num_value) != 1)
        {
            LogDebug("Dehacked: Warning - Line %d: unreadable %s value: %s\n",
                     line_num, section_name[active_section], equal_pos);
            return;
        }
    }

    switch (active_section)
    {
        case kDEH_THING:
            things::AlterThing(num_value);
            break;
        case kDEH_SOUND:
            sounds::AlterSound(num_value);
            break;
        case kDEH_FRAME:
            frames::AlterFrame(num_value);
            break;
        case kDEH_AMMO:
            ammo::AlterAmmo(num_value);
            break;
        case kDEH_WEAPON:
            weapons::AlterWeapon(num_value);
            break;
        case kDEH_PTR:
            frames::AlterPointer(num_value);
            break;
        case kDEH_MISC:
            miscellaneous::AlterMisc(num_value);
            break;

        case kDEH_CHEAT:
            text_strings::AlterCheat(equal_pos);
            break;
        case kDEH_SPRITE: /* ignored */
            break;

        case BEX_CODEPTR:
            frames::AlterBexCodePtr(equal_pos);
            break;
        case BEX_STRINGS:
            ProcessBexString();
            break;

        case BEX_SOUNDS:
            sounds::AlterBexSound(equal_pos);
            break;
        case BEX_MUSIC:
            music::AlterBexMusic(equal_pos);
            break;
        case BEX_SPRITES:
            sprites::AlterBexSprite(equal_pos);
            break;

        default:
            FatalError("Dehacked: Error - Bad active_section value %d\n",
                    active_section);
    }
}

DehackedResult LoadDiff(bool no_header)
{
    // set these to defaults
    doom_ver  = no_header ? 19 : 16;
    patch_fmt = no_header ? 6 : 5;

    line_num = 0;

    bool got_info = false;

    syncing = true;

    while (!pat_buf->EndOfFile())
    {
        GetNextLine();

        if (line_buf[0] == 0 || line_buf[0] == '#') continue;

        // LogPrint("LINE %d: <%s>\n", line_num, line_buf);

        if (epi::StringPrefixCaseCompareASCII(line_buf, "Doom version") == 0)
        {
            if (!equal_pos)
            {
                SetErrorMsg("Badly formed directive !\nLine %d: %s\n", line_num,
                            line_buf);
                return kDehackedConversionParseError;
            }

            doom_ver = (int)strtol(equal_pos + 1, nullptr, 10);

            if (!(doom_ver == 12 || (doom_ver >= 16 && doom_ver <= 21) ||
                  doom_ver == 2021 /* DSDehacked */))
            {
                SetErrorMsg("Unknown doom version found: V%d.%d\n",
                            doom_ver / 10, (doom_ver + 1000) % 10);
                return kDehackedConversionParseError;
            }

            // I don't think the DeHackEd code supports this correctly
            if (doom_ver == 12)
            {
                SetErrorMsg("Text patches for DOOM V1.2 are not supported.\n");
                return kDehackedConversionParseError;
            }
        }

        if (epi::StringPrefixCaseCompareASCII(line_buf, "Patch format") == 0)
        {
            if (got_info)
            {
                // Dasho: Just ignore extra version declarations and continue
                // loading?
                continue;
            }

            got_info = true;

            if (!equal_pos)
            {
                SetErrorMsg("Badly formed directive !\nLine %d: %s\n", line_num,
                            line_buf);
                return kDehackedConversionParseError;
            }

            patch_fmt = (int)strtol(equal_pos + 1, nullptr, 10);

            if (patch_fmt < 5 || patch_fmt > 6)
            {
                SetErrorMsg("Unknown dehacked patch format found: %d\n",
                            patch_fmt);
                return kDehackedConversionParseError;
            }

            VersionMsg();
        }

        if (epi::StringPrefixCaseCompareASCII(line_buf, "include") == 0)
        {
            LogPrint("- Warning: BEX INCLUDE directive not supported!\n");
            continue;
        }

        if (epi::StringPrefixCaseCompareASCII(line_buf, "Text") == 0 &&
            epi::IsSpaceASCII(line_buf[4]))
        {
            int len1, len2;

            if (sscanf(line_buf + 4, " %i %i ", &len1, &len2) == 2 && len1 > 1)
            {
                ProcessTextSection(len1, len2);
                syncing = true;
                continue;
            }
        }

        if (CheckNewSection())
        {
            syncing = false;
            continue;
        }

        if (!syncing) { ProcessLine(); }
    }

    return kDehackedConversionOK;
}

DehackedResult LoadNormal(void)
{
    char idstr[32];

    memset(idstr, 0, sizeof(idstr));

    pat_buf->Read(idstr, 24);

    // Note: the 'P' is checked elsewhere
    if (epi::StringCaseCompareASCII(idstr, "atch File for DeHackEd v") != 0)
    {
        SetErrorMsg("Not a DeHackEd patch file !\n");
        return kDehackedConversionParseError;
    }

    memset(idstr, 0, 4);

    pat_buf->Read(idstr, 3);

    if (!epi::IsDigitASCII(idstr[0]) || idstr[1] != '.' ||
        !epi::IsDigitASCII(idstr[2]))
    {
        SetErrorMsg(
            "Bad version string in DeHackEd patch file.\n"
            "[String %s is not digit . digit]\n",
            idstr);
        return kDehackedConversionParseError;
    }

    dhe_ver = (idstr[0] - '0') * 10 + (idstr[2] - '0');

    if (dhe_ver < 20 || dhe_ver > 31)
    {
        SetErrorMsg(
            "This patch file has an incorrect version number !\n"
            "[Version %s]\n",
            idstr);
        return kDehackedConversionParseError;
    }

    if (dhe_ver < 23) return LoadBinary();

    DetectMsg("text-based");
    return LoadDiff(false);
}
}  // namespace patch

DehackedResult patch::Load(InputBuffer *buf)
{
    pat_buf = buf;
    EPI_ASSERT(pat_buf);

    DehackedResult result = kDehackedConversionOK;

    file_error = false;

    char tempver = pat_buf->GetCharacter();

    if (tempver == 12) { result = LoadReallyOld(); }
    else if (tempver == 'P') { result = LoadNormal(); }
    else if (!pat_buf->IsBinary())
    {
        pat_buf->UngetCharacter(tempver);

        LogPrint("Missing header -- assuming text-based BEX patch !\n");
        dhe_ver = 31;
        result  = LoadDiff(true);
    }
    else /* unknown binary format */
    {
        SetErrorMsg("Not a DeHackEd patch file !\n");
        result = kDehackedConversionParseError;
    }

    LogPrint("\n");
    pat_buf = nullptr;

    return result;
}

}  // namespace dehacked
