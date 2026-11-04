-- Cross-project workload — UnLuaNova implementation.
--
-- Run it inside a live game/editor:
--     lua.dofile CrossProjectBench
-- (copy this file to Content/Script/CrossProjectBench.lua first, or add
--  Tools/CrossProject/workload/unluanova to the script search path.)
--
-- It prints one #UNLUANOVA_BENCH_BEGIN block; Tools/CrossProject/runner.py turns that
-- block into a normalized JSON and compares it against the other projects' blocks.
--
-- First run on a new binding: run it and read the per-case failures. A case whose UE API
-- is not reachable in this binding is reported as a failure and still emitted with
-- peropus=0, so the runner refuses the comparison instead of quietly comparing fewer
-- cases. Fix the call, do not delete the case.
--
-- The contract that makes the numbers comparable is deliberately boring:
--   * every case takes an iteration count and does that many operations
--   * the harness warms up, then takes 5 samples of at least 100 ms each
--   * a sample keeps running whole batches until it has spent 100 ms, and reports
--     elapsed / (batches * iterations) — so it is a per-operation mean, not a total
--   * iteration counts below are part of the contract; a run that reports different
--     counts is refused by the runner, not silently compared

local Bench = {}

-- machine / cpu are alignment fields in Tools/CrossProject/runner.py: two runs on
-- different hardware must never be compared. A literal placeholder would satisfy a
-- naive "field is present" check and silently produce a bogus table, so there is no
-- default value at all - a run without real values is refused before it emits a block.
Bench.Machine = nil
Bench.Cpu = nil
Bench.Config = "Development"
Bench.Carrier = "Editor"           -- or Game
Bench.Revision = "1"

