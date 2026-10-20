local M = UnLuaNova.Class()

-- Bumps a per-object counter (so two objects can be told apart) and a global one (so a
-- case can tell whether Lua ran at all).
--
-- The error flag lets one fixture cover both the happy path and "the override raised":
-- a second stub class would be a second module to keep in sync, and what is being
-- asserted here is the engine's reaction to a raising override, not the raising itself.
function M:Overridable()
    if _G.OverridesStubShouldError then
        error('override boom')
    end

    self.Counter = self.Counter + 1
    _G.OverridesStubGlobalCalls = (_G.OverridesStubGlobalCalls or 0) + 1
    return self.Counter
end

return M
