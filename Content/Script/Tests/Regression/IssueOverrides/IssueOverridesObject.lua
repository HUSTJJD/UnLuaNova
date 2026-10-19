local M = UnLuauNova.Class()

function M:CollectInfo()
    return self.Overridden.CollectInfo(self) + 1
end

return M
