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

using UnrealBuildTool;

public class UnLuaNovaDemo : ModuleRules
{
    public UnLuaNovaDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
            });

        // Do NOT list a Lua backend here. The backend is a compile-time choice owned by
        // UnLuaNova (Config/DefaultUnLuaNovaEditor.ini -> LuaBackend), and UnLuaNova
        // re-exports whichever one it picked as a *public* dependency, so it arrives here
        // on its own.
        //
        // Naming "Lua" (or "Luau") here puts the other backend on the include path too.
        // Both ship lua.h / lualib.h under those names, and plain Lua's directory is added
        // as a plain /I while Luau's are /external:I (searched last), so plain Lua's lua.h
        // wins #include "lua.h" even when UNLUANOVA_LUA_BACKEND says Luau. That is how the
        // game module once compiled Luau call sites against Lua's headers: luaL_Reg
        // undefined, lua_pushcclosurek missing, lua_rawlen returning the wrong type.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnLuaNova",
        });
    }
}
