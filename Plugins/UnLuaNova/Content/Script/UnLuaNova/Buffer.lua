--[[
    buffer 与 TArray 互转的小工具（UnLuaNova.Buffer）

    Luau 的 buffer 是一块定长、可按类型读写的字节块，适合承载字节流；
    UE 侧对应的通常是 TArray<uint8>。这两个函数做显式转换：

        local Bytes = UE.TArray(0)
        Bytes:Add(1) Bytes:Add(2)
        local Buf = UnLuaNova.Buffer.FromArray(Bytes)   -- TArray -> buffer
        local Back = UnLuaNova.Buffer.ToArray(Buf)      -- buffer -> TArray

    Luau 后端下这两个函数会走 C++（UnLuaNova.BufferFromArray / BufferToArray）。
    为什么值得下沉：Lua 版本每个字节要跨一次语言边界（array:Get(i) / array:Add(x)），
    C++ 版本对 **1 字节元素**（TArray<uint8> —— 字节数组 UPROPERTY 绑出来的就是它）是
    一次 memcpy，因为数组的存储本身就是那段字节；更宽的元素类型只能逐元素，但循环在
    C++ 里，不再来回跨边界。

    关于"零拷贝"：做不到，而且不是我们没做。Luau 的 C API 只有 lua_newbuffer 和
    lua_tobuffer，没有任何把外部内存包成 buffer 的接口 —— buffer 永远拥有 VM 自己
    分配的那一块内存，不可能成为数组存储的别名。1 字节元素下"一次 memcpy"就是上限。

    逐字节的 Lua 实现保留作回退（plain Lua 后端没有 buffer 类型，HasBuffer() 为 false；
    C++ 函数也只在 Luau 下注册）。
]]

local M = {}

-- 注意：与 Vector.lua 一样，这里刻意不写类型注解 —— 本模块要能在两个后端下加载。

--- 当前后端有没有 Luau 的 buffer 类型
function M.HasBuffer()
    return type(buffer) == "table"
end

--- TArray<number> -> buffer
function M.FromArray(array)
    if type(buffer) ~= "table" then
        return nil
    end

    if UnLuaNova.BufferFromArray then
        return UnLuaNova.BufferFromArray(array)
    end

    local Count = array:Length()
    local Buf = buffer.create(Count)
    for i = 1, Count do
        buffer.writeu8(Buf, i - 1, array:Get(i))
    end
    return Buf
end

--- buffer -> TArray<number>
function M.ToArray(buf)
    if type(buffer) ~= "table" then
        return nil
    end

    if UnLuaNova.BufferToArray then
        return UnLuaNova.BufferToArray(buf)
    end

    local array = UE.TArray(0)
    for i = 0, buffer.len(buf) - 1 do
        array:Add(buffer.readu8(buf, i))
    end
    return array
end

return M
