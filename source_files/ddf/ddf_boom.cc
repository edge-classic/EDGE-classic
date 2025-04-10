//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (BOOM Compatibility)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "ddf_local.h"

static LineTypeContainer   genlinetypes;   // <-- Generalised
static SectorTypeContainer gensectortypes; // <-- Generalised

//
// DDFIsBoomLineType
//
bool DDFIsBoomLineType(int num)
{
    return (num >= 0x2F80 && num <= 0x7FFF);
}

//
// DDFIsBoomSectorType
//
bool DDFIsBoomSectorType(int num)
{
    return (num >= 0x20 && num <= 0xFFFF); // Might as well extend to 16 bits to allow for
                                           // more MBF21-type expansions
}

//
// DDFBoomClearGeneralizedTypes
//
void DDFBoomClearGeneralizedTypes(void)
{
    genlinetypes.Reset();
    gensectortypes.Reset();
}

//----------------------------------------------------------------------------

//
// DDFBoomMakeGeneralizedSector
//
// Decodes the BOOM generalised sector number and fills in the DDF
// sector type `sec' (which has already been instantiated with default
// values).
//
// NOTE: This code based on "Section 15" of BOOMREF.TXT.
//
// -AJA- 2001/06/22: written.
//
void DDFBoomMakeGeneralizedSector(SectorType *sec, int number)
{
    //  LogDebug("- Making Generalized Sector 0x%03x\n", number);

    // handle lower 5 bits: Lighting
    switch (number & 0x1F)
    {
    case 0: // normal
        break;

    case 1: // random off
        sec->l_.type_       = kLightSpecialTypeFlash;
        sec->l_.chance_     = 0.1f;
        sec->l_.darktime_   = 8;
        sec->l_.brighttime_ = 8;
        break;

    case 2:
    case 4: // blink 0.5 second
        sec->l_.type_       = kLightSpecialTypeStrobe;
        sec->l_.darktime_   = 15;
        sec->l_.brighttime_ = 5;
        break;

    case 3: // blink 1.0 second
        sec->l_.type_       = kLightSpecialTypeStrobe;
        sec->l_.darktime_   = 35;
        sec->l_.brighttime_ = 5;
        break;

    case 8: // oscillates
        sec->l_.type_       = kLightSpecialTypeGlow;
        sec->l_.darktime_   = 1;
        sec->l_.brighttime_ = 1;
        break;

    case 12: // blink 0.5 second, sync
        sec->l_.type_       = kLightSpecialTypeStrobe;
        sec->l_.darktime_   = 15;
        sec->l_.brighttime_ = 5;
        sec->l_.sync_       = 20;
        break;

    case 13: // blink 1.0 second, sync
        sec->l_.type_       = kLightSpecialTypeStrobe;
        sec->l_.darktime_   = 35;
        sec->l_.brighttime_ = 5;
        sec->l_.sync_       = 40;
        break;

    case 17: // flickers
        sec->l_.type_       = kLightSpecialTypeFireFlicker;
        sec->l_.darktime_   = 4;
        sec->l_.brighttime_ = 4;
        break;
    }

    // handle bits 5-6: Damage
    switch ((number >> 5) & 0x3)
    {
    case 0: // no damage
        break;

    case 1: // 5 units
        sec->damage_.nominal_ = 5;
        sec->damage_.delay_   = 32;
        break;

    case 2: // 10 units
        sec->damage_.nominal_ = 10;
        sec->damage_.delay_   = 32;
        break;

    case 3: // 20 units
        sec->damage_.nominal_ = 20;
        sec->damage_.delay_   = 32;
        break;
    }

    // handle bit 7: Secret
    if ((number >> 7) & 1)
        sec->secret_ = true;

    // ignoring bit 8: Ice/Mud effect

    // ignoring bit 9: Wind effect

    // ignoring bit 10: Suppress all sounds in sector - This is alluded to in
    // boomref, not sure if ever implemented - Dasho

    // ignoring bit 11: Suppress all floor/ceiling movement sounds in sector -
    // Same as above

    // handle bit 12: Alternate damage mode (MBF21)
    if ((number >> 12) & 1)
    {
        sec->damage_.only_affects_ |= epi::BitSetFromChar('P');
        switch ((number >> 5) & 0x3)
        {
        case 0: // Kill player if no radsuit or invul status
            sec->damage_.delay_                         = 0;
            sec->damage_.instakill_                     = true;
            sec->damage_.damage_unless_                 = new Benefit;
            sec->damage_.damage_unless_->type           = kBenefitTypePowerup;
            sec->damage_.damage_unless_->sub.type       = kPowerTypeAcidSuit;
            sec->damage_.damage_unless_->next           = new Benefit;
            sec->damage_.damage_unless_->next->type     = kBenefitTypePowerup;
            sec->damage_.damage_unless_->next->sub.type = kPowerTypeInvulnerable;
            sec->damage_.damage_unless_->next->next     = nullptr;
            break;

        case 1: // Kill player
            sec->damage_.delay_      = 0;
            sec->damage_.bypass_all_ = true;
            sec->damage_.instakill_  = true;
            break;

        case 2: // Kill all players and exit map (normal)
            sec->damage_.delay_       = 0;
            sec->damage_.all_players_ = true;
            sec->damage_.instakill_   = true;
            sec->damage_.bypass_all_  = true;
            sec->e_exit_              = kExitTypeNormal;
            break;

        case 3: // Kill all players and exit map (secret)
            sec->damage_.delay_       = 0;
            sec->damage_.all_players_ = true;
            sec->damage_.instakill_   = true;
            sec->damage_.bypass_all_  = true;
            sec->e_exit_              = kExitTypeSecret;
            break;
        }
    }

    // handle bit 13: Kill grounded monsters (MBF21)
    if ((number >> 13) & 1)
    {
        sec->damage_.delay_             = 0;
        sec->damage_.instakill_         = true;
        sec->damage_.only_affects_     |= epi::BitSetFromChar('M');
    }
}

