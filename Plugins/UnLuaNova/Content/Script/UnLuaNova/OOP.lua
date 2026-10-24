-- UnLuaNova.OOP —— 脚本侧的面向对象（纯 Lua，随插件发布，可插拔）
--
-- 为什么内置：Lua 没有 class，于是每个项目都自己写一个，而手写版本迟早在
-- "构造链"和"Super 访问"上出事 —— 要么父类的 Ctor 被跳过，要么 `Super:Method()`
-- 把父类当 self 传进去。这两类 bug 都不报错，只是行为不对，排查成本极高。
--
-- 提供什么：
--   * 单继承 + 方法查找沿父类链（setmetatable 到父类，不是复制方法表）
--   * New() 时**从基类到子类**依次调用 Ctor，参数透传
--   * Class.Super 显式可用（Dog.Super.Ctor(self, ...)）
--   * Is(obj, class) 判继承关系
--
-- 不提供什么（有意的边界）：
--   * 多继承 / mixin —— 组合优于继承，需要就在上层叠
--   * 继承 UE 类型 —— 那是 native 的 UnLuaNova.Class() 干的（绑定 UCLASS），
--     两者不是一回事，别混用
--   * 生命周期钩子（OnInit/OnTick/OnClear）—— 那是项目框架层的口味，属于 Ann 那层
--
-- 语法保持 Lua 5.1 兼容：不用 continue / 复合赋值 / 类型标注，两个后端都能编。

local M = {}

if _G.UnLuaNova and _G.UnLuaNova.HasFeature and not _G.UnLuaNova.HasFeature('oop') then
	error("UnLuaNova.OOP: 当前构建关闭了该内置库（UNLUANOVA_HAS_STDLIB_OOP=0，"
		.. "见 DefaultUnLuaNova.ini 的 bOOP，改完要重编）", 0)
end

local type = _G.type
local error = _G.error
local tostring = _G.tostring
local setmetatable = _G.setmetatable
local getmetatable = _G.getmetatable
local rawget = _G.rawget
local tinsert = _G.table.insert

--- 从基类到子类的构造链，创建类时算一次，别让每次 New 都爬一遍继承链
local function BuildChain(Class)
	local Reversed = {}
	local Current = Class
	while Current do
		tinsert(Reversed, 1, Current)
		Current = rawget(Current, "Super")
	end
	return Reversed
end

--- 建一个类。两种写法：Class("Dog") / Class("Dog", Animal)，也可以 Class(Animal) 省略名字。
function M.Class(NameOrSuper, MaybeSuper)
	local Name, Super
	if type(NameOrSuper) == "string" then
		Name = NameOrSuper
		Super = MaybeSuper
	else
		Name = nil
		Super = NameOrSuper
	end

	if Super ~= nil and type(Super) ~= "table" then
		error("UnLuaNova.OOP.Class: 父类必须是表（或 nil）", 2)
	end

	local Class = {}
	Class.__IsClass = true
	Class.ClassName = Name or "Unnamed"
	Class.Super = Super
	Class.__index = Class        -- 实例查方法先查自己的类
	Class.__Chain = nil          -- 惰性算，见 New

	-- 类本身查方法沿父类链：不复制方法表，父类后来加的方法子类也能看到
	if Super then
		setmetatable(Class, { __index = Super })
	end

	Class.__tostring = function(Instance)
		return "instance of " .. tostring(Class.ClassName)
	end

	function Class:New(...)
		if not self.__Chain then
			self.__Chain = BuildChain(self)
		end

		local Instance = setmetatable({}, self)

		-- 基类 → 子类依次构造，参数原样透传给每一层
		local Chain = self.__Chain
		for i = 1, #Chain do
			local Ctor = rawget(Chain[i], "Ctor")
			if Ctor then
				Ctor(Instance, ...)
			end
		end

		return Instance
	end

	return Class
end

--- 判断实例是否属于某个类（含父类）。非表一律 false，不去碰 userdata 的 metatable。
function M.Is(Instance, Class)
	if type(Instance) ~= "table" or type(Class) ~= "table" then
		return false
	end

	local Current = getmetatable(Instance)
	while Current do
		if Current == Class then
			return true
		end
		Current = rawget(Current, "Super")
	end
	return false
end

--- 是不是本模块建出来的类
-- 用 rawget：实例的 __index 指向类，用 `.` 取会把实例也判成类
function M.IsClass(Value)
	return type(Value) == "table" and rawget(Value, "__IsClass") == true
end

--- 类名，用于日志；不是类就退回 type()
function M.NameOf(Value)
	if M.IsClass(Value) then
		return Value.ClassName
	end
	if type(Value) == "table" then
		local Meta = getmetatable(Value)
		if Meta and rawget(Meta, "__IsClass") then
			return rawget(Meta, "ClassName")
		end
	end
	return type(Value)
end

return M
