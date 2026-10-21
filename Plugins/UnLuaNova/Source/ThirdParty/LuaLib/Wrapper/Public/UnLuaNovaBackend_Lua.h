// The ONLY entry point to the plain-Lua backend's headers.
//
// Both backends ship files called lua.h / lualib.h / lauxlib.h, so an
// `#include "lua.h"` is decided by include-path order - and plain Lua's directory is
// added as a plain /I while Luau's are /external:I (searched last), so plain Lua wins
// whenever both modules end up in the build. That mismatch compiles far enough to bury
// the cause under dozens of unrelated errors.
//
// Two things make that impossible from here:
//   1. this file's name is unique to the backend, so it can never be satisfied by the
//      other one;
//   2. the includes below are relative to *this file*, so they can only ever reach
//      ThirdParty/LuaLib/Lua, whatever else is on the include path. ('..' is LuaLib,
//      '../..' is LuaLib's parent, so the submodule is '../../Lua'. If these directories
//      are ever moved, the __has_include check below says so instead of a bare C1083.)
//
// Include this instead of "lua.h" / "lua.hpp". Better still, include LuaBackend.h.

#pragma once

#if defined(__has_include)
#if !__has_include("../../Lua/lua.h")
#error "UnLuaNovaBackend_Lua.h cannot find Lua's lua.h at ../../Lua/lua.h. Either the Lua submodule is not checked out (git submodule update --init --recursive) or this wrapper moved relative to it."
#endif
#endif

// Upstream plain Lua headers are C only; lua.hpp exists precisely to add this. Skipped
// for LUA_COMPILE_AS_CPP, where the library itself is built as C++ (C++ linkage).
#if defined(__cplusplus) && (!defined(LUA_COMPILE_AS_CPP) || !LUA_COMPILE_AS_CPP)
extern "C" {
#endif

#include "../../Lua/lua.h"
#include "../../Lua/lualib.h"
#include "../../Lua/lauxlib.h"

#if defined(__cplusplus) && (!defined(LUA_COMPILE_AS_CPP) || !LUA_COMPILE_AS_CPP)
}
#endif
