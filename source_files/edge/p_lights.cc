//----------------------------------------------------------------------------
//  EDGE Sector Lighting Code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// -KM- 1998/09/27 Lights generalised for ddf
//

#include <list>
#include <vector>

#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"

std::vector<LightSpecial *> active_lights;

//
// GENERALISED LIGHT
//
// -AJA- 2000/09/20: added FADE type.
//
static void DoLight(LightSpecial *light)
{
    const LightSpecialDefinition *type = light->type;

    if (light->count == 0)
        return;

    if (type->type_ == kLightSpecialTypeNone || --light->count)
        return;

    // Flashing lights
    switch (type->type_)
    {
    case kLightSpecialTypeSet: {
        light->sector->properties.light_level = light->maximum_light;

        // count is 0, i.e. this light is now disabled
        return;
    }

    case kLightSpecialTypeFade: {
        int diff = light->maximum_light - light->minimum_light;

        if (HMM_ABS(diff) < type->step_)
        {
            light->sector->properties.light_level = light->maximum_light;

            // count is 0, i.e. this light is now disabled
            return;
        }

        // step towards the target light level
        if (diff < 0)
            light->minimum_light -= type->step_;
        else
            light->minimum_light += type->step_;

        light->sector->properties.light_level = light->minimum_light;
        light->count                          = type->brighttime_;
        break;
    }

    case kLightSpecialTypeFlash: {
        // Dark
        if (RandomByteTest(type->chance_))
        {
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->minimum_light;
            light->count = type->darktime_;
        }
        else
        {
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->maximum_light;
            light->count = type->brighttime_;
        }
        break;
    }

    case kLightSpecialTypeStrobe:
        if (light->sector->properties.light_level == light->maximum_light)
        {
            // Go dark
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->minimum_light;
            light->count = type->darktime_;
        }
        else
        {
            // Go Bright
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->maximum_light;
            light->count = type->brighttime_;
        }
        break;

    case kLightSpecialTypeGlow:
        if (light->direction == -1)
        {
            // Go dark
            light->sector->properties.light_level -= type->step_;
            if (light->sector->properties.light_level <= light->minimum_light)
            {
                light->sector->properties.light_level = light->minimum_light;
                light->count                          = type->brighttime_;
                light->direction                      = +1;
            }
            else
            {
                light->count = type->darktime_;
            }
        }
        else
        {
            // Go Bright
            light->sector->properties.light_level += type->step_;
            if (light->sector->properties.light_level >= light->maximum_light)
            {
                light->sector->properties.light_level = light->maximum_light;
                light->count                          = type->darktime_;
                light->direction                      = -1;
            }
            else
            {
                light->count = type->brighttime_;
            }
        }
        break;

    case kLightSpecialTypeFireFlicker: {
        // -ES- 2000/02/13 Changed this to original DOOM style flicker
        int amount = (RandomByte() & 7) * type->step_;

        if (light->sector->properties.light_level - amount < light->minimum_light)
        {
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->minimum_light;
            light->count = type->darktime_;
        }
        else
        {
            if (reduce_flash)
                light->sector->properties.light_level = (light->maximum_light + light->minimum_light) / 2;
            else
                light->sector->properties.light_level = light->maximum_light - amount;
            light->count = type->brighttime_;
        }
    }

    default:
        break;
    }
}

void RunLineTagLights(int tag, int bright)
{
    /* TURN LINE'S TAG LIGHTS ON */

    for (int i = 0; i < total_level_sectors; i++)
    {
        Sector *sector = level_sectors + i;

        if (sector->tag == tag)
        {
            // bright == 0 means to search for highest light level
            // surrounding sector
            if (!bright)
            {
                for (int j = 0; j < sector->line_count; j++)
                {
                    Line *templine = sector->lines[j];

                    Sector *temp = GetLineSectorAdjacent(templine, sector);

                    if (!temp)
                        continue;

                    if (temp->properties.light_level > bright)
                        bright = temp->properties.light_level;
                }
            }
            // bright == 1 means to search for lowest light level
            // surrounding sector
            if (bright == 1)
            {
                bright = 255;
                for (int j = 0; j < sector->line_count; j++)
                {
                    Line *templine = sector->lines[j];

                    Sector *temp = GetLineSectorAdjacent(templine, sector);

                    if (!temp)
                        continue;

                    if (temp->properties.light_level < bright)
                        bright = temp->properties.light_level;
                }
            }
            sector->properties.light_level = bright;
        }
    }
}

