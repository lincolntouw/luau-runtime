#include "runtime.h"
#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

#include <fstream>
#include <sstream>
#include <iostream>

static std::string readFileToString(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("could not open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}
     
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <script.luau>\n";
        return 1;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);   

    // import other libs
    registerTaskLibrary(L);     
    registerFsLibrary(L);   

    std::string source;
    try {
        source = readFileToString(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // compile
    std::string bytecode = Luau::compile(source);
    if (luau_load(L, argv[1], bytecode.data(), bytecode.size(), 0) != 0) {
        std::cerr << "load error: " << lua_tostring(L, -1) << "\n";
        return 1;
    }

    // run top lvl chunk as first scheduled thread
    int status = lua_pcall(L, 0, 0, 0);
    if (status != 0) {
        std::cerr << "runtime error: " << lua_tostring(L, -1) << "\n";
        return 1;
    }
     
    Scheduler::instance().run(L);

    lua_close(L);
    return 0;
}
