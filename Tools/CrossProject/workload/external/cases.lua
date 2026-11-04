-- Cross-project workload — external-binding adapter (slua-unreal / FUnLua).
--
-- Copy this file into the target project's script path (e.g. CrossProjectBench.lua).
-- Configure it before running:
--     Bench = require("CrossProjectBench")
--     Bench.SetProject("slua-unreal", "Lua", "5.8.3")
--     Bench.SetMachineInfo("my-pc", "AMD-Ryzen-7-4700G")
--     Bench.Run()
--
-- The 10 cases, their iteration counts, the warmup/sampling rules and the printed
-- #UNLUANOVA_BENCH_* contract are identical to workload/unluanova/cases.lua; see
-- Tools/CrossProject/WORKLOAD.md (revision 1) and runner.py. Do not delete a case:
-- a case whose UE API is unreachable is emitted with peropus=0 and a
-- "-- case failed:" line, which makes the runner refuse the comparison. Fix the
-- adapter hook instead — dropping a slow case is how fake "faster" tables are made.
--
-- UE access differs between bindings, so every engine call goes through UEAPI.
-- FUnLua exposes UE/UE4 globals; slua-unreal exposes import() and slua globals.
-- UEAPI.Static / UEAPI.Vector try both shapes and fail loudly if neither is
-- reachable. If your project uses a different spelling, edit ONLY those two hooks
-- (and document the spelling in the comparison report) — never change the case set.

local Bench = {}

Bench.Machine = nil
Bench.Cpu = nil
Bench.Project = nil
Bench.Backend = nil
Bench.Engine = nil
Bench.Config = "Development"
Bench.Carrier = "Editor"           -- or Game
Bench.Revision = "1"

local function now()
    -- CPU time, not wall time, matching the UnLuaNova workload.
    return os.clock()
end

-- ---------------------------------------------------------------------------
-- Binding adapter: the only project-specific code in this file.
-- ---------------------------------------------------------------------------

local UEAPI = {}

local function Roots()
    return { rawget(_G, "UE"), rawget(_G, "UE4"), rawget(_G, "slua") }
end

--- Call a static UFUNCTION, e.g. UKismetMathLibrary.DegreesToRadians.
function UEAPI.Static(Class, Method, ...)
    for _, Root in ipairs(Roots()) do
        if Root ~= nil then
            local reached, Lib = pcall(function() return Root[Class] end)
            if reached and Lib ~= nil then
                local found, Fn = pcall(function() return Lib[Method] end)
                if found and type(Fn) == "function" then
                    return Fn(...)
                end
            end
        end
    end
    if type(rawget(_G, "import")) == "function" then
        local imported, Lib = pcall(import, Class)
        if imported and Lib ~= nil then
            local found, Fn = pcall(function() return Lib[Method] end)
            if found and type(Fn) == "function" then
                return Fn(...)
            end
        end
    end
    error(Class .. "." .. Method .. " is not reachable in this binding")
end

--- Construct an FVector. Bindings expose it as a callable global, a .New static,
--- or an import()-ed type; try all of them.
function UEAPI.Vector(X, Y, Z)
    local function Construct(Ctor)
        if Ctor == nil then return nil end
        local direct, Value = pcall(function() return Ctor(X, Y, Z) end)
        if direct and Value ~= nil then return Value end
        local viaNew, Value2 = pcall(function() return Ctor.New(X, Y, Z) end)
        if viaNew and Value2 ~= nil then return Value2 end
        return nil
    end

    for _, Root in ipairs(Roots()) do
        if Root ~= nil then
            local reached, Ctor = pcall(function() return Root.FVector or Root.Vector end)
            if reached then
                local Value = Construct(Ctor)
                if Value ~= nil then return Value end
            end
        end
    end
    if type(rawget(_G, "import")) == "function" then
        local imported, Ctor = pcall(import, "Vector")
        if imported then
            local Value = Construct(Ctor)
            if Value ~= nil then return Value end
        end
    end
    error("FVector constructor is not reachable in this binding")
end

-- ---------------------------------------------------------------------------
-- Cases. Each Fn(n) performs n operations.
-- ---------------------------------------------------------------------------

