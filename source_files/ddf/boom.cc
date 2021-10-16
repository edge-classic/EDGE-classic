//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (BOOM Compatibility)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#include "local.h"


static linetype_container_c   genlinetypes;    // <-- Generalised
static sectortype_container_c gensectortypes;  // <-- Generalised

//
// DDF_IsBoomLineType
//
bool DDF_IsBoomLineType(int num)
{
	return (num >= 0x2F80 && num <= 0x7FFF);
}

//
// DDF_IsBoomSectorType
//
bool DDF_IsBoomSectorType(int num)
{
	return (num >= 0x20 && num <= 0xFFF);
}

//
// DDF_BoomClearGenTypes
//
void DDF_BoomClearGenTypes(void)
{
	genlinetypes.Reset();
	gensectortypes.Reset();
}

//----------------------------------------------------------------------------

//
// DDF_BoomMakeGenSector
//
// Decodes the BOOM generalised sector number and fills in the DDF
// sector type `sec' (which has already been instantiated with default
// values).
//
// NOTE: This code based on "Section 15" of BOOMREF.TXT.
// 
// -AJA- 2001/06/22: written.
// 
void DDF_BoomMakeGenSector(sectortype_c *sec, int number)
{
//  I_Debugf("- Making Generalized Sector 0x%03x\n", number);

	// handle lower 5 bits: Lighting
	switch (number & 0x1F)
	{
		case 0: // normal
			break;

		case 1: // random off
			sec->l.type = LITE_Flash;
			sec->l.chance = PERCENT_MAKE(10);
			sec->l.darktime = 8;
			sec->l.brighttime = 8;
			break;

		case 2:
		case 4: // blink 0.5 second
			sec->l.type = LITE_Strobe;
			sec->l.darktime = 15;
			sec->l.brighttime = 5;
			break;

		case 3: // blink 1.0 second
			sec->l.type = LITE_Strobe;
			sec->l.darktime = 35;
			sec->l.brighttime = 5;
			break;

		case 8: // oscillates
			sec->l.type = LITE_Glow;
			sec->l.darktime = 1;
			sec->l.brighttime = 1;
			break;

		case 12: // blink 0.5 second, sync
			sec->l.type = LITE_Strobe;
			sec->l.darktime = 15;
			sec->l.brighttime = 5;
			sec->l.sync = 20;
			break;

		case 13: // blink 1.0 second, sync
			sec->l.type = LITE_Strobe;
			sec->l.darktime = 35;
			sec->l.brighttime = 5;
			sec->l.sync = 40;
			break;

		case 17: // flickers
			sec->l.type = LITE_FireFlicker;
			sec->l.darktime = 4;
			sec->l.brighttime = 4;
			break;
	}

	// handle bits 5-6: Damage
	switch ((number >> 5) & 0x3)
	{
		case 0: // no damage
			break;

		case 1: // 5 units
			sec->damage.nominal = 5;
			sec->damage.delay = 32;
			break;

		case 2: // 10 units
			sec->damage.nominal = 10;
			sec->damage.delay = 32;
			break;

		case 3: // 20 units
			sec->damage.nominal = 20;
			sec->damage.delay = 32;
			break;
	}

	// handle bit 7: Secret
	if ((number >> 7) & 1)
		sec->secret = true;

	// ignoring bit 8: Ice/Mud effect

	// ignoring bit 9: Wind effect
}

//
// DDF_BoomGetGenSector
// 
sectortype_c *DDF_BoomGetGenSector(int number)
{
	SYS_ASSERT(DDF_IsBoomSectorType(number));

	sectortype_c *sec = gensectortypes.Lookup(number);
			
	// Create if it doesn't exist
	if (!sec)
	{
		sec = new sectortype_c;
		sec->Default();
		
		DDF_BoomMakeGenSector(sec, number);
		
		gensectortypes.Insert(sec);
	}

	return sec;
}

//----------------------------------------------------------------------------

static void HandleLineTrigger(linetype_c *line, int trigger)
{
	if ((trigger & 0x1) == 0)
		line->count = 1;
	else
		line->count = -1;

	switch (trigger & 0x6)
	{
		case 0:  // W1 and WR
			line->type = line_walkable; 
			break;

		case 2:  // S1 and SR
			line->type = line_pushable; 
			break;

		case 4:  // G1 and GR
			line->type = line_shootable; 
			break;

		case 6:  // P1 and PR
			line->type = line_manual;
			break;
	}
}

