// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

struct lua_State;

namespace UnLuaNova
{
    /**
     * An `io` library, for the backend that does not have one.
     *
     * Luau removes io on purpose - see luau.org/sandbox: it gives access to files and lets
     * a script run processes, so a VM meant to execute untrusted code cannot expose it.
     * That reasoning is about *untrusted* code. A game's own scripts are trusted, and a
     * scripting layer that cannot read a data file it shipped is missing a tool projects
     * reach for.
     *
     * So this is not "put io back as it was", it is io with the part that made it unsafe
     * replaced by a decision the project makes:
     *
     *   * opt-in        - `lua.io` (default 0); nothing is installed unless asked for.
     *   * rooted        - every path is resolved under `lua.ioroot` (default: the project
     *                     directory). Absolute paths and `..` escapes are rejected, so a
     *                     script cannot reach outside it even by mistake.
     *   * no processes  - there is no io.popen, no os.execute, nothing that runs anything.
     *   * no stdin      - io.read() has no console to read from; it raises.
     *
     * Only installed when the backend has no `io` already: plain Lua keeps its own.
     */
    void OpenIoLibrary(lua_State* L);
}
