
#include "i_defs.h"
#include "e_player.h"
#include "lua_compat.h"


extern player_t *ui_hud_who;

extern player_t *ui_player_who;


void LUA_NewGame(void)
{
    LUA_CallGlobalFunction(LUA_GetGlobalVM(), "new_game");
}

void LUA_LoadGame(void)
{
    // Need to set these to prevent NULL references if using any player.xxx in the load_level hook
    ui_hud_who    = players[displayplayer];
    ui_player_who = players[displayplayer];

    LUA_CallGlobalFunction(LUA_GetGlobalVM(), "load_game");
}

void LUA_SaveGame(void)
{
    LUA_CallGlobalFunction(LUA_GetGlobalVM(), "save_game");
}

void LUA_BeginLevel(void)
{
    // Need to set these to prevent NULL references if using player.xxx in the begin_level hook
    ui_hud_who    = players[displayplayer];
    ui_player_who = players[displayplayer];
    LUA_CallGlobalFunction(LUA_GetGlobalVM(), "begin_level");
}

void LUA_EndLevel(void)
{
    LUA_CallGlobalFunction(LUA_GetGlobalVM(), "end_level");
}

