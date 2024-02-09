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

#pragma once

#include "deh_mobj.h"

namespace dehacked
{

enum FieldType
{
    kFieldTypeAny,
    kFieldTypeZeroOrGreater,
    kFieldTypeOneOrGreater,
    kFieldTypeFrameNumber,
    kFieldTypeSoundNumber,
    kFieldTypeSpriteNumber,
    kFieldTypeSubspriteNumber,
    kFieldTypeAmmoNumber,
    kFieldTypeBitflags,
};

struct FieldReference
{
    const char *dehacked_name;

    // offset into the structure (like an mobjtype_t).
    size_t offset;

    FieldType field_type;
};

// returns false if name not found
bool FieldAlter(const FieldReference *references, const char *dehacked_field, int *object, int new_value);

} // namespace dehacked