//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Miscellaneous)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// This file handles
//    LightSpecial         [LITE]
//    Button        [BUTN]
//    rad_trigger_t   [TRIG]
//    drawtip_t       [DTIP]
//
//    PlaneMover    [PMOV]
//    SlidingDoorMover   [SMOV]
//
// TODO HERE:
//   +  Fix donuts.
//   -  Button off_sound field.
//

#include "epi.h"
#include "r_misc.h"
#include "rad_trig.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "sv_main.h"

// forward decls.
int   SV_ButtonCountElems(void);
int   SV_ButtonGetIndex(Button *elem);
void *SV_ButtonFindByIndex(int index);
void  SV_ButtonCreateElems(int num_elems);
void  SV_ButtonFinaliseElems(void);

int   SV_LightCountElems(void);
int   SV_LightGetIndex(LightSpecial *elem);
void *SV_LightFindByIndex(int index);
void  SV_LightCreateElems(int num_elems);
void  SV_LightFinaliseElems(void);

int   SV_TriggerCountElems(void);
int   SV_TriggerGetIndex(TriggerScriptTrigger *elem);
void *SV_TriggerFindByIndex(int index);
void  SV_TriggerCreateElems(int num_elems);
void  SV_TriggerFinaliseElems(void);

int   SV_TipCountElems(void);
int   SV_TipGetIndex(ScriptDrawTip *elem);
void *SV_TipFindByIndex(int index);
void  SV_TipCreateElems(int num_elems);
void  SV_TipFinaliseElems(void);

int   SV_PlaneMoveCountElems(void);
int   SV_PlaneMoveGetIndex(PlaneMover *elem);
void *SV_PlaneMoveFindByIndex(int index);
void  SV_PlaneMoveCreateElems(int num_elems);
void  SV_PlaneMoveFinaliseElems(void);

int   SV_SliderMoveCountElems(void);
int   SV_SliderMoveGetIndex(PlaneMover *elem);
void *SV_SliderMoveFindByIndex(int index);
void  SV_SliderMoveCreateElems(int num_elems);
void  SV_SliderMoveFinaliseElems(void);

bool SR_LightGetType(void *storage, int index, void *extra);
void SR_LightPutType(void *storage, int index, void *extra);

bool SaveGameGetTriggerScript(void *storage, int index, void *extra);
void SaveGamePutTriggerScript(void *storage, int index, void *extra);

bool SaveGameTriggerGetState(void *storage, int index, void *extra);
void SaveGameTriggerPutState(void *storage, int index, void *extra);

bool SR_TipGetString(void *storage, int index, void *extra);
void SR_TipPutString(void *storage, int index, void *extra);

bool SR_PlaneMoveGetType(void *storage, int index, void *extra);
void SR_PlaneMovePutType(void *storage, int index, void *extra);

