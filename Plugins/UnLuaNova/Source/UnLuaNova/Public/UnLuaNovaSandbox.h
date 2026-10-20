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
    void RestoreTablesAfterSandbox(lua_State* L);
}
