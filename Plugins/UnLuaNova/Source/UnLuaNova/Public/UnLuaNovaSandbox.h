// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under
// the License.

#pragma once
#include "LuaBackend.h"

namespace UnLuaNova
{
    /**
     * Undo the part of luaL_sandbox that UnLuaNova cannot live with.
     *
     * Sandboxing marks every table hanging off the globals read-only. That is right for the
     * globals themselves and wrong for the tables UnLuaNova keeps writing to after startup:
     *
     *   * UE.<Type>      - types are registered lazily, on first access
     *   * Lua.<Module>   - the __index fallback caches what it just required
     *   * package.loaded - written by every require
     *
     * Freezing those hardens nothing a script can legitimately reach; it turns type
     * registration and require into errors. Call this immediately after UNLUANOVA_SANDBOX.
     *
     * A no-op on backends without read-only tables (plain Lua), so it is safe to call
     * unconditionally - but it only ever matters when UNLUANOVA_HAS_SANDBOX is 1.
     */
    UNLUANOVA_API void RestoreTablesAfterSandbox(lua_State* L);

    /**
     * Sandbox every coroutine as it is created, by claiming Luau's userthread callback.
     *
     * luaL_sandbox hardens the *global* table, which is one table shared by the main
     * thread and every coroutine in it. Luau's own guidance is stronger: a thread gets its
     * own global table that proxies reads to the builtin one, so code running in one
     * thread can neither see nor clobber another thread's globals. luaL_sandboxthread is
     * how that is done, and the only place to apply it is the VM's userthread callback -
     * the plugin never calls lua_newthread, every coroutine comes out of
     * coroutine.create.
     *
     * It is also a performance switch, not only a security one: Luau's "imports" and
     * fastcall optimizations are only valid for functions whose environment is known to be
     * pure, and lua_setsafeenv is what marks it pure. A thread that skips this keeps an
     * env the VM must assume can change underneath it.
     *
     * Opt-in (lua.sandboxthread), because the isolation is observable: a global written
     * inside a sandboxed coroutine lands in that coroutine's own table and is invisible
     * everywhere else. Reads still fall through to the real globals, so this only bites
     * code that *writes* globals from a coroutine and expects to read them back outside.
     *
     * No-op where UNLUANOVA_HAS_SANDBOX or UNLUANOVA_HAS_THREAD_CALLBACK is 0.
     */
    UNLUANOVA_API void InstallSandboxThreadHardening(lua_State* L);
}
