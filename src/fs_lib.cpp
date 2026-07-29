#include "runtime.h"
#include <fstream>
#include <sstream>
#include <filesystem>

static int lua_fs_readFile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        luaL_error(L, "fs.readFile: could not open '%s'", path);
        return 0;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string contents = ss.str();
    lua_pushlstring(L, contents.data(), contents.size());
    return 1;
}

static int lua_fs_writeFile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len;
    const char* data = luaL_checklstring(L, 2, &len);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        luaL_error(L, "fs.writeFile: could not open '%s' for writing", path);
        return 0;
    }
    file.write(data, len);
    return 0;
}

static int lua_fs_exists  (lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, std::filesystem::exists(path));
    return 1;
}

void registerFsLibrary(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_fs_readFile, "readFile");
    lua_setfield(L, -2, "readFile");

    lua_pushcfunction(L, lua_fs_writeFile, "writeFile");
    lua_setfield(L, -2, "writeFile");

    lua_pushcfunction(L, lua_fs_exists, "exists");
    lua_setfield(L, -2, "exists");

    lua_setglobal(L, "fs");
}