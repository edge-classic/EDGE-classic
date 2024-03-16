

#include "e_player.h"
#include "lua_compat.h"

extern Player *ui_hud_who;

extern Player *ui_player_who;

void LuaNewGame(void)
{
    LuaCallGlobalFunction(LuaGetGlobalVM(), "new_game");
}

void LuaLoadGame(void)
{
    // Need to set these to prevent nullptr references if using any player.xxx
    // in the load_level hook
    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];

    LuaCallGlobalFunction(LuaGetGlobalVM(), "load_game");
}

void LuaSaveGame(void)
{
    LuaCallGlobalFunction(LuaGetGlobalVM(), "save_game");
}

void LuaBeginLevel(void)
{
    // Need to set these to prevent nullptr references if using player.xxx in
    // the begin_level hook
    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];
    LuaCallGlobalFunction(LuaGetGlobalVM(), "begin_level");
}

void LuaEndLevel(void)
{
    LuaCallGlobalFunction(LuaGetGlobalVM(), "end_level");
}
