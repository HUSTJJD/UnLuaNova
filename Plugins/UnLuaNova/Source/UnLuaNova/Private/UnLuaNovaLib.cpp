#include "UnLuaNovaLib.h"
#include "LowLevel.h"
#include "LuaBackend.h"
#include "LuaCore.h"             // GetCppInstanceFast - containers are userdata with a descriptor header
#include "LuaEnv.h"
#include "LuaProfiler.h"        // FLuaProfiler::IsSelfTimeSampled() - the 'profilerselftime' feature
#include "UnLuaNovaBase.h"
#include "UnLuaNovaSandbox.h"
#include "UnLuaNovaSettings.h"   // 特性开关（bTimer / bEvent / bOOP / bDataDrivenBinding / bDebugAdapter）

#include "Containers/LuaArray.h"
#include "Registries/ContainerRegistry.h"
#include "Registries/PropertyRegistry.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace UnLuaNova
{
    namespace UnLuaNovaLib
    {
        static const char* PACKAGE_PATH_KEY = "PackagePath";

        static FString GetMessage(lua_State* L)
        {
            const auto ArgCount = lua_gettop(L);
            FString Message;
            if (!lua_checkstack(L, ArgCount))
            {
                luaL_error(L, "too many arguments, stack overflow");
                return Message;
            }
            for (int ArgIndex = 1; ArgIndex <= ArgCount; ArgIndex++)
            {
                if (ArgIndex > 1)
                    Message += "\t";
                Message += UTF8_TO_TCHAR(luaL_tolstring(L, ArgIndex, NULL));
            }
            return Message;
        }

        static int LogInfo(lua_State* L)
        {
            const auto Msg = GetMessage(L);
            UE_LOG(LogUnLuaNova, Log, TEXT("%s"), *Msg);
            return 0;
        }

        static int LogWarn(lua_State* L)
        {
            const auto Msg = GetMessage(L);
            UE_LOG(LogUnLuaNova, Warning, TEXT("%s"), *Msg);
            return 0;
        }

        static int LogError(lua_State* L)
        {
            const auto Msg = GetMessage(L);
            UE_LOG(LogUnLuaNova, Error, TEXT("%s"), *Msg);
            return 0;
        }

        static int HotReload(lua_State* L)
        {
#if UNLUANOVA_WITH_HOT_RELOAD
            if (luaL_dostring(L, "require('UnLuaNova.HotReload').reload()") != 0)
            {
                LogError(L);
            }
#endif
            return 0;
        }

        static int Ref(lua_State* L)
        {
            const auto Object = GetUObject(L, -1);
            if (!Object)
                return luaL_error(L, "invalid UObject");

            const auto& Env = FLuaEnv::FindEnvChecked(L);
            Env.GetObjectRegistry()->AddManualRef(L, Object);
            return 1;
        }

        static int Unref(lua_State* L)
        {
            const auto Object = GetUObject(L, -1);
            if (!Object)
                return luaL_error(L, "invalid UObject");

            const auto& Env = FLuaEnv::FindEnvChecked(L);
            Env.GetObjectRegistry()->RemoveManualRef(Object);
            return 0;
        }

        // Which backend this build is running on. Scripts use it to gate Luau-only syntax
        // (continue, compound assignment, type annotations) behind a runtime check, so one
        // file can live in a project that is built either way: the Luau-only modules are
        // only required when the answer is "Luau", and on Lua 5.5 they are never compiled.
        static int GetBackend(lua_State* L)
        {
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            lua_pushstring(L, "Luau");
#else
            lua_pushstring(L, "Lua");
#endif
            return 1;
        }

        // Ask for a capability instead of inferring it from the backend name: the whole
        // point of the capability flags in LuaBackend.h is that call sites branch on what
        // is actually available. Unknown names answer false rather than raising, so a
        // script written against a newer build degrades instead of dying.
        /**
         * 特性表：全插件**唯一**的开关清单。
         *
         * 统一格式，不管这个值从哪来：
         *   * 后端能力（sandbox / vector / buffer …）—— 由后端决定，不可配置
         *   * 配置开关（timer / event / oop / datadrivenbinding / debugadapter …）
         *     —— 读 UUnLuaNovaSettings 的 `b<CamelCase>`，见 UUnLuaNovaSettings 的 Features 段
         *
         * 对外只有一个入口：UnLuaNova.HasFeature('<名>')，名字大小写不敏感
         * （`oop` 与 `OOP` 同义，省掉"到底是大写还是小写"这种问题）。
         * 加一个特性 = 表里加一行 + 设置类里加一个 `b<Name>`，不再有别的地方。
         */
        struct FFeature
        {
            const char* Name;
            /** 每次查询都重新取值：设置可以在编辑器里改，不能缓存成常量。 */
            bool (*Get)();
        };

        /** ASCII 大小写不敏感比较：不引平台相关的 stricmp，两个后端/两个平台都同一个行为。 */
        static bool NameEquals(const char* A, const char* B)
        {
            for (; *A && *B; ++A, ++B)
            {
                const char CA = (*A >= 'A' && *A <= 'Z') ? static_cast<char>(*A + 32) : *A;
                const char CB = (*B >= 'A' && *B <= 'Z') ? static_cast<char>(*B + 32) : *B;
                if (CA != CB)
                    return false;
            }
            return *A == *B;
        }

        // --- 后端能力：编译期常量 ---
        static bool FeatureSandbox() { return UNLUANOVA_HAS_SANDBOX != 0; }
        static bool FeatureBreakpoint() { return UNLUANOVA_HAS_BREAKPOINT != 0; }
        static bool FeatureAllocCallbacks() { return UNLUANOVA_HAS_ALLOC_CALLBACKS != 0; }
        static bool FeatureVector() { return UNLUANOVA_HAS_NATIVE_VECTOR != 0; }
        static bool FeatureBuffer() { return UNLUANOVA_HAS_NATIVE_BUFFER != 0; }

        // --- 配置开关：读 UUnLuaNovaSettings ---
        static bool FeatureTimer()
        {
            // 编译期关掉的库不在这里注册（见 FLuaEnv），但值仍以宏为准，两边不会打架
            return UNLUANOVA_HAS_STDLIB_TIMER != 0 && GetDefault<UUnLuaNovaSettings>()->bTimer;
        }

        static bool FeatureEvent() { return UNLUANOVA_HAS_STDLIB_EVENT != 0 && GetDefault<UUnLuaNovaSettings>()->bEvent; }
        static bool FeatureOOP() { return UNLUANOVA_HAS_STDLIB_OOP != 0 && GetDefault<UUnLuaNovaSettings>()->bOOP; }
        static bool FeatureDataDrivenBinding() { return GetDefault<UUnLuaNovaSettings>()->bDataDrivenBinding; }
        static bool FeatureDebugAdapter() { return GetDefault<UUnLuaNovaSettings>()->bDebugAdapter; }

        /**
         * Profiler 的 self time 是"实测"还是"采样归因"。
         *
         * Lua 后端靠 call/return 配对量出真实区间；Luau 只有安全点，self time 是把两段安全点
         * 之间的时间归给当时在跑的函数 —— 能排序，不能当精确数值用。这两者回答的是不同的
         * 问题，而它能被问出来之前，看报告的人只能猜自己看的是哪一种。
         */
        static bool FeatureProfilerSelfTime() { return !UnLuaNova::FLuaProfiler::IsSelfTimeSampled(); }

        /**
         * userdata 会不会被终结（容器 / FText / 文件句柄这些带 C++ 析构的）。
         *
         * 两个后端现在都是 true —— 曾经 Luau 上是 false，那是泄漏不是缺特性。留着这条是因为
         * "会不会被回收"是个脚本不该猜的事：猜错了就是内存一直涨而没有报错。
         */
        static bool FeatureUserdataFinalizer() { return UNLUANOVA_HAS_USERDATA_FINALIZER != 0; }

        static const FFeature Features[] = {
            // 后端能力
            {"sandbox", FeatureSandbox},
            {"breakpoint", FeatureBreakpoint},
            {"allocCallbacks", FeatureAllocCallbacks},
            {"vector", FeatureVector},
            {"buffer", FeatureBuffer},
            {"profilerselftime", FeatureProfilerSelfTime},
            {"userdatafinalizer", FeatureUserdataFinalizer},
            // 配置开关（UUnLuaNovaSettings 的 b<Name>）
            {"timer", FeatureTimer},
            {"event", FeatureEvent},
            {"oop", FeatureOOP},
            {"datadrivenbinding", FeatureDataDrivenBinding},
            {"debugadapter", FeatureDebugAdapter},
        };

        static int HasFeature(lua_State* L)
        {
            const char* Name = lua_tostring(L, 1);
            bool bAvailable = false;
            if (Name)
            {
                for (const FFeature& Feature : Features)
                {
                    // 大小写不敏感：bOOP ↔ "oop" ↔ "OOP" 是同一个开关
                    if (NameEquals(Name, Feature.Name))
                    {
                        bAvailable = Feature.Get();
                        break;
                    }
                }
            }

            lua_pushboolean(L, bAvailable ? 1 : 0);
            return 1;
        }

#if UNLUANOVA_HAS_NATIVE_BUFFER
        /**
         * TArray -> buffer, in one pass instead of one VM call per element.
         *
         * The interesting case is a 1-byte element type (TArray<uint8>, which is what a
         * byte-array UPROPERTY binds to): the array's storage *is* the byte sequence, so
         * the whole conversion is a single memcpy. Anything wider has to go element by
         * element, but still inside C++ - the Lua version paid a call into the VM per byte.
         *
         * "Zero copy" is not on offer and cannot be: Luau's C API has lua_newbuffer and
         * lua_tobuffer and nothing else, so a buffer always owns VM-allocated memory and
         * can never alias the array's. One memcpy is the ceiling here, and for a 1-byte
         * element it is the whole cost.
         */
        static int BufferFromArray(lua_State* L)
        {
            FLuaArray* Array = (FLuaArray*)GetCppInstanceFast(L, 1);
            if (!Array)
                return luaL_error(L, "UnLuaNova.BufferFromArray: expected a TArray");

            const int32 Count = Array->Num();
            uint8* Dest = (uint8*)lua_newbuffer(L, size_t(Count));
            if (Count == 0)
                return 1;

            if (Array->Inner->GetSize() == 1)
            {
                FMemory::Memcpy(Dest, Array->GetData(0), size_t(Count));
                return 1;
            }

            for (int32 Index = 0; Index < Count; ++Index)
            {
                Array->Inner->ReadValue(L, Array->GetData(Index), false);
                if (lua_type(L, -1) != LUA_TNUMBER)
                {
                    lua_pop(L, 1);
                    return luaL_error(L, "UnLuaNova.BufferFromArray: element %d is not a number", Index + 1);
                }

                Dest[Index] = (uint8)lua_tointeger(L, -1);
                lua_pop(L, 1);
            }

            return 1;
        }

        /**
         * buffer -> TArray.
         *
         * The element type is int32, i.e. exactly what `UE.TArray(0)` produces, so this is
         * a drop-in replacement for the Lua version and not a behaviour change. TArray<uint8>
         * would be the natural answer for bytes, but FPropertyRegistry has no byte type
         * interface (bool/int/int64/float/string/name/text/struct only), so there is nothing
         * to ask for - and widening bytes to int32 is the cheaper direction to be stuck with.
         */
        static int BufferToArray(lua_State* L)
        {
            size_t Len = 0;
            const uint8* Src = (const uint8*)lua_tobuffer(L, 1, &Len);
            if (!Src)
                return luaL_error(L, "UnLuaNova.BufferToArray: expected a buffer");

            auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);

            lua_pushinteger(L, 0); // element type, exactly what `UE.TArray(0)` passes
            auto ElementType = Env.GetPropertyRegistry()->CreateTypeInterface(L, -1);
            lua_pop(L, 1);
            if (!ElementType)
                return luaL_error(L, "UnLuaNova.BufferToArray: could not resolve the element type");

            FLuaArray* Array = Env.GetContainerRegistry()->NewArray(L, ElementType, FLuaArray::OwnedBySelf);
            if (!Array)
                return luaL_error(L, "UnLuaNova.BufferToArray: failed to create the array");

            if (Len == 0)
                return 1;

            // One grow for the whole buffer rather than Len separate Add() calls.
            Array->AddUninitialized((int32)Len);

            for (size_t Index = 0; Index < Len; ++Index)
            {
                lua_pushinteger(L, Src[Index]);
                Array->Inner->WriteValue_InContainer(L, Array->GetData((int32)Index), -1);
                lua_pop(L, 1);
            }

            return 1;
        }
#endif // UNLUANOVA_HAS_NATIVE_BUFFER

        static constexpr luaL_Reg UNLUANOVA_Functions[] = {
            {"Log", LogInfo},
            {"LogWarn", LogWarn},
            {"LogError", LogError},
            {"HotReload", HotReload},
            {"Ref", Ref},
            {"Unref", Unref},
            {"GetBackend", GetBackend},
            {"HasFeature", HasFeature},
#if UNLUANOVA_HAS_NATIVE_BUFFER
            {"BufferFromArray", BufferFromArray},
            {"BufferToArray", BufferToArray},
#endif
            {"FTextEnabled", nullptr},
            {NULL, NULL}
        };

#pragma region Legacy Support

        int32 GetUProperty(lua_State* L)
        {
            auto Ptr = lua_touserdata(L, 2);
            if (!Ptr)
                return 0;

            auto Property = static_cast<TSharedPtr<UnLuaNova::ITypeOps>*>(Ptr);
            if (!Property->IsValid())
                return 0;

            auto Self = GetCppInstance(L, 1);
            if (!Self)
                return 0;

            if (UnLuaNova::LowLevel::IsReleasedPtr(Self))
                return luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("attempt to read property '%s' on released object"), *(*Property)->GetName())));

            if (!LowLevel::CheckPropertyOwner(L, (*Property).Get(), Self))
                return 0;

            (*Property)->ReadValue_InContainer(L, Self, false);
            return 1;
        }

        int32 SetUProperty(lua_State* L)
        {
            auto Ptr = lua_touserdata(L, 2);
            if (!Ptr)
                return 0;

            auto Property = static_cast<TSharedPtr<UnLuaNova::ITypeOps>*>(Ptr);
            if (!Property->IsValid())
                return 0;

            auto Self = GetCppInstance(L, 1);
            if (LowLevel::IsReleasedPtr(Self))
                return luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("attempt to write property '%s' on released object"), *(*Property)->GetName())));

            if (!LowLevel::CheckPropertyOwner(L, (*Property).Get(), Self))
                return 0;

            (*Property)->WriteValue_InContainer(L, Self, 3);
            return 0;
        }

        static constexpr luaL_Reg UNLUANOVA_LegacyFunctions[] = {
            {"GetUProperty", GetUProperty},
            {"SetUProperty", SetUProperty},
            {NULL, NULL}
        };

        static void LegacySupport(lua_State* L)
        {
            static const char* Chunk = R"(
            local rawget = _G.rawget
            local rawset = _G.rawset
            local rawequal = _G.rawequal
            local type = _G.type
            local getmetatable = _G.getmetatable
            local require = _G.require

            local GetUProperty = GetUProperty
            local SetUProperty = SetUProperty

            local NotExist = {}

            local function Index(t, k)
                local mt = getmetatable(t)
                local super = mt
                while super do
                    local v = rawget(super, k)
                    if v ~= nil and not rawequal(v, NotExist) then
                        rawset(t, k, v)
                        return v
                    end
                    super = rawget(super, "Super")
                end

                local p = mt[k]
                if p ~= nil then
                    if type(p) == "userdata" then
                        return GetUProperty(t, p)
                    elseif type(p) == "function" then
                        rawset(t, k, p)
                    elseif rawequal(p, NotExist) then
                        return nil
                    end
                else
                    rawset(mt, k, NotExist)
                end

                return p
            end

            local function NewIndex(t, k, v)
                local mt = getmetatable(t)
                local p = mt[k]
                if type(p) == "userdata" then
                    return SetUProperty(t, p, v)
                end
                rawset(t, k, v)
            end

            local function Class(super_name)
                local super_class = nil
                if super_name ~= nil then
                    super_class = require(super_name)
                end

                local new_class = {}
                new_class.__index = Index
                new_class.__newindex = NewIndex
                new_class.Super = super_class

              return new_class
            end

            _G.Class = Class
            _G.GetUProperty = GetUProperty
            _G.SetUProperty = SetUProperty
            )";

            lua_register(L, "UEPrint", LogInfo);
            luaL_loadstring(L, Chunk);
            lua_newtable(L);
            lua_getglobal(L, LUA_GNAME);
            lua_setfield(L, -2, LUA_GNAME);
            luaL_setfuncs(L, UNLUANOVA_LegacyFunctions, 0);
            if (!UnLuaNovaSetChunkEnv(L, -2))
                UE_LOG(LogUnLuaNova, Warning, TEXT("Failed to set the environment of the legacy support chunk; its globals will be the real ones."));
            // 0 results, not LUA_MULTRET: this must leave the stack exactly as it found
            // it, because the lua_setfield below addresses -2. With MULTRET a failed
            // chunk left its error message there, and setting a field on a string is an
            // unprotected index error - on Luau that is an abort(), not a Lua error.
            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
            {
                UE_LOG(LogUnLuaNova, Error, TEXT("Failed to run the legacy support chunk: %s"), UTF8_TO_TCHAR(lua_tostring(L, -1)));
                lua_pop(L, 1);
            }
            lua_getglobal(L, "Class");
            lua_setfield(L, -2, "Class");
        }

