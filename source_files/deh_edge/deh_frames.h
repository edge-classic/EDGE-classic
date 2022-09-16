//------------------------------------------------------------------------
//  FRAME Handling
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __FRAMES_HDR__
#define __FRAMES_HDR__

namespace Deh_Edge
{

typedef enum
{
	AF_EXPLODE    = (1 << 0),   // uses A_Explode
	AF_KEENDIE    = (1 << 1),   // uses A_KeenDie
	AF_LOOK       = (1 << 2),   // uses A_Look
	AF_DETONATE   = (1 << 3),   // uses A_Detonate

	AF_SPREAD     = (1 << 6),   // uses A_FatAttack1/2/3
	AF_CHASER     = (1 << 7),   // uses A_Chase
	AF_FALLER     = (1 << 8),   // uses A_Fall
	AF_RAISER     = (1 << 9),   // uses A_ResChase

	AF_FLASH      = (1 << 14),  // weapon will go into flash state
	AF_MAKEDEAD   = (1 << 15),  // action needs an extra MAKEDEAD state
	AF_FACE       = (1 << 16),  // action needs FACE_TARGET state
	AF_SPECIAL    = (1 << 17),  // special action (uses misc1/2)
	AF_UNIMPL     = (1 << 18),  // not yet supported

	AF_WEAPON_ST  = (1 << 20),  // uses a weapon state
	AF_THING_ST   = (1 << 21)   // uses a thing state
}
actflags_e;

namespace Frames
{
	typedef enum
	{
		RANGE = 0, COMBAT = 1, SPARE = 2
	}
	atkmethod_e;

	extern const char *attack_slot[3];
	extern int act_flags;

	void Startup(void);

	void MarkState(int st_num);
	void StateDependencies(void);

	void AlterFrame(int new_val);
	void AlterPointer(int new_val);
	void AlterBexCodePtr(const char * new_action);

	void ResetAll(void); // also resets the slots and flags
	int  BeginGroup(int first, char group);
	void SpreadGroups(void);
	bool CheckSpawnRemove(int first);
	bool CheckWeaponFlash(int first);
	void OutputGroup(int first, char group);

	// debugging stuff
	void DebugRange(const char *kind, const char *entry);
}

}  // Deh_Edge

#endif /* __FRAMES_HDR__ */
