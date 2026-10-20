local Spec = require("Tests.Lua.Spec")
local Event = require("UnLuaNova.Event")

Spec.File("RuntimeContracts.spec.lua")
Spec.describe("Runtime contracts", function()
    Spec.it("example exercises recursive Once and owner cleanup", function()
        Spec.equal(require("Examples.RuntimeContracts").Run(), true)
    end)

    Spec.it("Off releases the owner even when the caller retains the token", function()
        local owner = {}
        local token = Event.On("Spec.OwnerCleanup", function() end, owner)
        Spec.equal(Event.Off(token), true)
        Spec.equal(token.Owner, nil)
        Spec.equal(token.Fn, nil)
        Spec.equal(Event.Off(token), false)
    end)
end)
