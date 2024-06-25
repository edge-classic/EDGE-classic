
#include "AlmostEquals.h"
#include "ddf_flat.h"
#include "ddf_types.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "lua_compat.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "r_state.h"
#include "rad_trig.h"
#include "s_sound.h"

extern Player *ui_player_who;

//------------------------------------------------------------------------
//  PLAYER MODULE
//------------------------------------------------------------------------

// player.num_players()
//
static int PL_num_players(lua_State *L)
{
    lua_pushinteger(L, total_players);
    return 1;
}

// player.set_who(index)
//
static int PL_set_who(lua_State *L)
{
    int index = luaL_checknumber(L, 1);

    if (index < 0 || index >= total_players)
        FatalError("player.set_who: bad index value: %d (numplayers=%d)\n", index, total_players);

    if (index == 0)
    {
        ui_player_who = players[console_player];
        return 0;
    }

    int who = display_player;

    for (; index > 1; index--)
    {
        do
        {
            who = (who + 1) % kMaximumPlayers;
        } while (players[who] == nullptr);
    }

    ui_player_who = players[who];

    return 0;
}

// player.is_bot()
//
static int PL_is_bot(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->player_flags_ & kPlayerFlagBot) ? 1 : 0);
    return 1;
}

// player.get_name()
//
static int PL_get_name(lua_State *L)
{
    lua_pushstring(L, ui_player_who->player_name_);
    return 1;
}

// player.get_pos()
//
static int PL_get_pos(lua_State *L)
{
    HMM_Vec3 v;

    v.X = ui_player_who->map_object_->x;
    v.Y = ui_player_who->map_object_->y;
    v.Z = ui_player_who->map_object_->z;

    LuaPushVector3(L, v);
    return 1;
}

// player.get_angle()
//
static int PL_get_angle(lua_State *L)
{
    float value = epi::DegreesFromBAM(ui_player_who->map_object_->angle_);

    if (value > 360.0f)
        value -= 360.0f;
    if (value < 0)
        value += 360.0f;

    lua_pushnumber(L, value);
    return 1;
}

// player.get_mlook()
//
static int PL_get_mlook(lua_State *L)
{
    float value = epi::DegreesFromBAM(ui_player_who->map_object_->vertical_angle_);

    if (value > 180.0f)
        value -= 360.0f;

    lua_pushnumber(L, value);
    return 1;
}

// player.health()
//
static int PL_health(lua_State *L)
{
    float health = ui_player_who->health_;
    if (health < 1 && health > 0)
        lua_pushinteger(L, 1);
    else if (health > 99 && health < 100)
        lua_pushinteger(L, 99);
    else
        lua_pushinteger(L, (int)health);
    return 1;
}

// player.armor(type)
//
static int PL_armor(lua_State *L)
{
    int kind = (int)luaL_checknumber(L, 1);

    if (kind < 1 || kind > kTotalArmourTypes)
        FatalError("player.armor: bad armor index: %d\n", kind);

    kind--;
    // lua_pushnumber(L, floor(ui_player_who->armours_[kind] + 0.99));

    float a = ui_player_who->armours_[kind];
    if (a < 98)
        a += 0.99f;

    lua_pushinteger(L, floor(a));
    return 1;
}

// player.total_armor(type)
//
static int PL_total_armor(lua_State *L)
{
    // lua_pushnumber(L, floor(ui_player_who->total_armour_ + 0.99));

    float a = ui_player_who->total_armour_;
    if (a < 98)
        a += 0.99f;

    lua_pushinteger(L, floor(a));
    return 1;
}

// player.frags()
//
static int PL_frags(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->frags_);
    return 1;
}

// player.under_water()
//
static int PL_under_water(lua_State *L)
{
    lua_pushboolean(L, ui_player_who->underwater_ ? 1 : 0);
    return 1;
}

// player.on_ground()
//
static int PL_on_ground(lua_State *L)
{
    // not a 3D floor?
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        // on the edge above water/lava/etc? Handles edge walker case
        if (!AlmostEquals(ui_player_who->map_object_->floor_z_,
                          ui_player_who->map_object_->subsector_->sector->floor_height) &&
            !ui_player_who->map_object_->subsector_->sector->floor_vertex_slope)
            lua_pushboolean(L, 0);
        else
        {
            // touching the floor? Handles jumping or flying
            if (ui_player_who->map_object_->z <= ui_player_who->map_object_->floor_z_)
                lua_pushboolean(L, 1);
            else
                lua_pushboolean(L, 0);
        }
    }
    else
    {
        if (ui_player_who->map_object_->z <= ui_player_who->map_object_->floor_z_)
            lua_pushboolean(L, 1);
        else
            lua_pushboolean(L, 0);
    }

    return 1;
}

// player.is_swimming()
//
static int PL_is_swimming(lua_State *L)
{
    lua_pushboolean(L, ui_player_who->swimming_ ? 1 : 0);
    return 1;
}

// player.is_jumping()
//
static int PL_is_jumping(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->jump_wait_ > 0) ? 1 : 0);
    return 1;
}

// player.is_crouching()
//
static int PL_is_crouching(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->map_object_->extended_flags_ & kExtendedFlagCrouching) ? 1 : 0);
    return 1;
}

// player.is_attacking()
//
static int PL_is_attacking(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->attack_button_down_[0] || ui_player_who->attack_button_down_[1] ||
                        ui_player_who->attack_button_down_[2] || ui_player_who->attack_button_down_[3])
                           ? 1
                           : 0);
    return 1;
}

// player.is_rampaging()
//
static int PL_is_rampaging(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->attack_sustained_count_ >= 70) ? 1 : 0);
    return 1;
}

// player.is_grinning()
//
static int PL_is_grinning(lua_State *L)
{
    lua_pushboolean(L, (ui_player_who->grin_count_ > 0) ? 1 : 0);
    return 1;
}

// player.is_using()
//
static int PL_is_using(lua_State *L)
{
    lua_pushboolean(L, ui_player_who->use_button_down_ ? 1 : 0);
    return 1;
}

// player.is_zoomed()
//
static int PL_is_zoomed(lua_State *L)
{
    lua_pushboolean(L, view_is_zoomed ? 1 : 0);
    return 1;
}

// player.is_action1()
//
static int PL_is_action1(lua_State *L)
{
    lua_pushboolean(L, ui_player_who->action_button_down_[0] ? 1 : 0);
    return 1;
}

// player.is_action2()
//
static int PL_is_action2(lua_State *L)
{
    lua_pushboolean(L, ui_player_who->action_button_down_[1] ? 1 : 0);
    return 1;
}

// player.move_speed()
//
static int PL_move_speed(lua_State *L)
{
    lua_pushnumber(L, ui_player_who->actual_speed_);
    return 1;
}

// player.air_in_lungs()
//
static int PL_air_in_lungs(lua_State *L)
{
    if (ui_player_who->air_in_lungs_ <= 0)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    float value = ui_player_who->air_in_lungs_ * 100.0f / ui_player_who->map_object_->info_->lung_capacity_;

    value = HMM_Clamp(0.0f, value, 100.0f);

    lua_pushnumber(L, value);
    return 1;
}

// player.has_key(key)
//
static int PL_has_key(lua_State *L)
{
    int key = (int)luaL_checknumber(L, 1);

    if (key < 1 || key > 16)
        FatalError("player.has_key: bad key number: %d\n", key);

    key--;

    int value = (ui_player_who->cards_ & (1 << key)) ? 1 : 0;

    lua_pushboolean(L, value);
    return 1;
}

