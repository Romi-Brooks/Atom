# Lua Binding
add_library(Atom_Lua STATIC
        Engine/Atom/Lua/lua.hpp
        Engine/Atom/Lua/LuaContext.hpp
        Engine/Atom/Lua/LuaGuard.hpp
        Engine/Atom/Lua/BindingRegistry.hpp
        Engine/Atom/Lua/LuaHost.hpp
        Engine/Atom/Lua/LuaHost.cpp
        Engine/Atom/Lua/LuaBinding/ContextBridge.hpp
        Engine/Atom/Lua/LuaBinding/LuaMusic.cpp
        Engine/Atom/Lua/LuaBinding/LuaSFX.cpp
        Engine/Atom/Lua/LuaBinding/LuaVolume.cpp
        Engine/Atom/Lua/LuaBinding/LuaLog.cpp
)
target_include_directories(Atom_Lua
        PUBLIC "${PROJECT_SOURCE_DIR}/Engine"
        PRIVATE "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Lua PUBLIC
        Atom_3rd_Lua
        Atom_Log
        Atom_Media
        Atom_FS)
