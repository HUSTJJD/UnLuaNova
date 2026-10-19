---@type UDemoGameInstance
local M = UnLuaNova.Class()

function M:ReceiveInit()
	print("DemoGameInstance:ReceiveInit")
end

function M:ReceiveShutdown()
    print("DemoGameInstance:ReceiveShutdown")
end

return M