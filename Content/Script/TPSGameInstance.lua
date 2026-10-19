---@type UTPSGameInstance
local M = UnLuaNova.Class()

function M:ReceiveInit()
	print("TPSGameInstance:ReceiveInit")
end

function M:ReceiveShutdown()
    print("TPSGameInstance:ReceiveShutdown")
end

return M