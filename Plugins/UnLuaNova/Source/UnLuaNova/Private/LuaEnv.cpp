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

#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "LuaEnv.h"
#include "Binding.h"
#include "LowLevel.h"
#include "Registries/ObjectRegistry.h"
#include "Registries/ClassRegistry.h"
#include "LuaCore.h"
#include "LuaDynamicBinding.h"
#include "UELib.h"
#include "ObjectReferencer.h"
#include "UnLuaNovaDelegates.h"
#include "UnLuaNovaInterface.h"
#include "UnLuaNovaLegacy.h"
#include "UnLuaNovaLib.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaBase.h"   // UNLUANOVA_HAS_STDLIB_*（由 Build.cs 定义）
#include "LuaBackend.h"
#include "LuaLib_Io.h"
#include "LuaLib_Timer.h"
#include "LuaMemoryStats.h"
#include "LuaProfiler.h"
#include "UnLuaNovaSandbox.h"

namespace UnLuaNova
{
#if UE_VERSION_NEWER_THAN(5, 5, 0)
    constexpr EInternalObjectFlags AsyncObjectFlags = EInternalObjectFlags_AsyncLoading | EInternalObjectFlags::Async;
#else
    constexpr EInternalObjectFlags AsyncObjectFlags = EInternalObjectFlags::AsyncLoading | EInternalObjectFlags::Async;
#endif

    TMap<lua_State*, FLuaEnv*> FLuaEnv::AllEnvs;
    FLuaEnv::FOnCreated FLuaEnv::OnCreated;
    FLuaEnv::FOnDestroyed FLuaEnv::OnDestroyed;

    // Fills package.loaded with the standard libraries. Expects `package` on top of the
    // stack and leaves it there.
    //
    // Plain Lua's luaL_openlibs registers every standard library as a module too, so
    // `require('string')` returns the string table - that is what package.loaded *is*
    // there. Luau has no package library at all, so nothing is registered and the same
    // line raises "module 'string' not found". Third-party scripts do exactly that:
    // LuaSocket's socket.lua opens with require("string") and require("math"), so on this
    // backend it cannot even be loaded. Each name is published only when the global of
    // that name is really a table, because the set differs per backend (Luau adds vector
    // and buffer, and has no io).
    static void PublishStdLibModules(lua_State* L)
    {
        static const char* const StdLibModules[] = {
            "string", "table", "math", "os", "io", "coroutine", "debug", "utf8", "bit32",
            "vector", "buffer", "integer",
        };

        lua_getfield(L, -1, "loaded");                  // [package, loaded]
        for (const char* Name : StdLibModules)
        {
            lua_getglobal(L, Name);                     // [package, loaded, lib]
            if (lua_istable(L, -1))
            {
                lua_pushvalue(L, -1);                   // [package, loaded, lib, lib]
                lua_setfield(L, -3, Name);              // loaded[Name] = lib
            }
            lua_pop(L, 1);                              // [package, loaded]
        }
        lua_pop(L, 1);                                  // [package]
    }

    // The profiler hook used to live here. It now lives in LuaProfiler.cpp, which caches
    // one Insights spec id per function rather than formatting an FString on every single
    // call/return, and can be toggled at runtime instead of only at compile time.

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    /**
     * GC tunables from CVars, applied on top of UNLUANOVA_SETUP_GC.
     *
     * Luau's collector has no generational mode, so G (goal) and S (step multiplier) are
     * the whole story, and they are not independent: S has to be able to keep up with the
     * heap growth G permits. lua.h spells the pairing out - S belongs in
     * [100/(G-100), 100+100/(G-100)) with a floor of 150%, so [150,200] at G=200 - and a
     * pair outside it does not merely collect more or less, it collects *wrong*: too small
     * an S lets the heap outrun the collector and the effective goal silently grows, and
     * too large an S spends CPU on a cycle that keeps restarting.
     *
     * The check is a warning rather than a clamp because a project that deliberately
     * overrides both should be allowed to; it just has to be told when the pair is one
     * the VM cannot honour.
     */
    static void ApplyLuauGcTuning(lua_State* L)
    {
        static auto GoalCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.gcgoal"),
            200,
            TEXT("Luau GC goal G, in percent of live heap size. 200 allows the heap to double. Luau backend only."),
            ECVF_Default);

        static auto StepMulCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.gcstepmul"),
            200,
            TEXT("Luau GC step multiplier S, in percent: how much collector work is done per byte allocated. Keep it in the interval documented for lua.gcgoal. Luau backend only."),
            ECVF_Default);

        static auto StepSizeCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.gcstepsize"),
            1,
            TEXT("Luau GC step size in KB: how often the collector interrupts to do a step. Luau backend only."),
            ECVF_Default);

        const int32 Goal = FMath::Max(101, GoalCVar->GetInt());
        const int32 StepMul = FMath::Max(100, StepMulCVar->GetInt());
        const int32 StepSizeKb = FMath::Max(1, StepSizeCVar->GetInt());

        lua_gc(L, LUA_GCSETGOAL, Goal);
        lua_gc(L, LUA_GCSETSTEPMUL, StepMul);
        lua_gc(L, LUA_GCSETSTEPSIZE, StepSizeKb);

        // 10000/(G-100) is the recommended interval expressed in percent.
        const int32 Ratio = 10000 / (Goal - 100);
        const int32 MinStepMul = FMath::Max(150, Ratio);
        const int32 MaxStepMul = 100 + Ratio;

        if (StepMul < MinStepMul || StepMul >= MaxStepMul)
        {
            UE_LOG(LogUnLuaNova, Warning,
                TEXT("lua.gcstepmul=%d is outside the interval [%d, %d) that lua.gcgoal=%d needs; "
                     "the collector will not reach the goal. See the GC note in LuaBackend.h."),
                StepMul, MinStepMul, MaxStepMul, Goal);
        }
    }
