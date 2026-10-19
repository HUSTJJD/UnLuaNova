// Tencent is pleased to support the open source community by making UNLUANOVA available.
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

#include "CoreUObject.h"
#include "UNLUANOVABase.h"

#if NO_LOGGING
    #define UNLUANOVA_LOG(L, CategoryName, Verbosity, Format, ...)
    #define UNLUANOVA_LOGWARNING(L, CategoryName, Verbosity, Format, ...)
    #define UNLUANOVA_LOGERROR(L, CategoryName, Verbosity, Format, ...)
#else
#define UNLUANOVA_LOG(L, CategoryName, Verbosity, Format, ...) \
    {\
    	FString LogMsg = FString::Printf(Format, ##__VA_ARGS__);\
        luaL_traceback(L, L, "", 0); \
        UE_LOG(LogUNLUANOVA, Log, TEXT("%s%s"),*LogMsg,UTF8_TO_TCHAR(lua_tostring(L,-1))); \
        lua_pop(L,1); \
    }

#define UNLUANOVA_LOGWARNING(L, CategoryName, Verbosity, Format, ...) \
    {\
    	FString LogMsg = FString::Printf(Format, ##__VA_ARGS__);\
        luaL_traceback(L, L, "", 0); \
        UE_LOG(LogUNLUANOVA, Warning, TEXT("%s%s"),*LogMsg,UTF8_TO_TCHAR(lua_tostring(L,-1))); \
        lua_pop(L,1); \
    }

#define UNLUANOVA_LOGERROR(L, CategoryName, Verbosity, Format, ...) \
    {\
    	FString LogMsg = FString::Printf(Format, ##__VA_ARGS__);\
        luaL_traceback(L, L, "", 0); \
        UE_LOG(LogUNLUANOVA, Error, TEXT("%s%s"),*LogMsg,UTF8_TO_TCHAR(lua_tostring(L,-1))); \
        lua_pop(L,1); \
    }
#endif

#if STATS
DECLARE_STATS_GROUP(TEXT("UNLUANOVA"), STATGROUP_UnLua, STATCAT_Advanced);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Lua Memory"), STAT_UnLua_Lua_Memory, STATGROUP_UnLua, /*UNLUANOVA_API*/);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Persistent Parameter Buffer Memory"), STAT_UnLua_PersistentParamBuffer_Memory, STATGROUP_UnLua, /*UNLUANOVA_API*/);
DECLARE_MEMORY_STAT_EXTERN(TEXT("OutParmRec Memory"), STAT_UnLua_OutParmRec_Memory, STATGROUP_UnLua, /*UNLUANOVA_API*/);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Container Element Cache Memory"), STAT_UnLua_ContainerElementCache_Memory, STATGROUP_UnLua, /*UNLUANOVA_API*/);

#define UNLUANOVA_DEFINE_STAT(Name) \
    DEFINE_STAT(STAT_UnLua_##Name);

#define UNLUANOVA_STAT_MEMORY_ALLOC(Pointer, CounterName) \
    const auto _AllocedSize = FMemory::GetAllocSize(Pointer); \
    INC_MEMORY_STAT_BY(STAT_UnLua_##CounterName##_Memory, _AllocedSize);

#define UNLUANOVA_STAT_MEMORY_FREE(PointerName, CounterName) \
    const auto _FreedSize = FMemory::GetAllocSize(PointerName); \
    DEC_MEMORY_STAT_BY(STAT_UnLua_##CounterName##_Memory, _FreedSize);

#define UNLUANOVA_STAT_MEMORY_REALLOC(Pointer, NewPointer, CounterName) \
    struct FReallocGuard { \
        uint32 OldSize; \
        void* Ptr; \
        void** NewPtr; \
        ~FReallocGuard() { \
            const auto NewSize = FMemory::GetAllocSize(*NewPtr); \
            if (NewSize > OldSize) \
                INC_MEMORY_STAT_BY(STAT_UnLua_##CounterName##_Memory, NewSize - OldSize) \
            else \
                DEC_MEMORY_STAT_BY(STAT_UnLua_##CounterName##_Memory, OldSize - NewSize) \
        } \
    } _ReallocGuard; \
    _ReallocGuard.OldSize = FMemory::GetAllocSize(Pointer); \
    _ReallocGuard.Ptr = Pointer; \
    _ReallocGuard.NewPtr = &NewPointer; \

#define UNLUANOVA_DECLARE_CYCLE_STAT(FriendlyName, StatName) \
    DECLARE_CYCLE_STAT(TEXT(FriendlyName), STAT_##StatName, STATGROUP_UnLua)

#define UNLUANOVA_SCOPE_CYCLE_COUNTER(StatName) \
    SCOPE_CYCLE_COUNTER(STAT_##StatName)

#else

#define UNLUANOVA_DEFINE_STAT(Name)

#define UNLUANOVA_STAT_MEMORY_ALLOC(Pointer, CounterName)
#define UNLUANOVA_STAT_MEMORY_FREE(PointerName, CounterName)
#define UNLUANOVA_STAT_MEMORY_REALLOC(Pointer, NewPointer, CounterName)

#define UNLUANOVA_DECLARE_CYCLE_STAT(FriendlyName, StatName)
#define UNLUANOVA_SCOPE_CYCLE_COUNTER(StatName)

#endif

UNLUANOVA_API extern FString GLuaSrcRelativePath;
UNLUANOVA_API extern FString GLuaSrcFullPath;
