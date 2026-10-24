// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once
#include "LuaBackend.h"

namespace UnLuaNova
{
    namespace UELib
    {
        int Open(lua_State* L);

        void SetTableForClass(lua_State* L, const char* Name);

        /**
         * The UE namespace has to stay writable for the binding layer: types are registered
         * lazily, on first access, with lua_rawset. luaL_sandbox marks every table hanging
         * off the globals read-only, which would freeze it mid-flight. Scripts are still
         * stopped - by __newindex - so handing write access back costs nothing.
         */
        void SetNamespaceWritable(lua_State* L, bool bWritable);
    }
}