// player.has_power(power)
//
static int PL_has_power(lua_State *L)
{
    int power = (int)luaL_checknumber(L, 1);

    if (power < 1 || power > kTotalPowerTypes)
        FatalError("player.has_power: bad powerup number: %d\n", power);

    power--;

    int value = (ui_player_who->powers_[power] > 0) ? 1 : 0;

    // special check for GOD mode
    if (power == kPowerTypeInvulnerable && (ui_player_who->cheats_ & kCheatingGodMode))
        value = 1;

    lua_pushboolean(L, value);
    return 1;
}

// player.power_left(power)
//
static int PL_power_left(lua_State *L)
{
    int power = (int)luaL_checknumber(L, 1);

    if (power < 1 || power > kTotalPowerTypes)
        FatalError("player.power_left: bad powerup number: %d\n", power);

    power--;

    float value = ui_player_who->powers_[power];

    if (value > 0)
        value /= kTicRate;

    lua_pushnumber(L, value);
    return 1;
}

// player.has_weapon_slot(slot)
//
static int PL_has_weapon_slot(lua_State *L)
{
    int slot = (int)luaL_checknumber(L, 1);

    if (slot < 0 || slot > 9)
        FatalError("player.has_weapon_slot: bad slot number: %d\n", slot);

    int value = ui_player_who->available_weapons_[slot] ? 1 : 0;

    lua_pushboolean(L, value);
    return 1;
}

// player.cur_weapon_slot()
//
static int PL_cur_weapon_slot(lua_State *L)
{
    int slot;

    if (ui_player_who->ready_weapon_ < 0)
        slot = -1;
    else
        slot = ui_player_who->weapons_[ui_player_who->ready_weapon_].info->bind_key_;

    lua_pushinteger(L, slot);
    return 1;
}

// player.has_weapon(name)
//
static int PL_has_weapon(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    for (int j = 0; j < kMaximumWeapons; j++)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[j];

        if (pw->owned && !(pw->flags & kPlayerWeaponRemoving) && DDFCompareName(name, pw->info->name_.c_str()) == 0)
        {
            lua_pushboolean(L, 1);
            return 1;
        }
    }

    lua_pushboolean(L, 0);
    return 1;
}

// player.cur_weapon()
//
static int PL_cur_weapon(lua_State *L)
{
    if (ui_player_who->pending_weapon_ >= 0)
    {
        lua_pushstring(L, "change");
        return 1;
    }

    if (ui_player_who->ready_weapon_ < 0)
    {
        lua_pushstring(L, "none");
        return 1;
    }

    WeaponDefinition *info = ui_player_who->weapons_[ui_player_who->ready_weapon_].info;

    lua_pushstring(L, info->name_.c_str());
    return 1;
}

static void LuaSetPlayerSprite(Player *p, int position, int stnum, WeaponDefinition *info = nullptr)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0)
    {
        // object removed itself
        psp->state = psp->next_state = nullptr;
        return;
    }

    // state is old? -- Mundo hack for DDF inheritance
    if (info && stnum < info->state_grp_.back().first)
    {
        State *st = &states[stnum];

        if (st->label)
        {
            int new_state = DDFStateFindLabel(info->state_grp_, st->label, true /* quiet */);
            if (new_state != 0)
                stnum = new_state;
        }
    }

    State *st = &states[stnum];

    // model interpolation stuff
    if (psp->state && (st->flags & kStateFrameFlagModel) && (psp->state->flags & kStateFrameFlagModel) &&
        (st->sprite == psp->state->sprite) && st->tics > 1)
    {
        p->weapon_last_frame_ = psp->state->frame;
    }
    else
        p->weapon_last_frame_ = -1;

    psp->state      = st;
    psp->tics       = st->tics;
    psp->next_state = (st->nextstate == 0) ? nullptr : (states + st->nextstate);

    // call action routine

    p->action_player_sprite_ = position;

    if (st->action)
        (*st->action)(p->map_object_);
}

//
// SetPlayerSpriteDeferred
//
// -AJA- 2004/11/05: This is preferred method, doesn't run any actions,
//       which (ideally) should only happen during MovePlayerSprites().
//
static void LuaSetPlayerSpriteDeferred(Player *p, int position, int stnum)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0 || psp->state == nullptr)
    {
        LuaSetPlayerSprite(p, position, stnum);
        return;
    }

    psp->tics       = 0;
    psp->next_state = (states + stnum);
}

// player.weapon_state()
//
static int PL_weapon_state(lua_State *L)
{
    const char *weapon_name  = luaL_checkstring(L, 1);
    const char *weapon_state = luaL_checkstring(L, 2);

    if (ui_player_who->pending_weapon_ >= 0)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (ui_player_who->ready_weapon_ < 0)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    // WeaponDefinition *info =
    // ui_player_who->weapons_[ui_player_who->ready_weapon_].info;
    WeaponDefinition *oldWep = weapondefs.Lookup(weapon_name);
    if (!oldWep)
    {
        FatalError("player.weapon_state: Unknown weapon name '%s'.\n", weapon_name);
    }

    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < kMaximumWeapons; pw_index++)
    {
        if (!ui_player_who->weapons_[pw_index].owned)
            continue;

        if (ui_player_who->weapons_[pw_index].info == oldWep)
            break;
    }

    if (pw_index == kMaximumWeapons) // we dont have the weapon
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    ui_player_who->ready_weapon_ = (WeaponSelection)pw_index; // insta-switch to it

    int state = DDFStateFindLabel(oldWep->state_grp_, weapon_state, true /* quiet */);
    if (state == 0)
        FatalError("player.weapon_state: frame '%s' in [%s] not found!\n", weapon_state, weapon_name);
    // state += 1;

    LuaSetPlayerSpriteDeferred(ui_player_who, kPlayerSpriteWeapon,
                               state); // refresh the sprite

    lua_pushboolean(L, 1);
    return 1;
}

// player.ammo(type)
//
static int PL_ammo(lua_State *L)
{
    int ammo = (int)luaL_checknumber(L, 1);

    if (ammo < 1 || ammo > kTotalAmmunitionTypes)
        FatalError("player.ammo: bad ammo number: %d\n", ammo);

    ammo--;

    lua_pushinteger(L, ui_player_who->ammo_[ammo].count);
    return 1;
}

// player.ammomax(type)
//
static int PL_ammomax(lua_State *L)
{
    int ammo = (int)luaL_checknumber(L, 1);

    if (ammo < 1 || ammo > kTotalAmmunitionTypes)
        FatalError("player.ammomax: bad ammo number: %d\n", ammo);

    ammo--;

    lua_pushinteger(L, ui_player_who->ammo_[ammo].maximum);
    return 1;
}

// player.inventory(type)
//
static int PL_inventory(lua_State *L)
{
    int inv = (int)luaL_checknumber(L, 1);

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.inventory: bad inv number: %d\n", inv);

    inv--;

    lua_pushinteger(L, ui_player_who->inventory_[inv].count);
    return 1;
}

// player.inventorymax(type)
//
static int PL_inventorymax(lua_State *L)
{
    int inv = (int)luaL_checknumber(L, 1);

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.inventorymax: bad inv number: %d\n", inv);

    inv--;

    lua_pushinteger(L, ui_player_who->inventory_[inv].maximum);
    return 1;
}