static void MakeBoomFloor(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int model   = (number >> 5) & 0x1;
	int dir     = (number >> 6) & 0x1;
	int target  = (number >> 7) & 0x7;
	int change  = (number >> 10) & 0x3;
	int crush   = (number >> 12) & 0x1;

	line->obj = (trigacttype_e)(trig_player | ((change == 0 && model) ? trig_monster : 0));

	line->f.type = mov_Once;
	line->f.dest = 0;

	if (crush)
		line->f.crush_damage = 10;

	switch (target)
	{
		case 0:  // HnF (Highest neighbour Floor)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_HIGHEST);
			break;

		case 1:  // LnF (Lowest neighbour Floor)
			line->f.destref = REF_Surrounding;
			break;

		case 2:  // NnF (Next neighbour Floor)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_NEXT);

			// guesswork here:
			if (dir == 0)
				line->f.destref = (heightref_e)(line->f.destref | REF_HIGHEST);

			break;

		case 3:  // LnC (Lowest neighbour Ceiling)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_CEILING);
			break;

		case 4:  // ceiling
			line->f.destref = (heightref_e)(REF_Current | REF_CEILING);
			break;

		case 5:  // shorted texture
			line->f.destref = REF_LowestLoTexture;
			break;

		case 6:  // 24
			line->f.destref = REF_Current;  // FLOOR
			line->f.dest = dir ? 24 : -24;
			break;

		case 7:  // 32
			line->f.destref = REF_Current;  // FLOOR
			line->f.dest = dir ? 32 : -32;
			break;
	}

	switch (dir)
	{
		case 0:  // Down
			line->f.speed_down = 1 << speed;
			line->f.sfxdown = sfxdefs.GetEffect("STNMOV");
			break;

		case 1:  // Up;
			line->f.speed_up = 1 << speed;
			line->f.sfxup = sfxdefs.GetEffect("STNMOV");
			break;
	}

	// handle change + model (pretty dodgy this bit)
	if (change > 0)
	{
		line->f.tex.Set(model ? "+" : "-");
	}
}

static void MakeBoomCeiling(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int model   = (number >> 5) & 0x1;
	int dir     = (number >> 6) & 0x1;
	int target  = (number >> 7) & 0x7;
	int change  = (number >> 10) & 0x3;
	int crush   = (number >> 12) & 0x1;

	line->obj = (trigacttype_e)(trig_player | ((change == 0 && model) ? trig_monster : 0));

	line->c.type = mov_Once;
	line->c.dest = 0;

	if (crush)
		line->c.crush_damage = 10;

	switch (target)
	{
		case 0:  // HnC (Highest neighbour Ceiling)
			line->c.destref = (heightref_e)(REF_Surrounding | REF_CEILING | REF_HIGHEST);
			break;

		case 1:  // LnC (Lowest neighbour Ceiling)
			line->c.destref = (heightref_e)(REF_Surrounding | REF_CEILING);
			break;

		case 2:  // NnC (Next neighbour Ceiling)
			line->c.destref = (heightref_e)(REF_Surrounding | REF_CEILING | REF_NEXT);

			// guesswork here:
			if (dir == 0)
				line->c.destref = (heightref_e)(line->c.destref | REF_HIGHEST);

			break;

		case 3:  // HnF (Highest neighbour Floor)
			line->c.destref = (heightref_e)(REF_Surrounding | REF_HIGHEST);
			break;

		case 4:  // floor
			line->c.destref = REF_Current;  // FLOOR
			break;

		case 5:  // shorted texture
			line->c.destref = REF_LowestLoTexture;
			break;

		case 6:  // 24
			line->c.destref = (heightref_e)(REF_Current | REF_CEILING);
			line->f.dest = dir ? 24 : -24;
			break;

		case 7:  // 32
			line->c.destref = (heightref_e)(REF_Current | REF_CEILING);
			line->f.dest = dir ? 32 : -32;
			break;
	}

	switch (dir)
	{
		case 0:  // Down
			line->c.speed_down = 1 << speed;
			line->c.sfxdown = sfxdefs.GetEffect("STNMOV");
			break;

		case 1:  // Up;
			line->c.speed_up = 1 << speed;
			line->c.sfxup = sfxdefs.GetEffect("STNMOV");
			break;
	}

	// handle change + model (this logic is pretty dodgy)
	if (change > 0)
	{
		line->c.tex.Set(model ? "+" : "-");
	}
}

