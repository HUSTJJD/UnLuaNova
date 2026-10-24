local M = {}

function M.Start(worldContext, onTick)
    ---@type UnLuaNovaTimer
    local Timer = require("UnLuaNova.Timer")
    assert(type(onTick) == "function")
    local session = { Count = 0 }
    session.Handle = Timer.Set(function()
        session.Count = session.Count + 1
        onTick(session.Count)
    end, 0.25, true, 0.25, false, worldContext)

    function session:Pause(paused)
        return Timer.Pause(self.Handle, paused)
    end

    function session:Stop()
        return Timer.Clear(self.Handle)
    end

    function session:IsActive()
        return Timer.IsValid(self.Handle)
    end

    return session
end

return M
