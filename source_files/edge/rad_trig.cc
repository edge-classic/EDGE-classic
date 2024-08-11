//----------------------------------------------------------------------------
//  EDGE Radius Trigger / Tip Code
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
// -KM- 1998/11/25 Fixed problems created by DDF.
//   Radius Triggers can be added to wad files.  RSCRIPT is the lump.
//   Tip function can handle graphics.
//   New functions: ondeath, #version
//   Radius Triggers with radius < 0 affect entire map.
//   Radius triggers used to save compatibility with hacks in Doom/Doom2
//       (eg MAP07, E2M8, E3M8, MAP32 etc..)
//
// -AJA- 1999/10/23: Began work on a state model for RTS actions.
//
// -AJA- 1999/10/24: Split off actions into rad_act.c, and structures
//       into the rad_main.h file.
//
// -AJA- 2000/01/04: Split off parsing code into rad_pars.c.
//

#include "rad_trig.h"

#include "am_map.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_misc.h"
#include "r_modes.h"
#include "rad_act.h"
#include "s_sound.h"
#include "sokol_color.h"
#include "w_wad.h"

// Static Scripts.  Never change once all scripts have been read in.
RADScript *current_scripts = nullptr;

// Dynamic Triggers.  These only exist for the current level.
RADScriptTrigger *active_triggers = nullptr;

class rts_menu_c
{
  private:
    static const int MAX_TITLE  = 24;
    static const int MAX_CHOICE = 9;

    RADScriptTrigger *trigger;

    Style *style;

    std::string title;

    std::vector<std::string> choices;

  public:
    int current_choice;

  public:
    rts_menu_c(ScriptShowMenuParameter *menu, RADScriptTrigger *_trigger, Style *_style)
        : trigger(_trigger), style(_style), title(), choices()
    {
        const char *text = menu->title;
        if (menu->use_ldf)
            text = language[text];

        title = text;

        bool no_choices = (!menu->options[0] || !menu->options[1]);

        for (int idx = 0; (idx < 9) && menu->options[idx]; idx++)
            AddChoice(no_choices ? 0 : ('1' + idx), menu->options[idx], menu->use_ldf);

        current_choice = 0;

        if (choices.size() > 1)
        {
            choices[0].replace(0, 1, ">");
        }
    }

    ~rts_menu_c()
    { /* nothing to do */
    }

  private:
    void AddChoice(char key, const char *text, bool use_ldf)
    {
        if (use_ldf)
            text = language[text];

        std::string choice_line = text;

        if (key)
        {
            char buffer[8];
            sprintf(buffer, "%c. ", key);

            choice_line = "  " + std::string(buffer) + choice_line;
        }

        choices.push_back(choice_line);
    }

  public:
    int NumChoices() const
    {
        return (int)choices.size();
    }

    void NotifyResult(int result)
    {
        trigger->menu_result = result;
    }

    void ChoiceDown()
    {
        if (current_choice + 1 < (int)choices.size())
        {
            choices[current_choice].replace(0, 1, " ");
            current_choice += 1;
            choices[current_choice].replace(0, 1, ">");
        }
    }

    void ChoiceUp()
    {
        if (current_choice - 1 >= 0)
        {
            choices[current_choice].replace(0, 1, " ");
            current_choice -= 1;
            choices[current_choice].replace(0, 1, ">");
        }
    }

