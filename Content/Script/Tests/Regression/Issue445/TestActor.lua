local M = UnLuaNova.Class()

function M:TestForIssue445()
    return UE.UClass.Load("/UnLuaNovaTestSuite/Tests/Regression/Issue445/UMG_Issue445.UMG_Issue445_C")
end

return M
