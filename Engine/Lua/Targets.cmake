# Lua Binding
add_library(Atom_Lua STATIC
        Engine/Lua/lua.hpp
        Engine/Lua/LuaContext.hpp
        Engine/Lua/LuaGuard.hpp
        Engine/Lua/BindingRegistry.hpp
        Engine/Lua/LuaHost.hpp
        Engine/Lua/LuaHost.cpp
        Engine/Lua/LuaBinding/ContextBridge.hpp
        Engine/Lua/LuaBinding/LuaMusic.cpp
        Engine/Lua/LuaBinding/LuaSFX.cpp
        Engine/Lua/LuaBinding/LuaVolume.cpp
        Engine/Lua/LuaBinding/LuaLog.cpp
)
target_include_directories(Atom_Lua
        PUBLIC "${PROJECT_SOURCE_DIR}/Engine"
        PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Lua")
target_link_libraries(Atom_Lua PUBLIC
        Atom_3rd_Lua
        Atom_Log
        Atom_Media
        Atom_FS)
