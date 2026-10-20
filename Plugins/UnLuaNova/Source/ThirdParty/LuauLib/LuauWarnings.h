// Copyright Epic Games, Inc. All Rights Reserved.
//
// Wrapper-local header: Luau.Build.cs force-includes it (/FI) into every Luau
// translation unit, which is the only way to silence diagnostics coming out of
// the submodule's own sources without editing them - Luau/ belongs to
// luau-lang/luau and is not ours to patch.
//
// Only what upstream Luau actually trips is listed here, so a real problem in
// this wrapper's own code still shows up.

#pragma once

// 4457 - declaration of 'x' hides function parameter.
//        CppCompileWarningSettings.ShadowVariableWarningLevel only covers
//        4456/4458/4459 on MSVC, so there is no UBT switch for this one.
// 4389 - '==' : signed/unsigned mismatch.
//        Same story: SignCompareWarningLevel maps to clang -Wsign-compare only.
// 4310 - cast truncates constant value (lstrlib.cpp).
// 4324 - structure was padded due to alignment specifier (lua_jmpbuf in ldo.cpp).
//        We force LUA_USE_LONGJMP=1, so that layout is the one we asked for.
// 4505 - unreferenced local function with internal linkage was removed.
// 4702 - unreachable code (ltable.cpp).
// 4611 - interaction between '_setjmp' and C++ object destruction is
//        non-portable. Same longjmp choice as above: it is deliberate.
// 4996 - strcpy/strcat/strncat/strncpy flagged as unsafe CRT. Luau.Build.cs
//        also defines _CRT_SECURE_NO_WARNINGS; this is belt and braces for
//        translation units that include <string.h> before any of that applies.
#pragma warning(disable: 4457 4389 4310 4324 4505 4702 4611 4996)