#endif

    FLuaEnv::FLuaEnv()
        : FLuaEnv(nullptr, nullptr)
    {
    }

    FLuaEnv::FLuaEnv(lua_Alloc Allocator, void* AllocatorUserdata)
        : bStarted(false)
    {
        const auto Settings = GetDefault<UUnLuaNovaSettings>();
        ModuleLocator = Settings->ModuleLocatorClass.GetDefaultObject();
        ensureMsgf(ModuleLocator, TEXT("Invalid lua module locator, lua binding will not work properly. please check UnLuaNova runtime settings."));

        RegisterDelegates();

#if PLATFORM_WINDOWS
        // 防止类似AppleProResMedia插件忘了恢复Dll查找目录
        // https://github.com/Tencent/UnLuaNova/issues/534
        const auto Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Binaries/Win64"));
        FPlatformProcess::PushDllDirectory(*Dir);
        L = UNLUANOVA_NEWSTATE(Allocator ? Allocator : GetLuaAllocator(), AllocatorUserdata);
        FPlatformProcess::PopDllDirectory(*Dir);
#else
        L = UNLUANOVA_NEWSTATE(Allocator ? Allocator : GetLuaAllocator(), AllocatorUserdata);
#endif

        AllEnvs.Add(L, this);

        luaL_openlibs(L);

        // Luau ships no package library, so there is no require() and no package table at
        // all - and every UnLuaNova script, starting with the startup module, is written
        // against both.
        //
        // On plain Lua the standard library owns require() and package, and the three
        // loaders below end up in package.searchers. On this backend we build the same two
        // things: our own require(), and a package table carrying the same two fields,
        // so a script cannot tell the backends apart.
        //
        // Both are only installed when missing, so a backend that already provides them
        // keeps its own.
        lua_getglobal(L, "require");
        const bool bHasRequire = lua_isfunction(L, -1) != 0;
        lua_pop(L, 1);
        if (!bHasRequire)
        {
            lua_newtable(L);                     // the module cache
            lua_pushcclosure(L, &FLuaEnv::Require, 1);
            lua_setglobal(L, "require");
            // Nothing to pop here. lua_pushcclosure already took the cache as its upvalue
            // and lua_setglobal already took the closure, so the stack is back where it
            // started. The extra lua_pop that used to sit here pushed the stack top below
            // the frame base: on Luau lua_gettop() is frame-relative (L->top - L->base), so
            // the depth stayed -1 for the rest of the env's life and the first
            // UnLuaNova::Call() computed MessageHandlerIdx = gettop - 1 = 0.
        }

        lua_getglobal(L, "package");
        const bool bHasPackage = lua_istable(L, -1) != 0;
        lua_pop(L, 1);
        if (!bHasPackage)
        {
            // package.loaded stays the single source of truth: LowLevel::GetLoadedModule()
            // reads it to tell whether a module is already in, and it is the very table
            // require() above caches into.
            //
            // package.searchers matters just as much. Plain Lua's own require walks it, and
            // so does any script that loads a module itself - HotReload.lua does exactly
            // that with `ipairs(package.searchers)`. Publishing package without it is worse
            // than publishing nothing: the table looks usable, and then indexing it hands
            // back nil.
            lua_newtable(L);                     // [package]
            lua_getglobal(L, "require");         // [package, require]
            lua_getupvalue(L, -1, 1);            // [package, require, cache]
            lua_remove(L, -2);                   // [package, cache]
            lua_setfield(L, -2, "loaded");       // package.loaded = cache
            lua_newtable(L);                     // [package, searchers]
            lua_setfield(L, -2, "searchers");    // package.searchers = {}

            // The rest of the shape a Lua program expects to find on package.
            lua_newtable(L);                     // [package, preload]
            lua_setfield(L, -2, "preload");      // package.preload = {}

            // Directory separator, path separator, the substitution character in a path
            // template, the executable-directory placeholder, and the ignore mark - the
            // five lines plain Lua puts here.
            lua_pushliteral(L, "/\n;\n?\n!\n-\n");
            lua_setfield(L, -2, "config");

            // path and cpath are published because scripts read them, but UnLuaNova's own
            // loader does not: it resolves modules from the script root and from whatever
            // a project adds to package.searchers. A project that wants path-based
            // resolution can register a searcher that reads these - which works, because
            // package.searchers is real on both backends.
            lua_pushliteral(L, "./?.lua;./?/init.lua");
            lua_setfield(L, -2, "path");
            lua_pushliteral(L, "");
            lua_setfield(L, -2, "cpath");

            // ...and the standard libraries, which plain Lua publishes as modules and
            // this backend otherwise does not publish at all.
            PublishStdLibModules(L);             // [package]

            lua_setglobal(L, "package");
        }

        // Now that package.searchers exists on both backends, the loaders can go into it -
        // which is also what makes a searcher added later from script work.
        AddSearcher(LoadFromPreload, 1);
        AddSearcher(LoadFromCustomLoader, 2);
        AddSearcher(LoadFromFileSystem, 3);
        AddSearcher(LoadFromBuiltinLibs, 4);

        // Fill in what Luau leaves out - the debug functions (getupvalue, setupvalue,
        // upvalueid, ...) and load(). Has to happen before any script runs, because
        // HotReload.lua reaches for them the moment it is required during
        // UnLuaNovaLib::Open below.
        UNLUANOVA_SETUP_SCRIPT_COMPAT(L);

        UELib::Open(L);

        // Same reasoning as UELib::Open: anything published as a global has to be in place
        // before luaL_sandbox freezes the globals table. Off by default.
        OpenIoLibrary(L);

        // Sandbox only after UnLua has published its own globals - otherwise we would be
        // locking the very table we are about to write to. Off by default: hardening the
        // globals is a behaviour change for existing scripts, so it has to be opted into.
        //
        // The CVar itself is registered on every backend, not just the sandboxing one. It
        // is the switch a project reads and sets, so a backend that cannot honour it has
        // to answer "off" - not "no such command", which is what an operator typing
        // `lua.sandbox 1` gets when it is compiled away. Whether it does anything is a
        // separate question, and scripts ask that one with HasFeature('sandbox').
        {
            static auto SandboxCVar = IConsoleManager::Get().RegisterConsoleVariable(
                TEXT("lua.sandbox"),
                0,
                TEXT("Harden the Lua global table once startup finishes. 0=off, 1=on. No effect on the plain-Lua backend, which has no read-only tables - see UnLuaNova.HasFeature('sandbox')."),
                ECVF_Default);
#if UNLUANOVA_HAS_SANDBOX
            if (SandboxCVar->GetInt() != 0)
            {
                UNLUANOVA_SANDBOX(L);

                // ...and then give back the tables it froze that we still write to. Without
                // this the sandbox does not merely harden the globals: UE.<Type> can no
                // longer be registered on first access and require() can no longer record
                // what it loaded, so the next script to ask for a new type errors out.
                UnLuaNova::RestoreTablesAfterSandbox(L);
            }
#else
            // Nothing to enforce here: this backend has no read-only tables, so the
            // global table cannot be frozen however the switch is set. Reading the
            // value anyway keeps the registration above from being dead code on this
            // build - and keeps both backends compiling the same shape.
            (void)SandboxCVar->GetInt();
#endif

            // Independent of lua.sandbox: hardening the globals is one switch, giving each
            // coroutine its own globals is another, and the second is the one that changes
            // what scripts can see. See UnLuaNovaSandbox.h.
            UnLuaNova::InstallSandboxThreadHardening(L);
        }

        ObjectRegistry = new FObjectRegistry(this);
        ClassRegistry = new FClassRegistry(this);
        ClassRegistry->Initialize();

        FunctionRegistry = new FFunctionRegistry(this);
        DelegateRegistry = new FDelegateRegistry(this);
        ContainerRegistry = new FContainerRegistry(this);
        PropertyRegistry = new FPropertyRegistry(this);
        EnumRegistry = new FEnumRegistry(this);
        EnumRegistry->Initialize();

        DanglingCheck = new FDanglingCheck(this);
        DeadLoopCheck = new FDeadLoopCheck(this);

        AutoObjectReference.SetName("UNLUANOVA_AutoReference");
        ManualObjectReference.SetName("UNLUANOVA_ManualReference");

        lua_pushstring(L, "StructMap"); // create weak table 'StructMap'
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushstring(L, "ArrayMap"); // create weak table 'ArrayMap'
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);

        if (FUnLuaNovaDelegates::ConfigureLuaGC.IsBound())
        {
            FUnLuaNovaDelegates::ConfigureLuaGC.Execute(L);
        }
        else
        {
            UNLUANOVA_SETUP_GC(L);
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            ApplyLuauGcTuning(L);
#endif
        }

        // Created after the setup above so it starts in the mode the VM was actually
        // configured for. It does nothing per frame until someone asks for TimeSliced.
        GcScheduler = new FLuaGcScheduler(L);

        FUnLuaNovaDelegates::OnPreStaticallyExport.Broadcast();

        // register statically exported classes
        auto ExportedNonReflectedClasses = GetExportedNonReflectedClasses();
        for (const auto& Pair : ExportedNonReflectedClasses)
            Pair.Value->Register(L);

        // register statically exported global functions
        auto ExportedFunctions = GetExportedFunctions();
        for (const auto& Function : ExportedFunctions)
            Function->Register(L);

        // register statically exported enums
        auto ExportedEnums = GetExportedEnums();
        for (const auto& Enum : ExportedEnums)
            Enum->Register(L);

        UnLuaNovaLib::Open(L);

        // 内建（native）模块：require('UnLuaNova.Timer') 走 LoadFromBuiltinLibs 拿到这里注册的
        // 加载器。计时器必须由 C++ 持有到期时间，是因为它要用世界时间 —— 见
        // UUnLuaNovaTimerSubsystem 的头注释。
        //
        // 开关是编译期的：关掉就一个字都不注册，require 直接失败、HasFeature 回答 false，
        // 而不是"注册了但里面空转"。