// player.counter(type)
//
static int PL_counter(lua_State *L)
{
    int cntr = (int)luaL_checknumber(L, 1);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.counter: bad counter number: %d\n", cntr);

    cntr--;

    lua_pushinteger(L, ui_player_who->counters_[cntr].count);
    return 1;
}

// player.counter_max(type)
//
static int PL_counter_max(lua_State *L)
{
    int cntr = (int)luaL_checknumber(L, 1);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.counter_max: bad counter number: %d\n", cntr);

    cntr--;

    lua_pushinteger(L, ui_player_who->counters_[cntr].maximum);
    return 1;
}

// player.set_counter(type, value)
//
static int PL_set_counter(lua_State *L)
{
    int cntr = (int)luaL_checknumber(L, 1);
    int amt  = (int)luaL_checknumber(L, 2);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.set_counter: bad counter number: %d\n", cntr);

    cntr--;

    if (amt < 0)
        FatalError("player.set_counter: target amount cannot be negative!\n");

    if (amt > ui_player_who->counters_[cntr].maximum)
        FatalError("player.set_counter: target amount %d exceeds limit for counter "
                   "number %d\n",
                   amt, cntr);

    ui_player_who->counters_[cntr].count = amt;

    return 0;
}

// player.main_ammo(clip)
//
static int PL_main_ammo(lua_State *L)
{
    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        if (pw->info->ammo_[0] != kAmmunitionTypeNoAmmo)
        {
            if (pw->info->show_clip_)
            {
                EPI_ASSERT(pw->info->ammopershot_[0] > 0);

                value = pw->clip_size[0] / pw->info->ammopershot_[0];
            }
            else
            {
                value = ui_player_who->ammo_[pw->info->ammo_[0]].count;

                if (pw->info->clip_size_[0] > 0)
                    value += pw->clip_size[0];
            }
        }
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.ammo_type(ATK)
//
static int PL_ammo_type(lua_State *L)
{
    int ATK = (int)luaL_checknumber(L, 1);

    if (ATK < 1 || ATK > 2)
        FatalError("player.ammo_type: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = 1 + (int)pw->info->ammo_[ATK];
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.ammo_pershot(ATK)
//
static int PL_ammo_pershot(lua_State *L)
{
    int ATK = (int)luaL_checknumber(L, 1);

    if (ATK < 1 || ATK > 2)
        FatalError("player.ammo_pershot: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->info->ammopershot_[ATK];
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.clip_ammo(ATK)
//
static int PL_clip_ammo(lua_State *L)
{
    int ATK = (int)luaL_checknumber(L, 1);

    if (ATK < 1 || ATK > 2)
        FatalError("player.clip_ammo: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->clip_size[ATK];
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.clip_size(ATK)
//
static int PL_clip_size(lua_State *L)
{
    int ATK = (int)luaL_checknumber(L, 1);

    if (ATK < 1 || ATK > 2)
        FatalError("player.clip_size: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->info->clip_size_[ATK];
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.clip_is_shared()
//
static int PL_clip_is_shared(lua_State *L)
{
    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        if (pw->info->shared_clip_)
            value = 1;
    }

    lua_pushboolean(L, value);
    return 1;
}

// player.hurt_by()
//
static int PL_hurt_by(lua_State *L)
{
    if (ui_player_who->damage_count_ <= 0)
    {
        lua_pushstring(L, "");
        return 1;
    }

    // getting hurt because of your own damn stupidity
    if (ui_player_who->attacker_ == ui_player_who->map_object_)
        lua_pushstring(L, "self");
    else if (ui_player_who->attacker_ && (ui_player_who->attacker_->side_ & ui_player_who->map_object_->side_))
        lua_pushstring(L, "friend");
    else if (ui_player_who->attacker_)
        lua_pushstring(L, "enemy");
    else
        lua_pushstring(L, "other");

    return 1;
}

// player.hurt_mon()
//
static int PL_hurt_mon(lua_State *L)
{
    if (ui_player_who->damage_count_ > 0 && ui_player_who->attacker_ &&
        ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        lua_pushstring(L, ui_player_who->attacker_->info_->name_.c_str());
        return 1;
    }

    lua_pushstring(L, "");
    return 1;
}

// player.hurt_pain()
//
static int PL_hurt_pain(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->damage_pain_);
    return 1;
}

// player.hurt_dir()
//
static int PL_hurt_dir(lua_State *L)
{
    int dir = 0;

    if (ui_player_who->attacker_ && ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        MapObject *badguy = ui_player_who->attacker_;
        MapObject *pmo    = ui_player_who->map_object_;

        BAMAngle diff = PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y) - pmo->angle_;

        if (diff >= kBAMAngle45 && diff <= kBAMAngle135)
        {
            dir = -1;
        }
        else if (diff >= kBAMAngle225 && diff <= kBAMAngle315)
        {
            dir = +1;
        }
    }

    lua_pushinteger(L, dir);
    return 1;
}

// player.hurt_angle()
//
static int PL_hurt_angle(lua_State *L)
{
    float value = 0;

    if (ui_player_who->attacker_ && ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        MapObject *badguy = ui_player_who->attacker_;
        MapObject *pmo    = ui_player_who->map_object_;

        BAMAngle real_a = PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y);

        value = epi::DegreesFromBAM(real_a);

        if (value > 360.0f)
            value -= 360.0f;

        if (value < 0)
            value += 360.0f;
    }

    lua_pushinteger(L, value);
    return 1;
}

// player.kills()
// Lobo: November 2021
static int PL_kills(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->kill_count_);
    return 1;
}

// player.secrets()
// Lobo: November 2021
static int PL_secrets(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->secret_count_);
    return 1;
}

// player.items()
// Lobo: November 2021
static int PL_items(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->item_count_);
    return 1;
}

// player.map_enemies()
// Lobo: November 2021
static int PL_map_enemies(lua_State *L)
{
    lua_pushinteger(L, intermission_stats.kills);
    return 1;
}

// player.map_secrets()
// Lobo: November 2021
static int PL_map_secrets(lua_State *L)
{
    lua_pushinteger(L, intermission_stats.secrets);
    return 1;
}

// player.map_items()
// Lobo: November 2021
static int PL_map_items(lua_State *L)
{
    lua_pushinteger(L, intermission_stats.items);
    return 1;
}

// player.floor_flat()
// Lobo: November 2021
static int PL_floor_flat(lua_State *L)
{
    // If no 3D floors, just return the flat
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        lua_pushstring(L, ui_player_who->map_object_->subsector_->sector->floor.image->name_.c_str());
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it, then return the control sector's flat
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (player_floor_height + 1 > ef->top_height)
            {
                lua_pushstring(L, ef->top->image->name_.c_str());
                return 1;
            }
        }
        // Fallback if nothing else satisfies these conditions
        lua_pushstring(L, ui_player_who->map_object_->subsector_->sector->floor.image->name_.c_str());
    }

    return 1;
}

// player.sector_tag()
// Lobo: November 2021
static int PL_sector_tag(lua_State *L)
{
    lua_pushinteger(L, ui_player_who->map_object_->subsector_->sector->tag);
    return 1;
}

