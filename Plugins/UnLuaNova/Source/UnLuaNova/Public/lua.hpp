// Tencent is pleased to support the open source community by making UnLua available.
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

// Third-party Lua bindings (LuaSocket, LuaProtobuf, LuaRapidjson) include "lua.hpp"
// expecting plain Lua's headers. Plain Lua and Luau both ship lua.h/lualib.h, so that
// include used to be answered by whichever backend directory happened to come first on
// the include path - which is not necessarily the one being built.
//
// Route it through LuaBackend.h instead: that is the one place that knows which backend
// is selected, and it reaches the backend's headers by path rather than by name. The
// extern "C" wrapping lives in UnLuaNovaBackend_Lua.h (Luau's headers self-guard).
#include "LuaBackend.h"
