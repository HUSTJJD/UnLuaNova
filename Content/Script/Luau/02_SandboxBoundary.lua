--!strict
--[[
    说明：沙盒（luaL_sandbox）下的安全边界

    沙盒是**启动期 CVar**，默认关闭。开启方式：

        ; Config/DefaultEngine.ini
        [ConsoleVariables]
        lua.sandbox=1

    它在每个 Lua 环境创建时读取一次，运行中再改不会生效。开启后全局表变为只读，
    热更/下载的脚本就无法意外改写引擎绑定了。仅 Luau 后端支持。

    注意：本文件用了类型注解，只能在 Luau 后端下加载。
]]

local M = UnLuaNova.Class()

local Attempt = 0

-- 把一次"本来会崩"的操作包起来，把结果打印出来，示例本身不会中断
local function Try(Label: string, Fn: () -> ())
    Attempt += 1
    local Ok, Err = pcall(Fn)
    if Ok then
        print(string.format("[sandbox] %d. %s -> 成功（沙盒未开启）", Attempt, Label))
    else
        print(string.format("[sandbox] %d. %s -> 被拦截：%s", Attempt, Label, tostring(Err)))
    end
end

function M:Initialize()
    print("[sandbox] HasFeature('sandbox') = " .. tostring(UnLuaNova.HasFeature("sandbox")))

    Try("向全局表写入新字段", function()
        rawset(_G, "LuauSandboxProbe", 1)
    end)

    Try("改写引擎绑定", function()
        rawset(_G, "UE", nil)
    end)

    Try("改写 UE 命名空间里的类型", function()
        local Namespace = rawget(_G, "UE")
        if type(Namespace) == "table" then
            rawset(Namespace, "FVector", nil)
        end
    end)

    print("[sandbox] 想体验拦截效果：在 DefaultEngine.ini 里打开 lua.sandbox 后重启编辑器")
end

function M:Run()
    print("[sandbox] 边界演示已完成，见上面的输出")
end

return M
