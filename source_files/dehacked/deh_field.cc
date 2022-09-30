//------------------------------------------------------------------------
//  FIELD lookup, validation
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <string>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_info.h"
#include "deh_field.h"
#include "deh_frames.h"
#include "deh_misc.h"
#include "deh_mobj.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_things.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_util.h"
#include "deh_weapons.h"


namespace Deh_Edge
{

bool Field_ValidateValue(const fieldreference_t *ref, int new_val)
{
	if (ref->field_type == FT_ANY || ref->field_type == FT_BITS)
		return true;

	if (new_val < 0 || (new_val == 0 && ref->field_type == FT_GTEQ1))
	{
		PrintWarn("Line %d: bad value '%d' for %s\n",
			Patch::line_num, new_val, ref->deh_name);
		return false;
	}

	if (ref->field_type == FT_NONEG || ref->field_type == FT_GTEQ1)
		return true;

	if (ref->field_type == FT_SUBSPR)  // ignore the bright bit
		new_val &= ~32768;

	int min_obj = 0;
	int max_obj = 0;

	if (Patch::patch_fmt <= 5)
	{
		switch (ref->field_type)
		{
			case FT_AMMO:   max_obj = NUMAMMO    - 1; break;
			case FT_FRAME:  max_obj = NUMSTATES  - 1; break;
			case FT_SOUND:  max_obj = NUMSFX     - 1; break;
			case FT_SPRITE: max_obj = NUMSPRITES - 1; break;
			case FT_SUBSPR: max_obj = 31; break;

			default:
				InternalError("Bad field type %d\n", ref->field_type);
		}
	}
	else  /* patch_fmt == 6, allow BOOM/MBF stuff */
	{
		switch (ref->field_type)
		{
			case FT_AMMO:   max_obj = NUMAMMO - 1; break;
			case FT_SUBSPR: max_obj = 31; break;

			case FT_FRAME:  max_obj = NUMSTATES_BEX - 1; break;

			// for DSDehacked, allow very high values
			case FT_SPRITE: max_obj = 32767; break;
			case FT_SOUND:  max_obj = 32767; break;

			default:
				InternalError("Bad field type %d\n", ref->field_type);
		}
	}

	if (new_val < min_obj || new_val > max_obj)
	{
		PrintWarn("Line %d: bad value '%d' for %s\n",
			Patch::line_num, new_val, ref->deh_name);

		return false;
	}

	return true;
}


bool Field_Alter(const fieldreference_t *refs, const char *deh_field, int *object, int new_val)
{
	for (; refs->deh_name ; refs++)
	{
		if (StrCaseCmp(refs->deh_name, deh_field) != 0)
			continue;

		// found it...

		if (Field_ValidateValue(refs, new_val))
		{
			// prevent BOOM/MBF specific flags from being set using
			// numeric notation.  Only settable via AA+BB+CC notation.
			if (refs->field_type == FT_BITS)
				new_val &= ~ ALL_BEX_FLAGS;

			// Yup, we play a bit dirty here
			object[refs->offset] = new_val;
		}

		return true;
	}

	return false;
}

}  // Deh_Edge
