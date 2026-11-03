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
--   HP                   an unconditional Lua field declared through GetLifetimeReplicatedProps;
--                        the replication component ships it to every client and OnRep pushes it
--                        back into the client's table.
--   OwnerSecret          a second field in the SAME declaration list, marked OwnerOnly. Only the
--                        owning connection may receive it; a non-owner must still see nil. The
--                        C++ driver reads it back on each peer as a connected-peer assertion.
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
        -- OwnerOnly is deliberately mixed into the same declaration list as an unconditional
        -- field: it must land in its own storage group, and only the owning connection may
        -- receive it. The C++ driver reads OwnerSecret back on every peer to prove that.
        { Name = "OwnerSecret", Type = "Int", Condition = "OwnerOnly" },
    }
end

function M:Initialize()
    self.HP = 100
    -- OwnerSecret is intentionally NOT initialised on clients. A non-owner seeing nil here
    -- is the evidence that a conditional field was never delivered to that connection.
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
    -- Only the server executes this RPC. Polled/Explicit replication must deliver 777 to the
    -- owning client and to nobody else.
    self.OwnerSecret = 777
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