    void Drawer()
    {
        style->DrawBackground();

        HUDReset();

        HUDSetAlignment(0, -1);

        HUDSetScale(style->definition_->text_[StyleDefinition::kTextSectionTitle].scale_); // LOBO: Use TITLE.SCALE from styles.ddf
        HUDSetFont(style->fonts_[StyleDefinition::kTextSectionTitle]);                     // LOBO: Use TITLE.FONT from styles.ddf

        float total_h = HUDStringHeight(title.c_str());
        total_h += HUDFontHeight() * (NumChoices() + 1);

        float y = 100 - total_h / 2.0f;

        //Dropshadow code
        const Colormap *Dropshadow_colmap_Title = style->definition_->text_[StyleDefinition::kTextSectionTitle].dropshadow_colmap_;
        if (Dropshadow_colmap_Title) //we want a dropshadow
        {
            float Dropshadow_Offset = style->definition_->text_[StyleDefinition::kTextSectionTitle].dropshadow_offset_;
            Dropshadow_Offset *= style->definition_->text_[StyleDefinition::kTextSectionTitle].scale_;
            HUDSetTextColor(GetFontColor(Dropshadow_colmap_Title));
            HUDDrawText(160 + Dropshadow_Offset, y + Dropshadow_Offset, title.c_str());
        }

        if (style->definition_->text_[StyleDefinition::kTextSectionTitle].colmap_)
        {
            HUDSetTextColor(
                GetFontColor(style->definition_->text_[StyleDefinition::kTextSectionTitle].colmap_)); // LOBO: Use TITLE.COLOURMAP from styles.ddf
        }
        else
        {
            HUDSetTextColor(SG_WHITE_RGBA32);
        }

        HUDDrawText(160, y, title.c_str());

        HUDSetScale();
        HUDSetFont();
        HUDSetTextColor();

        HUDSetScale(style->definition_->text_[StyleDefinition::kTextSectionText].scale_); // LOBO: Use TEXT.SCALE from styles.ddf
        HUDSetFont(style->fonts_[StyleDefinition::kTextSectionText]);                     // LOBO: Use TEXT.FONT from styles.ddf

        y += HUDStringHeight(title.c_str());
        y += HUDFontHeight();

        
        const Colormap *Dropshadow_colmap_Text = style->definition_->text_[StyleDefinition::kTextSectionText].dropshadow_colmap_;

        for (int c = 0; c < NumChoices(); c++, y += HUDFontHeight())
        {
            if (Dropshadow_colmap_Text) //we want a dropshadow
            {
                float Dropshadow_Offset = style->definition_->text_[StyleDefinition::kTextSectionText].dropshadow_offset_;
                Dropshadow_Offset *= style->definition_->text_[StyleDefinition::kTextSectionText].scale_;
                HUDSetTextColor(GetFontColor(Dropshadow_colmap_Text));
                HUDDrawText(160 + Dropshadow_Offset, y + Dropshadow_Offset, choices[c].c_str());
            }

            if (style->definition_->text_[StyleDefinition::kTextSectionText].colmap_)
            {
                HUDSetTextColor(
                GetFontColor(style->definition_->text_[StyleDefinition::kTextSectionText].colmap_)); // LOBO: Use TEXT.COLOURMAP from styles.ddf
            }
            else
            {
                HUDSetTextColor(SG_LIGHT_BLUE_RGBA32);
            }
            HUDDrawText(160, y, choices[c].c_str());
        }
        HUDSetScale();
        HUDSetFont();
        HUDSetAlignment();
        HUDSetTextColor();
    }

    int Check(int key)
    {
        if (key == kDownArrow || key == kGamepadDown || key == kMouseWheelDown)
            ChoiceDown();

        if (key == kUpArrow || key == kGamepadUp || key == kMouseWheelUp)
            ChoiceUp();

        if ('a' <= key && key <= 'z')
            key = epi::ToUpperASCII(key);

        if (key == 'Q' || key == 'X' || key == kGamepadB || key == kMouse2 || key == kMouse3)
            return 0;

        if ('1' <= key && key <= ('0' + NumChoices()))
            return key - '0';

        if (key == kSpace || key == kEnter || key == 'Y' || key == kGamepadA || key == kMouse1 ||
            CheckKeyMatch(key_use, key))
            return current_choice + 1;

        return -1; /* invalid */
    }
};

// RTS menu active ?
bool               rts_menu_active = false;
static rts_menu_c *rts_curr_menu   = nullptr;

