--!strict
--[[
    说明：Luau 的渐进类型 —— 类型注解 + --!strict + 冻结的常量表

    --!strict 必须是文件的最前面几行之一，Luau 才会按严格模式检查这个文件。
    配合编辑器导出的 UnLuaNova.d.lua（工具栏 → 导出智能提示），
    luau-analyze 可以在运行之前发现 UE API 的误用。

    注意：本文件用了类型注解，只能在 Luau 后端下加载，
    请通过 require "Luau.Index" 访问，不要在 Lua 5.5 后端下直接 require。
]]

-- 配置表冻结：写错一个字段会立刻报错，而不是静默产生一个新字段
local Config = table.freeze({
    MaxHp = 100,
    RegenPerSecond = 2,
    TeamNames = table.freeze({ "Red", "Blue" }),
})

type Character = {
    Hp: number,
    MaxHp: number,
    Alive: boolean,
}

local M = UnLuaNova.Class()

local function ClampRegen(hp: number, deltaSeconds: number): number
    local next = hp + Config.RegenPerSecond * deltaSeconds
    if next > Config.MaxHp then
        return Config.MaxHp
    end
    return next
end

function M:Initialize()
    local Stats: Character = {
        Hp = 50,
        MaxHp = Config.MaxHp,
        Alive = true,
    }

    Stats.Hp = ClampRegen(Stats.Hp, 10)
    print(string.format("[strict] hp=%d/%d team=%s", Stats.Hp, Stats.MaxHp, Config.TeamNames[1]))

    -- 冻结的表写不进去，这一行会报错（注释掉即可正常运行）
    -- Config.MaxHp = 999
end

-- 单独调用这一页的示例
function M:Run()
    print("[strict] Config 已冻结：" .. tostring(table.isfrozen(Config)))
end

return M
