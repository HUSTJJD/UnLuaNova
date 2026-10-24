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

#include "CoreMinimal.h"
#include "LuaBackend.h"

namespace UnLuaNova
{
    class FLuaEnv;

    // Exported: the specs live in their own DLL (UnLuaNovaTestSuite) and drive the check
    // from C++ - MakeGuard(), FGuard's destructor and Enabled all have to be importable.
    class UNLUANOVA_API FDanglingCheck
    {
    public:
        static bool Enabled;

        // Exported in its own right: MSVC does not inherit the outer class's dllinterface
        // into a nested class, so without this the guard's ctor/dtor stay private to
        // UnLuaNova.dll and TUniquePtr<FGuard> in a consumer module cannot link.
        class UNLUANOVA_API FGuard final
        {
        public:
            explicit FGuard(FDanglingCheck* Owner);

            ~FGuard();

        private:
            FDanglingCheck* Owner;
        };

        explicit FDanglingCheck(FLuaEnv* Env);

        TUniquePtr<FGuard> MakeGuard();

        void CaptureStruct(lua_State* L, void* Value);

        void CaptureContainer(lua_State* L, void* Value);

    private:
        FLuaEnv* Env;
        int32 GuardCount;
        TSet<void*> CapturedStructs;
        TSet<void*> CapturedContainers;
    };
}
