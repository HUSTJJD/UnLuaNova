// Single translation unit that compiles the whole official Lua library.
//
// The real sources live in the ../Lua submodule. Upstream ships onelua.c, the official
// aggregation of every core + stdlib file, so including it here builds Lua on every UE
// target with no CMake / make / NDK toolchain in the loop - UBT compiles it like any
// other module source.
//
// MAKE_LIB keeps the stand-alone interpreter (lua.c) and compiler (luac.c) out, so this
// produces a pure library with no main(). ltests.c is only pulled in under LUA_DEBUG,
// which we never define.

#define MAKE_LIB

#include "../Lua/onelua.c"