// player.play_footstep(flat name)
// Dasho: January 2022
// Now uses the new DDFFLAT construct
static int PL_play_footstep(lua_State *L)
{
    const char *flat = luaL_checkstring(L, 1);
    if (!flat)
        FatalError("player.play_footstep: No flat name given!\n");

    FlatDefinition *current_flatdef = flatdefs.Find(flat);

    if (!current_flatdef)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!current_flatdef->footstep_)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    else
    {
        // Probably need to add check to see if the sfx is valid - Dasho
        StartSoundEffect(current_flatdef->footstep_);
        lua_pushboolean(L, 1);
    }

    return 1;
}

// player.use_inventory(type)
//
static int PL_use_inventory(lua_State *L)
{
    int         inv         = (int)luaL_checknumber(L, 1);
    std::string script_name = "INVENTORY";

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.use_inventory: bad inventory number: %d\n", inv);

    if (inv < 10)
        script_name.append("0").append(std::to_string(inv));
    else
        script_name.append(std::to_string(inv));

    inv--;

    //******
    // If the same inventory script is already running then
    // don't start the same one again
    if (!CheckActiveScriptByTag(nullptr, script_name.c_str()))
    {
        if (ui_player_who->inventory_[inv].count > 0)
        {
            ui_player_who->inventory_[inv].count -= 1;
            ScriptEnableByTag(nullptr, script_name.c_str(), false);
        }
    }

    return 0;
}

// player.rts_enable_tagged(tag)
//
static int PL_rts_enable_tagged(lua_State *L)
{
    std::string name = luaL_checkstring(L, 1);

    if (!name.empty())
        ScriptEnableByTag(nullptr, name.c_str(), false);

    return 0;
}

// AuxStringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
static std::string AuxStringReplaceAll(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// GetMobjBenefits(mobj);
//
static std::string GetMobjBenefits(MapObject *obj, bool KillBenefits = false)
{
    std::string temp_string;
    Benefit    *list;
    int         temp_num = 0;

    if (KillBenefits)
        list = obj->info_->kill_benefits_;
    else
        list = obj->info_->pickup_benefits_;

    for (; list != nullptr; list = list->next)
    {
        switch (list->type)
        {
        case kBenefitTypeWeapon:
            // If it's a weapon all bets are off: we'll want to parse
            // it differently, not here.
            temp_string = "WEAPON=1";
            break;

        case kBenefitTypeAmmo:
            temp_string += "AMMO";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeHealth: // only benefit without a sub.type so just
                                 // give it 01
            temp_string += "HEALTH01=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeArmour:
            temp_string += "ARMOUR" + std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeInventory:
            temp_string += "INVENTORY";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeCounter:
            temp_string += "COUNTER";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeKey:
            temp_string += "KEY";
            temp_num = log2((int)list->sub.type);
            temp_num++;
            temp_string += std::to_string(temp_num);
            break;

        case kBenefitTypePowerup:
            temp_string += "POWERUP" + std::to_string((int)list->sub.type + 1);
            break;

        default:
            break;
        }
    }
    return temp_string;
}

// GetQueryInfoFromMobj(mobj, whatinfo)
//
static std::string GetQueryInfoFromMobj(MapObject *obj, int whatinfo)
{
    int         temp_num = 0;
    std::string temp_string;

    switch (whatinfo)
    {
    case 1: // name
        if (obj)
        {
            // try CAST_TITLE first
            temp_string = language[obj->info_->cast_title_];

            if (temp_string.empty()) // fallback to DDFTHING entry name
            {
                temp_string = obj->info_->name_;
                temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
            }
        }
        break;

    case 2: // current health
        if (obj)
        {
            temp_num    = obj->health_;
            temp_string = std::to_string(temp_num);
        }
        break;

    case 3: // spawn health
        if (obj)
        {
            temp_num    = obj->spawn_health_;
            temp_string = std::to_string(temp_num);
        }
        break;

    case 4: // pickup_benefits
        if (obj)
        {
            temp_string = GetMobjBenefits(obj, false);
        }
        break;

    case 5: // kill_benefits
        if (obj)
        {
            temp_string = GetMobjBenefits(obj, true);
        }
        break;
    }

    if (temp_string.empty())
        return ("");

    return (temp_string.c_str());
}

// GetQueryInfoFromWeapon(mobj, whatinfo, [secattackinfo])
//
static std::string GetQueryInfoFromWeapon(MapObject *obj, int whatinfo, bool secattackinfo = false)
{
    int         temp_num = 0;
    std::string temp_string;

    if (!obj->info_->pickup_benefits_)
        return "";
    if (!obj->info_->pickup_benefits_->sub.weap)
        return "";
    if (obj->info_->pickup_benefits_->type != kBenefitTypeWeapon)
        return "";

    WeaponDefinition *objWep = obj->info_->pickup_benefits_->sub.weap;
    if (!objWep)
        return "";

    int attacknum = 0; // default to primary attack
    if (secattackinfo)
        attacknum = 1;

    AttackDefinition *objAtck = objWep->attack_[attacknum];
    if (!objAtck && whatinfo > 2)
        return ""; // no attack to get info about (only should happen with
                   // secondary attacks)

    const DamageClass *damtype;
    float              temp_num2;

    switch (whatinfo)
    {
    case 1: // name
        temp_string = objWep->name_;
        temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
        break;

    case 2: // ZOOM_FACTOR
        temp_num2   = 90.0f / objWep->zoom_fov_;
        temp_string = std::to_string(temp_num2);
        break;

    case 3: // AMMOTYPE
        temp_num    = (objWep->ammo_[attacknum]) + 1;
        temp_string = std::to_string(temp_num);
        break;

    case 4: // AMMOPERSHOT
        temp_num    = objWep->ammopershot_[attacknum];
        temp_string = std::to_string(temp_num);
        break;

    case 5: // CLIPSIZE
        temp_num    = objWep->clip_size_[attacknum];
        temp_string = std::to_string(temp_num);
        break;

    case 6: // DAMAGE Nominal
        damtype     = &objAtck->damage_;
        temp_num    = damtype->nominal_;
        temp_string = std::to_string(temp_num);
        break;

    case 7: // DAMAGE Max
        damtype     = &objAtck->damage_;
        temp_num    = damtype->linear_max_;
        temp_string = std::to_string(temp_num);
        break;

    case 8: // Range
        temp_num    = objAtck->range_;
        temp_string = std::to_string(temp_num);
        break;

    case 9: // AUTOMATIC
        if (objWep->autofire_[attacknum])
            temp_string = "1";
        else
            temp_string = "0";
        break;
    }

    if (temp_string.empty())
        return ("");

    return (temp_string.c_str());
}

// player.query_object(maxdistance,whatinfo)
//
static int PL_query_object(lua_State *L)
{
    int maxdistance = (int)luaL_checknumber(L, 1);
    int whatinfo    = (int)luaL_checknumber(L, 2);

    if (whatinfo < 1 || whatinfo > 5)
        FatalError("player.query_object: bad whatInfo number: %d\n", whatinfo);

    MapObject *obj = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!obj)
    {
        lua_pushstring(L, "");
        return 1;
    }

    std::string temp_string;

    temp_string = GetQueryInfoFromMobj(obj, whatinfo);

    if (temp_string.empty())
        lua_pushstring(L, "");
    else
        lua_pushstring(L, temp_string.c_str());

    return 1;
}

