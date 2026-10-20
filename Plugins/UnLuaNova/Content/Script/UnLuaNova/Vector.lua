--[[
    vector 与 FVector 互操作的小工具（UnLuaNova.Vector）

    之所以做成模块而不是写进全局 string/table：沙盒开启后全局表是只读的，
    往全局塞东西会失败；模块形式也能按需 require。

    这些函数在两个后端下都能安全调用：Lua 5.5 没有 vector 类型，
    HasVector() 返回 false，Create/FromFVector 返回 nil。
]]

local M = {}

-- 注意：本模块故意不写类型注解。它是跨后端通用的，
-- 注解是 Luau 语法，写了就只能在 Luau 后端下加载了。

--- 当前后端有没有 Luau 原生 vector
function M.HasVector()
    return type(vector) == "table"
end

--- 构造一个原生 vector；非 Luau 后端返回 nil
function M.Create(x, y, z)
    if type(vector) ~= "table" then
        return nil
    end
    return vector.create(x, y, z)
end

--- 原生 vector -> FVector（FVector 参数也可以直接传 vector，这一步通常不必要）
function M.ToFVector(v)
    return UE.FVector(v)
end

--- FVector -> 原生 vector；取不到返回 nil
function M.FromFVector(v)
    if not v or type(v.ToVector) ~= "function" then
        return nil
    end
    return v:ToVector()
end

return M
