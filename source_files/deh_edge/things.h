//------------------------------------------------------------------------
//  THING conversion
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __THINGS_HDR__
#define __THINGS_HDR__

#include "mobj.h"

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

	// pointer to the field in the very FIRST entry (e.g. mobjinfo[0]).
	// From that we can compute the pointer for an arbitrary entry.
	int *var;

	int field_type;
}
fieldreference_t;


namespace Things
{
	void Startup(void);

	void MarkThing(int mt_num);  // attacks too

	void FixHeights(void);
	void ConvertTHING(void);

	void HandleFlags(const mobjinfo_t *info, int mt_num, int player);
	void HandleAttacks(const mobjinfo_t *info, int mt_num);

	const char *GetSpeed(int speed);
	bool CheckIsMonster(const mobjinfo_t *info, int mt_num, int player,
		bool use_act_flags);

	// returns false if name not found
	bool AlterOneField(const fieldreference_t *refs, const char *deh_field,
		int entry_offset, int new_val);

	void AlterThing(int new_val);
	void AlterBexBits(char *bit_str);
}

}  // Deh_Edge

#endif /* __THINGS_HDR__ */
