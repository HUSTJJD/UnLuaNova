local M = UnLuaNova.Class()

-- The initializer table is SpawnActor's last argument, and UnLuaNova hands it to
-- Initialize() - the only place a script can read construction arguments. Keeping it on
-- the instance is what lets the spec assert the table actually arrived.
function M:Initialize(Initializer)
    self.Initializer = Initializer
end

return M