#if UNLUANOVA_HAS_STDLIB_TIMER
        AddBuiltInLoader(TEXT("UnLuaNova.Timer"), UnLuaNova::LuaTimerLib::Open);
#endif

        OnCreated.Broadcast(*this);
        FUnLuaNovaDelegates::OnLuaStateCreated.Broadcast(L);

        // The profiler owns the VM hook from here on. It stays uninstalled until you turn
        // it on with `lua.profiler on`, which keeps the default build untouched and leaves
        // the hook slot free for FDeadLoopCheck.
        FLuaProfiler::Get().Attach(L);

        // Everything above runs straight from C++ at the top level of the main thread, so
        // the stack has to come back empty. It is not cosmetic: on Luau lua_gettop() is
        // frame-relative (L->top - L->base), so a stack that ends up below its own base
        // stays negative for the life of the env, and every later top-level call computes
        // its message handler index one slot too low - which is how
        // CallFunctionInternal() ends up asserting on MessageHandlerIdx > 0. Warn rather
        // than check: a leak here is worth knowing about, but it must not stop the editor
        // from opening.
        if (const int LeftOver = lua_gettop(L))
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("Lua stack is not balanced after the env was created: %d value(s) left on the stack. Check for a missing lua_pop / an extra one in FLuaEnv::FLuaEnv()."), LeftOver);
        }
    }

    FLuaEnv::~FLuaEnv()
    {
        bClosing = true;
        FLuaProfiler::Get().Detach(L);
        FLuaMemoryStats::Get().Detach(L);

        OnDestroyed.Broadcast(*this);
        lua_close(L);
        AllEnvs.Remove(L);

        delete ClassRegistry;
        delete ObjectRegistry;
        delete DelegateRegistry;
        delete FunctionRegistry;
        delete ContainerRegistry;
        delete EnumRegistry;
        delete PropertyRegistry;
        delete DanglingCheck;
        delete DeadLoopCheck;
        delete GcScheduler;

        if (!IsEngineExitRequested() && Manager)
        {
            Manager->Cleanup();
            Manager->RemoveFromRoot();
        }

        AutoObjectReference.Clear();
        ManualObjectReference.Clear();

        UnRegisterDelegates();

        CandidateInputComponents.Empty();
        FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartHandle);
    }

    TMap<lua_State*, FLuaEnv*>& FLuaEnv::GetAll()
    {
        return AllEnvs;
    }

    TArray<FLuaEnv*> FLuaEnv::GetAllEnvs()
    {
        TArray<FLuaEnv*> Envs;
        Envs.Reserve(AllEnvs.Num());

        for (const auto& Pair : AllEnvs)
        {
            if (Pair.Value)
                Envs.Add(Pair.Value);
        }

        // Ordered by main state address: it is the only identity an env has that is both
        // stable and comparable, and it keeps the index a user typed valid until that env
        // is destroyed. Sorted by name instead would reshuffle the moment Env_10 appears
        // ("Env_10" < "Env_2").
        Envs.Sort([](const FLuaEnv& A, const FLuaEnv& B)
        {
            return A.GetMainState() < B.GetMainState();
        });

        return Envs;
    }

    FLuaEnv* FLuaEnv::FindEnv(const lua_State* L)
    {
        if (!L)
            return nullptr;

        const auto MainThread = UnLuaNovaGetMainState(const_cast<lua_State*>(L));
        return AllEnvs.FindRef(MainThread);
    }

    FLuaEnv& FLuaEnv::FindEnvChecked(const lua_State* L)
    {
        return *AllEnvs.FindChecked(UnLuaNovaGetMainState(const_cast<lua_State*>(L)));
    }

    namespace
    {
        // Is the module in package.loaded? That table is the only record of "this module
        // ran": both backends write to it - plain Lua's own require, and the replacement
        // require() this env installs on Luau - so it answers the question without
        // caring which one built it. A backend without a package table at all counts as
        // "not loaded", never as an error: reading nil is what used to abort the process.
        bool IsModuleLoaded(lua_State* L, const char* ModuleName)
        {
            lua_getglobal(L, "package");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return false;
            }

            lua_getfield(L, -1, "loaded");
            lua_remove(L, -2);
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return false;
            }

            lua_getfield(L, -1, ModuleName);
            const bool bLoaded = lua_isnil(L, -1) == 0;
            lua_pop(L, 2);
            return bLoaded;
        }

        // Message handler for the startup call, installed in place of the generic
        // ReportLuaCallError. Two properties are worth the duplication:
        //
        //   * the module name is always in the message. ReportLuaCallError prints only
        //     whatever the VM raised, and whether that happens to name the module is up
        //     to the backend - plain Lua does, Luau does not report at all.
        //   * the level is always Error, so a missing startup module is never a silent
        //     no-op for whoever owns the env.
        //
        // Upvalue 1 is a bool the caller watches, so it can tell whether this ran and
        // report the failure itself when it did not.
        int ReportStartupModuleError(lua_State* L)
        {
            if (auto* bReported = static_cast<bool*>(lua_touserdata(L, lua_upvalueindex(1))))
                *bReported = true;

            // A project that installed its own reporter owns the report - including the
            // level it is logged at - so it is handed the message instead, once.
            if (FUnLuaNovaDelegates::ReportLuaCallError.IsBound())
                return FUnLuaNovaDelegates::ReportLuaCallError.Execute(L);

            const char* ModuleName = lua_tostring(L, lua_upvalueindex(2));
            const char* Message = lua_tostring(L, -1);
            UE_LOG(LogUnLuaNova, Error, TEXT("Startup module '%s' failed to load: %s"),
                UTF8_TO_TCHAR(ModuleName ? ModuleName : ""),
                UTF8_TO_TCHAR(Message ? Message : luaL_typename(L, -1)));
            return 0;
        }
    }

    void FLuaEnv::Start(const TMap<FString, UObject*>& Args)
    {
        const auto& Setting = *GetDefault<UUnLuaNovaSettings>();
        Start(Setting.StartupModuleName, Args);
    }

    void FLuaEnv::Start(const FString& StartupModuleName, const TMap<FString, UObject*>& Args)
    {
        if (bStarted)
            return;

        if (StartupModuleName.IsEmpty())
        {
            bStarted = true;
            return;
        }

        const auto Guard = GetDeadLoopCheck()->MakeGuard();
        const int Top = lua_gettop(L);

        // A startup module that never runs looks exactly like one that ran and did
        // nothing, so the failure has to be reported - once, at Error, naming the
        // module. Nothing about that used to be guaranteed: the pcall result was
        // dropped, and whether the generic error handler said anything at all was up to
        // the backend (plain Lua logs it, Luau stays silent). Hence the explicit check
        // below on top of the handler: if require() came back but never recorded the
        // module, that is reported too.
        bool bReported = false;
        lua_pushlightuserdata(L, &bReported);
        lua_pushstring(L, TCHAR_TO_UTF8(*StartupModuleName));
        lua_pushcclosure(L, ReportStartupModuleError, 2);

        lua_getglobal(L, "require");
        lua_pushstring(L, TCHAR_TO_UTF8(*StartupModuleName));
        lua_createtable(L, 0, Args.Num());
        for (auto Pair : Args)
        {
            PushUObject(L, Pair.Value);
            lua_setfield(L, -2, TCHAR_TO_UTF8(*Pair.Key));
        }
        const int Status = lua_pcall(L, 2, LUA_MULTRET, -4);

        if (Status != LUA_OK)
        {
            // Only reachable when the handler never ran - otherwise it has already
            // reported, and a second line would be one report too many.
            if (!bReported)
            {
                const char* Message = lua_tostring(L, -1);
                UE_LOG(LogUnLuaNova, Error, TEXT("Startup module '%s' failed to load: %s"), *StartupModuleName,
                    UTF8_TO_TCHAR(Message ? Message : luaL_typename(L, -1)));
            }
        }
        else if (!IsModuleLoaded(L, TCHAR_TO_UTF8(*StartupModuleName)))
        {
            UE_LOG(LogUnLuaNova, Error, TEXT("Startup module '%s' did not load: require() returned without recording it in package.loaded, so the module never ran."), *StartupModuleName);
        }

        // The results are ours, not the caller's - Start() returns void. Leaving them
        // on the stack would keep the env permanently off-balance, and on Luau, where
        // lua_gettop() is frame-relative, that shifts the message-handler index of every
        // later top-level call by one.
        lua_settop(L, Top);
        bStarted = true;
    }

    const FString& FLuaEnv::GetName()
    {
        return Name;
    }

    void FLuaEnv::SetName(FString InName)
    {
        Name = InName;
    }

    void FLuaEnv::NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index)
    {
        UObject* Object = (UObject*)ObjectBase;
        PropertyRegistry->NotifyUObjectDeleted(Object);
        FunctionRegistry->NotifyUObjectDeleted(Object);
        if (Manager)
            Manager->NotifyUObjectDeleted(Object);
        ObjectRegistry->NotifyUObjectDeleted(Object);
        ClassRegistry->NotifyUObjectDeleted(Object);
        EnumRegistry->NotifyUObjectDeleted(Object);

        if (CandidateInputComponents.Num() <= 0)
            return;

        const int32 NumRemoved = CandidateInputComponents.Remove((UInputComponent*)Object);
        if (NumRemoved > 0 && CandidateInputComponents.Num() < 1)
            FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartHandle);
    }

    void FLuaEnv::OnUObjectArrayShutdown()
    {
        GUObjectArray.RemoveUObjectDeleteListener(this);
        bObjectArrayListenerRegistered = false;
    }

    bool FLuaEnv::TryReplaceInputs(UObject* Object)
    {
        if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
            || !Object->IsA<UInputComponent>())
            return false;

        AActor* Actor = Cast<APlayerController>(Object->GetOuter());
        if (!Actor)
            Actor = Cast<APawn>(Object->GetOuter());

        if (!Actor)
            return false;

        CandidateInputComponents.AddUnique((UInputComponent*)Object);
        if (OnWorldTickStartHandle.IsValid())
            FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartHandle);
        OnWorldTickStartHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FLuaEnv::OnWorldTickStart);
        return true;
    }

    void FLuaEnv::OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime)
    {
        if (!Manager)
            return;

        for (UInputComponent* InputComponent : CandidateInputComponents)
        {
            if (!InputComponent->IsRegistered())
                continue;

#if ENGINE_MAJOR_VERSION >=5
            if (!IsValid(InputComponent))
                continue;
#else
            if (InputComponent->IsPendingKill())
                continue;
#endif

            AActor* Actor = Cast<AActor>(InputComponent->GetOuter());
            Manager->ReplaceInputs(Actor, InputComponent); // try to replace/override input events
        }

        CandidateInputComponents.Empty();
        FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartHandle);
    }

    bool FLuaEnv::TryBind(UObject* Object)
    {
        const auto Class = Object->IsA<UClass>() ? static_cast<UClass*>(Object) : Object->GetClass();
        if (Class->HasAnyClassFlags(CLASS_NewerVersionExists))
        {
            // filter out recompiled objects
            return false;
        }

        static UClass* InterfaceClass = UUnLuaNovaInterface::StaticClass();
        const bool bImplUnluaInterface = Class->ImplementsInterface(InterfaceClass);
        const bool bDynamicBinding = GLuaDynamicBinding.IsValid(Class);

        // 数据驱动绑定：没实现接口的对象要不要也问 locator 要模块名
        // （ini 的 ClassModuleBindings / Actor 上的 UUnLuaNovaBindingComponent），
        // 由配置决定而不是代码里的常量 —— 见 UUnLuaNovaSettings::bDataDrivenBinding。
        // 默认关，所以老行为逐字不变。
        const bool bCanLocateWithoutInterface =
            ModuleLocator
            && GetDefault<UUnLuaNovaSettings>()
            && GetDefault<UUnLuaNovaSettings>()->bDataDrivenBinding;

        if (IsInAsyncLoadingThread())
        {
            // avoid adding too many objects, affecting performance.
            if (bImplUnluaInterface || bDynamicBinding || bCanLocateWithoutInterface)
            {
                // all bind operation should be in game thread, include dynamic bind
                FScopeLock Lock(&CandidatesLock);
                Candidates.AddUnique(Object);
            }
            return false;
        }

        if (!bImplUnluaInterface)
        {
            // dynamic binding
            if (bDynamicBinding)
                return GetManager()->Bind(Object, *GLuaDynamicBinding.ModuleName, GLuaDynamicBinding.InitializerTableRef);

            // 数据驱动绑定：模块名由 locator 决定，拿不到就不绑（与旧行为一致）
            if (bCanLocateWithoutInterface)
            {
                const auto ModuleName = ModuleLocator->Locate(Object);
                if (ModuleName.IsEmpty())
                    return false;

                return GetManager()->Bind(Object, *ModuleName, GLuaDynamicBinding.InitializerTableRef);
            }

            return false;
        }

        if (Class->GetName().Contains(TEXT("SKEL_")))
            return false;

        if (!ensureMsgf(ModuleLocator, TEXT("Invalid lua module locator, lua binding will not work properly. please check UnLuaNova runtime settings.")))
            return false;

        const auto ModuleName = ModuleLocator->Locate(Object);
        if (ModuleName.IsEmpty())
            return false;

#if !UE_BUILD_SHIPPING
        if (GLuaDynamicBinding.IsValid(Class) && GLuaDynamicBinding.ModuleName != ModuleName)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("Dynamic binding '%s' ignored as it conflicts static binding '%s'."), *GLuaDynamicBinding.ModuleName, *ModuleName);
        }
