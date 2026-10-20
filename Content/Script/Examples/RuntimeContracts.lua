local M = {}

function M.Run()
    local Event = require("UnLuaNova.Event")
    local OOP = require("UnLuaNova.OOP")
    local owner = {}
    local count = 0
    local eventName = "UnLuaNova.Example.RuntimeContracts"
    Event.Once(eventName, function()
        count = count + 1
        Event.Emit(eventName)
    end, owner)
    Event.Emit(eventName)
    assert(count == 1 and Event.Count(eventName) == 0)

    Event.On(eventName, function() count = count + 1 end, owner)
    assert(Event.UnbindOwner(owner) == 1)
    Event.Emit(eventName)
    assert(count == 1)

    local Base = OOP.Class("ExampleBase")
    function Base:Ctor(value) self.Value = value end
    local Derived = OOP.Class("ExampleDerived", Base)
    local instance = Derived:New(42)
    assert(instance.Value == 42 and OOP.Is(instance, Base))
    return true
end

return M
