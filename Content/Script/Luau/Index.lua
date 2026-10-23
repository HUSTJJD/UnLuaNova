--[[
    Luau 后端示例集

    用法：
        local Examples = require "Luau.Index"
        if Examples.Available then
            Examples.StrictTyping:Run()
        end

    为什么要有这个入口：目录下的示例用了 Luau 专属语法（continue、复合赋值、类型注解），
    这些在 Lua 5.5 后端下是**编译期错误** —— 不是运行到才失败，是 require 的那一刻就崩。
    所以这里先问后端，只有确认是 Luau 才去 require 它们；Lua 5.5 下直接返回
    Available = false，一个示例文件都不会被加载。
]]

-- 两个 return 的字段集不一样（失败时给 Reason，成功时给示例模块），而 luau-analyze 拿第一个
-- return 当模块类型去推断，第二个就会被判成"缺字段"。所以这里显式声明返回类型，示例字段标成
-- 可选 —— 顺带把 Backend 从字面量放宽成 string：不写的话，第一个 return 会把 Backend 推断成
-- "Lua"（`~= "Luau"` 分支里的收窄结果），第二个 return 再赋 "Luau" 就成了类型不符。
type Index = {
    Backend: string,
    Available: boolean,
    Reason: string?,
    StrictTyping: any?,
    SandboxBoundary: any?,
    NativeDebug: any?,
    LuauSyntax: any?,
    VectorInterop: any?,
}

local Backend: string = UnLuaNova.GetBackend()

if Backend ~= "Luau" then
    local Unavailable: Index = {
        Backend = Backend,
        Available = false,
        Reason = "这些示例需要 Luau 后端，当前后端是 " .. tostring(Backend),
    }
    return Unavailable
end

local Examples: Index = {
    Backend = Backend,
    Available = true,

    -- 严格类型 + 冻结配置表
    StrictTyping = require "Luau.01_StrictTyping",

    -- 沙盒开启后，什么能写、什么会报错
    SandboxBoundary = require "Luau.02_SandboxBoundary",

    -- lua.dbg 原生断点的下断点目标
    NativeDebug = require "Luau.03_NativeDebug",

    -- continue / 复合赋值 / table.freeze / bit32 速览
    LuauSyntax = require "Luau.04_LuauSyntax",

    -- vector 与 FVector 互操作
    VectorInterop = require "Luau.05_VectorInterop",
}

return Examples
