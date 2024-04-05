//----------------------------------------------------------------------------
//  Radius Trigger header file
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

#include "e_event.h"
#include "rad_defs.h"

#define EDGE_DEBUG_TRIGGER_SCRIPTS 0

extern RADScript        *current_scripts;
extern RADScriptTrigger *active_triggers;

// Tip Prototypes
void InitializeScriptTips(void);
void ResetScriptTips(void);
void DisplayScriptTips(void);

// RadiusTrigger & Scripting Prototypes
void InitializeRADScripts(void);
void ReadRADScript(const std::string &_data, const std::string &source);
void SpawnScriptTriggers(const char *map_name);
void ClearScriptTriggers(void);
void GroupTriggerTags(RADScriptTrigger *trig);

// For UMAPINFO bossaction "clear" directive
void ClearDeathTriggersByMap(const std::string &mapname);

void                  RunScriptTriggers(void);
void                  ScriptTicker(void);
void                  ScriptDrawer(void);
bool                  ScriptResponder(InputEvent *ev);
bool                  ScriptRadiusCheck(MapObject *mo, RADScript *r);
RADScript        *FindScriptByName(const char *map_name, const char *name);
RADScriptTrigger *FindScriptTriggerByName(const char *name);
RADScriptState   *FindScriptStateByLabel(RADScript *scr, char *label);
void                  ScriptEnableByTag(MapObject *actor, uint32_t tag, bool disable, RADScriptTag tagtype);
void                  ScriptEnableByTag(MapObject *actor, const char *name, bool disable);
bool                  CheckActiveScriptByTag(MapObject *actor, const char *name);
void                  ScriptUpdateMonsterDeaths(MapObject *mo);

// Menu support
void ScriptMenuStart(RADScriptTrigger *R, ScriptShowMenuParameter *menu);
void ScriptMenuFinish(int result);

// Path support
bool ScriptUpdatePath(MapObject *thing);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