RADScript *FindScriptByName(const char *map_name, const char *name)
{
    RADScript *scr;

    for (scr = current_scripts; scr; scr = scr->next)
    {
        if (scr->script_name == nullptr)
            continue;

        if (strcmp(scr->mapid, map_name) != 0)
            continue;

        if (DDFCompareName(scr->script_name, name) == 0)
            return scr;
    }

    FatalError("RTS: No such script `%s' on map %s.\n", name, map_name);
    return nullptr;
}

RADScriptTrigger *FindScriptTriggerByName(const char *name)
{
    RADScriptTrigger *trig;

    for (trig = active_triggers; trig; trig = trig->next)
    {
        if (trig->info->script_name == nullptr)
            continue;

        if (DDFCompareName(trig->info->script_name, name) == 0)
            return trig;
    }

    LogWarning("RTS: No such trigger `%s'.\n", name);
    return nullptr;
}

static RADScriptTrigger *FindTriggerByScript(const RADScript *scr)
{
    RADScriptTrigger *trig;

    for (trig = active_triggers; trig; trig = trig->next)
    {
        if (trig->info == scr)
            return trig;
    }

    return nullptr; // no worries if none.
}

RADScriptState *FindScriptStateByLabel(RADScript *scr, char *label)
{
    RADScriptState *st;

    for (st = scr->first_state; st; st = st->next)
    {
        if (st->label == nullptr)
            continue;

        if (DDFCompareName(st->label, label) == 0)
            return st;
    }

    // NOTE: no error message, unlike the other Find funcs
    return nullptr;
}

void ClearDeathTriggersByMap(const std::string &mapname)
{
    for (RADScript *scr = current_scripts; scr; scr = scr->next)
    {
        if (epi::StringCaseCompareASCII(scr->mapid, mapname) == 0)
        {
            for (RADScriptState *state = scr->first_state; state; state = state->next)
            {
                if (state->action == ScriptWaitUntilDead)
                {
                    ScriptWaitUntilDeadParameter *wud = (ScriptWaitUntilDeadParameter *)state->param;
                    wud->tag                          = 0;
                    for (int n = 0; n < 10; n++)
                    {
                        if (wud->mon_names[n])
                            free((void *)wud->mon_names[n]);
                    }
                }
            }
        }
    }
}

//
// Looks for all current triggers with the given tag number, and
// either enables them or disables them (based on `disable').
// Actor can be nullptr.
//
void ScriptEnableByTag(MapObject *actor, uint32_t tag, bool disable, RADScriptTag tagtype)
{
    RADScriptTrigger *trig;

    for (trig = active_triggers; trig; trig = trig->next)
    {
        if (trig->info->tag[tagtype] == tag)
        {
            if (disable)
                trig->disabled = true;
            else
                trig->disabled = false;
        }
    }


}

//
// Looks for all current triggers based on a hash of the given string, and
// either enables them or disables them (based on `disable').
// Actor can be nullptr.
//
void ScriptEnableByTag(MapObject *actor, const char *name, bool disable)
{
    RADScriptTrigger *trig;

    uint32_t tag = epi::StringHash32(name);

    for (trig = active_triggers; trig; trig = trig->next)
    {
        if (trig->info->tag[kTriggerTagHash] == tag)
        {
            if (disable)
                trig->disabled = true;
            else
                trig->disabled = false;  
        }
    }

    
}

//
// Looks for all current triggers based on a hash of the given string, and
// check if it is active).
// Actor can be nullptr.
//
bool CheckActiveScriptByTag(MapObject *actor, const char *name)
{
    RADScriptTrigger *trig;

    uint32_t tag = epi::StringHash32(name);

    for (trig = active_triggers; trig; trig = trig->next)
    {
        if (trig->info->tag[1] == tag)
        {
            if (trig->disabled == false)
                return true;
        }
    }

    return false;

}

