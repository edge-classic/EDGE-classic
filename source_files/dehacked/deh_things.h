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

#ifndef __DEH_THINGS_HDR__
#define __DEH_THINGS_HDR__

#include "deh_mobj.h"

namespace Deh_Edge
{

namespace Things
{
	void Init();
	void Shutdown();

	void UseThing(int mt_num);
	void MarkThing(int mt_num);  // attacks too
	void MarkAllMonsters();

	mobjinfo_t *GetModifiedMobj(int mt_num);
	const char *GetMobjName(int mt_num);
	int         GetMobjMBF21Flags(int mt_num);

	bool IsSpawnable(int mt_num);

	void SetPlayerHealth(int new_value);
	const char *AddScratchAttack(int damage, const char *sfx);

	void ConvertTHING();
	void ConvertATK();

	void HandleFlags(const mobjinfo_t *info, int mt_num, int player);
	void HandleMBF21Flags(const mobjinfo_t *info, int mt_num, int player);
	void HandleAttacks(const mobjinfo_t *info, int mt_num);

	const char *GetSpeed(int speed);

	void AlterThing(int new_val);
	void AlterBexBits(char *bit_str);
	void AlterMBF21Bits(char *bit_str);
}

}  // Deh_Edge

#endif /* __DEH_THINGS_HDR__ */
