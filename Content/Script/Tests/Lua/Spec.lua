-- A minimal spec runner for the Lua modules the plugin ships.
--
-- Those modules (StringEx, Vector, Buffer) can only otherwise be exercised from C++, which
-- means every pure-Lua bug needs a C++ case to be observed at all. This is deliberately
-- about a hundred lines and depends on nothing: describe / it / equal, and a run() that
-- hands back a result table for the C++ side to turn into real automation cases.
--
-- Everything here has to work on both backends. The only backend-dependent part is reading
-- a line number, and it is probed rather than assumed.

local M = {}

M.Groups = {}
M.Current = nil
M.CurrentFile = "?"

-- Luau exposes debug.info; plain Lua exposes debug.getinfo. Ask for whichever exists
-- instead of branching on the backend name, which is wrong across versions.
local function GetLine(Depth)
    if type(debug) == "table" then
        if debug.info then
            local Line = debug.info(Depth, "l")
            if type(Line) == "number" then
                return Line
            end
        elseif debug.getinfo then
            local Info = debug.getinfo(Depth, "l")
            if Info and type(Info.currentline) == "number" then
                return Info.currentline
            end
        end
    end
    return 0
end

-- Each spec file declares which file it is before registering cases, so a failure can be
-- reported with a file name even though every file shares this one runner.
function M.File(Name)
    M.CurrentFile = Name
end

function M.describe(Name, Fn)
    local Group = { Name = Name, Cases = {} }
    M.Groups[#M.Groups + 1] = Group

    local Saved = M.Current
    M.Current = Group
    Fn()
    M.Current = Saved
end

function M.it(Name, Fn)
    local Group = M.Current
    if not Group then
        error("it() called outside describe()", 2)
    end

    Group.Cases[#Group.Cases + 1] = {
        Name = Name,
        Fn = Fn,
        File = M.CurrentFile,
        Line = GetLine(3),
    }
end

function M.equal(Actual, Expected)
    if Actual ~= Expected then
        error(string.format("expected %s, got %s", tostring(Expected), tostring(Actual)), 2)
    end
end

function M.throws(Fn)
    local Ok = pcall(Fn)
    if Ok then
        error("expected the function to raise, but it did not", 2)
    end
end

-- Runs every registered case. Never raises: one broken case must not hide the rest.
-- Returns a flat list so the C++ side can walk it without knowing about groups.
function M.run()
    local Results = {}
    for _, Group in ipairs(M.Groups) do
        for _, Case in ipairs(Group.Cases) do
            local Ok, Err = pcall(Case.Fn)
            Results[#Results + 1] = {
                Passed = Ok,
                File = Case.File,
                Line = Case.Line,
                Name = Group.Name .. " " .. Case.Name,
                Message = Ok and "" or tostring(Err),
            }
        end
    end
    return Results
end

-- Encodes the results as lines the C++ side can read back through a single global. A table
-- would need typed accessors that differ between the two backends; a delimited string does
-- not. Fields are separated by \t, which cannot appear in a test name or a message.
function M.encode(Results)
    local Lines = {}
    for _, R in ipairs(Results) do
        Lines[#Lines + 1] = table.concat({
            R.Passed and "PASS" or "FAIL",
            R.File,
            tostring(R.Line),
            R.Name,
            R.Message,
        }, "\t")
    end
    return table.concat(Lines, "\n")
end

return M
