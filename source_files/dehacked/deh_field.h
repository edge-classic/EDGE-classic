//------------------------------------------------------------------------
//  FIELD lookup, validation
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
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

#ifndef __DEH_FIELD_HDR__
#define __DEH_FIELD_HDR__

#include "deh_mobj.h"

namespace Deh_Edge
{

typedef enum
{
    FT_ANY,   // no checking
    FT_NONEG, // must be >= 0
    FT_GTEQ1, // must be >= 1

    FT_FRAME,  // frame number
    FT_SOUND,  // sound number
    FT_SPRITE, // sprite number
    FT_SUBSPR, // subsprite number
    FT_AMMO,   // ammo number
    FT_BITS,   // mobj bitflags
} fieldtype_e;

typedef struct
{
    const char *deh_name;

    // offset into the structure (like an mobjtype_t).
    size_t offset;

    fieldtype_e field_type;
} fieldreference_t;

// returns false if name not found
bool Field_Alter(const fieldreference_t *refs, const char *deh_field, int *object, int new_val);

} // namespace Deh_Edge

#endif /* __DEH_FIELD_HDR__ */
