local StringEx = require("UnLuaNova.StringEx")
local Spec = require("Tests.Lua.Spec")

Spec.File("StringEx.spec.lua")

-- The helpers are installed onto `string` at env startup, which is the form game scripts
-- actually use; the module table is what a script gets from require(). Both are asserted,
-- because "installed but wrong" and "correct but not installed" are different bugs.

Spec.describe("split", function()
    Spec.it("按字面量分隔", function()
        local Parts = string.split("a,b,c", ",")
        Spec.equal(#Parts, 3)
        Spec.equal(Parts[1], "a")
        Spec.equal(Parts[3], "c")
    end)

    Spec.it("分隔符默认是逗号", function()
        local Parts = string.split("a,b")
        Spec.equal(#Parts, 2)
    end)

    Spec.it("分隔符不是模式，点号按字面量处理", function()
        -- The reason this file has a case at all: string.find without the plain flag
        -- treats "." as any character and returns an empty table.
        local Parts = string.split("a.b", ".")
        Spec.equal(#Parts, 2)
        Spec.equal(Parts[1], "a")
        Spec.equal(Parts[2], "b")
    end)

    Spec.it("空分隔符返回整串", function()
        local Parts = string.split("abc", "")
        Spec.equal(#Parts, 1)
        Spec.equal(Parts[1], "abc")
    end)
end)

Spec.describe("strip", function()
    Spec.it("去掉首尾空白", function()
        Spec.equal(string.strip("  x  "), "x")
    end)

    Spec.it("trim 是同一个函数", function()
        Spec.equal(string.trim("  x  "), "x")
    end)

    Spec.it("没有空白时原样返回", function()
        Spec.equal(string.strip("x"), "x")
    end)
end)

Spec.describe("startswith / endswith", function()
    Spec.it("前后缀判断", function()
        Spec.equal(string.startswith("UnLuaNova", "UnLua"), true)
        Spec.equal(string.endswith("UnLuaNova", "Nova"), true)
    end)

    Spec.it("空串恒为真", function()
        Spec.equal(string.startswith("x", ""), true)
        Spec.equal(string.endswith("x", ""), true)
    end)

    Spec.it("不匹配时返回 false", function()
        Spec.equal(string.startswith("x", "y"), false)
        Spec.equal(string.endswith("x", "y"), false)
    end)
end)

Spec.describe("count", function()
    Spec.it("统计不重叠出现次数", function()
        Spec.equal(string.count("aaa", "a"), 3)
        Spec.equal(string.count("aaaa", "aa"), 2)
    end)

    Spec.it("没有出现时返回 0", function()
        Spec.equal(string.count("abc", "z"), 0)
    end)
end)

Spec.describe("join", function()
    Spec.it("用分隔符拼接", function()
        Spec.equal(StringEx.join(",", { 1, 2, 3 }), "1,2,3")
    end)

    Spec.it("不在 string 表上", function()
        -- Deliberate: separator-first reads wrong as a method.
        Spec.equal(string.join, nil)
    end)
end)