bool ScriptRadiusCheck(MapObject *mo, RADScript *r)
{
    int sec_tag = r->sector_tag;
    if (sec_tag > 0)
    {
        if (mo->subsector_->sector->tag != sec_tag)
            return false;
        if (r->rad_z >= 0 && fabs(r->z - MapObjectMidZ(mo)) > r->rad_z + mo->height_ / 2)
            return false;
        return true;
    }

    int sec_ind = r->sector_index;
    if (sec_ind >= 0 && sec_ind <= total_level_sectors)
    {
        if (mo->subsector_->sector - level_sectors != sec_ind)
            return false;
        if (r->rad_z >= 0 && fabs(r->z - MapObjectMidZ(mo)) > r->rad_z + mo->height_ / 2)
            return false;
        return true;
    }

    if (r->rad_x >= 0 && fabs(r->x - mo->x) > r->rad_x + mo->radius_)
        return false;

    if (r->rad_y >= 0 && fabs(r->y - mo->y) > r->rad_y + mo->radius_)
        return false;

    if (r->rad_z >= 0 && fabs(r->z - MapObjectMidZ(mo)) > r->rad_z + mo->height_ / 2)
    {
        return false;
    }

    return true;
}

static int ScriptAlivePlayers(void)
{
    int result = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];

        if (p && p->player_state_ != kPlayerDead)
            result |= (1 << pnum);
    }

    return result;
}

static int ScriptAllPlayersInRadius(RADScript *r, int mask)
{
    int result = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];

        if (p && (mask & (1 << pnum)) && ScriptRadiusCheck(p->map_object_, r))
            result |= (1 << pnum);
    }

    return result;
}

static int ScriptAllPlayersUsing(int mask)
{
    int result = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];

        if (p && p->use_button_down_)
            result |= (1 << pnum);
    }

    return result & mask;
}

static int ScriptAllPlayersCheckCondition(RADScript *r, int mask)
{
    int result = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];

        if (p && (mask & (1 << pnum)) && GameCheckConditions(p->map_object_, r->cond_trig))
            result |= (1 << pnum);
    }

    return result;
}

static bool ScriptCheckBossTrigger(RADScriptTrigger *trig, ScriptOnDeathParameter *cond)
{
    MapObject *mo;

    int count = 0;

    // lookup thing type if we haven't already done so
    if (!cond->cached_info)
    {
        if (cond->thing_name)
            cond->cached_info = mobjtypes.Lookup(cond->thing_name);
        else
        {
            cond->cached_info = mobjtypes.Lookup(cond->thing_type);

            if (cond->cached_info == nullptr)
                FatalError("RTS ONDEATH: Unknown thing type %d.\n", cond->thing_type);
        }
    }

    // scan the remaining mobjs to see if all bosses are dead
    for (mo = map_object_list_head; mo != nullptr; mo = mo->next_)
    {
        if (seen_monsters.count(cond->cached_info) == 0)
            return false; // Never on map?

        if (mo->info_ == cond->cached_info && mo->health_ > 0)
        {
            count++;

            if (count > cond->threshhold)
                return false;
        }
    }

    return true;
}

static bool ScriptCheckHeightTrigger(RADScriptTrigger *trig, ScriptOnHeightParameter *cond)
{
    float h;

    // lookup sector if we haven't already done so
    if (!cond->cached_sector)
    {
        if (cond->sec_num >= 0)
        {
            if (cond->sec_num >= total_level_sectors)
                FatalError("RTS ONHEIGHT: no such sector %d.\n", cond->sec_num);

            cond->cached_sector = &level_sectors[cond->sec_num];
        }
        else
        {
            cond->cached_sector = PointInSubsector(trig->info->x, trig->info->y)->sector;
        }
    }

    if (cond->is_ceil)
        h = cond->cached_sector->ceiling_height;
    else
        h = cond->cached_sector->floor_height;

    return (cond->z1 <= h && h <= cond->z2);
}