local function now()
    -- CPU time, not wall time: the point is to compare bindings, and CPU time is what a
    -- binding actually costs. Available on both backends.
    return os.clock()
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
        -- Cheapest possible crossing into UE: one static UFUNCTION, one float in, one out.
        Name = "UE.CallMath1", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for i = 1, n do acc = acc + UE.UKismetMathLibrary.DegreesToRadians(1.0) end
            return acc
        end,
    },
    {
        -- Four parameters, so marshalling dominates rather than the call itself.
        Name = "UE.CallMath4", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for _ = 1, n do acc = acc + UE.UKismetMathLibrary.FInterpTo(0.0, 1.0, 0.016, 1.0) end
            return acc
        end,
    },
    {
        Name = "UE.StructRoundTrip", Iterations = 20000,
        Fn = function(n)
            local acc = 0.0
            for _ = 1, n do
                local v = UE.FVector(1.0, 2.0, 3.0)
                local r = UE.UKismetMathLibrary.Add_VectorVector(v, v)
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
                local text = UE.UKismetTextLibrary.Conv_StringToText("bench")
                acc = acc + #UE.UKismetTextLibrary.Conv_TextToString(text)
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
-- Harness
-- ---------------------------------------------------------------------------

local WARMUP_MS = 250
local SAMPLE_MS = 100
local SAMPLES = 5

--- Run one batch, return seconds spent.
local function time_batch(fn, iterations)
    local t0 = now()
    fn(iterations)
    return now() - t0
end

--- Warm up, then take SAMPLES samples of at least SAMPLE_MS each.
--- Returns the median per-operation microseconds and the raw samples.
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

--- Provide the hardware identity once per machine before running. Call it from the
--- console (`lua.do "require('CrossProjectBench').SetMachineInfo('my-pc','Ryzen 7 4700G')"`)
--- or from the wrapper that requires this module. Values are printed verbatim into the
--- #meta line, so they must not contain spaces (the runner tokenises on whitespace).
function Bench.SetMachineInfo(Machine, Cpu)
    Bench.Machine = Machine and tostring(Machine) or nil
    Bench.Cpu = Cpu and tostring(Cpu) or nil
end

-- ---------------------------------------------------------------------------
-- Non-time metrics (revision 1 optional extension).
--
-- Pure-VM measurements only: collectgarbage / os.clock / load / debug / io. They
-- deliberately do NOT go through any engine binding hook. A value that cannot be
-- measured honestly is printed as value=unavailable -- never 0, never a placeholder.
-- ---------------------------------------------------------------------------

local function finite_nonneg(x)
    return type(x) == "number" and x == x and x >= 0 and x < math.huge
end

local function emit_metric(name, value, unit)
    print(string.format("#metric name=%s value=%s unit=%s", name, value, unit))
end

-- peak_kb is sampled by Bench.Run across the case loop; nil means "no measurement".
local function PrintNonTimeMetrics(peak_kb)
    -- memory.peak_kb: observed Lua heap peak (collectgarbage("count")), in KB.
    if finite_nonneg(peak_kb) then
        emit_metric("memory.peak_kb", string.format("%.3f", peak_kb), "kb")
    else
        emit_metric("memory.peak_kb", "unavailable", "kb")
    end

    -- startup.load_us: os.clock() around loading + running an init chunk, in us.
    local load_ok, load_us = pcall(function()
        local loader = rawget(_G, "load") or rawget(_G, "loadstring")
        if type(loader) ~= "function" then return nil end
        local rounds = 32
        local t0 = os.clock()
        for _ = 1, rounds do
            local chunk = loader("local t={} for i=1,2000 do t[i]=i end return t")
            if type(chunk) ~= "function" then return nil end
            chunk()
        end
        return (os.clock() - t0) * 1e6 / rounds
    end)
    if load_ok and finite_nonneg(load_us) then
        emit_metric("startup.load_us", string.format("%.3f", load_us), "us")
    else
        emit_metric("startup.load_us", "unavailable", "us")
    end

    -- gc.pause_us: average os.clock() pause of one full GC, in us.
    local gc_ok, gc_us = pcall(function()
        local rounds = 32
        local t0 = os.clock()
        for _ = 1, rounds do collectgarbage("collect") end
        return (os.clock() - t0) * 1e6 / rounds
    end)
    if gc_ok and finite_nonneg(gc_us) then
        emit_metric("gc.pause_us", string.format("%.3f", gc_us), "us")
    else
        emit_metric("gc.pause_us", "unavailable", "us")
    end

    -- code_size.bytes: size of this script, only if its source file can be read back.
    -- In a packaged / asset-hosted VM this is unavailable -> do NOT fake 0.
    local size_ok, size_bytes = pcall(function()
        local dbg = rawget(_G, "debug")
        local io_lib = rawget(_G, "io")
        if type(dbg) ~= "table" or type(io_lib) ~= "table" then return nil end
        local info = dbg.getinfo(1, "S")
        local source = info and info.source
        if type(source) ~= "string" or source:sub(1, 1) ~= "@" then return nil end
        local handle = io_lib.open(source:sub(2), "rb")
        if not handle then return nil end
        local data = handle:read("*a")
        handle:close()
        if type(data) ~= "string" then return nil end
        return #data
    end)
    if size_ok and finite_nonneg(size_bytes) then
        emit_metric("code_size.bytes", string.format("%d", size_bytes), "bytes")
    else
        emit_metric("code_size.bytes", "unavailable", "bytes")
    end
end

function Bench.Run()
    if not Bench.Machine or not Bench.Cpu or Bench.Machine == "" or Bench.Cpu == ""
        or Bench.Machine:find("%s") or Bench.Cpu:find("%s") then
        print("-- bench refused: call Bench.SetMachineInfo(machine, cpu) with non-empty,")
        print("-- space-free real values before Bench.Run(); they are alignment fields in runner.py.")
        return
    end

    local base_ok, mem_peak_kb = pcall(function() return collectgarbage("count") end)
    if not base_ok or type(mem_peak_kb) ~= "number" then mem_peak_kb = nil end

    print("#UNLUANOVA_BENCH_BEGIN")
    print(string.format("#meta project=UnLuaNova backend=%s engine=%s machine=%s cpu=%s "
        .. "config=%s carrier=%s workload_revision=%s measured_utc=%s",
        tostring(UnLuaNova and UnLuaNova.GetBackend and UnLuaNova.GetBackend() or "?"),
        tostring(UnLuaNova and UnLuaNova.GetEngineVersion and UnLuaNova.GetEngineVersion() or "?"),
        Bench.Machine, Bench.Cpu, Bench.Config, Bench.Carrier, Bench.Revision,
        os.date("!%Y-%m-%dT%H:%M:%SZ")))

    local failures = {}
    for _, case in ipairs(Bench.Cases) do
        -- pcall returns (true, median, samples) on success, (false, message) on error.
        local ok, median, samples = pcall(Bench.Measure, case.Fn, case.Iterations)
        if not ok then
            failures[#failures + 1] = string.format("%s: %s", case.Name, tostring(median))
            -- Report the case with no result rather than dropping it: the runner refuses
            -- to compare runs whose case sets differ, and a silent drop would look like
            -- "the other project is faster because it has fewer slow cases".
            print(string.format("#case name=%s iterations=%d peropus=0 samples=",
                case.Name, case.Iterations))
        else
            local formatted = {}
            for i, v in ipairs(samples) do formatted[i] = string.format("%.6f", v) end
            print(string.format("#case name=%s iterations=%d peropus=%.6f samples=%s",
                case.Name, case.Iterations, median, table.concat(formatted, ",")))
        end
        if mem_peak_kb then
            local cur_ok, cur_kb = pcall(function() return collectgarbage("count") end)
            if cur_ok and type(cur_kb) == "number" and cur_kb > mem_peak_kb then
                mem_peak_kb = cur_kb
            end
        end
    end
    for _, message in ipairs(failures) do
        print("-- case failed: " .. message)
    end

    -- Optional non-time metrics: separate #metric lines, emitted on their own.
    PrintNonTimeMetrics(mem_peak_kb)

    print("#UNLUANOVA_BENCH_END")
end

return Bench
