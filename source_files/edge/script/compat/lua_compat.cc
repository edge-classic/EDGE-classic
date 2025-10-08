

#include "lua_compat.h"

#include "epi_file.h"
#include "w_wad.h"

#ifdef EDGE_CLASSIC
extern bool GetCOALDetected();
#endif

lua_State *global_lua_state = nullptr;

struct pending_lua_script_c
{
    std::string data   = "";
    std::string source = "";
};

static std::vector<pending_lua_script_c> pending_scripts;

void LuaInit()
{
    EPI_ASSERT(!global_lua_state);
    global_lua_state = LuaCreateVM();

    LuaRegisterCoreLibraries(global_lua_state);
    LuaRegisterHUDLibrary(global_lua_state);
    LuaRegisterPlayerLibrary(global_lua_state);
}

void LuaAddScript(const std::string &data, const std::string &source)
{
    pending_scripts.push_back(pending_lua_script_c{data, source});
}

void LuaLoadScripts()
{
#ifdef EDGE_CLASSIC
    if (LuaGetLuaHUDDetected() && GetCOALDetected())
    {
        LogWarning("Lua and COAL huds detected, selecting Lua hud\n");
    }
#endif
    int top = lua_gettop(global_lua_state);
    for (auto &info : pending_scripts)
    {
        LogPrint("Compiling: %s\n", info.source.c_str());

        int results = LuaDoFile(global_lua_state, info.source.c_str(), info.data.c_str());
        if (results)
        {
            lua_pop(global_lua_state, results);
        }
    }

    if (IsLumpInPwad("STBAR"))
    {
        LuaSetBoolean(global_lua_state, "hud", "custom_stbar", true);
    }

    EPI_ASSERT(lua_gettop(global_lua_state) == top);
}

lua_State *LuaGetGlobalVM()
{
    return global_lua_state;
}
#ifdef EDGE_CLASSIC
static bool lua_detected = false;
void        LuaSetLuaHUDDetected(bool detected)
{
    lua_detected = detected;
}

bool LuaGetLuaHUDDetected()
{
    return lua_detected;
}

bool LuaUseLuaHUD()
{
    return lua_detected || !GetCOALDetected();
}
#endif