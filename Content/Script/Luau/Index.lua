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

    因此这个文件自己必须是**两个后端都能解析**的语法：它是 Lua 5.5 下唯一会被 require
    到的 Luau 示例文件，一旦写了 `type X = ...` 这类注解，守卫就先于它自己的判断失效 ——
    require 直接语法错误，Lua 后端连"我不是 Luau"这句话都说不出来。同理，两个分支共用
    同一张表、只有一个 return：静态检查器（luau-lsp）只拿第一个 return 当模块类型去推断，分成两个
    形状不同的 return 会让第二个被判成"缺字段"。
]]

local Backend = UnLuaNova.GetBackend()

-- 两个分支返回的是同一张表，用 Available 区分；示例字段只在 Luau 下存在。
local Examples = {
    Backend = Backend,
    Available = false,
}

if Backend ~= "Luau" then
    Examples.Reason = "这些示例需要 Luau 后端，当前后端是 " .. tostring(Backend)
else
    Examples.Available = true

    -- 严格类型 + 冻结配置表
    Examples.StrictTyping = require "Luau.01_StrictTyping"

    -- 沙盒开启后，什么能写、什么会报错
    Examples.SandboxBoundary = require "Luau.02_SandboxBoundary"

    -- lua.dbg 原生断点的下断点目标
    Examples.NativeDebug = require "Luau.03_NativeDebug"

    -- continue / 复合赋值 / table.freeze / bit32 速览
    Examples.LuauSyntax = require "Luau.04_LuauSyntax"

    -- vector 与 FVector 互操作
    Examples.VectorInterop = require "Luau.05_VectorInterop"
    Examples.BufferInterop = require "Luau.06_BufferInterop"
end

return Examples
