//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2022  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __DDF_WEAPON_H__
#define __DDF_WEAPON_H__

#include "epi.h"
#include "arrays.h"

#include "types.h"
#include "states.h"


// ------------------------------------------------------------------
// -----------------------WEAPON HANDLING----------------------------
// ------------------------------------------------------------------

#define WEAPON_KEYS 10

// -AJA- 2000/01/12: Weapon special flags
typedef enum
{
	WPSP_None = 0,

	WPSP_SilentToMon = (1 << 0), // monsters cannot hear this weapon
	WPSP_Animated    = (1 << 1), // raise/lower states are animated

	WPSP_SwitchAway  = (1 << 4), // select new weapon when we run out of ammo

	// reload flags:
	WPSP_Trigger = (1 << 8), // allow reload while holding trigger
	WPSP_Fresh   = (1 << 9), // automatically reload when new ammo is avail
	WPSP_Manual  = (1 << 10), // enables the manual reload key
	WPSP_Partial = (1 << 11), // manual reload: allow partial refill

	// MBF21 flags:
	WPSP_NoAutoFire = (1 << 12), // Do not fire if switched to while trigger is held
}
weapon_flag_e;

#define DEFAULT_WPSP  (weapon_flag_e)(WPSP_Trigger | WPSP_Manual | WPSP_SwitchAway | WPSP_Partial)

class weapondef_c
{
public:
	weapondef_c();
	~weapondef_c();
	
public:
	void Default(void);
	void CopyDetail(weapondef_c &src);

	// Weapon's name, etc...
	std::string name;

	atkdef_c *attack[2];	// Attack type used.
  
	ammotype_e ammo[2];		// Type of ammo this weapon uses.
	int ammopershot[2];		// Ammo used per shot.
	int clip_size[2];		// Amount of shots in a clip (if <= 1, non-clip weapon)
	bool autofire[2];		// If true, this is an automatic else it's semiauto 

	float kick;				// Amount of kick this weapon gives
  
	// range of states used
	state_group_t state_grp;
  
	int up_state;			// State to use when raising the weapon 
	int down_state;			// State to use when lowering the weapon (if changing weapon)
	int ready_state;		// State that the weapon is ready to fire in
	int empty_state;        // State when weapon is empty.  Usually zero
	int idle_state;			// State to use when polishing weapon

	int attack_state[2];	// State showing the weapon 'firing'
	int reload_state[2];	// State showing the weapon being reloaded
	int discard_state[2];	// State showing the weapon discarding a clip
	int warmup_state[2];	// State showing the weapon warming up
	int flash_state[2];		// State showing the muzzle flash

	int crosshair;			// Crosshair states
	int zoom_state;			// State showing viewfinder when zoomed.  Can be zero

	bool no_cheat;          // Not given for cheats (Note: set by #CLEARALL)

	bool autogive;			// The player gets this weapon on spawn.  (Fist + Pistol)
	bool feedback;			// This weapon gives feedback on hit (chainsaw)

	weapondef_c *upgrade_weap; // This weapon upgrades a previous one.
 
	// This affects how it will be selected if out of ammo.  Also
	// determines the cycling order when on the same key.  Dangerous
	// weapons are not auto-selected when out of ammo.
	int priority;
	bool dangerous;
 
  	// Attack type for the WEAPON_EJECT code pointer.

	atkdef_c *eject_attack;
  
	// Sounds.
	// Played at the start of every readystate
	struct sfx_s *idle;
  
  	// Played while the trigger is held (chainsaw)
	struct sfx_s *engaged;
  
	// Played while the trigger is held and it is pointed at a target.
	struct sfx_s *hit;
  
	// Played when the weapon is selected
	struct sfx_s *start;
  
	// Misc sounds
	struct sfx_s *sound1;
	struct sfx_s *sound2;
	struct sfx_s *sound3;
  
	// This close combat weapon should not push the target away (chainsaw)
	bool nothrust;
  
	// which number key this weapon is bound to, or -1 for none
	int bind_key;
  
	// -AJA- 2000/01/12: weapon special flags
	weapon_flag_e specials[2];

	// -AJA- 2000/03/18: when > 0, this weapon can zoom
	int zoom_fov;

	// Dasho - When > 0, this weapon can zoom and will use this value instead of zoom_fov
	float zoom_factor;

	// -AJA- 2000/05/23: weapon loses accuracy when refired.
	bool refire_inacc;

	// -AJA- 2000/10/20: show current clip in status bar (not total)
	bool show_clip;

	// -AJA- 2007/11/12: clip is shared between 1st/2nd attacks.
	bool shared_clip;

	// controls for weapon bob (up & down) and sway (left & right).
	// Given as percentages in DDF.
	percent_t bobbing;
	percent_t swaying;

	// -AJA- 2004/11/15: idle states (polish weapon, crack knuckles)
	int idle_wait;
	percent_t idle_chance;

	int model_skin;  // -AJA- 2007/10/16: MD2 model support
	float model_aspect;
	float model_bias;
	float model_forward;
	float model_side;
	
	//Lobo 2022: render order is Crosshair, Flash, Weapon
	// instead of Weapon, Flash, CrossHair
	bool render_invert;
	
	//Lobo 2022: sprite Y offset, mainly for Heretic weapons
	float y_adjust;

public:
	inline int KeyPri(int idx) const  // next/prev order value
	{
		int key = 1 + MAX(-1, MIN(10, bind_key));
		int pri = 1 + MAX(-1, MIN(900, priority));

		return (pri * 20 + key) * 100 + idx;
	}

private:
	// disable copy construct and assignment operator
	explicit weapondef_c(weapondef_c &rhs) { }
	weapondef_c& operator= (weapondef_c &rhs) { return *this; }
};


class weapondef_container_c : public epi::array_c
{
public:
	weapondef_container_c();
	~weapondef_container_c();

private:
	void CleanupObject(void *obj);

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(weapondef_c *w) { return InsertObject((void*)&w); }
	
	weapondef_c* operator[](int idx) 
	{ 
		return *(weapondef_c**)FetchObject(idx); 
	} 

	// Search Functions
	int FindFirst(const char *name, int startpos = -1);
	weapondef_c* Lookup(const char* refname);
};


// -------EXTERNALISATIONS-------

extern weapondef_container_c weapondefs;	// -ACB- 2004/07/14 Implemented

void DDF_ReadWeapons(const std::string& data);

#endif // __DDF_WEAPON_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
