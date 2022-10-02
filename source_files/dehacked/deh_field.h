//------------------------------------------------------------------------
//  FIELD lookup, validation
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_FIELD_HDR__
#define __DEH_FIELD_HDR__

#include "deh_mobj.h"

namespace Deh_Edge
{

typedef enum
{
	FT_ANY,     // no checking
	FT_NONEG,   // must be >= 0
	FT_GTEQ1,   // must be >= 1

	FT_FRAME,   // frame number
	FT_SOUND,   // sound number
	FT_SPRITE,  // sprite number
	FT_SUBSPR,  // subsprite number
	FT_AMMO,    // ammo number
	FT_BITS     // mobj bitflags
}
fieldtype_e;


typedef struct
{
	const char *deh_name;

	// offset into the structure (like an mobjtype_t).
	size_t offset;

	fieldtype_e field_type;
}
fieldreference_t;


// returns false if name not found
bool Field_Alter(const fieldreference_t *refs, const char *deh_field, int *object, int new_val);


}  // Deh_Edge

#endif /* __DEH_FIELD_HDR__ */
