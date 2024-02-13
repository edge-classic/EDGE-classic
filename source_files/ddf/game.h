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

#include "types.h"

class IntermissionMapPosition
{
   public:
    std::string name_;

    int x_, y_;

   public:
    IntermissionMapPosition();
    IntermissionMapPosition(IntermissionMapPosition &rhs);
    ~IntermissionMapPosition();

   public:
    IntermissionMapPosition &operator=(IntermissionMapPosition &rhs);

   private:
    void Copy(IntermissionMapPosition &src);
};

class IntermissionMapPositionContainer
    : public std::vector<IntermissionMapPosition *>
{
   public:
    IntermissionMapPositionContainer();
    IntermissionMapPositionContainer(IntermissionMapPositionContainer &rhs);
    ~IntermissionMapPositionContainer();

   private:
    void Copy(IntermissionMapPositionContainer &src);

   public:
    IntermissionMapPositionContainer &operator=(
        IntermissionMapPositionContainer &rhs);
};

class IntermissionFrame
{
   public:
    std::string pic_;    // Name of pic to display.
    int         tics_;   // Tics on this frame
    int         x_, y_;  // Position on screen where this goes

   public:
    IntermissionFrame();
    IntermissionFrame(IntermissionFrame &rhs);
    ~IntermissionFrame();

   public:
    void               Default(void);
    IntermissionFrame &operator=(IntermissionFrame &rhs);

   private:
    void Copy(IntermissionFrame &src);
};

class IntermissionFrameContainer : public std::vector<IntermissionFrame *>
{
   public:
    IntermissionFrameContainer();
    IntermissionFrameContainer(IntermissionFrameContainer &rhs);
    ~IntermissionFrameContainer();

   private:
    void Copy(IntermissionFrameContainer &rhs);

   public:
    IntermissionFrameContainer &operator=(IntermissionFrameContainer &rhs);
};

class IntermissionAnimation
{
   public:
    enum AnimationType
    {
        kIntermissionAnimationNormal,
        kIntermissionAnimationLevel
    };

    AnimationType type_;

    std::string level_;

    IntermissionFrameContainer frames_;

   public:
    IntermissionAnimation();
    IntermissionAnimation(IntermissionAnimation &rhs);
    ~IntermissionAnimation();

   public:
    IntermissionAnimation &operator=(IntermissionAnimation &rhs);
    void                   Default(void);

   private:
    void Copy(IntermissionAnimation &rhs);
};

class IntermissionAnimationContainer
    : public std::vector<IntermissionAnimation *>
{
   public:
    IntermissionAnimationContainer();
    IntermissionAnimationContainer(IntermissionAnimationContainer &rhs);
    ~IntermissionAnimationContainer();

   private:
    void Copy(IntermissionAnimationContainer &src);

   public:
    IntermissionAnimationContainer &operator=(
        IntermissionAnimationContainer &rhs);
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

    IntermissionAnimationContainer   anims_;
    IntermissionMapPositionContainer mappos_;

    std::string background_;
    std::string splatpic_;
    std::string yah_[2];

    // -AJA- 1999/10/22: background cameras.
    std::string bg_camera_;

    int           music_;
    bool          no_skill_menu_;
    struct sfx_s *percent_;
    struct sfx_s *done_;
    struct sfx_s *endmap_;
    struct sfx_s *nextmap_;
    struct sfx_s *accel_snd_;
    struct sfx_s *frag_snd_;

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
    explicit GameDefinition(GameDefinition &rhs) { (void)rhs; }
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

extern GameDefinitionContainer gamedefs;  // -ACB- 2004/06/21 Implemented

void DDF_ReadGames(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
