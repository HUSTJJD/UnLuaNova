-- UnLuaNova.Event —— 事件总线（纯 Lua，随插件发布）
--
-- 为什么放在插件里而不是每个项目自己写：它的难点不在"存一张表"，在这四件事，
-- 手写版本几乎每条都会漏：
--
--   ① 广播中增删监听者 —— 边遍历边改表，漏发或重发（这里用快照）
--   ② 一个监听者报错带崩整条广播 —— 这里逐个 pcall，错误只记日志
--   ③ 监听者持有已销毁的 UObject —— 这里提供 On(name, fn, owner) + UnbindOwner(owner)，
--      对象销毁时显式摘干净；token 仍强持有 owner，弱键索引不提供自动解绑
--   ④ 热重载后残留旧闭包 —— 旧模块表没了、旧闭包还挂在总线上。唯一可靠的做法是
--      "带 owner 注册 + 停用时 UnbindOwner"，本模块为此提供这两个 API
--
-- 它不规定事件名或发送权限；调用方仍需在 owner 停用时显式解绑。
-- 项目层的玩法事件语义（UI 栈、场景流）请自己在上层定，不要往这里塞。
--
-- 语法保持 Lua 5.1 兼容：不用 continue / 复合赋值 / 类型标注，两个后端都能编。

local M = {}

-- 编译期开关自检：本库被关掉时，**明确报错**而不是返回一个半残的表。
-- 脚本侧拿到 nil 或空表只会演化成"后面某个地方莫名其妙不好使"，而这里能一次说清原因。
if _G.UnLuaNova and _G.UnLuaNova.HasFeature and not _G.UnLuaNova.HasFeature('event') then
	error("UnLuaNova.Event: 当前构建关闭了该内置库（UNLUANOVA_HAS_STDLIB_EVENT=0，"
		.. "见 DefaultUnLuaNova.ini 的 bEvent，改完要重编）", 0)
end

local type = _G.type
local error = _G.error
local pcall = _G.pcall
local setmetatable = _G.setmetatable
local tinsert = _G.table.insert
local tremove = _G.table.remove

local LogError = _G.UnLuaNova and _G.UnLuaNova.LogError or _G.print

--- name -> { token, ... }
local Listeners = {}

--- owner -> { [token] = name }; live tokens retain their owner until unsubscribed.
local OwnerIndex = setmetatable({}, { __mode = "k" })

local NextTokenId = 0

local function ListFor(name)
	local list = Listeners[name]
	if not list then
		list = {}
		Listeners[name] = list
	end
	return list
end

local function Track(token)
	if token.Owner == nil then
		return
	end
	local tokens = OwnerIndex[token.Owner]
	if not tokens then
		tokens = {}
		OwnerIndex[token.Owner] = tokens
	end
	tokens[token] = token.Name
end

local function Untrack(token)
	if token.Owner == nil then
		return
	end
	local tokens = OwnerIndex[token.Owner]
	if tokens then
		tokens[token] = nil
	end
end

local function Register(name, fn, owner, once)
	if type(name) ~= "string" then
		error("UnLuaNova.Event: 事件名必须是字符串", 3)
	end
	if type(fn) ~= "function" then
		error("UnLuaNova.Event: 回调必须是函数", 3)
	end

	NextTokenId = NextTokenId + 1
	local token = {
		Id = NextTokenId,
		Name = name,
		Fn = fn,
		Once = once and true or false,
		Owner = owner,
	}
	tinsert(ListFor(name), token)
	Track(token)
	return token
end

--- 订阅。owner 可选：传了就能用 UnbindOwner(owner) 一次摘干净（热重载 / 对象销毁时用它）
function M.On(name, fn, owner)
	return Register(name, fn, owner, false)
end

--- 只触发一次
function M.Once(name, fn, owner)
	return Register(name, fn, owner, true)
end

local function RemoveToken(token)
	token.Fn = nil          -- 置空后，正在进行的广播会跳过它
	Untrack(token)
	token.Owner = nil
	local list = Listeners[token.Name]
	if not list then
		return false
	end
	for i = #list, 1, -1 do
		if list[i] == token then
			tremove(list, i)
			return true
		end
	end
	return false
end

--- 取消订阅，三种写法：
---   Off(token)                 取消某一个
---   Off(name, fn)              取消某个事件下这个函数（全部）
---   Off(name)                  清空这个事件
function M.Off(tokenOrName, fn)
	local kind = type(tokenOrName)

	if kind == "table" then
		return RemoveToken(tokenOrName)
	end

	if kind ~= "string" then
		return false
	end

	local list = Listeners[tokenOrName]
	if not list then
		return false
	end

	if type(fn) == "function" then
		local removed = 0
		for i = #list, 1, -1 do
			if list[i].Fn == fn then
				RemoveToken(list[i])
				removed = removed + 1
			end
		end
		return removed > 0
	end

	for i = #list, 1, -1 do
		RemoveToken(list[i])
	end
	return true
end

--- 摘掉某个 owner 名下的全部订阅。对象销毁 / 模块停用时调用它，
--- 这是"监听者抓着已经死掉的 UObject"这类崩溃的正解。
function M.UnbindOwner(owner)
	local tokens = OwnerIndex[owner]
	if not tokens then
		return 0
	end

	local removed = 0
	for token in pairs(tokens) do
		if RemoveToken(token) then
			removed = removed + 1
		end
	end
	OwnerIndex[owner] = nil
	return removed
end

--- 广播。返回实际触发的监听者数量。
--- 快照后再逐个 pcall：广播中 On/Off 不会漏发或重发，单个监听者报错不影响别人。
function M.Emit(name, ...)
	local list = Listeners[name]
	if not list or #list == 0 then
		return 0
	end

	local snapshot = {}
	for i = 1, #list do
		snapshot[i] = list[i]
	end

	local fired = 0
	for i = 1, #snapshot do
		local token = snapshot[i]
		if token.Fn ~= nil then
			fired = fired + 1
			local fn = token.Fn
			-- Detach before calling so recursive Emit cannot invoke a Once listener again.
			if token.Once then
				RemoveToken(token)
			end
			local ok, err = pcall(fn, ...)
			if not ok then
				LogError(string.format("UnLuaNova.Event: '%s' 的监听者出错：%s",
					tostring(name), tostring(err)))
			end
		end
	end
	return fired
end

--- 某个事件下有多少监听者
function M.Count(name)
	local list = Listeners[name]
	return list and #list or 0
end

--- 清空（不传 name 就全清）。热重载后想从干净状态开始时用它。
function M.Clear(name)
	if type(name) == "string" then
		local list = Listeners[name]
		if list then
			for i = #list, 1, -1 do
				RemoveToken(list[i])
			end
		end
		return
	end
	for _, list in pairs(Listeners) do
		for i = #list, 1, -1 do
			RemoveToken(list[i])
		end
	end
end

return M