static void MakeBoomDoor(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int kind    = (number >> 5) & 0x3;
	int monster = (number >> 7) & 0x1;
	int delay   = (number >> 8) & 0x3;

	line->obj = (trigacttype_e)(trig_player | (monster ? trig_monster : 0));

	line->c.type = (kind & 1) ? mov_Once : mov_MoveWaitReturn;

	line->c.speed_up = 2 << speed;
	line->c.speed_down = line->c.speed_up;
	line->c.sfxup = sfxdefs.GetEffect("DOROPN");
	line->c.sfxdown = sfxdefs.GetEffect("DORCLS");

	switch (kind & 2)
	{
		case 0: // open types (odc and o)
			line->c.destref = (heightref_e)(REF_Surrounding | REF_CEILING);  // LnC
			line->c.dest = -4;
			break;

		case 2: // close types (cdo and c)
			line->c.destref = REF_Current;  // FLOOR
			line->c.dest = 0;
			break;
	}

	switch (delay)
	{
		case 0: line->c.wait = 35;   break;
		case 1: line->c.wait = 150;  break;
		case 2: line->c.wait = 300;  break;
		case 3: line->c.wait = 1050; break;
	}
}

static void MakeBoomLockedDoor(linetype_c *line, int number)
{
	int speed = (number >> 3) & 0x3;
	int kind  = (number >> 5) & 0x1;
	int lock  = (number >> 6) & 0x7;
	int sk_ck = (number >> 9) & 0x1;

	line->obj = trig_player;  // never allow monsters

	line->c.type = kind ? mov_Once : mov_MoveWaitReturn;
	line->c.destref =(heightref_e)(REF_Surrounding | REF_CEILING);  // LnC
	line->c.dest = -4;

	line->c.speed_up = 2 << speed;
	line->c.speed_down = line->c.speed_up;
	line->c.sfxup = sfxdefs.GetEffect("DOROPN");
	line->c.sfxdown = sfxdefs.GetEffect("DORCLS");
	line->c.wait = 150;

	// handle keys

	switch (lock)
	{
		case 0:  // ANY
			line->keys = (keys_e)(KF_RedCard | KF_BlueCard | KF_YellowCard | KF_RedSkull | KF_BlueSkull | KF_YellowSkull);
			line->failedmessage.Set("NeedAnyForDoor");
			break;

		case 1:  // Red Card
			line->keys = (keys_e)(KF_RedCard | (sk_ck ? KF_RedSkull : 0));
			line->failedmessage.Set("NeedRedForDoor");
			break;

		case 2:  // Blue Card
			line->keys = (keys_e)(KF_BlueCard | (sk_ck ? KF_BlueSkull : 0));
			line->failedmessage.Set("NeedBlueForDoor");
			break;

		case 3:  // Yellow Card
			line->keys = (keys_e)(KF_YellowCard | (sk_ck ? KF_YellowSkull : 0));
			line->failedmessage.Set("NeedYellowForDoor");
			break;

		case 4:  // Red Skull
			line->keys = (keys_e)(KF_RedSkull | (sk_ck ? KF_RedCard : 0));
			line->failedmessage.Set("NeedRedForDoor");
			break;

		case 5:  // Blue Skull
			line->keys = (keys_e)(KF_BlueSkull | (sk_ck ? KF_BlueCard : 0));
			line->failedmessage.Set("NeedBlueForDoor");
			break;

		case 6:  // Yellow Skull
			line->keys = (keys_e)(KF_YellowSkull | (sk_ck ? KF_YellowCard : 0));
			line->failedmessage.Set("NeedYellowForDoor");
			break;

		case 7:  // ALL  
			line->keys = (keys_e)((sk_ck ? KF_BOOM_SKCK : 0) | KF_STRICTLY_ALL |
				(KF_RedCard | KF_BlueCard | KF_YellowCard |
				KF_RedSkull | KF_BlueSkull | KF_YellowSkull));

			line->failedmessage.Set("NeedAllForDoor");
			break;
	}
}

