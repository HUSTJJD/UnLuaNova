-- String helpers.
--
-- The point of this file is backend independence: Luau ships string.split, plain Lua
-- does not, and a couple of the others differ in edge cases. Rather than let game
-- scripts silently depend on which backend was compiled in, every helper is implemented
-- once here and installed onto `string`.
--
-- Written to Lua 5.1 semantics so it runs unchanged on both backends (Luau is a 5.1
-- derivative; plain Lua is 5.5). No 5.2+ features.
--
-- Automatically installed when the env starts (see UnLuaNovaLib.cpp). Scripts may also
-- require("UnLuaNova.StringEx") explicitly - the module table is returned as well.
-- (The require prefix is the directory name, which is why it differs from the global.)

local StringEx = {}

--- True when s begins with prefix. Empty prefix is always true.
function StringEx.startswith(s, prefix)
    if type(s) ~= "string" or type(prefix) ~= "string" then
        return false
    end
    if prefix == "" then
        return true
    end
    return s:sub(1, #prefix) == prefix
end

--- True when s ends with suffix. Empty suffix is always true.
function StringEx.endswith(s, suffix)
    if type(s) ~= "string" or type(suffix) ~= "string" then
        return false
    end
    if suffix == "" then
        return true
    end
    return s:sub(-#suffix) == suffix
end

--- Removes leading and trailing whitespace. `trim` is an alias.
function StringEx.strip(s)
    if type(s) ~= "string" then
        return s
    end
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

--- Splits on a literal separator (no pattern matching), returning an array.
-- Kept as a plain search on purpose: callers passing "." or "|" get the obvious answer
-- instead of an empty table.
function StringEx.split(s, sep)
    local Result = {}
    if type(s) ~= "string" then
        return Result
    end

    sep = sep or ","
    if sep == "" then
        Result[1] = s
        return Result
    end

    local Start = 1
    while true do
        local First, Last = s:find(sep, Start, true)
        if not First then
            Result[#Result + 1] = s:sub(Start)
            break
        end
        Result[#Result + 1] = s:sub(Start, First - 1)
        Start = Last + 1
    end
    return Result
end

--- Joins a list with a separator. Values are run through tostring.
function StringEx.join(sep, list)
    if type(list) ~= "table" then
        return ""
    end

    local Parts = {}
    for i = 1, #list do
        Parts[i] = tostring(list[i])
    end
    return table.concat(Parts, sep or "")
end

--- Counts non-overlapping occurrences of a literal substring.
function StringEx.count(s, sub)
    if type(s) ~= "string" or type(sub) ~= "string" or sub == "" then
        return 0
    end

    local Total = 0
    local Start = 1
    while true do
        local _, Last = s:find(sub, Start, true)
        if not Last then
            break
        end
        Total = Total + 1
        Start = Last + 1
    end
    return Total
end

-- --- installation -------------------------------------------------------------
-- Deliberately overwrites string.split even where the backend already provides it, so
-- behaviour cannot drift between backends.

string.startswith = StringEx.startswith
string.endswith = StringEx.endswith
string.strip = StringEx.strip
string.trim = StringEx.strip
string.split = StringEx.split
string.count = StringEx.count
-- Not installed as string.join: the name reads wrong as a method (".".join(...) is
-- separator-first here) and it would shadow nothing on either backend. Use
-- require("UnLuaNova.StringEx").join, or table.concat.

return StringEx
