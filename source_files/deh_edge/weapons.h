//------------------------------------------------------------------------
//  WEAPON conversion
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __WEAPONS_HDR__
#define __WEAPONS_HDR__

namespace Deh_Edge
{

// Weapon info: sprite frames, ammunition use.
typedef struct
{
	const char *ddf_name;

    int ammo, ammo_per_shot;
	int bind_key, priority;
	
	const char *flags;

    int upstate;
    int downstate;
    int readystate;
    int atkstate;
    int flashstate;
}
weaponinfo_t;

// The defined weapons...
typedef enum
{
    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    wp_supershotgun,
                                                                                            
    NUMWEAPONS
}
weapontype_e;

extern weaponinfo_t weapon_info[NUMWEAPONS];

namespace Weapons
{
	void Startup(void);

	void MarkWeapon(int wp_num);

	void AlterWeapon(int new_val);

	void ConvertWEAP(void);
}

}  // Deh_Edge

#endif /* __WEAPONS_HDR__ */
