-- Luau declarations for the script-facing API - the Luau twin of UnLuaNova.lua in this same
-- directory (that one is EmmyLua, for LuaLS and IDE completion; this one is Luau, for
-- luau-analyze and Luau's language server). Two dialects, one API: when the API changes, both
-- files have to change.
--
-- Hand-written because the API is a Lua table the runtime injects, so reflection cannot see
-- it, and its shapes (a class table, a proxy object) have no UE type behind them. The members
-- mirror UnLuaNovaLib.cpp's UNLUANOVA_Functions, plus Class and PackagePath, which Open()
-- copies onto the same table.
--
-- Exported by the editor into IntelliSense/Luau/UnLuaNova.d.lua, appended to the reflected
-- engine types. Nothing should require this file at runtime.

declare extern type LuaLib with
    -- Where LoadFromFileSystem looks for modules; see FLuaEnv::LoadFromFileSystem.
    PackagePath: string

    -- When true, FText stops being treated as a string at runtime.
    FTextEnabled: boolean

    -- Defines a Lua class for binding. Returns the class table.
    Class: (string?) -> any

    -- Hot reloads modules; without a list it reloads whatever changed on disk.
    HotReload: ({ any }?) -> ()

    -- "Luau" or "Lua" - which backend this build runs.
    GetBackend: () -> "Lua" | "Luau"

    -- Capability check: "sandbox", "breakpoint", "allocCallbacks", "vector", "buffer".
    -- Unknown names answer false rather than raising, so asking about a newer build's flag
    -- degrades instead of dying.
    HasFeature: (string) -> boolean

    -- Logs through UE's logging; `print` is registered to the same function.
    Log: (...any) -> ()
    LogWarn: (...any) -> ()
    LogError: (...any) -> ()

    -- Adds a manual reference so UE's GC keeps the object; returns the proxy that holds it.
    -- Dropping the proxy releases the reference.
    -- `Object`, not `UObject`: the declarations are named the way reflection reports them,
    -- which is without the UE prefix (UObject -> Object, FVector -> Vector).
    Ref: (Object) -> any

    -- Removes every manual reference to the object.
    Unref: (Object) -> ()
end

-- Lua is the primary global; UnLuaNova is registered as an alias at runtime (see
-- UnLuaNovaLib.cpp), so either spelling reaches this same table.
declare Lua: LuaLib
declare UnLuaNova: LuaLib

-- Type annotation for require('UnLuaNova.Timer'); callbacks cannot yield.
declare extern type LuaTimerLib with
    Set: (() -> (), number?, boolean?, number?, boolean?, Object?) -> number
    Clear: (number) -> boolean
    Pause: (number, boolean) -> boolean
    IsValid: (number) -> boolean
    ClearAll: (Object?) -> ()
    Count: (Object?) -> number
    Time: (boolean?, Object?) -> number
end