static void MakeBoomLift(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int monster = (number >> 5) & 0x1;
	int delay   = (number >> 6) & 0x3;
	int target  = (number >> 8) & 0x3;

	line->obj = (trigacttype_e)(trig_player | (monster ? trig_monster : 0));

	line->f.type = mov_MoveWaitReturn;
	line->f.dest = 0;
	line->f.other = 0;

	line->f.speed_up = 1 << speed;
	line->f.speed_down = line->f.speed_up;
	line->f.sfxstart = sfxdefs.GetEffect("PSTART");
	line->f.sfxstop  = sfxdefs.GetEffect("PSTOP");

	switch (target)
	{
		case 0:  // LnF (Lowest neighbour Floor)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_INCLUDE);
			break;

		case 1:  // NnF (Next lowest neighbour Floor)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_NEXT | REF_HIGHEST);
			break;

		case 2:  // LnC (Lowest neighbour Ceiling)
			line->f.destref = (heightref_e)(REF_Surrounding | REF_CEILING | REF_INCLUDE);
			break;

		case 3:  // Perpetual lift LnF<->HnF
			line->f.type = mov_Continuous;
			line->f.destref  = (heightref_e)(REF_Surrounding | REF_INCLUDE);
			line->f.otherref = (heightref_e)(REF_Surrounding | REF_HIGHEST | REF_INCLUDE);
			break;
	}

	switch (delay)
	{
		case 0: line->f.wait = 35;  break;
		case 1: line->f.wait = 105; break;
		case 2: line->f.wait = 165; break;
		case 3: line->f.wait = 350; break;
	}
}

static void MakeBoomStair(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int monster = (number >> 5) & 0x1;
	int step    = (number >> 6) & 0x3;
	int dir     = (number >> 8) & 0x1;
	int igntxt  = (number >> 9) & 0x1;

	line->obj = (trigacttype_e)(trig_player | (monster ? trig_monster : 0));

	line->f.type = mov_Stairs;

	// generalized repeatable stairs alternate between up and down
	if (number & 1)
	{
		line->newtrignum = number ^ 0x100;
	}

	line->f.destref = REF_Current;  // FLOOR
	line->f.dest = ((dir == 0) ? -1 : 1) * (step ? 8 * step : 4);

	// speed values are 0.25, 0.5, 2.0, 4.0 (never 1.0)
	if (speed >= 2)
		speed++;

	line->f.speed_down = (1 << speed) / 4.0f;
	line->f.speed_up = line->f.speed_down;

	line->f.sfxdown = sfxdefs.GetEffect("STNMOV");
	line->f.sfxup = line->f.sfxdown;

	if (igntxt)
		line->f.ignore_texture = true;
}

static void MakeBoomCrusher(linetype_c *line, int number)
{
	int speed   = (number >> 3) & 0x3;
	int monster = (number >> 5) & 0x1;
	int silent  = (number >> 6) & 0x1;

	line->obj = (trigacttype_e)(trig_player | (monster ? trig_monster : 0));

	line->c.type = mov_Continuous;
	line->c.destref = REF_Current;  // FLOOR
	line->c.dest = 8;

	line->c.speed_up = 1 << speed;
	line->c.speed_down = line->c.speed_up;
	line->c.crush_damage = 10;

	if (! silent)
	{
		line->c.sfxup = sfxdefs.GetEffect("STNMOV");
		line->c.sfxdown = line->c.sfxup;
	}
}


//
// DDF_BoomMakeGenLine
//
// Decodes the BOOM generalised linedef number and fills in the DDF
// linedef type `line' (which has already been instantiated with
// default values).
//
// NOTE: This code based on "Section 13" of BOOMREF.TXT.
//
// -AJA- 2001/06/22: began work on this.
// 
void DDF_BoomMakeGenLine(linetype_c *line, int number)
{
//	I_Debugf("- Making Generalized Linedef 0x%04x\n", number);

	// trigger values are the same for all ranges
	HandleLineTrigger(line, number & 0x7);

	if (number >= 0x6000)
		MakeBoomFloor(line, number);

	else if (number >= 0x4000)
		MakeBoomCeiling(line, number);

	else if (number >= 0x3c00)
		MakeBoomDoor(line, number);

	else if (number >= 0x3800)
		MakeBoomLockedDoor(line, number);

	else if (number >= 0x3400)
		MakeBoomLift(line, number);

	else if (number >= 0x3000)
		MakeBoomStair(line, number);

	else if (number >= 0x2F80)
		MakeBoomCrusher(line, number);
}


linetype_c *DDF_BoomGetGenLine(int number)
{
	SYS_ASSERT(DDF_IsBoomLineType(number));

	linetype_c *line = genlinetypes.Lookup(number);
	
	// If this hasn't be found, create it 
	if (! line)
	{
		line = new linetype_c;
		line->Default();

		line->number = number;

		DDF_BoomMakeGenLine(line, number);

		genlinetypes.Insert(line);
	}

	return line;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