Bench.Cases = {
    {
        Name = "VM.Arithmetic", Iterations = 200000,
        Fn = function(n)
            local acc = 0.0
            for i = 1, n do acc = acc + i * 1.5 end
            return acc
        end,
    },
    {
        Name = "VM.Call", Iterations = 200000,
        Fn = function(n)
            local function empty() end
            for _ = 1, n do empty() end
        end,
    },
    {
        Name = "VM.TableRead", Iterations = 200000,
        Fn = function(n)
            local t = { a = 1, b = 2, c = 3, d = 4, e = 5 }
            local acc = 0
            for i = 1, n do acc = acc + t[(i % 5) + 1] end
            return acc
        end,
    },
    {
        Name = "VM.TableGrow", Iterations = 200000,
        Fn = function(n)
            local t = {}
            for i = 1, n do t[i] = i end
            return t
        end,
    },
    {
        Name = "VM.StringConcat", Iterations = 100000,
        Fn = function(n)
            local s = ""
            for i = 1, n do s = s .. "x" end
            return s
        end,
    },
    {
        Name = "UE.CallMath1", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for _ = 1, n do
                acc = acc + UEAPI.Static("UKismetMathLibrary", "DegreesToRadians", 1.0)
            end
            return acc
        end,
    },
    {
        Name = "UE.CallMath4", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for _ = 1, n do
                acc = acc + UEAPI.Static("UKismetMathLibrary", "FInterpTo", 0.0, 1.0, 0.016, 1.0)
            end
            return acc
        end,
    },
    {
        Name = "UE.StructRoundTrip", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for _ = 1, n do
                local v = UEAPI.Vector(1.0, 2.0, 3.0)
                local r = UEAPI.Static("UKismetMathLibrary", "Add_VectorVector", v, v)
                acc = acc + r.X
            end
            return acc
        end,
    },
    {
        Name = "UE.StringRoundTrip", Iterations = 20000,
        Fn = function(n)
            local acc = 0
            for _ = 1, n do
                local text = UEAPI.Static("UKismetTextLibrary", "Conv_StringToText", "bench")
                acc = acc + #UEAPI.Static("UKismetTextLibrary", "Conv_TextToString", text)
            end
            return acc
        end,
    },
    {
        Name = "GC.Collect", Iterations = 200,
        Fn = function(n)
            for _ = 1, n do collectgarbage("collect") end
        end,
    },
}

-- ---------------------------------------------------------------------------
-- Harness (identical to the UnLuaNova workload)
-- ---------------------------------------------------------------------------

local WARMUP_MS = 250
local SAMPLE_MS = 100
local SAMPLES = 5

local function time_batch(fn, iterations)
    local t0 = now()
    fn(iterations)
    return now() - t0
end

function Bench.Measure(fn, iterations)
    local warm = 0.0
    while warm * 1000.0 < WARMUP_MS do
        warm = warm + time_batch(fn, iterations)
    end

    local per_op = {}
    for _ = 1, SAMPLES do
        local elapsed = 0.0
        local batches = 0
        while elapsed * 1000.0 < SAMPLE_MS do
            elapsed = elapsed + time_batch(fn, iterations)
            batches = batches + 1
        end
        per_op[#per_op + 1] = (elapsed * 1e6) / (batches * iterations)
    end

    table.sort(per_op)
    local median = per_op[(#per_op // 2) + 1]
    return median, per_op
end

function Bench.SetMachineInfo(Machine, Cpu)
    Bench.Machine = Machine and tostring(Machine) or nil
    Bench.Cpu = Cpu and tostring(Cpu) or nil
end

--- project / backend / engine are alignment fields checked by runner.py.
function Bench.SetProject(Project, Backend, Engine)
    Bench.Project = Project and tostring(Project) or nil
    Bench.Backend = Backend and tostring(Backend) or nil
    Bench.Engine = Engine and tostring(Engine) or nil
end

local function BadIdentity(Value)
    return not Value or Value == "" or Value:find("%s") ~= nil
end

function Bench.Run()
    if BadIdentity(Bench.Machine) or BadIdentity(Bench.Cpu)
        or BadIdentity(Bench.Project) or BadIdentity(Bench.Backend)
        or BadIdentity(Bench.Engine) then
        print("-- bench refused: call SetProject(project, backend, engine) and")
        print("-- SetMachineInfo(machine, cpu) with non-empty, space-free real values.")
        return
    end

    print("#UNLUANOVA_BENCH_BEGIN")
    print(string.format("#meta project=%s backend=%s engine=%s machine=%s cpu=%s "
        .. "config=%s carrier=%s workload_revision=%s measured_utc=%s",
        Bench.Project, Bench.Backend, Bench.Engine, Bench.Machine, Bench.Cpu,
        Bench.Config, Bench.Carrier, Bench.Revision,
        os.date("!%Y-%m-%dT%H:%M:%SZ")))

    local failures = {}
    for _, case in ipairs(Bench.Cases) do
        local ok, median, samples = pcall(Bench.Measure, case.Fn, case.Iterations)
        if not ok then
            failures[#failures + 1] = string.format("%s: %s", case.Name, tostring(median))
            print(string.format("#case name=%s iterations=%d peropus=0 samples=",
                case.Name, case.Iterations))
        else
            local formatted = {}
            for i, v in ipairs(samples) do formatted[i] = string.format("%.6f", v) end
            print(string.format("#case name=%s iterations=%d peropus=%.6f samples=%s",
                case.Name, case.Iterations, median, table.concat(formatted, ",")))
        end
    end
    for _, message in ipairs(failures) do
        print("-- case failed: " .. message)
    end

    print("#UNLUANOVA_BENCH_END")
end

return Bench
