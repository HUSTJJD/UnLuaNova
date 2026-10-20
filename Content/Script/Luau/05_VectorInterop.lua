--!strict
--[[
    说明：Luau 原生 vector 与 FVector 互操作

    Luau 的 vector 是寄存器里的值类型（3 个 float），运算不分配任何对象；
    FVector 则是带元表的 userdata，每次运算都要新建一个 userdata。
    UnLuaNova 打通了这几条路：

        UE.FVector(native)      -- 直接从 vector 构造 FVector，不走三次 lua_tonumber
        fvector:Set(native)     -- 一次性写入三个分量
        fvector:ToVector()      -- 取回原生 vector（Lua 5.5 后端返回 nil）
        UFUNCTION 参数           -- FVector 参数可以直接传 vector，不再需要 userdata

    注意：本文件用了类型注解与 vector 库，只能在 Luau 后端下加载。
]]

local M = UnLuaNova.Class()

function M:Initialize()
    if not UnLuaNova.HasFeature("vector") then
        print("[vector] 当前后端没有原生 vector，跳过")
        return
    end

    -- 1. vector -> FVector
    local Native = vector.create(1, 2, 3)
    local Vec = UE.FVector(Native)
    print("[vector] UE.FVector(vector) = " .. tostring(Vec))

    -- 2. FVector -> vector
    local Back = Vec:ToVector()
    print("[vector] 取回 vector = " .. tostring(Back))

    -- 3. 原生运算：不产生 userdata
    local A = vector.create(1, 0, 0)
    local B = vector.create(0, 2, 0)
    local C = (A + B) * 2
    print("[vector] 原生运算 (A+B)*2 = " .. tostring(C))

    -- 4. 批量写回
    Vec:Set(vector.create(10, 20, 30))
    print("[vector] Set(vector) 之后 = " .. tostring(Vec))

    -- 5. 直接传给 UFUNCTION 的 FVector 参数（self 需要是 Actor）
    local Ok, Err = pcall(function()
        self:K2_SetActorLocation(vector.create(0, 0, 100), false, false)
    end)
    print("[vector] 把 vector 直接传给 UFUNCTION：" .. (Ok and "OK" or tostring(Err)))
end

function M:Run()
    print("[vector] vector/FVector 互操作示例已完成，见上面的输出")
end

return M
