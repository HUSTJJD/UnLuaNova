-- SmokeExtensions.lua -- MANUAL smoke check for the three optional extension plugins.
--
-- This is NOT a UE automation test. It registers no UnLuaNova* automation case, is not
-- referenced by any C++ *.spec.cpp, and does not change the automation case-name set in
-- Tools/baseline/{lua,lua-game,luau,luau-game}.json. Running it therefore does not affect
-- the --fail-on-drift gate and does not need a UE build. See README.md next to this file.
--
-- It is meant to be executed by a person from the editor Lua console / PIE. Each probe is
-- isolated with pcall, so a disabled or broken plugin yields a diagnostic ok=false/err
-- entry instead of aborting the whole script. The final return value is a summary table.
--
-- Modules are the real built-in loader names registered by the plugins' *Module.cpp:
--   pb           (LuaProtobuf,  also pb.unsafe)
--   rapidjson    (LuaRapidjson)
--   socket.core  (LuaSocket,    also socket / mime.core)

local Smoke = {}

-- Run one probe in isolation. Never raises: a disabled module (require fails) is reported
-- as ok=false with the original error text, which is what makes the result diagnosable.
local function Probe(Name, Fn)
    local Ok, Result = pcall(Fn)
    if Ok then
        return { name = Name, ok = true, detail = Result }
    end
    return { name = Name, ok = false, err = tostring(Result) }
end

-- protobuf: load a tiny schema with the vendored protoc.lua helper, then do a real
-- encode/decode round-trip. protoc.lua is shipped by LuaProtobuf under Content/Script,
-- which LuaProtobufModule.cpp appends to UnLuaNova.PackagePath at env creation.
local function CheckProtobuf()
    local pb = require("pb")
    local protoc = require("protoc")
    assert(protoc:load([[
        message SmokeMessage {
            optional int32 number = 1;
            optional string label = 2;
        }
    ]], "smoke.proto"))
    local bytes = assert(pb.encode("SmokeMessage", { number = 42, label = "ok" }))
    local back = assert(pb.decode("SmokeMessage", bytes))
    assert(back.number == 42 and back.label == "ok", "protobuf round-trip mismatch")
    return "pb.encode/pb.decode round-trip ok (" .. tostring(#bytes) .. " bytes)"
end

-- JSON: a real encode/decode round-trip through lua-rapidjson.
local function CheckRapidjson()
    local rapidjson = require("rapidjson")
    local encoded = assert(rapidjson.encode({ number = 7, label = "ok" }))
    local back = assert(rapidjson.decode(encoded))
    assert(back.number == 7 and back.label == "ok", "JSON round-trip mismatch")
    return "rapidjson.encode/rapidjson.decode ok (" .. encoded .. ")"
end

-- socket: create a basic TCP object from the C core. No network I/O is attempted; object
-- creation alone proves the built-in loader is wired up. close() is best-effort.
local function CheckSocket()
    local core = require("socket.core")
    local skt = assert(core.tcp(), "socket.core.tcp() returned nil")
    pcall(function() skt:close() end)
    return "socket.core.tcp() object created"
end

-- Returns a summary table and, when a console print function exists, a human-readable
-- line per module. Load the module Tests.Extensions.SmokeExtensions from the editor Lua
-- console and call run() on the returned table (see README.md for the exact command).
function Smoke.run()
    local Results = {
        Probe("pb", CheckProtobuf),
        Probe("rapidjson", CheckRapidjson),
        Probe("socket.core", CheckSocket),
    }

    local AllOk = true
    for _, R in ipairs(Results) do
        if not R.ok then
            AllOk = false
        end
        if type(print) == "function" then
            local Line = R.ok
                and (R.name .. ": ok - " .. tostring(R.detail))
                or (R.name .. ": ERR - " .. tostring(R.err))
            print("[SmokeExtensions] " .. Line)
        end
    end

    return {
        all_ok = AllOk,
        modules = Results,
        note = "manual smoke; not a UE automation case; does not touch baseline case lists",
    }
end

return Smoke
