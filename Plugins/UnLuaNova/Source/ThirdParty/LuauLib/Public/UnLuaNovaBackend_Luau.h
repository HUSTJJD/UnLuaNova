// The ONLY entry point to the Luau backend's headers.
//
// Both backends ship files called lua.h / lualib.h, so an `#include "lua.h"` is decided
// by include-path order - and plain Lua's directory is added as a plain /I while Luau's
// are /external:I (searched last), so plain Lua wins whenever both modules end up in the
// build. That mismatch compiles far enough to bury the cause under dozens of unrelated
// errors.
//
// Two things make that impossible from here:
//   1. this file's name is unique to the backend, so it can never be satisfied by the
//      other one;
//   2. the includes below are relative to *this file*, so they can only ever reach
//      ThirdParty/LuauLib/Luau, whatever else is on the include path. ('..' is LuauLib,
//      so the submodule is '../Luau'. If these directories are ever moved, the
//      __has_include check below says so instead of a bare C1083.)
//
// Include this instead of "lua.h" / "lualib.h" / "luacode.h". Better still, include
// LuaBackend.h.

#pragma once

#if defined(__has_include)
#if !__has_include("../Luau/VM/include/lua.h")
#error "UnLuaNovaBackend_Luau.h cannot find Luau's lua.h at ../Luau/VM/include/lua.h. Either the Luau submodule is not checked out (git submodule update --init --recursive) or this wrapper moved relative to it."
#endif
#endif

// Luau's public headers already guard themselves with extern "C".
//
// One name has to be renamed on the way in. Luau's luaL_Strbuf forward-declares
// `struct TString` in the *global* namespace (VM/include/lualib.h), and Unreal has a
// global `TString<CharType>` too (Core/Public/Misc/ExpressionParserTypes.h). Whichever
// the compiler sees first wins and the other one stops compiling: a translation unit
// that reaches Luau before Core's header dies with C2059 on
// `TString<CharType> GetString() const`. LuaProtobufModule.cpp hit exactly that.
//
// The member is an opaque pointer that nothing outside Luau ever names, so renaming it
// changes no layout and no ABI - and Luau's own translation units are unaffected,
// because they include ../Luau/VM/src/*.cpp directly and never see this wrapper.
#define TString UnLuaNovaLuauTString
#include "../Luau/VM/include/lua.h"
#include "../Luau/VM/include/lualib.h"
#include "../Luau/Compiler/include/luacode.h"
// The optional JIT. luacodegen.h is tiny - three functions - and it is what lets a host
// hand the hottest modules to the native compiler. luau_codegen_supported() answers 0
// wherever there is no backend for the target (x64 and arm64 only), so including it is
// not the same as having it.
#include "../Luau/CodeGen/include/luacodegen.h"
#undef TString