// mapobject.query_tagged(thing tag, whatinfo)
//
static int MO_query_tagged(lua_State *L)
{
    int whattag  = (int)luaL_checknumber(L, 1);
    int whatinfo = (int)luaL_checknumber(L, 2);

    MapObject  *mo;
    std::string temp_value;

    for (mo = map_object_list_head; mo; mo = mo->next_)
    {
        if (mo->tag_ == whattag)
        {
            temp_value = GetQueryInfoFromMobj(mo, whatinfo);
            break;
        }
    }

    if (temp_value.empty())
        lua_pushstring(L, "");
    else
        lua_pushstring(L, temp_value.c_str());

    return 1;
}

// CreateLuaTable_Benefits(LuaState, mobj, Killbenefits);
//
static void CreateLuaTable_Benefits(lua_State *L, MapObject *obj, bool KillBenefits = false)
{
    Benefit    *list;
    std::string BenefitName;
    int         BenefitType   = 0;
    int         BenefitAmount = 0;
    int         BenefitLimit  = 0;

    if (KillBenefits)
        list = obj->info_->kill_benefits_;
    else
        list = obj->info_->pickup_benefits_;

    // how many benefits do we have?
    int NumberOfBenefits = 0;
    for (; list != nullptr; list = list->next)
    {
        NumberOfBenefits++;
    }

    if (NumberOfBenefits < 1)
        return;

    lua_pushstring(L, "benefits");

    int NumberOfBenefitFields = 4;           // how many fields in a row
    lua_createtable(L, NumberOfBenefits, 0); // create BENEFITS table

    int CurrentBenefit = 1;                  // counter
    if (KillBenefits)
        list = obj->info_->kill_benefits_;   // need to grab these again
    else
        list = obj->info_->pickup_benefits_; // need to grab these again

    for (; list != nullptr; list = list->next)
    {
        BenefitName.clear();
        BenefitType   = 0;
        BenefitAmount = 0;
        BenefitLimit  = 0;

        switch (list->type)
        {
        case kBenefitTypeWeapon:
            // If it's a weapon we'll want to parse
            // it differently to get the name.
            BenefitName = "WEAPON";
            if (list->sub.weap)
            {
                WeaponDefinition *objWep = list->sub.weap;
                BenefitName              = objWep->name_;
                BenefitName              = AuxStringReplaceAll(BenefitName, std::string("_"), std::string(" "));
            }
            BenefitType   = 0;
            BenefitAmount = 1;
            break;

        case kBenefitTypeAmmo:
            BenefitName   = "AMMO";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            if ((game_skill == kSkillBaby) || (game_skill == kSkillNightmare))
                BenefitAmount <<= 1; // double the ammo
            if (BenefitAmount > 1 && obj->flags_ & kMapObjectFlagDropped)
                BenefitAmount /= 2;  // dropped ammo gives half
            BenefitLimit = (int)list->limit;
            break;

        case kBenefitTypeHealth: // only benefit without a sub.type so just
                                 // give it 1
            BenefitName   = "HEALTH";
            BenefitType   = 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeArmour:
            BenefitName   = "ARMOUR";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeInventory:
            BenefitName   = "INVENTORY";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeCounter:
            BenefitName   = "COUNTER";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeKey:
            BenefitName   = "KEY";
            BenefitType   = (log2((int)list->sub.type) + 1);
            BenefitAmount = 1;
            break;

        case kBenefitTypePowerup:
            BenefitName   = "POWERUP";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeAmmoLimit:
            BenefitName   = "AMMOLIMIT";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeInventoryLimit:
            BenefitName   = "INVENTORYLIMIT";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        case kBenefitTypeCounterLimit:
            BenefitName   = "COUNTERLIMIT";
            BenefitType   = (int)list->sub.type + 1;
            BenefitAmount = (int)list->amount;
            BenefitLimit  = (int)list->limit;
            break;

        default:
            break;
        }

        // add it to our table
        lua_pushnumber(L, CurrentBenefit);      // new benefit
        lua_createtable(L, 0,
                        NumberOfBenefitFields); // create Benefit subItem table

        lua_pushstring(L, BenefitName.c_str());
        lua_setfield(L, -2, "name");            // add to Benefit subItem table

        lua_pushinteger(L, BenefitType);
        lua_setfield(L, -2, "type");            // add to Benefit subItem table

        lua_pushinteger(L, BenefitAmount);
        lua_setfield(L, -2, "amount");          // add to Benefit subItem table

        lua_pushinteger(L, BenefitLimit);
        lua_setfield(L, -2, "limit");           // add to Benefit subItem table

        lua_settable(L, -3);                    // add to BENEFITS table

        CurrentBenefit++;
    }

    lua_settable(L, -3); // add to MOBJ Table
}

// CreateLuaTable_Mobj(LuaState, mobj)
//
static void CreateLuaTable_Mobj(lua_State *L, MapObject *mo)
{
    std::string temp_value;
    int         NumberOfItems = 12;       // how many fields in a row
    lua_createtable(L, 0, NumberOfItems); // our MOBJ table

    //---------------
    // object.name
    temp_value = language[mo->info_->cast_title_]; // try CAST_TITLE first
    if (temp_value.empty())                        // fallback to DDFTHING entry name
    {
        temp_value = mo->info_->name_;
        temp_value = AuxStringReplaceAll(temp_value, std::string("_"), std::string(" "));
    }

    lua_pushstring(L, temp_value.c_str());
    lua_setfield(L, -2, "name"); // add to MOBJ Table
    //---------------

    //---------------
    // object.tag
    lua_pushinteger(L, (int)mo->tag_);
    lua_setfield(L, -2, "tag"); // add to MOBJ Table
    //---------------

    //---------------
    // object.type
    temp_value = "SCENERY"; // default to scenery

    if (mo->extended_flags_ & kExtendedFlagMonster)
        temp_value = "MONSTER";
    if (mo->flags_ & kMapObjectFlagSpecial)
        temp_value = "PICKUP";
    if (mo->info_->pickup_benefits_)
    {
        if (mo->info_->pickup_benefits_->type == kBenefitTypeWeapon)
            temp_value = "WEAPON";
    }

    lua_pushstring(L, temp_value.c_str());
    lua_setfield(L, -2, "type"); // add to MOBJ Table
    //---------------

    //---------------
    // object.currenthealth
    lua_pushinteger(L, (int)mo->health_);
    lua_setfield(L, -2, "current_health"); // add to MOBJ Table
    //---------------

    //---------------
    // object.spawnhealth
    lua_pushinteger(L, (int)mo->spawn_health_);
    lua_setfield(L, -2, "spawn_health"); // add to MOBJ Table
    //---------------

    //---------------
    // object.x
    lua_pushinteger(L, (int)mo->x);
    lua_setfield(L, -2, "x"); // add to MOBJ Table
    //---------------

    //---------------
    // object.y
    lua_pushinteger(L, (int)mo->y);
    lua_setfield(L, -2, "y"); // add to MOBJ Table
    //---------------

    //---------------
    // object.z
    lua_pushinteger(L, (int)mo->z);
    lua_setfield(L, -2, "z"); // add to MOBJ Table
    //---------------

    //---------------
    // object.angle
    float value = epi::DegreesFromBAM(mo->angle_);
    if (value > 360.0f)
        value -= 360.0f;
    if (value < 0)
        value += 360.0f;

    lua_pushinteger(L, (int)value);
    lua_setfield(L, -2, "angle"); // add to MOBJ Table
    //---------------

    //---------------
    // object.mlook
    value = epi::DegreesFromBAM(mo->vertical_angle_);

    if (value > 180.0f)
        value -= 360.0f;

    lua_pushinteger(L, (int)value);
    lua_setfield(L, -2, "mlook"); // add to MOBJ Table
    //---------------

    //---------------
    // object.radius
    lua_pushinteger(L, (int)mo->radius_);
    lua_setfield(L, -2, "radius"); // add to MOBJ Table
    //---------------

    //---------------
    // object.benefits
    if (mo->extended_flags_ & kExtendedFlagMonster)
        CreateLuaTable_Benefits(L, mo, true);  // only want kill benefits
    else
        CreateLuaTable_Benefits(L, mo, false); // only want pickup benefits
    //---------------
}

