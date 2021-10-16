//----------------------------------------------------------------------------
//  Radius Trigger header file
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#ifndef __RAD_TRIG__
#define __RAD_TRIG__

#include "e_event.h"
#include "rad_defs.h"

#define DEBUG_RTS  0

extern rad_script_t *r_scripts;
extern rad_trigger_t *active_triggers;

// Tip Prototypes
void RAD_InitTips(void);
void RAD_ResetTips(void);
void RAD_DisplayTips(void);

// RadiusTrigger & Scripting Prototypes
void RAD_Init(void);
bool RAD_ReadScript(void *data, int size);
void RAD_LoadFile(const char *name);
void RAD_SpawnTriggers(const char *map_name);
void RAD_ClearTriggers(void);
void RAD_GroupTriggerTags(rad_trigger_t *trig);

void RAD_RunTriggers(void);
void RAD_Ticker(void);
void RAD_Drawer(void);
bool RAD_Responder(event_t * ev);
bool RAD_WithinRadius(mobj_t * mo, rad_script_t * r);
rad_script_t *RAD_FindScriptByName(const char *map_name, const char *name);
rad_trigger_t *RAD_FindTriggerByName(const char *name);
rts_state_t *RAD_FindStateByLabel(rad_script_t *scr, char *label);
void RAD_EnableByTag(mobj_t *actor, int tag, bool disable);
void RAD_MonsterIsDead(mobj_t *mo);

// Menu support
void RAD_StartMenu(rad_trigger_t *R, s_show_menu_t *menu);
void RAD_FinishMenu(int result);

// Path support
bool RAD_CheckReachedTrigger(mobj_t * thing);

//
//  PARSING
//
void RAD_ParserBegin(void);
void RAD_ParserDone(void);
void RAD_ParseLine(char *s);
int RAD_StringHashFunc(const char *s);

void RAD_Error    (const char *err, ...) GCCATTR((format (printf,1,2)));
void RAD_Warning  (const char *err, ...) GCCATTR((format (printf,1,2)));
void RAD_WarnError(const char *err, ...) GCCATTR((format (printf,1,2)));

extern int rad_cur_linenum;
extern const char *rad_cur_filename;

#endif  /* __RAD_TRIG__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