bool ScriptUpdatePath(MapObject *thing)
{
    RADScript        *scr = (RADScript *)thing->path_trigger_;
    RADScriptTrigger *trig;

    RADScriptPath *path;
    int            choice;

    if (!ScriptRadiusCheck(thing, scr))
        return false;

    // Thing has reached this path node. Update so it starts following
    // the next node.  Handle any PATH_EVENT too.  Enable the associated
    // trigger (could be none if there were no states).

    trig = FindTriggerByScript(scr);

    if (trig)
        trig->disabled = false;

    if (scr->path_event_label)
    {
        int state = MapObjectFindLabel(thing, scr->path_event_label);

        if (state)
            MapObjectSetStateDeferred(thing, state + scr->path_event_offset, 0);
    }

    if (scr->next_path_total == 0)
    {
        thing->path_trigger_ = nullptr;
        return true;
    }
    else if (scr->next_path_total == 1)
        choice = 0;
    else
        choice = RandomByteDeterministic() % scr->next_path_total;

    path = scr->next_in_path;
    EPI_ASSERT(path);

    for (; choice > 0; choice--)
    {
        path = path->next;
        EPI_ASSERT(path);
    }

    if (!path->cached_scr)
        path->cached_scr = FindScriptByName(scr->mapid, path->name);

    EPI_ASSERT(path->cached_scr);

    thing->path_trigger_ = path->cached_scr;
    return true;
}

static void DoRemoveTrigger(RADScriptTrigger *trig)
{
    // handle tag linkage
    if (trig->tag_next)
        trig->tag_next->tag_previous = trig->tag_previous;

    if (trig->tag_previous)
        trig->tag_previous->tag_next = trig->tag_next;

    // unlink and free it
    if (trig->next)
        trig->next->prev = trig->prev;

    if (trig->prev)
        trig->prev->next = trig->next;
    else
        active_triggers = trig->next;

    StopSoundEffect(&trig->sound_effects_origin);

    delete trig;
}

//
// Radius Trigger Event handler.
//
void RunScriptTriggers(void)
{
    RADScriptTrigger *trig, *next;

    // Start looking through the trigger list.
    for (trig = active_triggers; trig; trig = next)
    {
        next = trig->next;

        // stop running all triggers when an RTS menu becomes active
        if (rts_menu_active)
            break;

        // Don't process, if disabled
        if (trig->disabled)
            continue;

        // Handle repeat delay (from TAGGED_REPEATABLE).  This must be
        // done *before* all the condition checks, and that's what makes
        // it different from `wait_tics'.
        //
        if (trig->repeat_delay > 0)
        {
            trig->repeat_delay--;
            continue;
        }

        // Independent, means you don't have to stay within the trigger
        // radius for it to operate, It will operate on it's own.

        if (!(trig->info->tagged_independent && trig->activated))
        {
            int mask = ScriptAlivePlayers();

            // Immediate triggers are just that. Immediate.
            // Not within range so skip it.
            //
            if (!trig->info->tagged_immediate)
            {
                mask = ScriptAllPlayersInRadius(trig->info, mask);
                if (mask == 0)
                    continue;
            }

            // Check for use key trigger.
            if (trig->info->tagged_use)
            {
                mask = ScriptAllPlayersUsing(mask);
                if (mask == 0)
                    continue;
            }

            // height check...
            if (trig->info->height_trig)
            {
                ScriptOnHeightParameter *cur;

                for (cur = trig->info->height_trig; cur; cur = cur->next)
                    if (!ScriptCheckHeightTrigger(trig, cur))
                        break;

                // if they all succeeded, then cur will be nullptr...
                if (cur)
                    continue;
            }

            // ondeath check...
            if (trig->info->boss_trig)
            {
                ScriptOnDeathParameter *cur;

                for (cur = trig->info->boss_trig; cur; cur = cur->next)
                    if (!ScriptCheckBossTrigger(trig, cur))
                        break;

                // if they all succeeded, then cur will be nullptr...
                if (cur)
                    continue;
            }

            // condition check...
            if (trig->info->cond_trig)
            {
                mask = ScriptAllPlayersCheckCondition(trig->info, mask);
                if (mask == 0)
                    continue;
            }

            trig->activated    = true;
            trig->acti_players = mask;
        }

        // If we are waiting, decrement count and skip it.
        // Note that we must do this *after* all the condition checks.
        //
        if (trig->wait_tics > 0)
        {
            trig->wait_tics--;
            continue;
        }

        // Waiting until monsters are dead?
        while (trig->wait_tics == 0 && trig->wud_count <= 0)
        {
            // Execute current command
            RADScriptState *state = trig->state;
            EPI_ASSERT(state);

            // move to next state.  We do this NOW since the action itself
            // may want to change the trigger's state (to support GOTO type
            // actions and other possibilities).
            //
            trig->state = trig->state->next;

            (*state->action)(trig, state->param);

            if (!trig->state)
                break;

            trig->wait_tics += trig->state->tics;

            if (trig->disabled || rts_menu_active)
                break;
        }

        if (trig->state)
            continue;

        // we've reached the end of the states.  Delete the trigger unless
        // it is Tagged_Repeatable and has some more repeats left.
        //
        if (trig->info->repeat_count != 0)
            trig->repeats_left--;

        if (trig->repeats_left > 0)
        {
            trig->state        = trig->info->first_state;
            trig->wait_tics    = trig->state->tics;
            trig->repeat_delay = trig->info->repeat_delay;
            continue;
        }

        DoRemoveTrigger(trig);
    }
}

