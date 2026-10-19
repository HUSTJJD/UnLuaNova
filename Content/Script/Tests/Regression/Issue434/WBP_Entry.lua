local M = UnLuauNova.Class()

function M:InitState(Parent, Name)
    self.Name = Name
    Parent.TestDelegate:Add(self, M.OnTestDelegate)
end

function M:OnTestDelegate(Count)
    local text = tostring(Count)
    if UnLuauNova.FTextEnabled then
        text = UE.FText.FromString(text)
    end
    self.Text:SetText(text)
    _G[self.Name] = Count
end

return M