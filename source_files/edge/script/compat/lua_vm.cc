
#include <algorithm>

#include "../lua_debugger.h"
#include "con_var.h"
#include "edge_profiling.h"
#include "i_system.h"
#include "lua_compat.h"
#include "str_util.h"
#include "w_files.h"

// Enable Lua debugging
EDGE_DEFINE_CONSOLE_VARIABLE(lua_debug, "0", kConsoleVariableFlagReadOnly)

static void LuaError(const char *msg, const char *luaerror)
{
    std::string error(luaerror);
    std::replace(error.begin(), error.end(), '\t', '>');

    FatalError((msg + error).c_str());
}

static void LuaGetRequirePackPath(const char *name, std::string &out)
{
    std::string require_name(name);
    std::replace(require_name.begin(), require_name.end(), '.', '/');
    out = epi::StringFormat("scripts/lua/%s.lua", require_name.c_str());
}

static int LuaPackLoader(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    std::string pack_name;
    LuaGetRequirePackPath(name, pack_name);

    epi::File *file = OpenFileFromPack(pack_name);

    if (!file)
    {
        FatalError("LUA: %s.lua: NOT FOUND\n", name);
        return 0;
    }

    std::string source = file->ReadText();

    delete file;

    int results = LuaDoFile(L, pack_name.c_str(), source.c_str());
    return results;
}

static int LuaPackSearcher(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    std::string pack_name;
    LuaGetRequirePackPath(name, pack_name);

    if (CheckPackFilesForName(pack_name) == -1)
    {
        FatalError("LUA: Unable to load file %s", pack_name.c_str());
        return 0;
    }

    lua_pushcfunction(L, LuaPackLoader);
    lua_pushstring(L, name);
    return 2;
}

static int LuaMsgHandler(lua_State *L)
{
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr)
    {                                            /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
            return 1;                            /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    return 1;                     /* return the traceback */
}

int LuaDoFile(lua_State *L, const char *filename, const char *source)
{
    if (lua_debug.d_)
    {
        lua_getglobal(L, "__ec_debugger_source");
        lua_getfield(L, -1, filename);
        if (lua_isstring(L, -1))
        {
            LogWarning("LUA: Redundant execution of %s", filename);
            lua_pop(L, 2);
            return 0;
        }
        lua_pop(L, 1);
        lua_pushstring(L, source);
        lua_setfield(L, -2, filename);
        lua_pop(L, 1);
    }
    int top    = lua_gettop(L);
    int status = luaL_loadbuffer(L, source, strlen(source), (std::string("@") + filename).c_str());

    if (status != LUA_OK)
    {
        LuaError(epi::StringFormat("LUA: Error compiling %s\n", filename ? filename : "???").c_str(),
                 lua_tostring(L, -1));
    }

    if (lua_debug.d_)
    {
        status = dbg_pcall(L, 0, LUA_MULTRET, 0);
    }
    else
    {
        int base = lua_gettop(L);            // function index
        lua_pushcfunction(L, LuaMsgHandler); // push message handler */
        lua_insert(L, base);                 // put it under function and args */
        status = lua_pcall(L, 0, LUA_MULTRET, base);
        lua_remove(L, base);
    }

    if (status != LUA_OK)
    {
        LuaError(epi::StringFormat("LUA: Error in %s\n", filename ? filename : "???").c_str(), lua_tostring(L, -1));
    }

    return lua_gettop(L) - top;
}

// NOP dbg() for when debugger is disabled and someone has left some breakpoints
// in code
static bool dbg_nop_warn = false;
static int  LuaDbgNOP(lua_State *L)
{
    if (!dbg_nop_warn)
    {
        dbg_nop_warn = true;
        LogWarning("LUA: dbg() called without lua_debug being set.  Please check that "
                   "a stray dbg call didn't get left "
                   "in source.");
    }
    return 0;
}