//
// DDFBoomGetGeneralizedSector
//
SectorType *DDFBoomGetGeneralizedSector(int number)
{
    EPI_ASSERT(DDFIsBoomSectorType(number));

    SectorType *sec = gensectortypes.Lookup(number);

    // Create if it doesn't exist
    if (!sec)
    {
        sec = new SectorType;
        sec->Default();

        sec->number_ = number;

        DDFBoomMakeGeneralizedSector(sec, number);

        gensectortypes.push_back(sec);
    }

    return sec;
}

//----------------------------------------------------------------------------

static void HandleLineTrigger(LineType *line, int trigger)
{
    if ((trigger & 0x1) == 0)
        line->count_ = 1;
    else
        line->count_ = -1;

    switch (trigger & 0x6)
    {
    case 0: // W1 and WR
        line->type_ = kLineTriggerWalkable;
        break;

    case 2: // S1 and SR
        line->type_ = kLineTriggerPushable;
        break;

    case 4: // G1 and GR
        line->type_ = kLineTriggerShootable;
        break;

    case 6: // P1 and PR
        line->type_ = kLineTriggerManual;
        break;
    }
}

static void MakeBoomFloor(LineType *line, int number)
{
    int speed  = (number >> 3) & 0x3;
    int model  = (number >> 5) & 0x1;
    int dir    = (number >> 6) & 0x1;
    int target = (number >> 7) & 0x7;
    int change = (number >> 10) & 0x3;
    int crush  = (number >> 12) & 0x1;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | ((change == 0 && model) ? kTriggerActivatorMonster : 0));

    line->f_.type_ = kPlaneMoverOnce;
    line->f_.dest_ = 0;

    if (crush)
        line->f_.crush_damage_ = 10;

    switch (target)
    {
    case 0: // HnF (Highest neighbour Floor)
        line->f_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceHighest);
        break;

    case 1: // LnF (Lowest neighbour Floor)
        line->f_.destref_ = kTriggerHeightReferenceSurrounding;
        break;

    case 2: // NnF (Next neighbour Floor)
        line->f_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceNext);

        // guesswork here:
        if (dir == 0)
            line->f_.destref_ = (TriggerHeightReference)(line->f_.destref_ | kTriggerHeightReferenceHighest);

        break;

    case 3: // LnC (Lowest neighbour Ceiling)
        line->f_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceCeiling);
        break;

    case 4: // ceiling
        line->f_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceCurrent | kTriggerHeightReferenceCeiling);
        break;

    case 5: // shorted texture
        line->f_.destref_ = kTriggerHeightReferenceLowestLowTexture;
        break;

    case 6:                                                 // 24
        line->f_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
        line->f_.dest_    = dir ? 24 : -24;
        break;

    case 7:                                                 // 32
        line->f_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
        line->f_.dest_    = dir ? 32 : -32;
        break;
    }

    switch (dir)
    {
    case 0: // Down
        line->f_.speed_down_ = 1 << speed;
        line->f_.sfxdown_    = sfxdefs.GetEffect("STNMOV");
        break;

    case 1: // Up;
        line->f_.speed_up_ = 1 << speed;
        line->f_.sfxup_    = sfxdefs.GetEffect("STNMOV");
        break;
    }

    // handle change + model (pretty dodgy this bit)
    if (change > 0)
    {
        line->f_.tex_ = model ? "-" : "+";

        // Default behavior is to change both tex and type, and this is
        // fine with the non-generalized types, so append one of these
        // if applicable. We will check this when setting up the map - Dasho

        if (change == 1)      // Change tex, zero out type
            line->f_.tex_.append("changezero");
        else if (change == 2) // Texture only; type unaltered
            line->f_.tex_.append("changetexonly");
    }
}

