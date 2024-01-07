//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

#ifndef __DDF_LEVEL_H__
#define __DDF_LEVEL_H__

#include "epi.h"

#include "types.h"

class gamedef_c;

// ------------------------------------------------------------------
// ---------------MAP STRUCTURES AND CONFIGURATION-------------------
// ------------------------------------------------------------------

// -KM- 1998/11/25 Added generalised Finale type.
class map_finaledef_c
{
  public:
    map_finaledef_c();
    map_finaledef_c(map_finaledef_c &rhs);
    ~map_finaledef_c();

  private:
    void Copy(map_finaledef_c &src);

  public:
    void             Default(void);
    map_finaledef_c &operator=(map_finaledef_c &rhs);

    // Text
    std::string        text;
    std::string        text_back;
    std::string        text_flat;
    float              text_speed;
    unsigned int       text_wait;
    const colourmap_c *text_colmap;

    // Movie (pack file reference)
    std::string        movie;

    // Pic
    std::vector<std::string> pics;
    unsigned int             picwait;

    // Cast
    bool docast;

    // Bunny
    bool dobunny;

    // Music
    int music;
};

typedef enum
{
    MPF_None = 0x0,

    MPF_Jumping     = (1 << 0),
    MPF_Mlook       = (1 << 1),
    MPF_Cheats      = (1 << 2),
    MPF_ItemRespawn = (1 << 3),
    MPF_FastParm    = (1 << 4), // Fast Monsters
    MPF_ResRespawn  = (1 << 5), // Resurrect Monsters (else Teleport)

    MPF_True3D       = (1 << 6), // True 3D Gameplay
    MPF_Stomp        = (1 << 7), // Monsters can stomp players
    MPF_MoreBlood    = (1 << 8), // Make a bloody mess
    MPF_Respawn      = (1 << 9),
    MPF_AutoAim      = (1 << 10),
    MPF_AutoAimMlook = (1 << 11),
    MPF_ResetPlayer  = (1 << 12), // Force player back to square #1
    MPF_Extras       = (1 << 13),

    MPF_LimitZoom    = (1 << 14), // Limit zoom to certain weapons
    MPF_Crouching    = (1 << 15),
    MPF_Kicking      = (1 << 16), // Weapon recoil
    MPF_WeaponSwitch = (1 << 17),

    MPF_PassMissile = (1 << 18),
    MPF_TeamDamage  = (1 << 19),
} mapsettings_e;

typedef enum
{
    SKS_Unset   = -1,
    SKS_Mirror  = 0,
    SKS_Repeat  = 1,
    SKS_Stretch = 2,
    SKS_Vanilla = 3,
} skystretch_e;

typedef enum
{
    // standard Doom intermission stats
    WISTYLE_Doom = 0,

    // no stats at all
    WISTYLE_None = 1
} intermission_style_e;

class mapdef_c
{
  public:
    mapdef_c();
    ~mapdef_c();

  public:
    void Default(void);
    void CopyDetail(mapdef_c &src);

    // Member vars....
    std::string name;

    ///---	// next in the list
    ///---	mapdef_c *next;				// FIXME!! Gamestate information

    // level description, a reference to languages.ldf
    std::string description;

    std::string namegraphic;
    std::string leavingbggraphic;
    std::string enteringbggraphic;
    std::string lump;
    std::string sky;
    std::string surround;

    int music;

    int partime;

    gamedef_c  *episode; // set during DDF_CleanUp
    std::string episode_name;

    // flags come in two flavours: "force on" and "force off".  When not
    // forced, then the user is allowed to control it (not applicable to
    // all the flags, e.g. RESET_PLAYER).
    int force_on;
    int force_off;

    // name of the next normal level
    std::string nextmapname;

    // name of the secret level
    std::string secretmapname;

    // -KM- 1998/11/25 All lines with this trigger will be activated at
    // the level start. (MAP07)
    int autotag;

    intermission_style_e wistyle;

    // -KM- 1998/11/25 Generalised finales.
    map_finaledef_c f_pre;
    map_finaledef_c f_end;

    // optional *MAPINFO field
    std::string author;

    // sky stretch override
    skystretch_e forced_skystretch;

    colourmap_c *indoor_fog_cmap;
    rgbcol_t     indoor_fog_color;
    float        indoor_fog_density;
    colourmap_c *outdoor_fog_cmap;
    rgbcol_t     outdoor_fog_color;
    float        outdoor_fog_density;

  private:
    // disable copy construct and assignment operator
    explicit mapdef_c(mapdef_c &rhs)
    {
        (void)rhs;
    }
    mapdef_c &operator=(mapdef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our mapdefs container
class mapdef_container_c : public std::vector<mapdef_c *>
{
  public:
    mapdef_container_c()
    {
    }
    ~mapdef_container_c()
    {
      for (auto iter = begin(); iter != end(); iter++)
      {
          mapdef_c *map = *iter;
          delete map;
          map = nullptr;
      }
    }

  public:
    mapdef_c *Lookup(const char *name);
};

// -------EXTERNALISATIONS-------

extern mapdef_container_c mapdefs; // -ACB- 2004/06/29 Implemented

void DDF_ReadLevels(const std::string &data);

#endif // __DDF_LEVEL_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