static void CreateLuaTable_Attacks(lua_State *L, WeaponDefinition *objWep)
{
    AttackDefinition *objAtck;
    int               temp_num = 0;
    std::string       temp_string;

    // how many attacks do we have?
    int NumberOfAttacks = 0;
    for (NumberOfAttacks = 0; NumberOfAttacks < 3; NumberOfAttacks++)
    {
        if (!objWep->attack_[NumberOfAttacks])
            break;
    }

    lua_pushstring(L, "attacks");

    lua_createtable(L, NumberOfAttacks, 0); // create ATTACKS table

    int NumberOfAttackFields = 8;           // how many fields in a row
    int CurrentAttack        = 0;           // counter
    for (CurrentAttack = 0; CurrentAttack < NumberOfAttacks; CurrentAttack++)
    {
        objAtck = objWep->attack_[CurrentAttack];
        const DamageClass *damtype;

        // add it to our table
        lua_pushnumber(L, CurrentAttack);      // new attack
        lua_createtable(L, 0,
                        NumberOfAttackFields); // create attack subItem table

        // NAME
        temp_string = objAtck->name_;
        temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
        lua_pushstring(L, temp_string.c_str());
        lua_setfield(L, -2, "name"); // add to ATTACK subItem table

        // AMMOTYPE
        temp_num = (objWep->ammo_[CurrentAttack]) + 1;
        lua_pushinteger(L, temp_num);
        lua_setfield(L, -2, "ammo_type"); // add to ATTACK subItem table

        // AMMOPERSHOT
        temp_num = objWep->ammopershot_[CurrentAttack];
        lua_pushinteger(L, temp_num);
        lua_setfield(L, -2, "ammo_per_shot"); // add to ATTACK subItem table

        // CLIPSIZE
        temp_num = objWep->clip_size_[CurrentAttack];
        lua_pushinteger(L, temp_num);
        lua_setfield(L, -2, "clip_size"); // add to ATTACK subItem table

        // DAMAGE Nominal
        damtype  = &objAtck->damage_;
        temp_num = damtype->nominal_;
        lua_pushnumber(L, temp_num);
        lua_setfield(L, -2, "damage"); // add to ATTACK subItem table

        // DAMAGE Max
        temp_num = damtype->linear_max_;
        lua_pushnumber(L, temp_num);
        lua_setfield(L, -2, "damage_max"); // add to ATTACK subItem table

        // Range
        temp_num = objAtck->range_;
        lua_pushinteger(L, temp_num);
        lua_setfield(L, -2, "range"); // add to ATTACK subItem table

        // AUTOMATIC
        lua_pushboolean(L, objWep->autofire_[CurrentAttack] ? 1 : 0);
        lua_setfield(L, -2, "is_automatic"); // add to ATTACK subItem table

        lua_settable(L, -3);                 // add to ATTACKS table
    }

    lua_settable(L, -3);                     // add to WEAPON Table
}

// CreateLuaTable_Weapon(LuaState, mobj)
//
static void CreateLuaTable_Weapon(lua_State *L, WeaponDefinition *objWep)
{
    std::string temp_value;
    int         NumberOfItems = 3;        // how many fields in a row
    lua_createtable(L, 0, NumberOfItems); // our WEAPON table

    //---------------
    // weapon.name
    temp_value = objWep->name_;
    temp_value = AuxStringReplaceAll(temp_value, std::string("_"), std::string(" "));
    lua_pushstring(L, temp_value.c_str());
    lua_setfield(L, -2, "name"); // add to WEAPON Table
    //---------------

    //---------------
    // weapon.zoomfactor
    // float temp_num2   = 90.0f / objWep->zoom_fov;
    float temp_num2 = objWep->zoom_factor_;
    lua_pushnumber(L, temp_num2);
    lua_setfield(L, -2, "zoom_factor"); // add to WEAPON Table
    //---------------

    //---------------
    // weapon.attacks
    CreateLuaTable_Attacks(L, objWep);
    //---------------
}

// mapobject.weapon_info(maxdistance) LUA Only
//
static int MO_weapon_info(lua_State *L)
{
    int maxdistance = (int)luaL_checknumber(L, 1);

    MapObject *mo = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!mo)
    {
        lua_pushstring(L, "");
        return 1;
    }
    else
    {
        if (!mo->info_->pickup_benefits_)
        {
            lua_pushstring(L, "");
            return 1;
        }
        if (!mo->info_->pickup_benefits_->sub.weap)
        {
            lua_pushstring(L, "");
            return 1;
        }
        if (mo->info_->pickup_benefits_->type != kBenefitTypeWeapon)
        {
            lua_pushstring(L, "");
            return 1;
        }

        WeaponDefinition *objWep = mo->info_->pickup_benefits_->sub.weap;
        if (!objWep)
        {
            lua_pushstring(L, "");
            return 1;
        }
        else
        {
            CreateLuaTable_Weapon(L, objWep); // create table with weapon info
            return 1;
        }
    }
}

// mapobject.object_info(maxdistance) LUA Only
//
static int MO_object_info(lua_State *L)
{
    int maxdistance = (int)luaL_checknumber(L, 1);

    MapObject *mo = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!mo)
    {
        lua_pushstring(L, "");
        return 1;
    }
    else
    {
        CreateLuaTable_Mobj(L, mo); // create table with mobj info
        return 1;
    }
}

// mapobject.tagged_info(thing tag) LUA Only
//
static int MO_tagged_info(lua_State *L)
{
    int         whattag = (int)luaL_checknumber(L, 1);
    MapObject  *mo;
    std::string temp_value;

    for (mo = map_object_list_head; mo; mo = mo->next_)
    {
        if (mo->tag_ == whattag)
        {
            temp_value = "FOUNDIT";
            break;
        }
    }

    if (temp_value.empty())
    {
        lua_pushstring(L, ""); // Found nothing
        return 1;
    }
    else
    {
        CreateLuaTable_Mobj(L, mo); // create table with mobj info
        return 1;
    }
}

// mapobject.count(thing type/id)
//
static int MO_count(lua_State *L)
{
    int        thingid = (int)luaL_checknumber(L, 1);
    MapObject *mo;
    double     thingcount = 0;

    for (mo = map_object_list_head; mo; mo = mo->next_)
    {
        if (mo->info_->number_ == thingid && mo->health_ > 0)
            thingcount++;
    }

    lua_pushinteger(L, thingcount);

    return 1;
}

