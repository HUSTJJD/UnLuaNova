--[[
    buffer 与 TArray 互转的小工具（UnLuaNova.Buffer）

    Luau 的 buffer 是一块定长、可按类型读写的字节块，适合承载字节流；
    UE 侧对应的通常是 TArray<uint8>。这两个函数做显式转换：

        local Bytes = UE.TArray(0)
        Bytes:Add(1) Bytes:Add(2)
        local Buf = UnLuaNova.Buffer.FromArray(Bytes)   -- TArray -> buffer
        local Back = UnLuaNova.Buffer.ToArray(Buf)      -- buffer -> TArray

    逐字节循环是为了保持行为直白；它是 O(n)，不适合放在每帧的热路径上。
    Lua 5.5 后端没有 buffer 类型，HasBuffer() 返回 false。
]]

local M = {}

--- 当前后端有没有 Luau 的 buffer 类型
function M.HasBuffer(): boolean
    return type(buffer) == "table"
end

--- TArray<number> -> buffer
function M.FromArray(array)
    if type(buffer) ~= "table" then
        return nil
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

    local array = UE.TArray(0)
    for i = 0, buffer.len(buf) - 1 do
        array:Add(buffer.readu8(buf, i))
    end
    return array
end

return M
