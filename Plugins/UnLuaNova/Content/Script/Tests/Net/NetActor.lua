-- Lua module for the client/server automation test.
--
-- Everything this module does is observable from C++, so the spec can assert on it: each
-- RPC handler appends to a global table rather than only printing, and the replicated
-- field is a plain integer the component ships to clients.
--
-- The RPCs are declared here - they are NOT declared anywhere in C++ or in a blueprint.
-- That is the whole point of the feature: ULuaFunction::IsOverridable() rejects anything
-- carrying FUNC_Net, so before this existed, Lua could only re-implement an RPC the engine
-- already declared.

local M = UnLuaNova.Class()

M.ServerRPC = {
    FromClient = true,
}

M.ClientRPC = {
    FromServer = true,
}

M.MulticastRPC = {
    ToAll = true,
}

function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP", Type = "Int" },
    }
end

-- Which handlers ran on this peer. The spec reads this per-env, so "the server ran it and
-- the client did not" is a measurable statement rather than a matter of reading the log.
M.Calls = {}

local function Record(Name, Arg)
    M.Calls[Name] = (M.Calls[Name] or 0) + (Arg or 1)
    _G.UnLuaNovaNetCalls = M.Calls
end

function M:FromClient(Value)
    Record("FromClient", Value)
end

function M:FromServer(Value)
    Record("FromServer", Value)
end

function M:ToAll(Value)
    Record("ToAll", Value)
end

function M:Initialize()
    self.HP = 100
    M.Calls = {}
    _G.UnLuaNovaNetCalls = M.Calls
end

return M
