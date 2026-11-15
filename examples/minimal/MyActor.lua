--[[
    最小接入示例：把本文件复制到自己工程的 Content/Script/MyActor.lua。

    模块名 "MyActor" 对应 Content/Script/MyActor.lua —— 模块路径相对
    Content/Script/ 根目录，用点号分隔目录，不带 .lua 后缀。

    后端查询：UnLuaNova.GetBackend() 返回当前 Lua 后端（"Lua" 或 "Luau"）。
    本文件只写两个后端都能解析的语法；Luau 专属语法（continue、复合赋值、
    类型注解等）的示例请走 Content/Script/Luau/Index.lua 守卫入口，它在
    Lua 后端下会返回 Available = false，不会去加载那些文件。
]]

local M = UnLuaNova.Class()

function M:ReceiveBeginPlay()
    print(string.format("[MyActor] ReceiveBeginPlay, backend=%s", tostring(UnLuaNova.GetBackend())))
end

function M:ReceiveTick()
    -- 每帧回调；这里刻意留空，只用于确认脚本已绑定并在运行。
end

return M
