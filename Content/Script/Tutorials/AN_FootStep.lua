---@type AN_FootStep_C : UAnimNotify
local M = UnLuauNova.Class()

function M:Received_Notify(MeshComp, Animation)
    print("foot step")
    return true
end

return M