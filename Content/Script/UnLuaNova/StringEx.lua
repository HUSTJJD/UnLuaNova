-- Optional string helpers, in pure Lua - no C extension, no extra plugin.
--
-- Why this exists at all: the source project shipped these as a 242-line C extension
-- module. On Luau most of that is unnecessary - the interpreter is fast enough that
-- Lua implementations cost nothing measurable for these, and one of them (split) is
-- already a Luau builtin.
--
-- Why split is here despite being a Luau builtin: plain Lua has no string.split, and
-- UnLuaNova supports both backends. Scripts should behave identically on either, so
-- this provides it. On Luau the native string.split is still there and still faster -
-- prefer it when you do not need cross-backend parity.
--
-- Usage:
--     local StringEx = require("UnLuaNova.StringEx")
--     if StringEx.startswith(Path, "Content/") then ... end

local StringEx = {}

--- Split into a list. Separator defaults to runs of whitespace.
function StringEx.split(S, Sep, bPlain)
    Sep = Sep or "%s+"

    local Result = {}
    local From = 1

    while true do
        local Start, Stop = S:find(Sep, From, bPlain == true)
        if not Start then
            Result[#Result + 1] = S:sub(From)
            break
        end

        Result[#Result + 1] = S:sub(From, Start - 1)
        -- Always advance past at least one character, otherwise an empty separator
        -- (or an empty match) would spin here forever.
        From = math.max(Stop + 1, Start + 1)
    end

    return Result
end

function StringEx.strip(S)
    return (S:gsub("^%s+", ""):gsub("%s+$", ""))
end

function StringEx.lstrip(S)
    return (S:gsub("^%s+", ""))
end

function StringEx.rstrip(S)
    return (S:gsub("%s+$", ""))
end

function StringEx.startswith(S, Prefix)
    return S:sub(1, #Prefix) == Prefix
end

function StringEx.endswith(S, Suffix)
    -- An empty suffix matches everything, matching the usual convention.
    return #Suffix == 0 or S:sub(-#Suffix) == Suffix
end

--- Count non-overlapping occurrences. Plain by default so '.' means a literal dot.
function StringEx.count(S, Pattern, bPlain)
    local Count = 0
    local From = 1

    while true do
        local Start, Stop = S:find(Pattern, From, bPlain ~= false)
        if not Start then
            break
        end

        Count = Count + 1
        From = math.max(Stop + 1, Start + 1)
    end

    return Count
end

--- Join a list with a separator. Values are run through tostring.
function StringEx.join(Sep, List)
    local Parts = {}
    for i = 1, #List do
        Parts[i] = tostring(List[i])
    end
    return table.concat(Parts, Sep)
end

return StringEx
