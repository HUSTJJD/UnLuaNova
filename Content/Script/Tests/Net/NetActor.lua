-- Lua module for the client/server automation test (UnLuaNova.API.Net.ClientServer).
--
-- Every RPC here is declared in Lua - none of them exists in C++ or in a blueprint. That is
-- the whole point of the feature: ULuaFunction::IsOverridable() rejects anything carrying
-- FUNC_Net, so before it existed Lua could only re-implement an RPC the engine had already
-- declared.
--
-- What the C++ side reads back, and why it is written this way:
--
--   CallLog / CallCount  UPROPERTYs on the actor, one row per invocation. Per-instance, so
--                        the server's actor and the client's actor each keep their own log -
--                        which is what makes "ran on the server but not on the client" an
--                        assertion instead of a log reading exercise.
--   HP                   a plain Lua field declared through GetLifetimeReplicatedProps; the
--                        replication component ships it to clients and OnRep pushes it back
--                        into the client's table.
--
-- All three RPCs are declared Reliable. An unreliable RPC may be dropped, and a test that
-- fails because a packet was dropped says nothing about whether the mechanism works.

local M = UnLuaNova.Class()

M.ServerRPC = {
    FromClient = { Reliable = true, Params = { "Int" } },
}

M.ClientRPC = {
    FromServer = { Reliable = true, Params = { "Int" } },
}

M.MulticastRPC = {
    ToAll = { Reliable = true, Params = { "Int" } },
}

function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP", Type = "Int" },
    }
end

function M:Initialize()
    self.HP = 100
end

local function Record(self, Name, Value)
    self.CallLog = (self.CallLog or "") .. Name .. ":" .. tostring(Value) .. "|"
    self.CallCount = self.CallCount + 1
end

-- Runs on the server only. Also moves the replicated field: the test then reads HP off the
-- client to check that a Lua-declared field really crossed over.
function M:FromClient(Value)
    Record(self, "FromClient", Value)
    self.HP = Value
end

-- Runs on the owning client only.
function M:FromServer(Value)
    Record(self, "FromServer", Value)
end

-- Runs on the server and on every client.
function M:ToAll(Value)
    Record(self, "ToAll", Value)
end

return M