void ScriptUpdateMonsterDeaths(MapObject *mo)
{
    if (mo->hyper_flags_ & kHyperFlagWaitUntilDead)
    {
        mo->hyper_flags_ &= ~kHyperFlagWaitUntilDead;

        RADScriptTrigger *trig;

        for (trig = active_triggers; trig; trig = trig->next)
        {
            for (auto tag : epi::SeparatedStringVector(mo->wait_until_dead_tags_, ','))
            {
                if (trig->wud_tag == atoi(tag.c_str()))
                    trig->wud_count--;
            }
        }
    }
}

//
// Called from SpawnScriptTriggers to set the tag_next & tag_previous fields
// of each rad_trigger_t, keeping all triggers with the same tag in a
// linked list for faster handling.
//
void GroupTriggerTags(RADScriptTrigger *trig)
{
    RADScriptTrigger *cur;

    trig->tag_next = trig->tag_previous = nullptr;

    // find first trigger with the same tag #
    for (cur = active_triggers; cur; cur = cur->next)
    {
        if (cur == trig)
            continue;

        if ((cur->info->tag[0] && (cur->info->tag[0] == trig->info->tag[0])) ||
            (cur->info->tag[1] && (cur->info->tag[1] == trig->info->tag[1])))
            break;
    }

    if (!cur)
        return;

    // link it in

    trig->tag_next     = cur;
    trig->tag_previous = cur->tag_previous;

    if (cur->tag_previous)
        cur->tag_previous->tag_next = trig;

    cur->tag_previous = trig;
}

