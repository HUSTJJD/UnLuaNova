local M = {}

function M:Run()
    assert(UnLuaNova.HasFeature("buffer"))
    local Buffer = require("UnLuaNova.Buffer")
    local values = UE.TArray(0)
    values:Add(0)
    values:Add(127)
    values:Add(255)
    local bytes = Buffer.FromArray(values)
    assert(buffer.len(bytes) == 3)
    buffer.writeu8(bytes, 1, 42)
    assert(values:Get(2) == 127) -- The buffer owns a copy.
    local restored = Buffer.ToArray(bytes)
    assert(restored:Length() == 3 and restored:Get(2) == 42)
    return true
end

return M