void DestroyAllLights(void)
{
    std::vector<LightSpecial *>::iterator LI;

    for (LI = active_lights.begin(); LI != active_lights.end(); LI++)
    {
        delete (*LI);
    }

    active_lights.clear();
}

LightSpecial *NewLight(void)
{
    // Allocate and link in light.

    LightSpecial *light = new LightSpecial;

    active_lights.push_back(light);

    return light;
}

bool RunSectorLight(Sector *sec, const LightSpecialDefinition *type)
{
    // check if a light effect already is running on this sector.
    LightSpecial *light = nullptr;

    std::vector<LightSpecial *>::iterator LI;

    for (LI = active_lights.begin(); LI != active_lights.end(); LI++)
    {
        if ((*LI)->count == 0 || (*LI)->sector == sec)
        {
            light = *LI;
            break;
        }
    }

    if (!light)
    {
        // didn't already exist, create a new one
        light = NewLight();
    }

    light->type      = type;
    light->sector    = sec;
    light->direction = -1;

    switch (type->type_)
    {
    case kLightSpecialTypeSet:
    case kLightSpecialTypeFade: {
        light->minimum_light = sec->properties.light_level;
        light->maximum_light = type->level_;
        light->count         = type->brighttime_;
        break;
    }

    default: {
        light->minimum_light = FindMinimumSurroundingLight(sec, sec->properties.light_level);
        light->maximum_light = sec->properties.light_level;
        light->count         = type->sync_ ? (level_time_elapsed % type->sync_) + 1 : type->darktime_;

        // -AJA- 2009/10/26: DOOM compatibility
        if (type->type_ == kLightSpecialTypeStrobe && light->minimum_light == light->maximum_light)
            light->minimum_light = 0;
        break;
    }
    }

    return true;
}

//
// RunLights
//
// Executes all light effects of this tic
// Lights are quite simple to handle, since they never destroy
// themselves. Therefore, we do not need to bother about stuff like
// removal queues
//
void RunLights(void)
{
    std::vector<LightSpecial *>::iterator LI;

    for (LI = active_lights.begin(); LI != active_lights.end(); LI++)
    {
        DoLight(*LI);
    }
}

//----------------------------------------------------------------------------
//  AMBIENT SOUND CODE
//----------------------------------------------------------------------------

#define SECSFX_TIME 7 // every 7 tics (i.e. 5 times per second)

class ambientsfx_c
{
  public:
    Sector *sector;

    SoundEffect *sfx;

    // tics to go before next update
    int count;

  public:
    ambientsfx_c(Sector *_sec, SoundEffect *_fx) : sector(_sec), sfx(_fx), count(SECSFX_TIME)
    {
    }

    ~ambientsfx_c()
    {
    }
};

std::list<ambientsfx_c *> active_ambients;

void AddAmbientSounds(Sector *sec, SoundEffect *sfx)
{
    active_ambients.push_back(new ambientsfx_c(sec, sfx));
}

void DestroyAllAmbientSounds(void)
{
    while (!active_ambients.empty())
    {
        ambientsfx_c *amb = *active_ambients.begin();

        active_ambients.pop_front();

        StopSoundEffect(&amb->sector->sound_effects_origin);

        delete amb;
    }
}

void RunAmbientSounds(void)
{
    std::list<ambientsfx_c *>::iterator S;

    for (S = active_ambients.begin(); S != active_ambients.end(); S++)
    {
        ambientsfx_c *amb = *S;

        if (amb->count > 0)
            amb->count--;
        else
        {
            amb->count = SECSFX_TIME;

            StartSoundEffect(amb->sfx, kCategoryLevel, &amb->sector->sound_effects_origin);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
