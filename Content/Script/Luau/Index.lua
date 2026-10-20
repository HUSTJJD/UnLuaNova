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

local Backend = UnLuaNova.GetBackend()

if Backend ~= "Luau" then
    return {
        Backend = Backend,
        Available = false,
        Reason = "这些示例需要 Luau 后端，当前后端是 " .. tostring(Backend),
    }
end

return {
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