void LuaCallGlobalFunction(lua_State *L, const char *function_name)
{
    EDGE_ZoneScoped;

    int top = lua_gettop(L);
    lua_getglobal(L, function_name);
    int status = 0;
    if (lua_debug.d_)
    {
        status = dbg_pcall(L, 0, 0, 0);
    }
    else
    {
        int base = lua_gettop(L);            // function index
        lua_pushcfunction(L, LuaMsgHandler); // push message handler */
        lua_insert(L, base);                 // put it under function and args */

        status = lua_pcall(L, 0, 0, base);
    }

    if (status != LUA_OK)
    {
        LuaError(epi::StringFormat("Error calling global function %s\n", function_name).c_str(), lua_tostring(L, -1));
    }

    lua_settop(L, top);
}

static int LuaSandbox_Warning(lua_State *L)
{
    const char *function_name = luaL_checkstring(L, lua_upvalueindex(1));

    LogWarning("LUA: Called sandbox disabled function %s\n", function_name);

    return 0;
}

static void LuaSandbox_Module(lua_State *L, const char *module_name, const char **functions)
{
    int i = 0;
    lua_getglobal(L, module_name);
    while (const char *function_name = functions[i++])
    {
        lua_pushfstring(L, "%s.%s", module_name, function_name);
        lua_pushcclosure(L, LuaSandbox_Warning, 1);
        lua_setfield(L, -2, function_name);
    }
    lua_pop(L, 1);
}

static void LuaSandbox(lua_State *L)
{
    // clear out search path and loadlib
    lua_getglobal(L, "package");
    lua_pushnil(L);
    lua_setfield(L, -2, "loadlib");
    lua_pushnil(L);
    lua_setfield(L, -2, "searchpath");
    // pop package off stack
    lua_pop(L, 1);

    // os module
    const char *os_functions[] = {"execute", "exit", "getenv", "remove", "rename", "setlocale", "tmpname", nullptr};
    LuaSandbox_Module(L, "os", os_functions);

    // base/global functions
    const char *base_functions[] = {"dofile", "loadfile", nullptr};
    LuaSandbox_Module(L, "_G", base_functions);

    // if debugging is enabled, load debug/io libs and sandbox
    if (lua_debug.d_)
    {
        // open the debug library and io libraries
        luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);
        luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
        lua_pop(L, 2);

        const char *io_functions[] = {"close", "input", "lines", "open", "output", "popen", "tmpfile", "type", nullptr};
        LuaSandbox_Module(L, "io", io_functions);
    }
}

lua_State *LuaCreateVM()
{
    // we could specify a lua allocator, which would be a good idea to hook up
    // to a debug allocator library for tracing l = lua_newstate(lua_Alloc
    // alloc, nullptr);

    lua_State *L = luaL_newstate();

    /*
    ** these libs are loaded by lua.c and are readily available to any Lua
    ** program
    */
    const luaL_Reg loadedlibs[] = {
        {LUA_GNAME, luaopen_base},          {LUA_LOADLIBNAME, luaopen_package}, {LUA_OSLIBNAME, luaopen_os},
        {LUA_COLIBNAME, luaopen_coroutine}, {LUA_TABLIBNAME, luaopen_table},    {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},    {LUA_UTF8LIBNAME, luaopen_utf8},    {nullptr, nullptr}};

    const luaL_Reg *lib;
    /* "require" functions from 'loadedlibs' and set results to global table */
    for (lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); /* remove lib */
    }

    // replace searchers with only preload and custom searcher
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");
    lua_newtable(L);
    lua_geti(L, -2, 1);
    lua_seti(L, -2, 1);
    lua_pushcfunction(L, LuaPackSearcher);
    lua_seti(L, -2, 2);
    lua_setfield(L, -3, "searchers");
    // pop package and searchers off stack
    lua_pop(L, 2);

    LuaSandbox(L);

    if (lua_debug.d_)
    {
        lua_newtable(L);
        lua_setglobal(L, "__ec_debugger_source");
        dbg_setup_default(L);
    }
    else
    {
        lua_pushcfunction(L, LuaDbgNOP);
        lua_setglobal(L, "dbg");
    }

    EPI_ASSERT(!lua_gettop(L));

    return L;
}
