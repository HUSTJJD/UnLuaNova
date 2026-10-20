--!strict
--[[
    说明：Luau 语法与标准库速览

    这一页里的每一行都是 Luau 专属的，Lua 5.5 后端下**编译不过**：
    continue、复合赋值（+= -= ..=）、类型注解、table.freeze/clone、vector、buffer。

    注意：只能通过 require "Luau.Index" 在 Luau 后端下访问。
]]

local M = UnLuaNova.Class()

function M:Initialize()
    -- continue：跳过本次循环
    local Sum = 0
    for i = 1, 10 do
        if i % 2 == 0 then
            continue
        end
        Sum += i
    end
    print("[syntax] 1..10 奇数和 = " .. Sum)

    -- 复合赋值
    local Text = "hp="
    Text ..= tostring(Sum)
    print("[syntax] 字符串 ..= : " .. Text)

    -- table.freeze / clone / clear
    local Frozen = table.freeze({ A = 1, B = 2 })
    local Copy = table.clone(Frozen)
    Copy.B = 20
    print(string.format("[syntax] isfrozen=%s clone.B=%d", tostring(table.isfrozen(Frozen)), Copy.B))

    local Counter = { 1, 2, 3 }
    table.clear(Counter)
    print("[syntax] clear 之后长度 = " .. #Counter)

    -- bit32：Luau 标准库自带
    print("[syntax] bit32.band(0xF0, 0x3C) = " .. bit32.band(0xF0, 0x3C))

    -- typeof 对 userdata 返回类名，type 只返回 "userdata"
    print("[syntax] typeof(UE.FVector) = " .. typeof(UE.FVector))
end

function M:Run()
    print("[syntax] Luau 语法速览已输出到日志")
end

return M
