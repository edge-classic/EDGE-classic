#pragma once

#include "HandmadeMath.h"
#include "con_var.h"
#include "epi.h"
#include "lua.hpp"

lua_State *LuaCreateVM();

void LuaInit();
void LuaAddScript(const std::string &data, const std::string &source);
void LuaLoadScripts();

// Do a file, returns the number of return values on stack
int  LuaDoFile(lua_State *L, const char *filename, const char *source);
void LuaCallGlobalFunction(lua_State *L, const char *function_name);

// Game
void LuaNewGame(void);
void LuaLoadGame(void);
void LuaSaveGame(void);
void LuaBeginLevel(void);
void LuaEndLevel(void);

// Core
void LuaRegisterCoreLibraries(lua_State *L);

// Player
void LuaRegisterPlayerLibrary(lua_State *L);

// HUD
void LuaRunHUD(void);
void LuaRegisterHUDLibrary(lua_State *L);

// VM

lua_State *LuaGetGlobalVM();

inline HMM_Vec3 LuaCheckVector3(lua_State *L, int index)
{
    HMM_Vec3 v;

    luaL_checktype(L, index, LUA_TTABLE);

    lua_geti(L, index, 1);
    v.X = luaL_checknumber(L, -1);
    lua_geti(L, index, 2);
    v.Y = luaL_checknumber(L, -1);
    lua_geti(L, index, 3);
    v.Z = luaL_checknumber(L, -1);
    lua_pop(L, 3);
    return v;
}

inline void LuaPushVector3(lua_State *L, HMM_Vec3 v)
{
    lua_getglobal(L, "vec3");
    lua_pushnumber(L, v.X);
    lua_pushnumber(L, v.Y);
    lua_pushnumber(L, v.Z);
    lua_call(L, 3, 1);
}

inline void LuaSetVector3(lua_State *L, const char *module, const char *variable, HMM_Vec3 v)
{
    lua_getglobal(L, module);
    LuaPushVector3(L, v);
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

inline float LuaGetFloat(lua_State *L, const char *module, const char *variable)
{
    lua_getglobal(L, module);
    lua_getfield(L, -1, variable);
    float ret = (float)lua_tonumber(L, -1);
    lua_pop(L, 2);
    return ret;
}

inline void LuaSetFloat(lua_State *L, const char *module, const char *variable, float value)
{
    lua_getglobal(L, module);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

inline void LuaSetBoolean(lua_State *L, const char *module, const char *variable, bool value)
{
    lua_getglobal(L, module);
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

// Detects LUA in a pwad or epk
bool LuaGetLuaHUDDetected();
void LuaSetLuaHUDDetected(bool detected);
bool LuaUseLuaHUD();

extern lua_State *global_lua_state;