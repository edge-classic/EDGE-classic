//------------------------------------------------------------------------
//  FIELD lookup, validation
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

#include "deh_field.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include <string>

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_mobj.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_weapons.h"
#include "epi.h"
#include "str_compare.h"
namespace dehacked
{

static bool FieldValidateValue(const FieldReference *reference, int new_val)
{
    if (reference->field_type == kFieldTypeAny || reference->field_type == kFieldTypeBitflags)
        return true;

    if (new_val < 0 || (new_val == 0 && reference->field_type == kFieldTypeOneOrGreater))
    {
        LogDebug("Dehacked: Warning - Line %d: bad value '%d' for %s\n", patch::line_num, new_val,
                 reference->dehacked_name);
        return false;
    }

    if (reference->field_type == kFieldTypeZeroOrGreater || reference->field_type == kFieldTypeOneOrGreater)
        return true;

    if (reference->field_type == kFieldTypeSubspriteNumber) // ignore the bright bit
        new_val &= ~32768;

    int min_obj = 0;
    int max_obj = 0;

    if (patch::patch_fmt <= 5)
    {
        switch (reference->field_type)
        {
        case kFieldTypeAmmoNumber:
            max_obj = kTotalAmmoTypes - 1;
            break;
        case kFieldTypeFrameNumber:
            max_obj = kTotalStates - 1;
            break;
        case kFieldTypeSoundNumber:
            max_obj = kTotalSoundEffects - 1;
            break;
        case kFieldTypeSpriteNumber:
            max_obj = kTotalSprites - 1;
            break;
        case kFieldTypeSubspriteNumber:
            max_obj = 31;
            break;

        default:
            FatalError("Dehacked: Error - Bad field type %d\n", reference->field_type);
        }
    }
    else /* patch_fmt == 6, allow BOOM/MBF stuff */
    {
        switch (reference->field_type)
        {
        case kFieldTypeAmmoNumber:
            max_obj = kTotalAmmoTypes - 1;
            break;
        case kFieldTypeSubspriteNumber:
            max_obj = 31;
            break;

        // for DSDehacked, allow very high values
        case kFieldTypeFrameNumber:
            max_obj = 32767;
            break;
        case kFieldTypeSpriteNumber:
            max_obj = 32767;
            break;
        case kFieldTypeSoundNumber:
            max_obj = 32767;
            break;

        default:
            FatalError("Dehacked: Error - Bad field type %d\n", reference->field_type);
        }
    }

    if (new_val < min_obj || new_val > max_obj)
    {
        LogDebug("Dehacked: Warning - Line %d: bad value '%d' for %s\n", patch::line_num, new_val,
                 reference->dehacked_name);

        return false;
    }

    return true;
}

bool FieldAlter(const FieldReference *references, const char *dehacked_field, int *object, int new_value)
{
    for (; references->dehacked_name; references++)
    {
        if (epi::StringCaseCompareASCII(references->dehacked_name, dehacked_field) != 0)
            continue;

        // found it...

        if (FieldValidateValue(references, new_value))
        {
            // prevent BOOM/MBF specific flags from being set using
            // numeric notation.  Only settable via AA+BB+CC notation.
            if (references->field_type == kFieldTypeBitflags)
                new_value &= ~ALL_BEX_FLAGS;

            // Yup, we play a bit dirty here
            int *field = (int *)((char *)object + references->offset);
            *field     = new_value;
        }

        return true;
    }

    return false;
}

} // namespace dehacked