static void MakeBoomCeiling(LineType *line, int number)
{
    int speed  = (number >> 3) & 0x3;
    int model  = (number >> 5) & 0x1;
    int dir    = (number >> 6) & 0x1;
    int target = (number >> 7) & 0x7;
    int change = (number >> 10) & 0x3;
    int crush  = (number >> 12) & 0x1;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | ((change == 0 && model) ? kTriggerActivatorMonster : 0));

    line->c_.type_ = kPlaneMoverOnce;
    line->c_.dest_ = 0;

    if (crush)
        line->c_.crush_damage_ = 10;

    switch (target)
    {
    case 0: // HnC (Highest neighbour Ceiling)
        line->c_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding |
                                                     kTriggerHeightReferenceCeiling | kTriggerHeightReferenceHighest);
        break;

    case 1: // LnC (Lowest neighbour Ceiling)
        line->c_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceCeiling);
        break;

    case 2: // NnC (Next neighbour Ceiling)
        line->c_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding |
                                                     kTriggerHeightReferenceCeiling | kTriggerHeightReferenceNext);

        // guesswork here:
        if (dir == 0)
            line->c_.destref_ = (TriggerHeightReference)(line->c_.destref_ | kTriggerHeightReferenceHighest);

        break;

    case 3: // HnF (Highest neighbour Floor)
        line->c_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceHighest);
        break;

    case 4:                                                 // floor
        line->c_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
        break;

    case 5:                                                 // shorted texture
        line->c_.destref_ = kTriggerHeightReferenceLowestLowTexture;
        break;

    case 6: // 24
        line->c_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceCurrent | kTriggerHeightReferenceCeiling);
        line->c_.dest_    = dir ? 24 : -24;
        break;

    case 7: // 32
        line->c_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceCurrent | kTriggerHeightReferenceCeiling);
        line->c_.dest_    = dir ? 32 : -32;
        break;
    }

    switch (dir)
    {
    case 0: // Down
        line->c_.speed_down_ = 1 << speed;
        line->c_.sfxdown_    = sfxdefs.GetEffect("STNMOV");
        break;

    case 1: // Up;
        line->c_.speed_up_ = 1 << speed;
        line->c_.sfxup_    = sfxdefs.GetEffect("STNMOV");
        break;
    }

    // handle change + model (this logic is pretty dodgy)
    if (change > 0)
    {
        line->c_.tex_ = model ? "-" : "+";

        // Default behavior is to change both tex and type, and this is
        // fine with the non-generalized types, so append one of these
        // if applicable. We will check this when setting up the map - Dasho

        if (change == 1)      // Change tex, zero out type
            line->c_.tex_.append("changezero");
        else if (change == 2) // Texture only; type unaltered
            line->c_.tex_.append("changetexonly");
    }
}

static void MakeBoomDoor(LineType *line, int number)
{
    int speed   = (number >> 3) & 0x3;
    int kind    = (number >> 5) & 0x3;
    int monster = (number >> 7) & 0x1;
    int delay   = (number >> 8) & 0x3;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | (monster ? kTriggerActivatorMonster : 0));

    line->c_.type_ = (kind & 1) ? kPlaneMoverOnce : kPlaneMoverMoveWaitReturn;

    line->c_.speed_up_   = 2 << speed;
    line->c_.speed_down_ = line->c_.speed_up_;

    if (line->c_.speed_up_ > 7)
    {
        line->c_.sfxup_   = sfxdefs.GetEffect("BDOPN");
        line->c_.sfxdown_ = sfxdefs.GetEffect("BDCLS");
    }
    else
    {
        line->c_.sfxup_   = sfxdefs.GetEffect("DOROPN");
        line->c_.sfxdown_ = sfxdefs.GetEffect("DORCLS");
    }

    switch (kind & 2)
    {
    case 0: // open types (odc and o)
        line->c_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceCeiling); // LnC
        line->c_.dest_ = -4;
        break;

    case 2:                                                 // close types (cdo and c)
        line->c_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
        line->c_.dest_    = 0;
        break;
    }

    switch (delay)
    {
    case 0:
        line->c_.wait_ = 35;
        break;
    case 1:
        line->c_.wait_ = 150;
        break;
    case 2:
        line->c_.wait_ = 300;
        break;
    case 3:
        line->c_.wait_ = 1050;
        break;
    }
}

