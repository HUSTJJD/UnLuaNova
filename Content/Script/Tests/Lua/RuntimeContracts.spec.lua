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

    Spec.it("Emit prunes listeners whose owner died without an explicit UnbindOwner", function()
        local owner = { alive = true }
        local fired = 0
        Event.On("Spec.AutoCleanup", function() fired = fired + 1 end, owner)
        local previous = Event.SetOwnerValidator(function(o) return o.alive end)

        Spec.equal(Event.Emit("Spec.AutoCleanup"), 1)
        Spec.equal(Event.Count("Spec.AutoCleanup"), 1)
        owner.alive = false
        Spec.equal(Event.Emit("Spec.AutoCleanup"), 0)
        Spec.equal(Event.Count("Spec.AutoCleanup"), 0)
        Spec.equal(fired, 1)

        Event.SetOwnerValidator(previous)
    end)

    Spec.it("PruneDeadOwners keeps live owners and the validator is replaceable", function()
        Event.SetOwnerValidator(nil)
        local owner = { alive = true }
        local fired = 0
        Event.On("Spec.AutoCleanupLive", function() fired = fired + 1 end, owner)
        local previous = Event.SetOwnerValidator(function(o) return o.alive end)

        Spec.equal(Event.PruneDeadOwners(), 0)
        Spec.equal(Event.Emit("Spec.AutoCleanupLive"), 1)
        Spec.equal(fired, 1)

        Event.SetOwnerValidator(previous)
        Event.Clear("Spec.AutoCleanupLive")
    end)
end)
