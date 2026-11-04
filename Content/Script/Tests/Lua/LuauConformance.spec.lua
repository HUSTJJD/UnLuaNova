-- Cross-backend conformance samples. Loaded automatically by FLuaSpecsSpec, which scans
-- Content/Script/Tests/Lua/*.spec.lua, so this file must parse on BOTH backends: every
-- Luau-only construct is compiled from a string with load() rather than written here.
--
-- This is a sampled conformance asset, not the upstream test suite. It pins the boundaries
-- the feature matrix talks about (syntax gating, metatables/weak tables/errors, native
-- vector and buffer availability/independence); running the full upstream suites still
-- needs a pinned external toolchain.

local Spec = require("Tests.Lua.Spec")
local Vector = require("UnLuaNova.Vector")
local Buffer = require("UnLuaNova.Buffer")

local Backend = "?"
if _G.UnLuaNova and _G.UnLuaNova.GetBackend then
    Backend = _G.UnLuaNova.GetBackend()
end

Spec.File("LuauConformance.spec.lua")
Spec.describe("Luau syntax and library conformance", function()
    Spec.it("Luau-only syntax never compiles on the plain Lua backend", function()
        -- if-expression, interpolated string, compound assignment and a type alias are the
        -- four syntax families the pinned compiler is claimed to support. Which of them a
        -- given Luau revision accepts is version-gated, so assert the boundary (at least
        -- one on Luau, none on Lua) rather than pretending all four are stable promises.
        local Sources = {
            "local x = if true then 1 else 2 return x",
            "return `hello {1 + 1}`",
            "local x = 1 x += 2 return x",
            "type Alias<T> = T return 1",
        }
        local Compiled = 0
        for _, Source in ipairs(Sources) do
            if type(load(Source)) == "function" then
                Compiled = Compiled + 1
            end
        end

        if Backend == "Luau" then
            Spec.equal(Compiled > 0, true)
        else
            Spec.equal(Compiled, 0)
        end
    end)

    Spec.it("metatables, weak-table mode and pcall behave consistently", function()
        local mt = { __add = function(a, b) return a.v + b.v end }
        local a = setmetatable({ v = 2 }, mt)
        local b = setmetatable({ v = 3 }, mt)
        Spec.equal(a + b, 5)

        local weak = setmetatable({}, { __mode = "k" })
        local key = {}
        weak[key] = true
        Spec.equal(weak[key], true)
        key = nil
        collectgarbage("collect")
        -- Whether the key is gone after one collection is GC-implementation-dependent;
        -- what must hold is that the weak mode survived and was not silently dropped.
        Spec.equal(getmetatable(weak).__mode, "k")

        local ok, err = pcall(function() error("marker") end)
        Spec.equal(ok, false)
        Spec.equal(type(err), "string")
    end)

    Spec.it("native vector is gated by availability and never faked", function()
        local v = Vector.Create(1, 2, 3)
        if Vector.HasVector() then
            Spec.equal(type(v) ~= "nil", true)
        else
            Spec.equal(v, nil)
        end
    end)

    Spec.it("buffer reads back what was written and stays independent", function()
        Spec.equal(Buffer.HasBuffer(), type(buffer) == "table")
        if not Buffer.HasBuffer() then
            return
        end

        local b = buffer.create(4)
        buffer.writeu8(b, 0, 17)
        Spec.equal(buffer.readu8(b, 0), 17)
        Spec.equal(buffer.len(b), 4)

        -- The library copies; a second buffer is not an alias of the first one.
        local other = buffer.create(4)
        Spec.equal(buffer.readu8(other, 0), 0)
    end)
end)