void SpawnScriptTriggers(const char *map_name)
{
    RADScript        *scr;
    RADScriptTrigger *trig;

#ifdef DEVELOPERS
    if (active_triggers)
        FatalError("SpawnScriptTriggers without ScriptClearTriggers\n");
#endif

    for (scr = current_scripts; scr; scr = scr->next)
    {
        // This is from a different map!
        if (strcmp(map_name, scr->mapid) != 0 && strcmp(scr->mapid, "ALL") != 0)
            continue;

        // -AJA- 1999/09/25: Added skill checks.
        if (!CheckWhenAppear(scr->appear))
            continue;

        // -AJA- 2000/02/03: Added player num checks.
        if (total_players < scr->min_players || total_players > scr->max_players)
            continue;

        // ignore empty scripts (e.g. path nodes)
        if (!scr->first_state)
            continue;

        // OK, spawn new dynamic trigger
        trig = new RADScriptTrigger;

        trig->info         = scr;
        trig->disabled     = scr->tagged_disabled;
        trig->repeats_left = (scr->repeat_count < 0 || scr->repeat_count == 0) ? 1 : scr->repeat_count;
        trig->repeat_delay = 0;
        trig->tip_slot     = 0;
        trig->wud_tag = trig->wud_count = 0;

    //Lobo 2024: removed call to GroupTriggerTags() since we are not actually using it right now.
    // Left the code for posterity just in case we need it again.
        //GroupTriggerTags(trig);

        // initialise state machine
        trig->state     = scr->first_state;
        trig->wait_tics = scr->first_state->tics;

        // link it in
        trig->next = active_triggers;
        trig->prev = nullptr;

        if (active_triggers)
            active_triggers->prev = trig;

        active_triggers = trig;
    }
}

static void ScriptClearCachedInfo(void)
{
    RADScript               *scr;
    ScriptOnDeathParameter  *d_cur;
    ScriptOnHeightParameter *h_cur;

    for (scr = current_scripts; scr; scr = scr->next)
    {
        // clear ONDEATH cached info
        for (d_cur = scr->boss_trig; d_cur; d_cur = d_cur->next)
        {
            d_cur->cached_info = nullptr;
        }

        // clear ONHEIGHT cached info
        for (h_cur = scr->height_trig; h_cur; h_cur = h_cur->next)
        {
            h_cur->cached_sector = nullptr;
        }
    }
}

void ClearScriptTriggers(void)
{
    // remove all dynamic triggers
    while (active_triggers)
    {
        RADScriptTrigger *trig = active_triggers;
        active_triggers        = trig->next;

        delete trig;
    }

    ScriptClearCachedInfo();
    ResetScriptTips();
}

void InitializeRADScripts(void)
{
    InitializeScriptTips();
}

void ScriptMenuStart(RADScriptTrigger *R, ScriptShowMenuParameter *menu)
{
    EPI_ASSERT(!rts_menu_active);

    // find the right style
    StyleDefinition *def = nullptr;

    if (R->menu_style_name)
        def = styledefs.Lookup(R->menu_style_name);

    if (!def)
        def = styledefs.Lookup("RTS MENU");
    if (!def)
        def = styledefs.Lookup("MENU");
    if (!def)
        def = default_style;

    rts_curr_menu   = new rts_menu_c(menu, R, hud_styles.Lookup(def));
    rts_menu_active = true;
}

void ScriptMenuFinish(int result)
{
    if (!rts_menu_active)
        return;

    EPI_ASSERT(rts_curr_menu);

    // zero is cancelled, otherwise result is 1..N
    if (result < 0 || result > HMM_MAX(1, rts_curr_menu->NumChoices()))
        return;

    rts_curr_menu->NotifyResult(result);

    delete rts_curr_menu;

    rts_curr_menu   = nullptr;
    rts_menu_active = false;
}

static void ScriptMenuDrawer(void)
{
    EPI_ASSERT(rts_curr_menu);

    rts_curr_menu->Drawer();
}

void ScriptDrawer(void)
{
    if (!automap_active)
        DisplayScriptTips();

    if (rts_menu_active)
        ScriptMenuDrawer();
}

bool ScriptResponder(InputEvent *ev)
{
    if (ev->type != kInputEventKeyDown)
        return false;

    if (!rts_menu_active)
        return false;

    EPI_ASSERT(rts_curr_menu);

    int check = rts_curr_menu->Check(ev->value.key.sym);

    if (check >= 0)
    {
        ScriptMenuFinish(check);
        return true;
    }

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
