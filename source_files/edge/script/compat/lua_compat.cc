

#include "file.h"
#include "w_wad.h"
#include "lua_compat.h"


bool VM_GetCoalDetected();

lua_State *global_lua_state = nullptr;

struct pending_lua_script_c
{
    std::string data   = "";
    std::string source = "";
};

static std::vector<pending_lua_script_c> pending_scripts;

void LUA_Init()
{
    SYS_ASSERT(!global_lua_state);
    global_lua_state = LUA_CreateVM();

    LUA_RegisterCoreLibraries(global_lua_state);
    LUA_RegisterHudLibrary(global_lua_state);
    LUA_RegisterPlayerLibrary(global_lua_state);
    
}

void LUA_AddScript(const std::string &data, const std::string &source)
{
    pending_scripts.push_back(pending_lua_script_c{data, source});
}

void LUA_LoadScripts()
{
    if (LUA_GetLuaHudDetected() && VM_GetCoalDetected())
    {
        I_Warning("Lua and COAL huds detected, selecting Lua hud\n");
    }

    int top = lua_gettop(global_lua_state);
    for (auto &info : pending_scripts)
    {
        I_Printf("Compiling: %s\n", info.source.c_str());
        
        int results = LUA_DoFile(global_lua_state, info.source.c_str(), info.data.c_str());
        if (results)
        {
            lua_pop(global_lua_state, results);
        }        
    }

    if (W_IsLumpInPwad("STBAR"))
    {
        LUA_SetBoolean(global_lua_state, "hud", "custom_stbar", true);
    }

    SYS_ASSERT(lua_gettop(global_lua_state) == top);

}

lua_State* LUA_GetGlobalVM()
{
    return global_lua_state;
}


static bool lua_detected = false;
void LUA_SetLuaHudDetected(bool detected)
{
    // check whether redundant call, once enabled stays enabled
    if (lua_detected)
    {
        return;
    }

    lua_detected = detected;    
}

bool LUA_GetLuaHudDetected()
{
    return lua_detected;
}

bool LUA_UseLuaHud()
{
    return lua_detected || !VM_GetCoalDetected();
}