// player.query_weapon(maxdistance,whatinfo,[SecAttack])
//
static int PL_query_weapon(lua_State *L)
{
    int maxdistance   = (int)luaL_checknumber(L, 1);
    int whatinfo      = (int)luaL_checknumber(L, 2);
    int secattackinfo = (int)luaL_optnumber(L, 3, 0);

    if (whatinfo < 1 || whatinfo > 9)
        FatalError("player.query_weapon: bad whatInfo number: %d\n", whatinfo);

    if (secattackinfo < 0 || secattackinfo > 1)
        FatalError("player.query_weapon: bad secAttackInfo number: %d\n", whatinfo);

    MapObject *obj = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!obj)
    {
        lua_pushstring(L, "");
        return 1;
    }

    std::string temp_string;

    if (secattackinfo == 1)
        temp_string = GetQueryInfoFromWeapon(obj, whatinfo, true);
    else
        temp_string = GetQueryInfoFromWeapon(obj, whatinfo);

    if (temp_string.empty())
        lua_pushstring(L, "");
    else
        lua_pushstring(L, temp_string.c_str());

    return 1;
}

// player.sector_light()
// Lobo: May 2023
static int PL_sector_light(lua_State *L)
{
    lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->properties.light_level);
    return 1;
}

// player.sector_floor_height()
// Lobo: May 2023
static int PL_sector_floor_height(lua_State *L)
{
    // If no 3D floors, just return the current sector floor height
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->floor_height);
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //  then return the control sector floor height
        float       CurrentFloor        = 0;
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (CurrentFloor > ef->top_height)
            {
                lua_pushnumber(L, ef->top_height);
                return 1;
            }

            if (player_floor_height + 1 > ef->top_height)
            {
                CurrentFloor = ef->top_height;
            }
        }
        lua_pushnumber(L, CurrentFloor);
    }

    return 1;
}

// player.sector_ceiling_height()
// Lobo: May 2023
static int PL_sector_ceiling_height(lua_State *L)
{
    // If no 3D floors, just return the current sector ceiling height
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->ceiling_height);
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //   then return the control sector ceiling height
        float       HighestCeiling      = 0;
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (player_floor_height + 1 > ef->top_height)
            {
                HighestCeiling = ef->top_height;
            }
            if (HighestCeiling < ef->top_height)
            {
                lua_pushnumber(L, ef->bottom_height);
                return 1;
            }
        }
        // Fallback if nothing else satisfies these conditions
        lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->ceiling_height);
    }

    return 1;
}

// player.is_outside()
// Lobo: May 2023
static int PL_is_outside(lua_State *L)
{
    // Doesn't account for extrafloors by design. Reasoning is that usually
    //  extrafloors will be platforms, not roofs...
    if (ui_player_who->map_object_->subsector_->sector->ceiling.image != sky_flat_image) // is it outdoors?
        lua_pushboolean(L, 0);
    else
        lua_pushboolean(L, 1);

    return 1;
}

// Game.info() LUA Only
//
static int Game_info(lua_State *L)
{
    std::string temp_value;

    int NumberOfItems = 3;                // how many fields in a row
    lua_createtable(L, 0, NumberOfItems); // our GAME table

    //---------------
    // game.name
    GameDefinition *g = current_map->episode_;
    EPI_ASSERT(g);

    if (g->description_.empty())
    {
        temp_value = g->name_;
        temp_value = AuxStringReplaceAll(temp_value, std::string("_"), std::string(" "));
    }
    else
    {
        temp_value = language[g->description_]; // try for description
    }

    lua_pushstring(L, temp_value.c_str());
    lua_setfield(L, -2, "name"); // add to GAME Table
    //---------------

    //---------------
    // game.mode
    if (InDeathmatch())
        lua_pushstring(L, "dm");
    else if (InCooperativeMatch())
        lua_pushstring(L, "coop");
    else
        lua_pushstring(L, "sp");

    lua_setfield(L, -2, "mode"); // add to GAME Table
    //---------------

    //---------------
    // game.skill
    lua_pushinteger(L, game_skill);
    lua_setfield(L, -2, "skill"); // add to GAME Table
    //---------------

    return 1;
}

// MAP.info() LUA Only
//
static int Map_info(lua_State *L)
{
    std::string temp_value;

    int NumberOfItems = 6;                // how many fields in a row
    lua_createtable(L, 0, NumberOfItems); // our MAP table

    //---------------
    // MAP.name
    lua_pushstring(L, current_map->name_.c_str());
    lua_setfield(L, -2, "name"); // add to MAP Table
    //---------------

    //---------------
    // MAP.title
    lua_pushstring(L, language[current_map->description_]);
    lua_setfield(L, -2, "title"); // add to MAP Table
    //---------------

    //---------------
    // MAP.author
    lua_pushstring(L, current_map->author_.c_str());
    lua_setfield(L, -2, "author"); // add to MAP Table
    //---------------

    //---------------
    // MAP.secrets
    lua_pushinteger(L, intermission_stats.secrets);
    lua_setfield(L, -2, "secrets"); // add to MAP Table
    //---------------

    //---------------
    // MAP.enemies
    lua_pushinteger(L, intermission_stats.kills);
    lua_setfield(L, -2, "enemies"); // add to MAP Table
    //---------------

    //---------------
    // MAP.items
    lua_pushinteger(L, intermission_stats.items);
    lua_setfield(L, -2, "items"); // add to MAP Table
    //---------------

    return 1;
}