#pragma endregion

        /**
         * package.path：**按插件实例**拼，不再硬编码 "Plugins/UnLuaNova/Content/Script"。
         *
         * 硬编码的后果是"工程套插件"（submodule 是个工程，真正的插件在内层
         * Plugins/UnLuaNova/Plugins/UnLuaNova/）时，插件随包发布的那几个模块
         * （StringEx / Vector / Buffer / Input / HotReload / Event）全部 require 不到 ——
         * 而本仓库的扁平布局下一切正常，于是没人会发现。
         *
         * 顺序保持不变：工程目录在前，插件目录在后（工程侧可以覆盖插件侧的模块）。
         */
        static FString BuildPackagePath()
        {
            TArray<FString> Entries;
            Entries.Add(TEXT("Content/Script/?.lua"));

            // No IsAvailable() gate: it was removed from IPluginManager, and it could only
            // ever have been false before Projects was loaded - which cannot happen in a
            // module that links against Projects.
            for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
            {
                if (!Plugin->CanContainContent())
                    continue;

                if (!Plugin->GetName().StartsWith(TEXT("UnLuaNova")))
                    continue;

                FString Entry = Plugin->GetContentDir() / TEXT("?.lua");
                if (FPaths::MakePathRelativeTo(Entry, *FPaths::ProjectDir()))
                    Entries.AddUnique(Entry);
            }

            // 兜底：插件管理器还没起来（很早的启动阶段）或布局又变了的场合，
            // 至少保留老的那条，行为不比从前差
            Entries.AddUnique(TEXT("Plugins/UnLuaNova/Content/Script/?.lua"));
            return FString::Join(Entries, TEXT(";"));
        }

        static int LuaOpen(lua_State* L)
        {
            lua_newtable(L);
            luaL_setfuncs(L, UNLUANOVA_Functions, 0);
            lua_pushstring(L, TCHAR_TO_UTF8(*BuildPackagePath()));
            lua_setfield(L, -2, PACKAGE_PATH_KEY);
            return 1;
        }

        int Open(lua_State* L)
        {
            lua_register(L, "print", LogInfo);
            luaL_requiref(L, "Lua", LuaOpen, 1);
            luaL_dostring(L, R"(
                setmetatable(Lua, {
                    __index = function(t, k)
                        -- IMPORTANT: this is the *require path*, i.e. the on-disk directory
                        -- name (Content/Script/UnLuaNova). It is deliberately NOT the global
                        -- table name above - case-sensitive filesystems will not resolve any
                        -- other spelling. Do not "simplify" one to match the other.
                        local ok, result = pcall(require, "UnLuaNova." .. tostring(k))
                        if ok then
                            rawset(t, k, result)
                            return result
                        else
                            t.LogWarn(string.format("failed to load module UnLuaNova.%s\n%s", k, result))
                        end
                    end
                })
                -- 'Lua' is the global; the longer spellings remain as aliases so scripts
                -- written against either of them keep working.
                UnLua = Lua
                UnLuaNova = Lua
            )");

#if UNLUANOVA_ENABLE_FTEXT
            luaL_dostring(L, "Lua.FTextEnabled = true");
#else
            luaL_dostring(L, "Lua.FTextEnabled = false");
#endif

#if UNLUANOVA_WITH_HOT_RELOAD
            luaL_dostring(L, R"(
                pcall(function() _G.require = require('UnLuaNova.HotReload').require end)
            )");
#endif

            // StringEx installs backend-independent string helpers (see the file header for
            // why they are needed). Loaded eagerly because scripts rely on them being there
            // without requiring anything.
            luaL_dostring(L, R"(
                pcall(require, 'UnLuaNova.StringEx')
            )");

            LegacySupport(L);
            lua_pop(L, 1);
            return 1;
        }

        FString GetPackagePath(lua_State* L)
        {
            lua_getglobal(L, "Lua");
            checkf(lua_istable(L, -1), TEXT("UnLuaNovaLib not registered"));
            lua_getfield(L, -1, PACKAGE_PATH_KEY);
            const auto PackagePath = lua_tostring(L, -1);
            checkf(PackagePath, TEXT("invalid PackagePath"));
            lua_pop(L, 2);
            return UTF8_TO_TCHAR(PackagePath);
        }

        void SetPackagePath(lua_State* L, const FString& PackagePath)
        {
            lua_getglobal(L, "Lua");
            checkf(lua_istable(L, -1), TEXT("UnLuaNovaLib not registered"));
            lua_pushstring(L, TCHAR_TO_UTF8(*PackagePath));
            lua_setfield(L, -2, PACKAGE_PATH_KEY);
            lua_pop(L, 2);
        }
    }

    void RestoreTablesAfterSandbox(lua_State* L)
    {
        // luaL_sandbox marks every table hanging off the globals read-only. That is exactly
        // right for the globals themselves and exactly wrong for the tables UnLuaNova keeps
        // writing to after startup:
        //   * UE.<Type>      - types are registered lazily, on first access
        //   * Lua.<Module>   - the __index fallback caches what it just required
        //   * package.loaded - written by every require
        // Freezing those hardens nothing a script can legitimately reach; it just turns type
        // registration and require into errors. Hand write access back instead.
        static const char* const WritableGlobals[] = { "UE", "UE4", "Lua", "UnLua", "UnLuaNova", "package" };
        for (const char* Name : WritableGlobals)
        {
            lua_getglobal(L, Name);
            if (lua_istable(L, -1))
                UNLUANOVA_SET_READONLY(L, -1, false);
            lua_pop(L, 1);
        }

        // package.loaded sits one level down, so the loop above cannot reach it.
        lua_getglobal(L, "package");
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "loaded");
            if (lua_istable(L, -1))
                UNLUANOVA_SET_READONLY(L, -1, false);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
}
