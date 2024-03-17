

#include "ddf_main.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_windows.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "lua_compat.h"
#include "m_random.h"
#include "r_modes.h"
#include "version.h"
#include "vm_coal.h"
#include "w_wad.h"

//------------------------------------------------------------------------
//  SYSTEM MODULE
//------------------------------------------------------------------------

// sys.error(str)
//
static int SYS_error(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    FatalError("%s\n", s);
    return 0;
}

// sys.print(str)
//
static int SYS_print(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    LogPrint("%s\n", s);
    return 0;
}

// sys.debug_print(str)
//
static int SYS_debug_print(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    LogDebug("%s\n", s);
    return 0;
}

// sys.edge_version()
//
static int SYS_edge_version(lua_State *L)
{
    lua_pushnumber(L, edge_version.f_);
    return 1;
}

#ifdef WIN32
static bool console_allocated = false;
#endif
static int SYS_AllocConsole(lua_State *L)
{
#ifdef WIN32
    if (console_allocated)
    {
        return 0;
    }

    console_allocated = true;
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    return 0;
}

//------------------------------------------------------------------------
//  MATH EXTENSIONS
//------------------------------------------------------------------------

// math.rint(val)
static int MATH_rint(lua_State *L)
{
    double val = luaL_checknumber(L, 1);
    lua_pushinteger(L, RoundToInteger(val));
    return 1;
}

// SYSTEM
static const luaL_Reg syslib[] = {{"error", SYS_error},
                                  {"print", SYS_print},
                                  {"debug_print", SYS_debug_print},
                                  {"edge_version", SYS_edge_version},
                                  {"allocate_console", SYS_AllocConsole},
                                  {nullptr, nullptr}};

static int luaopen_sys(lua_State *L)
{
    luaL_newlib(L, syslib);
    return 1;
}

const luaL_Reg loadlibs[] = {{"sys", luaopen_sys}, {nullptr, nullptr}};

void LuaRegisterCoreLibraries(lua_State *L)
{
    const luaL_Reg *lib;
    /* "require" functions from 'loadedlibs' and set results to global table */
    for (lib = loadlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); /* remove lib */
    }

    // add rint to math
    lua_getglobal(L, "math");
    lua_pushcfunction(L, MATH_rint);
    lua_setfield(L, -2, "rint");
    lua_pop(L, 1);
}