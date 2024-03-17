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

#pragma once

#include "ddf_types.h"

class IntermissionMapPositionInfo
{
  public:
    std::string name_;

    int x_, y_;

  public:
    IntermissionMapPositionInfo();
    IntermissionMapPositionInfo(IntermissionMapPositionInfo &rhs);
    ~IntermissionMapPositionInfo();

  public:
    IntermissionMapPositionInfo &operator=(IntermissionMapPositionInfo &rhs);

  private:
    void Copy(IntermissionMapPositionInfo &src);
};

class IntermissionMapPositionInfoContainer : public std::vector<IntermissionMapPositionInfo *>
{
  public:
    IntermissionMapPositionInfoContainer();
    IntermissionMapPositionInfoContainer(IntermissionMapPositionInfoContainer &rhs);
    ~IntermissionMapPositionInfoContainer();

  private:
    void Copy(IntermissionMapPositionInfoContainer &src);

  public:
    IntermissionMapPositionInfoContainer &operator=(IntermissionMapPositionInfoContainer &rhs);
};

class IntermissionFrameInfo
{
  public:
    std::string pic_;   // Name of pic to display.
    int         tics_;  // Tics on this frame
    int         x_, y_; // Position on screen where this goes

  public:
    IntermissionFrameInfo();
    IntermissionFrameInfo(IntermissionFrameInfo &rhs);
    ~IntermissionFrameInfo();

  public:
    void                   Default(void);
    IntermissionFrameInfo &operator=(IntermissionFrameInfo &rhs);

  private:
    void Copy(IntermissionFrameInfo &src);
};

class IntermissionFrameInfoContainer : public std::vector<IntermissionFrameInfo *>
{
  public:
    IntermissionFrameInfoContainer();
    IntermissionFrameInfoContainer(IntermissionFrameInfoContainer &rhs);
    ~IntermissionFrameInfoContainer();

  private:
    void Copy(IntermissionFrameInfoContainer &rhs);

  public:
    IntermissionFrameInfoContainer &operator=(IntermissionFrameInfoContainer &rhs);
};

class IntermissionAnimationInfo
{
  public:
    enum AnimationType
    {
        kIntermissionAnimationInfoNormal,
        kIntermissionAnimationInfoLevel
    };

    AnimationType type_;

    std::string level_;

    IntermissionFrameInfoContainer frames_;

  public:
    IntermissionAnimationInfo();
    IntermissionAnimationInfo(IntermissionAnimationInfo &rhs);
    ~IntermissionAnimationInfo();

  public:
    IntermissionAnimationInfo &operator=(IntermissionAnimationInfo &rhs);
    void                       Default(void);

  private:
    void Copy(IntermissionAnimationInfo &rhs);
};

class IntermissionAnimationInfoContainer : public std::vector<IntermissionAnimationInfo *>
{
  public:
    IntermissionAnimationInfoContainer();
    IntermissionAnimationInfoContainer(IntermissionAnimationInfoContainer &rhs);
    ~IntermissionAnimationInfoContainer();

  private:
    void Copy(IntermissionAnimationInfoContainer &src);

  public:
    IntermissionAnimationInfoContainer &operator=(IntermissionAnimationInfoContainer &rhs);
};

enum LightingModel
{
    // standard Doom shading
    kLightingModelDoom = 0,
    // Doom shading without the brighter N/S, darker E/W walls
    kLightingModelDoomish = 1,
    // flat lighting (no shading at all)
    kLightingModelFlat = 2,
    // vertex lighting
    kLightingModelVertex = 3,
    // Invalid (-ACB- 2003/10/06: MSVC wants the invalid value as part of the
    // enum)
    kLightingModelInvalid = 999
};

class GameDefinition
{
  public:
    GameDefinition();
    ~GameDefinition();

  public:
    void Default(void);
    void CopyDetail(GameDefinition &src);

    std::string name_;

    IntermissionAnimationInfoContainer   anims_;
    IntermissionMapPositionInfoContainer mappos_;

    std::string background_;
    std::string splatpic_;
    std::string you_are_here_[2];

    // -AJA- 1999/10/22: background cameras.
    std::string bg_camera_;

    int                 music_;
    bool                no_skill_menu_;
    struct SoundEffect *percent_;
    struct SoundEffect *done_;
    struct SoundEffect *endmap_;
    struct SoundEffect *next_map_;
    struct SoundEffect *accel_snd_;
    struct SoundEffect *frag_snd_;

    std::string firstmap_;
    std::string namegraphic_;

    std::string titlemovie_;
    bool        movie_played_;

    std::vector<std::string> titlepics_;

    int titlemusic_;
    int titletics_;
    int special_music_;

    LightingModel lighting_;

    // Episode description, a reference to languages.ldf
    std::string description_;

  private:
    // disable copy construct and assignment operator
    explicit GameDefinition(GameDefinition &rhs)
    {
        (void)rhs;
    }
    GameDefinition &operator=(GameDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class GameDefinitionContainer : public std::vector<GameDefinition *>
{
  public:
    GameDefinitionContainer();
    ~GameDefinitionContainer();

  public:
    // Search Functions
    GameDefinition *Lookup(const char *refname);
};

extern GameDefinitionContainer gamedefs; // -ACB- 2004/06/21 Implemented

void DDF_ReadGames(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