#endif

        return GetManager()->Bind(Object, *ModuleName, GLuaDynamicBinding.InitializerTableRef);
    }

    bool FLuaEnv::DoString(const FString& Chunk, const FString& ChunkName)
    {
        const FTCHARToUTF8 ChunkUTF8(*Chunk);
        const FTCHARToUTF8 ChunkNameUTF8(*ChunkName);
        const auto Guard = GetDeadLoopCheck()->MakeGuard();
        const auto DanglingGuard = GetDanglingCheck()->MakeGuard();
        lua_pushcfunction(L, ReportLuaCallError);
        const auto MsgHandlerIdx = lua_gettop(L);
        if (!LoadBuffer(L, ChunkUTF8.Get(), ChunkUTF8.Length(), ChunkNameUTF8.Get()))
        {
            lua_pop(L, 1);
            return false;
        }

        const auto Result = lua_pcall(L, 0, LUA_MULTRET, MsgHandlerIdx);
        if (Result == LUA_OK)
        {
            lua_remove(L, MsgHandlerIdx);
            return true;
        }
        lua_pop(L, lua_gettop(L) - MsgHandlerIdx + 1);
        return false;
    }

    bool FLuaEnv::LoadBuffer(lua_State* InL, const char* Buffer, const size_t Size, const char* InName)
    {
        // Chunks are always compiled against the global environment - there is no _ENV
        // override - so the sandbox applies to every chunk the same way. On success the
        // compiled chunk is left on the stack for the caller to pcall (see RunChunk); on a
        // syntax error the message goes through ReportLuaCallError and nil is pushed
        // underneath it.

#if UNLUANOVA_LEGACY_ALLOW_BOM || WITH_EDITOR
        if (Size > 3 && Buffer[0] == static_cast<char>(0xEF) && Buffer[1] == static_cast<char>(0xBB) && Buffer[2] == static_cast<char>(0xBF))
        {
#if !UNLUANOVA_LEGACY_ALLOW_BOM
            UE_LOG(LogUnLuaNova, Warning, TEXT("Lua chunk with utf-8 BOM:%s"), UTF8_TO_TCHAR(InName));
#endif
            return LoadBuffer(InL, Buffer + 3, Size - 3, InName);
        }
#endif

        // loads the buffer as a Lua chunk
        const int32 Code = luaL_loadbufferx(InL, Buffer, Size, InName, nullptr);
        if (Code != LUA_OK)
        {
            // One report per failure, and it is the compiler's: ReportLuaCallError logs the
            // message below, which carries the chunk name and the line. The status code on
            // its own - 3 is LUA_ERRSYNTAX - carries neither, so it goes out at Verbose for
            // whoever is debugging the loader instead of as a second line at Warning. That
            // second line is what a test that feeds LoadBuffer a broken chunk on purpose
            // trips over: it declares the Error-level message expected, and the Warning is
            // still there, unnamed and unmatched.
            UE_LOG(LogUnLuaNova, Verbose, TEXT("Failed to call luaL_loadbufferx for '%s', error code: %d"), UTF8_TO_TCHAR(InName), Code);
            ReportLuaCallError(InL); // report pcall error
            lua_pushnil(InL); /* error (message is on top of the stack) */
            lua_insert(InL, -2); /* put before error message */
            return false;
        }

        // The chunk function is on top of the stack and not running yet, which is the one
        // moment Luau's native compiler will accept it. No-op unless lua.codegen is on.
        UnLuaNova::MaybeCompileNativeChunk(InL, InName);

        return true;
    }

    void FLuaEnv::GC()
    {
        lua_gc(L, LUA_GCCOLLECT, 0);
        lua_gc(L, LUA_GCCOLLECT, 0);

        // Keep the scheduler's bookkeeping honest: a manual collect settles debt the
        // scheduler would otherwise still try to pay off over the next few frames.
        if (GcScheduler)
            GcScheduler->Collect();
    }

    void FLuaEnv::SetGCMode(ELuaGcMode InMode)
    {
        if (GcScheduler)
            GcScheduler->SetMode(InMode);
    }

    ELuaGcMode FLuaEnv::GetGCMode() const
    {
        return GcScheduler ? GcScheduler->GetMode() : ELuaGcMode::Automatic;
    }

    void FLuaEnv::SetGCStepBudget(int64 BytesPerFrame)
    {
        if (GcScheduler)
            GcScheduler->SetMaxStepBytesPerFrame(BytesPerFrame);
    }

    void FLuaEnv::SetGCStepMultiplier(int32 Percent)
    {
        if (GcScheduler)
            GcScheduler->SetStepMultiplier(Percent);
    }

    void FLuaEnv::SetGCHardLimit(int64 Bytes)
    {
        if (GcScheduler)
            GcScheduler->SetHardLimitBytes(Bytes);
    }

    void FLuaEnv::TickGC(float DeltaSeconds)
    {
        if (GcScheduler)
            GcScheduler->Tick(DeltaSeconds);
    }

    const FLuaGcScheduler::FStats& FLuaEnv::GetGCStats() const
    {
        static const FLuaGcScheduler::FStats Empty;
        return GcScheduler ? GcScheduler->GetStats() : Empty;
    }

    void FLuaEnv::OnEndFrame()
    {
        // Nothing to do in the default mode; the call is cheap enough to leave
        // unconditional rather than add/remove the delegate on every mode switch.
        TickGC();
    }

    void FLuaEnv::HotReload()
    {
        DoString("Lua.HotReload()");
    }

    int32 FLuaEnv::FindThread(const lua_State* Thread)
    {
        int32* ThreadRefPtr = ThreadToRef.Find(Thread);
        return ThreadRefPtr ? *ThreadRefPtr : LUA_REFNIL;
    }

    void FLuaEnv::ResumeThread(int32 ThreadRef)
    {
        lua_State** ThreadPtr = RefToThread.Find(ThreadRef);
        if (!ThreadPtr)
            return;

        lua_State* Thread = *ThreadPtr;
        int NResults = 0;
        const int32 Status = UNLUANOVA_RESUME(Thread, L, 0, &NResults);
        if (Status == LUA_YIELD)
            return;

        if (Status != LUA_OK)
        {
            const auto ErrMsg = lua_tostring(Thread, -1);
            UE_LOG(LogUnLuaNova, Error, TEXT("%s"), UTF8_TO_TCHAR(ErrMsg));
        }

        ThreadToRef.Remove(Thread);
        RefToThread.Remove(ThreadRef);
        luaL_unref(L, LUA_REGISTRYINDEX, ThreadRef); // remove the reference if the coroutine finishes its execution
    }

    UUnLuaNovaManager* FLuaEnv::GetManager()
    {
        if (!Manager)
        {
            Manager = NewObject<UUnLuaNovaManager>();
            Manager->Env = this;
            Manager->AddToRoot();
        }
        return Manager;
    }

    void FLuaEnv::AddThread(lua_State* Thread, int32 ThreadRef)
    {
        ThreadToRef.Add(Thread, ThreadRef);
        RefToThread.Add(ThreadRef, Thread);
    }

    int32 FLuaEnv::FindOrAddThread(lua_State* Thread)
    {
        int32 ThreadRef = FindThread(Thread);
        if (ThreadRef == LUA_REFNIL)
        {
            int32 Value = lua_pushthread(Thread);
            if (Value == 1)
            {
                lua_pop(Thread, 1);
                return LUA_REFNIL;
            }

            ThreadRef = luaL_ref(Thread, LUA_REGISTRYINDEX);
            AddThread(Thread, ThreadRef);
        }
        return ThreadRef;
    }

    lua_Alloc FLuaEnv::GetLuaAllocator() const
    {
        return DefaultLuaAllocator;
    }

    void FLuaEnv::AddLoader(const FLuaFileLoader Loader)
    {
        CustomLoaders.Add(Loader);
    }

    void FLuaEnv::AddBuiltInLoader(const FString InName, const lua_CFunction Loader)
    {
        BuiltinLoaders.Add(InName, Loader);
    }

    void FLuaEnv::AddManualObjectReference(UObject* Object)
    {
        ManualObjectReference.Add(Object);
    }

    void FLuaEnv::RemoveManualObjectReference(UObject* Object)
    {
        ManualObjectReference.Remove(Object);
    }

    int FLuaEnv::LoadFromPreload(lua_State* L)
    {
        // package.preload[name] - the standard way for a host (or a test) to supply a
        // module without there being a file for it. Returning nothing means "not mine",
        // and Require moves on to the next searcher.
        lua_getglobal(L, "package");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return 0;
        }

        lua_getfield(L, -1, "preload");
        lua_remove(L, -2); // package
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return 0;
        }

        lua_pushvalue(L, 1); // the module name
        lua_rawget(L, -2);   // preload[name]

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 2); // nil and preload
            return 0;
        }

        return 1; // the loader function, on top of the stack where pcall wants it
    }

    int FLuaEnv::LoadFromBuiltinLibs(lua_State* L)
    {
        const FLuaEnv* Env = (FLuaEnv*)lua_touserdata(L, lua_upvalueindex(1));
        const FString Name = UTF8_TO_TCHAR(lua_tostring(L, 1));
        const auto Loader = Env->BuiltinLoaders.Find(Name);
        if (!Loader)
            return 0;
        lua_pushcfunction(L, *Loader);
        return 1;
    }

    int FLuaEnv::LoadFromCustomLoader(lua_State* L)
    {
        auto& Env = *(FLuaEnv*)lua_touserdata(L, lua_upvalueindex(1));
        if (FUnLuaNovaDelegates::CustomLoadLuaFile.IsBound())
        {
            // legacy support
            const FString FileName(UTF8_TO_TCHAR(lua_tostring(L, 1)));
            TArray<uint8> Data;
            FString ChunkName(TEXT("chunk"));
            if (FUnLuaNovaDelegates::CustomLoadLuaFile.Execute(Env, FileName, Data, ChunkName))
            {
                if (Env.LoadString(L, Data, ChunkName))
                    return 1;

                return luaL_error(L, "file loading from custom loader error");
            }
            return 0;
        }

        if (Env.CustomLoaders.Num() == 0)
            return 0;

        const FString FileName(UTF8_TO_TCHAR(lua_tostring(L, 1)));

        TArray<uint8> Data;
        FString ChunkName(TEXT("chunk"));
        for (auto& Loader : Env.CustomLoaders)
        {
            if (!Loader.Execute(Env, FileName, Data, ChunkName))
                continue;

            if (Env.LoadString(L, Data, ChunkName))
                break;

            return luaL_error(L, "file loading from custom loader error");
        }

        return 1;
    }

    int FLuaEnv::LoadFromFileSystem(lua_State* L)
    {
        FString FileName(UTF8_TO_TCHAR(lua_tostring(L, 1)));
        FileName.ReplaceInline(TEXT("."), TEXT("/"));

        auto& Env = *(FLuaEnv*)lua_touserdata(L, lua_upvalueindex(1));
        TArray<uint8> Data;
        FString FullPath;

        auto LoadIt = [&]
        {
            if (Env.LoadString(L, Data, FullPath))
                return 1;
            const auto Msg = FString::Printf(TEXT("file loading from file system error.\nfull path:%s"), *FullPath);
            return luaL_error(L, TCHAR_TO_UTF8(*Msg));
        };

        const auto PackagePath = UnLuaNovaLib::GetPackagePath(L);
        if (PackagePath.IsEmpty())
            return 0;

        TArray<FString> Patterns;
        if (PackagePath.ParseIntoArray(Patterns, TEXT(";"), false) == 0)
            return 0;

        // 优先加载下载目录下的单文件
        for (auto& Pattern : Patterns)
        {
            Pattern.ReplaceInline(TEXT("?"), *FileName);
            const auto PathWithPersistentDir = FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), Pattern);
            FullPath = FPaths::ConvertRelativePathToFull(PathWithPersistentDir);
            if (FFileHelper::LoadFileToArray(Data, *FullPath, FILEREAD_Silent))
                return LoadIt();
        }

        // 其次是打包目录下的文件
        for (auto& Pattern : Patterns)
        {
            const auto PathWithProjectDir = FPaths::Combine(FPaths::ProjectDir(), Pattern);
            FullPath = FPaths::ConvertRelativePathToFull(PathWithProjectDir);
            if (FFileHelper::LoadFileToArray(Data, *FullPath, FILEREAD_Silent))
                return LoadIt();
        }

        return 0;
    }

    void FLuaEnv::AddSearcher(lua_CFunction Searcher, int Index) const
    {
        lua_getglobal(L, "package");

        // Check the table BEFORE indexing into it. Luau has no package library, so
        // "package" is nil here and lua_getfield would raise an index error - and because
        // this runs straight from C++ with no protected call around it, Luau has no
        // errorJmp to unwind to and aborts the whole process. Plain Lua always had a
        // package table, which is why the old ordering never showed the problem.
        //
        // Skipping is safe on Luau: nothing consults package.searchers there, module
        // resolution goes through LoadFromCustomLoader / LoadFromFileSystem /
        // LoadFromBuiltinLibs, which UnLuaNova calls directly.
        if (!lua_istable(L, -1))
        {
            UE_LOG(LogUnLuaNova, Verbose, TEXT("No package table on this Lua backend; skipping searcher registration."));
            lua_pop(L, 1);
            return;
        }

        lua_getfield(L, -1, "searchers");
        lua_remove(L, -2);
        if (!lua_istable(L, -1))
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("Invalid package.searchers!"));
            return;
        }

        const uint32 Len = lua_rawlen(L, -1);
        Index = Index < 0 ? (int)(Len + Index + 2) : Index;
        // Plain Lua's package.searchers starts with four entries of its own, so the
        // indices above land inside a populated list. On Luau the list starts empty, and
        // writing to index 2 of an empty list leaves a hole at 1 - every ipairs() over it
        // then stops immediately. Keep the list dense instead.
        Index = FMath::Min(Index, (int)Len + 1);
        for (int e = (int)Len + 1; e > Index; e--)
        {
            lua_rawgeti(L, -1, e - 1);
            lua_rawseti(L, -2, e);
        }

        lua_pushlightuserdata(L, (void*)this);
        lua_pushcclosure(L, Searcher, 1);
        lua_rawseti(L, -2, Index);
        lua_pop(L, 1);
    }

    int FLuaEnv::Require(lua_State* L)
    {
        // Upvalue 1 is the module cache - the same table published as package.loaded.
        // The searchers carry their own FLuaEnv upvalue, so this one needs none.
        const int Name = 1;

        if (!lua_isstring(L, Name))
            return luaL_error(L, "require: module name expected");

        lua_pushvalue(L, lua_upvalueindex(1));   // 2: cache
        lua_pushvalue(L, Name);                  // 3: name
        lua_rawget(L, 2);                        // 3: cache[name]
        if (!lua_isnil(L, 3))
            return 1;                            // already loaded - do not run it again
        lua_pop(L, 2);                           // drop cache and the nil, back to the name

        // Walk package.searchers the way plain Lua's require does, rather than a private
        // list of the same loaders. Two reasons: the order then stays whatever AddSearcher
        // built, and a searcher a script installs afterwards is picked up here too -
        // HotReload.lua replaces require() precisely so it can do that.
        lua_getglobal(L, "package");             // 2: package
        lua_getfield(L, 2, "searchers");         // 3: searchers
        lua_remove(L, 2);                        // 2: searchers
        const int Searchers = 2;
        const int Chunk = 3;

        const int NumSearchers = (int)lua_rawlen(L, Searchers);
        for (int i = 1; i <= NumSearchers; i++)
        {
            lua_rawgeti(L, Searchers, i);        // Chunk: searcher
            lua_pushvalue(L, Name);              // Chunk+1: module name
            if (lua_pcall(L, 1, 1, 0) != 0)
                return luaL_error(L, "error loading module '%s': %s", lua_tostring(L, Name), lua_tostring(L, -1));

            if (lua_isfunction(L, Chunk))
                break;

            lua_pop(L, 1);                       // nil: this searcher does not handle it
        }

        if (!lua_isfunction(L, Chunk))
            return luaL_error(L, "module '%s' not found", lua_tostring(L, Name));

        // Run the chunk with the module name as its single argument, so one chunk can be
        // shared by several names the way Lua's own loaders are.
        lua_pushvalue(L, Name);                  // Chunk+1: name
        if (lua_pcall(L, 1, 1, 0) != 0)
            return luaL_error(L, "error loading module '%s': %s", lua_tostring(L, Name), lua_tostring(L, -1));

        // Cache the result. Lua stores true for a module that returns nothing, so a second
        // require() is a no-op instead of re-executing the module.
        if (lua_isnil(L, Chunk))
        {
            lua_pushboolean(L, 1);
            lua_replace(L, Chunk);
        }

        lua_pushvalue(L, lua_upvalueindex(1));   // cache
        lua_pushvalue(L, Name);                  // name
        lua_pushvalue(L, Chunk);                 // module
        lua_rawset(L, -3);
        lua_settop(L, Chunk);
        return 1;
    }

    void FLuaEnv::OnAsyncLoadingFlushUpdate()
    {
        TArray<FWeakObjectPtr> CandidatesTemp;
        TArray<int> CandidatesRemovedIndexes;

        TArray<UObject*> LocalCandidates;
        {
            {
                FScopeLock Lock(&CandidatesLock);
                CandidatesTemp.Append(Candidates);
            }


            for (int32 i = CandidatesTemp.Num() - 1; i >= 0; --i)
            {
                FWeakObjectPtr ObjectPtr = CandidatesTemp[i];
                if (!ObjectPtr.IsValid())
                {
                    // discard invalid objects
                    CandidatesRemovedIndexes.Add(i);
                    continue;
                }

                UObject* Object = ObjectPtr.Get();
                if (Object->HasAnyFlags(RF_NeedPostLoad)
                    || Object->HasAnyInternalFlags(AsyncObjectFlags)
                    || Object->GetClass()->HasAnyInternalFlags(AsyncObjectFlags))
                {
                    // delay bind on next update 
                    continue;
                }

                LocalCandidates.Add(Object);
                CandidatesRemovedIndexes.Add(i);
            }
        }

        {
            FScopeLock Lock(&CandidatesLock);
            for (int32 j = 0; j < CandidatesRemovedIndexes.Num(); ++j)
            {
                Candidates.RemoveAt(CandidatesRemovedIndexes[j]);
            }
        }

        for (int32 i = 0; i < LocalCandidates.Num(); ++i)
        {
            UObject* Object = LocalCandidates[i];
            TryBind(Object);
        }
    }

    FORCEINLINE void FLuaEnv::RegisterDelegates()
    {
        OnAsyncLoadingFlushUpdateHandle = FCoreDelegates::OnAsyncLoadingFlushUpdate.AddRaw(this, &FLuaEnv::OnAsyncLoadingFlushUpdate);
        // The per-frame GC slice needs a per-frame callback. OnEndFrame rather than a
        // world tick: it also runs in PIE-less editor worlds and in standalone games
        // between level loads, none of which have a world ticking every frame.
        OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLuaEnv::OnEndFrame);
        GUObjectArray.AddUObjectDeleteListener(this);
        bObjectArrayListenerRegistered = true;
    }

    FORCEINLINE void FLuaEnv::UnRegisterDelegates()
    {
        FCoreDelegates::OnAsyncLoadingFlushUpdate.Remove(OnAsyncLoadingFlushUpdateHandle);
        FCoreDelegates::OnEndFrame.Remove(OnEndFrameHandle);
        if (!bObjectArrayListenerRegistered)
            return;
        GUObjectArray.RemoveUObjectDeleteListener(this);
        bObjectArrayListenerRegistered = false;
    }

    void* FLuaEnv::DefaultLuaAllocator(void* ud, void* ptr, size_t osize, size_t nsize)
    {
        FLuaMemoryStats& Stats = FLuaMemoryStats::Get();

        if (nsize == 0)
        {
            if (ptr) Stats.TrackFree(osize);
            UNLUANOVA_STAT_MEMORY_FREE(ptr, Lua);
            FMemory::Free(ptr);
            return nullptr;
        }

        void* Buffer;
        if (!ptr)
        {
            Buffer = FMemory::Malloc(nsize);
            UNLUANOVA_STAT_MEMORY_ALLOC(Buffer, Lua);
        }
        else
        {
            UNLUANOVA_STAT_MEMORY_REALLOC(ptr, Buffer, Lua);
            Buffer = FMemory::Realloc(ptr, nsize);
        }

        // Lua passes a type tag in osize for some fresh allocations. A failed realloc
        // leaves the old block live and must not change accounting.
        if (Buffer) Stats.TrackAlloc(ptr ? osize : 0, nsize);
        return Buffer;
    }
}
