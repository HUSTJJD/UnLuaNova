--!strict
--[[
    说明：lua.dbg 原生断点的下断点目标

    Luau 的断点原语能让 VM 停在指定行，这是 lua_sethook 做不到的（它只能"报告"每一行）。
    操作步骤：

        1. 控制台执行  lua.dbg bp Luau.03_NativeDebug 34   （行号按本文件实际行号调整）
        2. 触发下面的 ReceiveBeginPlay（例如重新 PIE，或调用 Run()）
        3. 命中后：lua.dbg stack / lua.dbg locals / lua.dbg step / lua.dbg continue
        4. 收尾：lua.dbg clear，或 lua.dbg detach 卸载 Hook 恢复零开销

    不敲命令时不安装任何 Hook，所以常态是零开销的。
    断点是 Luau 专属能力，Lua 5.5 后端退化为按行号匹配。

    注意：本文件用了类型注解，只能在 Luau 后端下加载。
]]

local M = UnLuaNova.Class()

local function Double(Value: number): number
    local Result = Value * 2
    return Result
end

function M:ReceiveBeginPlay()
    print("[dbg] 在这一段下断点：lua.dbg bp Luau.03_NativeDebug <行号>")

    local A = 1
    local B = 2
    local Sum = Double(A) + Double(B)

    print("[dbg] A=" .. A .. " B=" .. B .. " Sum=" .. Sum)
end

function M:Run()
    self:ReceiveBeginPlay()
end

return M