static void MakeBoomLockedDoor(LineType *line, int number)
{
    int speed = (number >> 3) & 0x3;
    int kind  = (number >> 5) & 0x1;
    int lock  = (number >> 6) & 0x7;
    int sk_ck = (number >> 9) & 0x1;

    line->obj_ = kTriggerActivatorPlayer; // never allow monsters

    line->c_.type_ = kind ? kPlaneMoverOnce : kPlaneMoverMoveWaitReturn;
    line->c_.destref_ =
        (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceCeiling); // LnC
    line->c_.dest_ = -4;

    line->c_.speed_up_   = 2 << speed;
    line->c_.speed_down_ = line->c_.speed_up_;

    if (line->c_.speed_up_ > 7)
    {
        line->c_.sfxup_   = sfxdefs.GetEffect("BDOPN");
        line->c_.sfxdown_ = sfxdefs.GetEffect("BDCLS");
    }
    else
    {
        line->c_.sfxup_   = sfxdefs.GetEffect("DOROPN");
        line->c_.sfxdown_ = sfxdefs.GetEffect("DORCLS");
    }

    line->c_.wait_ = 150;

    // handle keys

    switch (lock)
    {
    case 0: // ANY
        line->keys_ = (DoorKeyType)(kDoorKeyRedCard | kDoorKeyBlueCard | kDoorKeyYellowCard | kDoorKeyRedSkull |
                                    kDoorKeyBlueSkull | kDoorKeyYellowSkull);
        line->failedmessage_ = "NeedAnyForDoor";
        break;

    case 1: // Red Card
        line->keys_          = (DoorKeyType)(kDoorKeyRedCard | (sk_ck ? kDoorKeyRedSkull : 0));
        line->failedmessage_ = "NeedRedCardForDoor";
        break;

    case 2: // Blue Card
        line->keys_          = (DoorKeyType)(kDoorKeyBlueCard | (sk_ck ? kDoorKeyBlueSkull : 0));
        line->failedmessage_ = "NeedBlueCardForDoor";
        break;

    case 3: // Yellow Card
        line->keys_          = (DoorKeyType)(kDoorKeyYellowCard | (sk_ck ? kDoorKeyYellowSkull : 0));
        line->failedmessage_ = "NeedYellowCardForDoor";
        break;

    case 4: // Red Skull
        line->keys_          = (DoorKeyType)(kDoorKeyRedSkull | (sk_ck ? kDoorKeyRedCard : 0));
        line->failedmessage_ = "NeedRedSkullForDoor";
        break;

    case 5: // Blue Skull
        line->keys_          = (DoorKeyType)(kDoorKeyBlueSkull | (sk_ck ? kDoorKeyBlueCard : 0));
        line->failedmessage_ = "NeedBlueSkullForDoor";
        break;

    case 6: // Yellow Skull
        line->keys_          = (DoorKeyType)(kDoorKeyYellowSkull | (sk_ck ? kDoorKeyYellowCard : 0));
        line->failedmessage_ = "NeedYellowSkullForDoor";
        break;

    case 7: // ALL
        line->keys_ = (DoorKeyType)((sk_ck ? kDoorKeyCardOrSkull : 0) | kDoorKeyStrictlyAllKeys |
                                    (kDoorKeyRedCard | kDoorKeyBlueCard | kDoorKeyYellowCard | kDoorKeyRedSkull |
                                     kDoorKeyBlueSkull | kDoorKeyYellowSkull));

        line->failedmessage_ = sk_ck ? "NeedAll3ForDoor" : "NeedAll6ForDoor";
        break;
    }
}