// SECTOR.info() LUA Only
//
static int Sector_info(lua_State *L)
{
    std::string temp_value;

    int NumberOfItems = 15;               // how many fields in a row
    lua_createtable(L, 0, NumberOfItems); // our SECTOR table

    //---------------
    // SECTOR.tag
    lua_pushinteger(L, ui_player_who->map_object_->subsector_->sector->tag);
    lua_setfield(L, -2, "tag"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.lightlevel
    lua_pushinteger(L, ui_player_who->map_object_->subsector_->sector->properties.light_level);
    lua_setfield(L, -2, "light_level"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.floor_height
    float CurrentSurface = 0;

    // Default is to just return the current sector floor height
    CurrentSurface = ui_player_who->map_object_->subsector_->sector->floor_height;

    // While we're here, grab the floor flat too
    temp_value = ui_player_who->map_object_->subsector_->sector->floor.image->name_;

    // If we have 3D floors, search...
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used != 0)
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //  then return the control sector floor height
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (CurrentSurface > ef->top_height)
            {
                CurrentSurface = ef->top_height;
                break;
            }

            if (player_floor_height + 1 > ef->top_height)
            {
                CurrentSurface = ef->top_height;
                temp_value     = ef->top->image->name_;
            }
        }
    }
    lua_pushinteger(L, (int)CurrentSurface);
    lua_setfield(L, -2, "floor_height"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.floor_flat
    lua_pushstring(L, temp_value.c_str());
    lua_setfield(L, -2, "floor_flat"); // add to MAP Table
    temp_value.clear();
    //---------------

    //---------------
    // SECTOR.ceiling_height

    // default is to just return the current sector ceiling height
    CurrentSurface = ui_player_who->map_object_->subsector_->sector->ceiling_height;

    // If we have 3D floors, search...
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used != 0)
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //   then return the control sector ceiling height
        float       HighestCeiling      = 0;
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (player_floor_height + 1 > ef->top_height)
            {
                HighestCeiling = ef->top_height;
            }
            if (HighestCeiling < ef->top_height)
            {
                CurrentSurface = ef->bottom_height;
                break;
            }
        }
    }
    lua_pushinteger(L, (int)CurrentSurface);
    lua_setfield(L, -2, "ceiling_height"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.is_outside
    // Doesn't account for extrafloors by design. Reasoning is that usually
    //  extrafloors will be platforms, not roofs...
    if (ui_player_who->map_object_->subsector_->sector->ceiling.image != sky_flat_image) // is it outdoors?
        lua_pushboolean(L, 0);
    else
        lua_pushboolean(L, 1);

    lua_setfield(L, -2, "is_outside"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.type
    lua_pushinteger(L, ui_player_who->map_object_->subsector_->sector->properties.type);
    lua_setfield(L, -2, "type"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.airless
    lua_pushboolean(L, ui_player_who->airless_ ? 1 : 0);
    lua_setfield(L, -2, "is_airless"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.swimmable
    lua_pushboolean(L, ui_player_who->swimming_ ? 1 : 0);
    lua_setfield(L, -2, "is_swimmable"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.gravity
    lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->properties.gravity);
    lua_setfield(L, -2, "gravity"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.friction
    lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->properties.friction);
    lua_setfield(L, -2, "friction"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.viscosity
    lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->properties.viscosity);
    lua_setfield(L, -2, "viscosity"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.drag
    lua_pushnumber(L, ui_player_who->map_object_->subsector_->sector->properties.drag);
    lua_setfield(L, -2, "drag"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.fogcolor
    HMM_Vec3  rgb;
    RGBAColor tempcolor = ui_player_who->map_object_->subsector_->sector->properties.fog_color;


    rgb.R = -1;
    rgb.G = -1;
    rgb.B = -1;
    if (tempcolor != 0)
    {
        if (tempcolor != kRGBANoValue)
        {
            rgb.R = epi::GetRGBARed(tempcolor);
            rgb.G = epi::GetRGBAGreen(tempcolor);
            rgb.B = epi::GetRGBABlue(tempcolor);
        }
    }

    LuaPushVector3(L, rgb);
    lua_setfield(L, -2, "fog_color"); // add to SECTOR Table
    //---------------

    //---------------
    // SECTOR.fogdensity

    // Convert to approximate percentage (a value between 0 and 100)
    float tempfogdensity = (ui_player_who->map_object_->subsector_->sector->properties.fog_density / 0.01f) * 100;
    tempfogdensity       = ceil(tempfogdensity);
    lua_pushinteger(L, (int)tempfogdensity);
    lua_setfield(L, -2, "fog_density"); // add to SECTOR Table
    //---------------

    return 1;
}

static const luaL_Reg playerlib[] = {{"num_players", PL_num_players},
                                     {"set_who", PL_set_who},
                                     {"is_bot", PL_is_bot},
                                     {"get_name", PL_get_name},
                                     {"get_pos", PL_get_pos},
                                     {"get_angle", PL_get_angle},
                                     {"get_mlook", PL_get_mlook},

                                     {"health", PL_health},
                                     {"armor", PL_armor},
                                     {"total_armor", PL_total_armor},
                                     {"ammo", PL_ammo},
                                     {"ammomax", PL_ammomax},
                                     {"frags", PL_frags},

                                     {"is_swimming", PL_is_swimming},
                                     {"is_jumping", PL_is_jumping},
                                     {"is_crouching", PL_is_crouching},
                                     {"is_using", PL_is_using},
                                     {"is_action1", PL_is_action1},
                                     {"is_action2", PL_is_action2},
                                     {"is_attacking", PL_is_attacking},
                                     {"is_rampaging", PL_is_rampaging},
                                     {"is_grinning", PL_is_grinning},

                                     {"under_water", PL_under_water},
                                     {"on_ground", PL_on_ground},
                                     {"move_speed", PL_move_speed},
                                     {"air_in_lungs", PL_air_in_lungs},

                                     {"has_key", PL_has_key},
                                     {"has_power", PL_has_power},
                                     {"power_left", PL_power_left},
                                     {"has_weapon", PL_has_weapon},
                                     {"has_weapon_slot", PL_has_weapon_slot},
                                     {"cur_weapon", PL_cur_weapon},
                                     {"cur_weapon_slot", PL_cur_weapon_slot},

                                     {"main_ammo", PL_main_ammo},
                                     {"ammo_type", PL_ammo_type},
                                     {"ammo_pershot", PL_ammo_pershot},
                                     {"clip_ammo", PL_clip_ammo},
                                     {"clip_size", PL_clip_size},
                                     {"clip_is_shared", PL_clip_is_shared},

                                     {"hurt_by", PL_hurt_by},
                                     {"hurt_mon", PL_hurt_mon},
                                     {"hurt_pain", PL_hurt_pain},
                                     {"hurt_dir", PL_hurt_dir},
                                     {"hurt_angle", PL_hurt_angle},

                                     {"kills", PL_kills},
                                     {"secrets", PL_secrets},
                                     {"items", PL_items},
                                     {"map_enemies", PL_map_enemies},
                                     {"map_secrets", PL_map_secrets},
                                     {"map_items", PL_map_items},
                                     {"floor_flat", PL_floor_flat},
                                     {"sector_tag", PL_sector_tag},

                                     {"play_footstep", PL_play_footstep},

                                     {"use_inventory", PL_use_inventory},
                                     {"inventory", PL_inventory},
                                     {"inventorymax", PL_inventorymax},

                                     {"rts_enable_tagged", PL_rts_enable_tagged},

                                     {"counter", PL_counter},
                                     {"counter_max", PL_counter_max},
                                     {"set_counter", PL_set_counter},

                                     {"query_object", PL_query_object},
                                     {"query_weapon", PL_query_weapon},
                                     {"is_zoomed", PL_is_zoomed},
                                     {"weapon_state", PL_weapon_state},

                                     {"sector_light", PL_sector_light},
                                     {"sector_floor_height", PL_sector_floor_height},
                                     {"sector_ceiling_height", PL_sector_ceiling_height},
                                     {"is_outside", PL_is_outside},
                                     {nullptr, nullptr}};

static int luaopen_player(lua_State *L)
{
    luaL_newlib(L, playerlib);
    return 1;
}

static const luaL_Reg mapobjectlib[] = {{"query_tagged", MO_query_tagged},
                                        {"tagged_info", MO_tagged_info},
                                        {"object_info", MO_object_info},
                                        {"weapon_info", MO_weapon_info},
                                        {"count", MO_count},
                                        {nullptr, nullptr}};

static int luaopen_mapobject(lua_State *L)
{
    luaL_newlib(L, mapobjectlib);
    return 1;
}

static const luaL_Reg gamelib[] = {{"info", Game_info}, {nullptr, nullptr}};

static int luaopen_game(lua_State *L)
{
    luaL_newlib(L, gamelib);
    return 1;
}

static const luaL_Reg maplib[] = {{"info", Map_info}, {nullptr, nullptr}};

static int luaopen_map(lua_State *L)
{
    luaL_newlib(L, maplib);
    return 1;
}

static const luaL_Reg sectorlib[] = {{"info", Sector_info}, {nullptr, nullptr}};

static int luaopen_sector(lua_State *L)
{
    luaL_newlib(L, sectorlib);
    return 1;
}

void LuaRegisterPlayerLibrary(lua_State *L)
{
    luaL_requiref(L, "_player", luaopen_player, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "_mapobject", luaopen_mapobject, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "_game", luaopen_game, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "_map", luaopen_map, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "_sector", luaopen_sector, 1);
    lua_pop(L, 1);
}