bool SR_SliderGetInfo(void *storage, int index, void *extra);
void SR_SliderPutInfo(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  BUTTON STRUCTURE
//
static Button dummy_button;

static SaveField sv_fields_button[] = {
    EDGE_SAVE_FIELD(dummy_button, line, "line", 1, kSaveFieldIndex, 4, "lines", SaveGameGetLine, SaveGamePutLine),
    EDGE_SAVE_FIELD(dummy_button, where, "where", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_button, button_image, "bimage", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetImage,
                    SaveGameLevelPutImage),
    EDGE_SAVE_FIELD(dummy_button, button_timer, "btimer", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    // FIXME: off_sound

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_button = {
    nullptr,                     // link in list
    "button_t",                  // structure name
    "butn",                      // start marker
    sv_fields_button,            // field descriptions
    (const char *)&dummy_button, // dummy base
    true,                        // define_me
    nullptr                      // pointer to known struct
};

SaveArray sv_array_button = {
    nullptr,                // link in list
    "buttonlist",           // array name
    &sv_struct_button,      // array type
    true,                   // define_me
    true,                   // allow_hub

    SV_ButtonCountElems,    // count routine
    SV_ButtonFindByIndex,   // index routine
    SV_ButtonCreateElems,   // creation routine
    SV_ButtonFinaliseElems, // finalisation routine

    nullptr,                // pointer to known array
    0                       // loaded size
};

//----------------------------------------------------------------------------
//
//  LIGHT STRUCTURE
//
static LightSpecial dummy_light;

static SaveField sv_fields_light[] = {
    EDGE_SAVE_FIELD(dummy_light, type, "type", 1, kSaveFieldString, 0, nullptr, SR_LightGetType, SR_LightPutType),
    EDGE_SAVE_FIELD(dummy_light, sector, "sector", 1, kSaveFieldIndex, 4, "sectors", SaveGameGetSector,
                    SaveGamePutSector),
    EDGE_SAVE_FIELD(dummy_light, count, "count", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_light, minimum_light, "minlight", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_light, maximum_light, "maxlight", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_light, direction, "direction", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_light, fade_count, "fade_count", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    // NOT HERE:
    //   - prev & next: automatically regenerated

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_light = {
    nullptr,                    // link in list
    "light_t",                  // structure name
    "lite",                     // start marker
    sv_fields_light,            // field descriptions
    (const char *)&dummy_light, // dummy base
    true,                       // define_me
    nullptr                     // pointer to known struct
};

SaveArray sv_array_light = {
    nullptr,               // link in list
    "lights",              // array name
    &sv_struct_light,      // array type
    true,                  // define_me
    true,                  // allow_hub

    SV_LightCountElems,    // count routine
    SV_LightFindByIndex,   // index routine
    SV_LightCreateElems,   // creation routine
    SV_LightFinaliseElems, // finalisation routine

    nullptr,               // pointer to known array
    0                      // loaded size
};

//----------------------------------------------------------------------------
//
//  TRIGGER STRUCTURE
//
static TriggerScriptTrigger dummy_trigger;

static SaveField sv_fields_trigger[] = {
    EDGE_SAVE_FIELD(dummy_trigger, info, "info", 1, kSaveFieldString, 0, nullptr, SaveGameGetTriggerScript,
                    SaveGamePutTriggerScript),

    EDGE_SAVE_FIELD(dummy_trigger, disabled, "disabled", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_trigger, activated, "activated", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_trigger, acti_players, "acti_players", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, repeats_left, "repeats_left", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, repeat_delay, "repeat_delay", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    EDGE_SAVE_FIELD(dummy_trigger, state, "state", 1, kSaveFieldNumeric, 4, nullptr, SaveGameTriggerGetState,
                    SaveGameTriggerPutState),
    EDGE_SAVE_FIELD(dummy_trigger, wait_tics, "wait_tics", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, tip_slot, "tip_slot", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, menu_style_name, "menu_style_name", 1, kSaveFieldString, 0, nullptr, SR_TipGetString,
                    SR_TipPutString),
    EDGE_SAVE_FIELD(dummy_trigger, menu_result, "menu_result", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, wud_tag, "wud_tag", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_trigger, wud_count, "wud_count", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    // NOT HERE
    //   - next & prev: can be regenerated.
    //   - tag_next & tag_previous: ditto
    //   - sound: can be recomputed.
    //   - last_con_message: doesn't matter.

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_trigger = {
    nullptr,                      // link in list
    "rad_trigger_t",              // structure name
    "trig",                       // start marker
    sv_fields_trigger,            // field descriptions
    (const char *)&dummy_trigger, // dummy base
    true,                         // define_me
    nullptr                       // pointer to known struct
};

SaveArray sv_array_trigger = {
    nullptr,                 // link in list
    "r_triggers",            // array name
    &sv_struct_trigger,      // array type
    true,                    // define_me
    true,                    // allow_hub

    SV_TriggerCountElems,    // count routine
    SV_TriggerFindByIndex,   // index routine
    SV_TriggerCreateElems,   // creation routine
    SV_TriggerFinaliseElems, // finalisation routine

    nullptr,                 // pointer to known array
    0                        // loaded size
};

//----------------------------------------------------------------------------
//
//  DRAWTIP STRUCTURE
//
static ScriptDrawTip dummy_draw_tip;

static SaveField sv_fields_drawtip[] = {
    // treating the `p' sub-struct here as if the fields were directly
    // in drawtip_t.
    EDGE_SAVE_FIELD(dummy_draw_tip, p.x_pos, "x_pos", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_draw_tip, p.y_pos, "y_pos", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_draw_tip, p.left_just, "left_just", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_draw_tip, p.translucency, "translucency", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_draw_tip, delay, "delay", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_draw_tip, tip_text, "tip_text", 1, kSaveFieldString, 0, nullptr, SR_TipGetString,
                    SR_TipPutString),
    EDGE_SAVE_FIELD(dummy_draw_tip, tip_graphic, "tip_graphic", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetImage,
                    SaveGameLevelPutImage),
    EDGE_SAVE_FIELD(dummy_draw_tip, playsound, "playsound", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_draw_tip, fade_time, "fade_time", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_draw_tip, fade_target, "fade_target", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_draw_tip, color, "color", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    // NOT HERE:
    //    p.slot_num, p.time: not used withing drawtip_t
    //    dirty: this is set in the finalizer
    //    hu_*: these are regenerated on next display
    //    p.color_name: only serves to generate 'color' field

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_drawtip = {
    nullptr,                       // link in list
    "drawtip_t",                   // structure name
    "dtip",                        // start marker
    sv_fields_drawtip,             // field descriptions
    (const char *)&dummy_draw_tip, // dummy base
    true,                          // define_me
    nullptr                        // pointer to known struct
};

SaveArray sv_array_drawtip = {
    nullptr,             // link in list
    "tip_slots",         // array name
    &sv_struct_drawtip,  // array type
    true,                // define_me
    true,                // allow_hub

    SV_TipCountElems,    // count routine
    SV_TipFindByIndex,   // index routine
    SV_TipCreateElems,   // creation routine
    SV_TipFinaliseElems, // finalisation routine

    nullptr,             // pointer to known array
    0                    // loaded size
};

//----------------------------------------------------------------------------
//
//  PLANEMOVE STRUCTURE
//
static PlaneMover dummy_plane_mover;

static SaveField sv_fields_plane_move[] = {
    EDGE_SAVE_FIELD(dummy_plane_mover, type, "type", 1, kSaveFieldString, 0, nullptr, SR_PlaneMoveGetType,
                    SR_PlaneMovePutType),
    EDGE_SAVE_FIELD(dummy_plane_mover, sector, "sector", 1, kSaveFieldIndex, 4, "sectors", SaveGameGetSector,
                    SaveGamePutSector),

    EDGE_SAVE_FIELD(dummy_plane_mover, is_ceiling, "is_ceiling", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_plane_mover, is_elevator, "is_elevator", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_plane_mover, start_height, "startheight", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_plane_mover, destination_height, "destheight", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_plane_mover, elevator_height, "elevheight", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_plane_mover, speed, "speed", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_plane_mover, crush, "crush", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),

    EDGE_SAVE_FIELD(dummy_plane_mover, direction, "direction", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_plane_mover, old_direction, "olddirection", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_plane_mover, tag, "tag", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_plane_mover, waited, "waited", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_plane_mover, sound_effect_started, "sfxstarted", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetBoolean, SaveGamePutBoolean),

    EDGE_SAVE_FIELD(dummy_plane_mover, new_special, "newspecial", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_plane_mover, new_image, "new_image", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetImage,
                    SaveGameLevelPutImage),

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_plane_move = {
    nullptr,                          // link in list
    "plane_move_t",                   // structure name
    "pmov",                           // start marker
    sv_fields_plane_move,             // field descriptions
    (const char *)&dummy_plane_mover, // dummy base
    true,                             // define_me
    nullptr                           // pointer to known struct
};

SaveArray sv_array_plane_move = {
    nullptr,                   // link in list
    "plane_movers",            // array name (virtual list)
    &sv_struct_plane_move,     // array type
    true,                      // define_me
    true,                      // allow_hub

    SV_PlaneMoveCountElems,    // count routine
    SV_PlaneMoveFindByIndex,   // index routine
    SV_PlaneMoveCreateElems,   // creation routine
    SV_PlaneMoveFinaliseElems, // finalisation routine

    nullptr,                   // pointer to known array
    0                          // loaded size
};

//----------------------------------------------------------------------------
//
//  SLIDERMOVE STRUCTURE
//
static SlidingDoorMover dummy_slider;

static SaveField sv_fields_slider_move[] = {
    EDGE_SAVE_FIELD(dummy_slider, info, "info", 1, kSaveFieldString, 0, nullptr, SR_SliderGetInfo, SR_SliderPutInfo),
    EDGE_SAVE_FIELD(dummy_slider, line, "line", 1, kSaveFieldIndex, 4, "lines", SaveGameGetLine, SaveGamePutLine),

    EDGE_SAVE_FIELD(dummy_slider, opening, "opening", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_slider, target, "target", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),

    EDGE_SAVE_FIELD(dummy_slider, direction, "direction", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_slider, waited, "waited", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    EDGE_SAVE_FIELD(dummy_slider, sound_effect_started, "sfxstarted", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetBoolean, SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_slider, final_open, "final_open", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),

    // NOT HERE:
    //   - line_length (can recreate)

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_slider_move = {
    nullptr,                     // link in list
    "slider_move_t",             // structure name
    "pmov",                      // start marker
    sv_fields_slider_move,       // field descriptions
    (const char *)&dummy_slider, // dummy base
    true,                        // define_me
    nullptr                      // pointer to known struct
};

SaveArray sv_array_slider_move = {
    nullptr,                    // link in list
    "active_sliders",           // array name (virtual list)
    &sv_struct_slider_move,     // array type
    true,                       // define_me
    true,                       // allow_hub

    SV_SliderMoveCountElems,    // count routine
    SV_SliderMoveFindByIndex,   // index routine
    SV_SliderMoveCreateElems,   // creation routine
    SV_SliderMoveFinaliseElems, // finalisation routine

    nullptr,                    // pointer to known array
    0                           // loaded size
};

//----------------------------------------------------------------------------

extern std::vector<Button *> active_buttons;

int SV_ButtonCountElems(void)
{
    // Note: also saves the unused Buttons (button_timer == 0)
    return (int)active_buttons.size();
}

void *SV_ButtonFindByIndex(int index)
{
    if (index < 0 || index >= (int)active_buttons.size())
    {
        LogWarning("LOADGAME: Invalid Button: %d\n", index);
        index = 0;
    }

    return active_buttons[index];
}

int SV_ButtonGetIndex(Button *elem)
{
    int index = 0;

    std::vector<Button *>::iterator LI;

    for (LI = active_buttons.begin(); LI != active_buttons.end() && (*LI) != elem; LI++)
        index++;

    if (LI == active_buttons.end())
        FatalError("LOADGAME: No such LightPtr: %p\n", elem);

    return index;
}

void SV_ButtonCreateElems(int num_elems)
{
    ClearButtons();

    for (; num_elems > 0; num_elems--)
    {
        Button *b = new Button;

        EPI_CLEAR_MEMORY(b, Button, 1);

        active_buttons.push_back(b);
    }
}

void SV_ButtonFinaliseElems(void)
{
    // nothing to do
}

//----------------------------------------------------------------------------

extern std::vector<LightSpecial *> active_lights;

int SV_LightCountElems(void)
{
    return (int)active_lights.size();
}

void *SV_LightFindByIndex(int index)
{
    if (index < 0 || index >= (int)active_lights.size())
        FatalError("LOADGAME: Invalid Light: %d\n", index);

    return active_lights[index];
}

int SV_LightGetIndex(LightSpecial *elem)
{
    int index = 0;

    std::vector<LightSpecial *>::iterator LI;

    for (LI = active_lights.begin(); LI != active_lights.end() && (*LI) != elem; LI++)
        index++;

    if (LI == active_lights.end())
        FatalError("LOADGAME: No such LightPtr: %p\n", elem);

    return index;
}

void SV_LightCreateElems(int num_elems)
{
    DestroyAllLights();

    for (; num_elems > 0; num_elems--)
    {
        LightSpecial *cur = NewLight();

        // initialise defaults
        cur->type   = &sectortypes.Lookup(0)->l_;
        cur->sector = level_sectors + 0;
    }
}

void SV_LightFinaliseElems(void)
{
    // nothing to do
}

//----------------------------------------------------------------------------

int SV_TriggerCountElems(void)
{
    TriggerScriptTrigger *cur;
    int                   count;

    for (cur = active_triggers, count = 0; cur; cur = cur->next, count++)
    { /* nothing here */
    }

    return count;
}

void *SV_TriggerFindByIndex(int index)
{
    TriggerScriptTrigger *cur;

    for (cur = active_triggers; cur && index > 0; cur = cur->next)
        index--;

    if (!cur)
        FatalError("LOADGAME: Invalid Trigger: %d\n", index);

    EPI_ASSERT(index == 0);
    return cur;
}

int SV_TriggerGetIndex(TriggerScriptTrigger *elem)
{
    TriggerScriptTrigger *cur;
    int                   index;

    for (cur = active_triggers, index = 0; cur && cur != elem; cur = cur->next)
        index++;

    if (!cur)
        FatalError("LOADGAME: No such TriggerPtr: %p\n", elem);

    return index;
}

void SV_TriggerCreateElems(int num_elems)
{
    ClearScriptTriggers();

    for (; num_elems > 0; num_elems--)
    {
        TriggerScriptTrigger *cur = new TriggerScriptTrigger;

        // link it in
        cur->next = active_triggers;
        cur->prev = nullptr;

        if (active_triggers)
            active_triggers->prev = cur;

        active_triggers = cur;

        // initialise defaults
        cur->info     = current_scripts;
        cur->state    = current_scripts ? current_scripts->first_state : nullptr;
        cur->disabled = true;
    }
}

void SV_TriggerFinaliseElems(void)
{
    /* Lobo: avoids a CTD when we have conflicting same named RTS scripts
    rad_trigger_t *cur;

    for (cur=active_triggers; cur; cur=cur->next)
    {
        RAD_GroupTriggerTags(cur);
    }
    */
}

//----------------------------------------------------------------------------

int SV_TipCountElems(void)
{
    return kMaximumTipSlots;
}

void *SV_TipFindByIndex(int index)
{
    if (index < 0 || index >= kMaximumTipSlots)
    {
        LogWarning("LOADGAME: Invalid Tip: %d\n", index);
        index = kMaximumTipSlots - 1;
    }

    return tip_slots + index;
}

int SV_TipGetIndex(ScriptDrawTip *elem)
{
    EPI_ASSERT(tip_slots <= elem && elem < (tip_slots + kMaximumTipSlots));

    return elem - tip_slots;
}

void SV_TipCreateElems(int num_elems)
{
    ResetScriptTips();
}

void SV_TipFinaliseElems(void)
{
    int i;

    // mark all active tip slots as dirty
    for (i = 0; i < kMaximumTipSlots; i++)
    {
        if (tip_slots[i].delay > 0)
            tip_slots[i].dirty = true;
    }
}

//----------------------------------------------------------------------------

extern std::vector<PlaneMover *> active_planes;

int SV_PlaneMoveCountElems(void)
{
    return (int)active_planes.size();
}

void *SV_PlaneMoveFindByIndex(int index)
{
    // Note: the index value starts at 0.

    if (index < 0 || index >= (int)active_planes.size())
        FatalError("LOADGAME: Invalid PlaneMove: %d\n", index);

    return active_planes[index];
}

int SV_PlaneMoveGetIndex(PlaneMover *elem)
{
    // returns the index value (starts at 0).

    int index = 0;

    std::vector<PlaneMover *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end() && (*PMI) != elem; PMI++)
    {
        index++;
    }

    if (PMI == active_planes.end())
        FatalError("LOADGAME: No such PlaneMove: %p\n", elem);

    return index;
}

void SV_PlaneMoveCreateElems(int num_elems)
{
    DestroyAllPlanes();

    for (; num_elems > 0; num_elems--)
    {
        PlaneMover *pmov = new PlaneMover;

        EPI_CLEAR_MEMORY(pmov, PlaneMover, 1);

        // link it in
        AddActivePlane(pmov);
    }
}

void SV_PlaneMoveFinaliseElems(void)
{
    // nothing to do
}

//----------------------------------------------------------------------------

extern std::vector<SlidingDoorMover *> active_sliders;

int SV_SliderMoveCountElems(void)
{
    return (int)active_sliders.size();
}

void *SV_SliderMoveFindByIndex(int index)
{
    // Note: the index value starts at 0.

    if (index < 0 || index >= (int)active_sliders.size())
        FatalError("LOADGAME: Invalid SliderMove: %d\n", index);

    return active_sliders[index];
}

int SV_SliderMoveGetIndex(SlidingDoorMover *elem)
{
    // returns the index value (starts at 0).

    int index = 0;

    std::vector<SlidingDoorMover *>::iterator SMI;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end() && (*SMI) != elem; SMI++)
    {
        index++;
    }

    if (SMI == active_sliders.end())
        FatalError("LOADGAME: No such SliderMove: %p\n", elem);

    return index;
}

void SV_SliderMoveCreateElems(int num_elems)
{
    DestroyAllSliders();

    for (; num_elems > 0; num_elems--)
    {
        SlidingDoorMover *smov = new SlidingDoorMover;

        EPI_CLEAR_MEMORY(smov, SlidingDoorMover, 1);

        // link it in
        AddActiveSlider(smov);
    }
}

void SV_SliderMoveFinaliseElems(void)
{
    std::vector<SlidingDoorMover *>::iterator SMI;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end(); SMI++)
    {
        SlidingDoorMover *smov = *SMI;

        if (smov->line)
            smov->line_length = RendererPointToDistance(0, 0, smov->line->delta_x, smov->line->delta_y);
    }
}

//----------------------------------------------------------------------------

bool SR_LightGetType(void *storage, int index, void *extra)
{
    (void)extra;

    const LightSpecialDefinition **dest = (const LightSpecialDefinition **)storage + index;

    int         number;
    const char *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':')
        FatalError("SR_LightGetType: invalid lighttype `%s'\n", str);

    number = strtol(str + 2, nullptr, 0);

    if (str[0] == 'S')
    {
        const SectorType *special = LookupSectorType(number);
        (*dest)                   = &special->l_;
    }
    else if (str[0] == 'L')
    {
        const LineType *special = LookupLineType(number);
        (*dest)                 = &special->l_;
    }
    else
        FatalError("SR_LightGetType: invalid lighttype `%s'\n", str);

    SaveChunkFreeString(str);
    return true;
}

//
// SR_LightPutType
//
// Format of the string:
//
//   <source char>  `:'  <source ref>
//
// The source char determines where the lighttype_t is found: `S' in a
// sector type or `L' in a linedef type.  The source ref is the
// numeric ID of the sector/line type in DDF.
//
void SR_LightPutType(void *storage, int index, void *extra)
{
    const LightSpecialDefinition *src = ((const LightSpecialDefinition **)storage)[index];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // look for it in the line types
    for (auto ln : linetypes)
    {
        if (src == &ln->l_)
        {
            std::string s = epi::StringFormat("L:%d", ln->number_);
            SaveChunkPutString(s.c_str());
            return;
        }
    }

    // look for it in the sector types
    for (auto sec : sectortypes)
    {
        if (src == &sec->l_)
        {
            std::string s = epi::StringFormat("S:%d", sec->number_);
            SaveChunkPutString(s.c_str());
            return;
        }
    }

    // not found !

    LogWarning("SAVEGAME: could not find lightdef_c %p !\n", src);
    SaveChunkPutString("S:1");
}

bool SaveGameTriggerGetState(void *storage, int index, void *extra)
{
    const TriggerScriptState **dest = (const TriggerScriptState **)storage + index;
    const TriggerScriptState  *temp;

    int                         value;
    const TriggerScriptTrigger *trig = (TriggerScriptTrigger *)sv_current_elem;

    value = SaveChunkGetInteger();

    if (value == 0)
    {
        (*dest) = nullptr;
        return true;
    }

    for (temp = trig->info->first_state; temp; temp = temp->next, value--)
    {
        if (value == 1)
            break;
    }

    if (!temp)
    {
        LogWarning("LOADGAME: invalid RTS state !\n");
        temp = trig->info->last_state;
    }

    (*dest) = temp;
    return true;
}

void SaveGameTriggerPutState(void *storage, int index, void *extra)
{
    const TriggerScriptState *src = ((const TriggerScriptState **)storage)[index];
    const TriggerScriptState *temp;

    int                         value;
    const TriggerScriptTrigger *trig = (TriggerScriptTrigger *)sv_current_elem;

    if (!src)
    {
        SaveChunkPutInteger(0);
        return;
    }

    // determine index value
    for (temp = trig->info->first_state, value = 1; temp; temp = temp->next, value++)
    {
        if (temp == src)
            break;
    }

    if (!temp)
        FatalError("INTERNAL ERROR: no such RTS state %p !\n", src);

    SaveChunkPutInteger(value);
}

bool SaveGameGetTriggerScript(void *storage, int index, void *extra)
{
    const TriggerScript **dest = (const TriggerScript **)storage + index;
    const TriggerScript  *temp;

    const char *swizzle;
    char        buffer[256];
    char       *base_p, *use_p;
    char       *map_name;

    int      idx_val;
    uint32_t crc;

    swizzle = SaveChunkGetString();

    if (!swizzle)
    {
        (*dest) = nullptr;
        return true;
    }

    epi::CStringCopyMax(buffer, swizzle, 256 - 1);
    SaveChunkFreeString(swizzle);

    if (buffer[0] != 'B' || buffer[1] != ':')
        FatalError("Corrupt savegame: bad script ref 1/4: `%s'\n", buffer);

    // get map name

    map_name = buffer + 2;
    base_p   = strchr(map_name, ':');

    if (base_p == nullptr || base_p == map_name || base_p[0] == 0)
        FatalError("Corrupt savegame: bad script ref 2/4: `%s'\n", map_name);

    // terminate the map name
    *base_p++ = 0;

    // get index value

    use_p  = base_p;
    base_p = strchr(use_p, ':');

    if (base_p == nullptr || base_p == use_p || base_p[0] == 0)
        FatalError("Corrupt savegame: bad script ref 3/4: `%s'\n", use_p);

    *base_p++ = 0;

    idx_val = strtol(use_p, nullptr, 0);
    EPI_ASSERT(idx_val >= 1);

    // get CRC value

    crc = (uint32_t)strtoul(base_p, nullptr, 16);

    // now find the bugger !
    // FIXME: move into RTS code

    for (temp = current_scripts; temp; temp = temp->next)
    {
        if (DDF_CompareName(temp->mapid, map_name) != 0)
            continue;

        if (temp->crc.GetCRC() != crc)
            continue;

        if (idx_val == 1)
            break;

        idx_val--;
    }

    if (!temp)
    {
        LogWarning("LOADGAME: No such RTS script !!\n");
        temp = current_scripts;
    }

    (*dest) = temp;
    return true;
}

//
// SaveGamePutTriggerScript
//
// Format of the string:
//
//   `B'  `:'  <map>  `:'  <index>  `:'  <crc>
//
// The `B' is a format descriptor -- future changes should use other
// letters.  The CRC is used to find the radius script.  There may be
// several in the same map with the same CRC, and the `index' part is
// used to differentiate them.  Index values begin at 1.  The CRC
// value is in hexadecimal.
//
void SaveGamePutTriggerScript(void *storage, int index, void *extra)
{
    const TriggerScript *src = ((const TriggerScript **)storage)[index];
    const TriggerScript *temp;

    int  idx_val;
    char buffer[256];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // determine index idx_val
    // FIXME: move into RTS code
    for (temp = current_scripts, idx_val = 1; temp; temp = temp->next)
    {
        if (DDF_CompareName(src->mapid, temp->mapid) != 0)
            continue;

        if (temp == src)
            break;

        if (temp->crc.GetCRC() == src->crc.GetCRC())
            idx_val++;
    }

    if (!temp)
        FatalError("SaveGamePutTriggerScript: invalid ScriptPtr %p\n", src);

    sprintf(buffer, "B:%s:%d:%X", src->mapid, idx_val, src->crc.GetCRC());

    SaveChunkPutString(buffer);
}

//----------------------------------------------------------------------------

bool SR_TipGetString(void *storage, int index, void *extra)
{
    const char **dest = (const char **)storage + index;

    SaveChunkFreeString(*dest);

    (*dest) = SaveChunkGetString();
    return true;
}

void SR_TipPutString(void *storage, int index, void *extra)
{
    const char *src = ((const char **)storage)[index];

    SaveChunkPutString(src);
}

bool SR_PlaneMoveGetType(void *storage, int index, void *extra)
{
    const PlaneMoverDefinition **dest = (const PlaneMoverDefinition **)storage + index;

    int         number;
    bool        is_ceil;
    const char *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':' || str[3] != ':')
        FatalError("SR_PlaneMoveGetType: invalid movestr `%s'\n", str);

    is_ceil = false;

    if (str[2] == 'F')
        ;
    else if (str[2] == 'C')
        is_ceil = true;
    else
        FatalError("SR_PlaneMoveGetType: invalid floortype `%s'\n", str);

    number = strtol(str + 4, nullptr, 0);

    if (str[0] == 'S')
    {
        const SectorType *special = LookupSectorType(number);
        (*dest)                   = is_ceil ? &special->c_ : &special->f_;
    }
    else if (str[0] == 'L')
    {
        const LineType *special = LookupLineType(number);
        (*dest)                 = is_ceil ? &special->c_ : &special->f_;
    }
    else if (str[0] == 'D')
    {
        // FIXME: this ain't gonna work, freddy
        (*dest) = is_ceil ? &donut[number].c_ : &donut[number].f_;
    }
    else
        FatalError("SR_PlaneMoveGetType: invalid srctype `%s'\n", str);

    SaveChunkFreeString(str);
    return true;
}

//
// Format of the string:
//
//   <line/sec>  `:'  <floor/ceil>  `:'  <ddf num>
//
// The first field contains `L' if the movplanedef_c is within a
// LineType, `S' for a SectorType, or `D' for the donut (which
// prolly won't work yet).  The second field is `F' for the floor
// field in the line/sectortype, or `C' for the ceiling field.  The
// last value is the line/sector DDF number.
//
void SR_PlaneMovePutType(void *storage, int index, void *extra)
{
    const PlaneMoverDefinition *src = ((const PlaneMoverDefinition **)storage)[index];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // check for donut
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            if (src == &donut[i].f_)
            {
                std::string s = epi::StringFormat("D:F:%d", i);
                SaveChunkPutString(s.c_str());
                return;
            }
            else if (src == &donut[i].c_)
            {
                std::string s = epi::StringFormat("D:C:%d", i);
                SaveChunkPutString(s.c_str());
                return;
            }
        }
    }

    // check all the line types
    for (auto ln : linetypes)
    {
        if (src == &ln->f_)
        {
            std::string s = epi::StringFormat("L:F:%d", ln->number_);
            SaveChunkPutString(s.c_str());
            return;
        }

        if (src == &ln->c_)
        {
            std::string s = epi::StringFormat("L:C:%d", ln->number_);
            SaveChunkPutString(s.c_str());
            return;
        }
    }

    // check all the sector types
    for (auto sec : sectortypes)
    {
        if (src == &sec->f_)
        {
            std::string s = epi::StringFormat("S:F:%d", sec->number_);
            SaveChunkPutString(s.c_str());
            return;
        }

        if (src == &sec->c_)
        {
            std::string s = epi::StringFormat("S:C:%d", sec->number_);
            SaveChunkPutString(s.c_str());
            return;
        }
    }

    // not found !

    LogWarning("SAVEGAME: could not find moving_plane %p !\n", src);
    SaveChunkPutString("L:C:1");
}

bool SR_SliderGetInfo(void *storage, int index, void *extra)
{
    const SlidingDoor **dest = (const SlidingDoor **)storage + index;
    const char         *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[0] != ':')
        FatalError("SR_SliderGetInfo: invalid special `%s'\n", str);

    const LineType *ld_type = LookupLineType(strtol(str + 1, nullptr, 0));

    (*dest) = &ld_type->s_;

    SaveChunkFreeString(str);
    return true;
}

//
// Format of the string will usually be a colon followed by the
// linedef number (e.g. ":123").
//
void SR_SliderPutInfo(void *storage, int index, void *extra)
{
    const SlidingDoor *src = ((const SlidingDoor **)storage)[index];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // check all the line types

    for (auto ld_type : linetypes)
    {
        if (src == &ld_type->s_)
        {
            std::string s = epi::StringFormat(":%d", ld_type->number_);
            SaveChunkPutString(s.c_str());
            return;
        }
    }

    // not found !

    LogWarning("SAVEGAME: could not find sliding door %p !\n", src);
    SaveChunkPutString(":1");
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