static void MakeBoomLift(LineType *line, int number)
{
    int speed   = (number >> 3) & 0x3;
    int monster = (number >> 5) & 0x1;
    int delay   = (number >> 6) & 0x3;
    int target  = (number >> 8) & 0x3;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | (monster ? kTriggerActivatorMonster : 0));

    line->f_.type_  = kPlaneMoverMoveWaitReturn;
    line->f_.dest_  = 0;
    line->f_.other_ = 0;

    line->f_.speed_up_   = 2 << speed;
    line->f_.speed_down_ = line->f_.speed_up_;
    line->f_.sfxstart_   = sfxdefs.GetEffect("PSTART");
    line->f_.sfxstop_    = sfxdefs.GetEffect("PSTOP");

    switch (target)
    {
    case 0: // LnF (Lowest neighbour Floor)
        line->f_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceInclude);
        break;

    case 1: // NnF (Next lowest neighbour Floor)
        line->f_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceNext |
                                                     kTriggerHeightReferenceHighest);
        break;

    case 2: // LnC (Lowest neighbour Ceiling)
        line->f_.destref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding |
                                                     kTriggerHeightReferenceCeiling | kTriggerHeightReferenceInclude);
        break;

    case 3: // Perpetual lift LnF<->HnF
        line->f_.type_ = kPlaneMoverContinuous;
        line->f_.destref_ =
            (TriggerHeightReference)(kTriggerHeightReferenceSurrounding | kTriggerHeightReferenceInclude);
        line->f_.otherref_ = (TriggerHeightReference)(kTriggerHeightReferenceSurrounding |
                                                      kTriggerHeightReferenceHighest | kTriggerHeightReferenceInclude);
        break;
    }

    switch (delay)
    {
    case 0:
        line->f_.wait_ = 35;
        break;
    case 1:
        line->f_.wait_ = 105;
        break;
    case 2:
        line->f_.wait_ = 165;
        break;
    case 3:
        line->f_.wait_ = 350;
        break;
    }
}

static void MakeBoomStair(LineType *line, int number)
{
    int speed   = (number >> 3) & 0x3;
    int monster = (number >> 5) & 0x1;
    int step    = (number >> 6) & 0x3;
    int dir     = (number >> 8) & 0x1;
    int igntxt  = (number >> 9) & 0x1;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | (monster ? kTriggerActivatorMonster : 0));

    line->f_.type_ = kPlaneMoverStairs;

    // generalized repeatable stairs alternate between up and down
    if (number & 1)
    {
        line->newtrignum_ = number ^ 0x100;
    }

    line->f_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
    line->f_.dest_    = ((dir == 0) ? -1 : 1) * (step ? 8 * step : 4);

    // speed values are 0.25, 0.5, 2.0, 4.0 (never 1.0)
    if (speed >= 2)
        speed++;

    line->f_.speed_down_ = (1 << speed) / 4.0f;
    line->f_.speed_up_   = line->f_.speed_down_;

    line->f_.sfxdown_ = sfxdefs.GetEffect("STNMOV");
    line->f_.sfxup_   = line->f_.sfxdown_;

    if (igntxt)
        line->f_.ignore_texture_ = true;
}

static void MakeBoomCrusher(LineType *line, int number)
{
    int speed   = (number >> 3) & 0x3;
    int monster = (number >> 5) & 0x1;
    int silent  = (number >> 6) & 0x1;

    line->obj_ = (TriggerActivator)(kTriggerActivatorPlayer | (monster ? kTriggerActivatorMonster : 0));

    line->c_.type_    = kPlaneMoverContinuous;
    line->c_.destref_ = kTriggerHeightReferenceCurrent; // FLOOR
    line->c_.dest_    = 8;

    line->c_.speed_up_     = 1 << speed;
    line->c_.speed_down_   = line->c_.speed_up_;
    line->c_.crush_damage_ = 10;

    if (!silent)
    {
        line->c_.sfxup_   = sfxdefs.GetEffect("STNMOV");
        line->c_.sfxdown_ = line->c_.sfxup_;
    }
}

//
// DDFBoomMakeGeneralizedLine
//
// Decodes the BOOM generalised linedef number and fills in the DDF
// linedef type `line' (which has already been instantiated with
// default values).
//
// NOTE: This code based on "Section 13" of BOOMREF.TXT.
//
// -AJA- 2001/06/22: began work on this.
//
void DDFBoomMakeGeneralizedLine(LineType *line, int number)
{
    //	LogDebug("- Making Generalized Linedef 0x%04x\n", number);

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

LineType *DDFBoomGetGeneralizedLine(int number)
{
    EPI_ASSERT(DDFIsBoomLineType(number));

    LineType *line = genlinetypes.Lookup(number);

    // If this hasn't be found, create it
    if (!line)
    {
        line = new LineType;
        line->Default();

        line->number_ = number;

        DDFBoomMakeGeneralizedLine(line, number);

        genlinetypes.push_back(line);
    }

    return line;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
