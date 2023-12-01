
#include "i_defs.h"
#include "file.h"
#include "w_wad.h"
#include "lua_compat.h"


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
    for (auto &info : pending_scripts)
    {
        I_Printf("Compiling: %s\n", info.source.c_str());

        int top = lua_gettop(global_lua_state);
        LUA_DoString(global_lua_state, info.source.c_str(), info.data.c_str());
        lua_settop(global_lua_state, top);
    }

    if (W_IsLumpInPwad("STBAR"))
    {
        LUA_SetBoolean(global_lua_state, "hud", "custom_stbar", true);
    }

}

lua_State* LUA_GetGlobalVM()
{
    return global_lua_state;
}
