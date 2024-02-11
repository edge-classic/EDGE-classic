#pragma once

#include "epi.h"
#include "con_var.h"
#include "lua.hpp"
#include "HandmadeMath.h"
lua_State *LUA_CreateVM();

void LUA_Init();
void LUA_AddScript(const std::string &data, const std::string &source);
void LUA_LoadScripts();

// Do a file, returns the number of return values on stack
int LUA_DoFile(lua_State *L, const char* filename, const char *source);
void LUA_CallGlobalFunction(lua_State *L, const char *function_name);

// Game
void LUA_NewGame(void);
void LUA_LoadGame(void);
void LUA_SaveGame(void);
void LUA_BeginLevel(void);
void LUA_EndLevel(void);

// Core
void LUA_RegisterCoreLibraries(lua_State* L);

// Player
void LUA_RegisterPlayerLibrary(lua_State *L);

// Hud
void LUA_RunHud(void);
void LUA_RegisterHudLibrary(lua_State *L);

// VM

lua_State* LUA_GetGlobalVM();

inline HMM_Vec3 LUA_CheckVector3(lua_State *L, int index)
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

inline void LUA_PushVector3(lua_State*L, HMM_Vec3 v)
{
    lua_getglobal(L, "vec3");
    lua_pushnumber(L, v.X);
    lua_pushnumber(L, v.Y);
    lua_pushnumber(L, v.Z);
    lua_call(L, 3, 1);    
}

inline void LUA_SetVector3(lua_State* L, const char* module, const char* variable, HMM_Vec3 v)
{
    lua_getglobal(L, module);
    LUA_PushVector3(L, v);    
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

inline float LUA_GetFloat(lua_State* L, const char* module, const char* variable)
{
    lua_getglobal(L, module);
    lua_getfield(L, -1, variable);
    float ret = (float) lua_tonumber(L, -1);
    lua_pop(L, 2);
    return ret;
}

inline void LUA_SetFloat(lua_State* L, const char* module, const char* variable, float value)
{
    lua_getglobal(L, module);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

inline void LUA_SetBoolean(lua_State* L, const char* module, const char* variable, bool value)
{
    lua_getglobal(L, module);
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, variable);
    lua_pop(L, 1);
}

// Detects LUA in a pwad or epk
bool LUA_GetLuaHudDetected();
void LUA_SetLuaHudDetected(bool detected);
bool LUA_UseLuaHud();


extern lua_State *global_lua_state;