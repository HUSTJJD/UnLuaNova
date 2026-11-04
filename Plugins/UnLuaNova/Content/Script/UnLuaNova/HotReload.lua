------------------------------------------
--- HotFix - 运行时HotFix支持 参考云风方案
--- 替换运行环境中的函数，保持upvalue和运行时的table
--- 为开发期设计，尽量替换。最差的结果就是重新启动
--- 原表改变，类型改变 后逻辑由业务保证正确
--- 基于lua 5.3，重定向require，使用env加载实现沙盒
--- 提供一系列回调注册，可以在hotfix前后做处理
------------------------------------------
--- 【失败重载回滚 / failed reload rollback】
--- reload 采用「两阶段提交」：先在沙盒里加载并计算新代码（不触碰任何旧对象），
--- 再用 xpcall 统一提交；任一阶段失败都会尽力恢复到提交前的状态。
--- M.reload() / reload_modules() 返回 ok, err：成功 true，失败 false + 错误信息。
---
--- ⚠ 回滚边界（这不是事务，只是尽力而为的状态恢复，切勿当作全量回滚）：
---   可回滚：模块表(old_module 顶层键值)、package.loaded、loaded_modules、
---           loaded_module_times —— 这几项在提交前做了强引用浅快照。
---   不可回滚：update_global 会原地改写 _G、运行栈局部变量、任意 table / upvalue /
---           registry，对它们做全量快照不可行。提交失败后这些地方可能残留指向
---           新函数 / 新表的引用，调用方不应假设「失败 == 状态完全没变」。
---   同理，无参 reload 只有在提交成功后才推进 mtime：失败必须保留旧时间戳，
---   否则下一次 reload 扫不到这次改动，改动会被静默丢失且无法重试。
------------------------------------------
-- 在Module中如果定义此标记，则对此module进行reload操作
-- local M = { HOT_RELOAD = true }

local HOT_RELOAD_MARK = "HOT_RELOAD"
local loaded_modules = setmetatable({}, { __mode = "v" })
local ignore_modules = {}
local config = {
    debug = false,
    script_root_path = UE.UUnLuaNovaFunctionLibrary.GetScriptRootPath(),
    ignore_modules = ignore_modules
}
local hook = {
    module_loaded = nil
}
local M = {
    config = config,
    hook = hook
}

local dump = function(tbl, max_indent)
    if not config.debug then return "" end
    local rep = string.rep
    local handled = {}
    local function traverse(tbl, indent)
        if not indent then indent = 0 end
        if type(tbl) ~= "table" then return tostring(tbl) end

        if(indent > max_indent) then return tostring(tbl) end

        if handled[tbl] then
            return ""
        end
        handled[tbl] = true
        local ret = rep(" ", indent) .. "{\r\n"
        indent = indent + 2 
        for k, v in pairs(tbl) do
            ret = ret .. rep(" ", indent)
            if type(k) == "number" then
                ret = ret .. "[" .. k .. "] = "
            elseif type(k) == "string" then
                ret = ret  .. k ..  " = "   
            else
                ret = ret  .. tostring(k) ..  " = "   
            end
            if type(v) == "number" then
                ret = ret .. v .. ",\r\n"
            elseif type(v) == "string" then
                ret = ret .. "\"" .. v .. "\",\r\n"
            elseif type(v) == "table" then
                ret = ret .. tostring(v) .. traverse(v, indent + 2) .. ",\r\n"
            else
                ret = ret .. "\"" .. tostring(v) .. "\",\r\n"
            end
        end
        ret = ret .. rep(" ", indent - 2) .. "}"
        return ret
    end

    if max_indent == nil then
        max_indent = 64
    end
    return traverse(tbl)
end

local print = function(...)
    if config.debug then
        Lua.Log("HotReload ", ...)
    end
end

local table = table
local debug = debug
local pairs = pairs
local origin_require = require

local function safe_pairs(t)
    local _next, _t, _nil = pairs(t)
    local function safe_next(_t, i)
        local ok, result, next_i, value = pcall(_next, _t, i)
        if ok then
            return result, next_i, value
        else
            return nil
        end
    end
    return safe_next, _t, _nil
end

local function load_error_handler(err)
    local msg = err .. "\n" .. debug.traceback()
    Lua.LogError(msg)
    return msg
end

local function call_hook(name, ...)
    local func = hook[name]
    if not func then
        return
    end
    local ok, result = pcall(func, ...)
    if not ok then
        print(string.format("calling hook function '%s' failed : %s", name, result))
    end
end

--- 脚本加载的修改时间
---@type table<string, number>
local loaded_module_times = {}

local function get_last_modified_time(module_name)
    local filename = config.script_root_path .. module_name:gsub("%.", "/") .. ".lua"
    return UE.UUnLuaNovaFunctionLibrary.GetFileLastModifiedTimestamp(filename)
end

local function make_sandbox()
    local reloading
    local loaded
    local proxy = setmetatable({}, { __index = _G, __newindex = _G })
    local env_mt = { __index = proxy, __newindex = proxy }

    local function load(module_name)
        local found, chunk, loader_data
        for i, searcher in ipairs(package.searchers) do
            chunk, loader_data = searcher(module_name)
            if type(chunk) == "function" then
                found = true
                break
            end
        end

        if not found then
            return
        end

        local env = {}
        setmetatable(env, env_mt)
        if setfenv then
            -- C loaders have no replaceable environment.
            pcall(setfenv, chunk, env)
        else
            local i = 1
            while true do
                local name = debug.getupvalue(chunk, i)
                if not name then break end
                if name == "_ENV" then
                    debug.setupvalue(chunk, i, env)
                    break
                end
                i = i + 1
            end
        end
        return chunk, env, loader_data
    end

    local function require(module_name, ...)
        -- https://github.com/lua/lua/blob/v5.4.0/loadlib.c#L680
        -- https://github.com/lua/lua/blob/v5.3/loadlib.c#L617
        -- lua5.4之后会返回2个值，这里保持一样的行为
        if package.loaded[module_name] ~= nil then
            return package.loaded[module_name], nil
        end

        local loaded = loaded_modules[module_name]
        if loaded then
            if package.loaded[module_name] == nil then
                package.loaded[module_name] = loaded
            end
            return loaded, nil
        end

        local func, env, loader_data = load(module_name)
        if func then
            local ok, new_module
            if select("#", ...) > 0 then
                -- FLuaEnv.Start supplies an explicit startup argument table.
                ok, new_module = xpcall(func, load_error_handler, ...)
            else
                ok, new_module = xpcall(func, load_error_handler, module_name, loader_data)
            end
            if not ok then error(new_module or ("error loading module " .. module_name), 0) end
            if loaded_modules[module_name] == nil then
                loaded_modules[module_name] = new_module
                -- package.loaded is the record of "this module ran": FLuaEnv::Start() and
                -- LowLevel::GetLoadedModule() both read it, and both native require()s -
                -- plain Lua's own, and the one this env installs on Luau - store `true`
                -- for a module that returns nothing. Writing the nil verbatim deletes the
                -- entry instead, so a module that did run reads back as one that never
                -- did (Start() reports it at Error) and the next require() runs it again.
                if new_module ~= nil then
                    package.loaded[module_name] = new_module
                elseif package.loaded[module_name] == nil then
                    package.loaded[module_name] = true
                end
                loaded_module_times[module_name] = get_last_modified_time(module_name)
                call_hook("module_loaded", new_module, module_name, false)
                return package.loaded[module_name], nil
            end
        else
            return origin_require(module_name, ...)
        end
        return env, nil
    end

    local function enter(modules)
        reloading = true
        loaded = setmetatable({}, { __mode = "kv" })

        for name, module in pairs(modules) do
            loaded[name] = module
            loaded[module] = name     
        end
    end

    local function exit()
        reloading = false
        loaded = setmetatable({}, { __mode = "kv" })
    end

    local function is_loaded(obj)
        return loaded[obj] ~= nil
    end

    proxy.require = function(module_name, ...)
        if reloading then
            if loaded[module_name] ~= nil then
                return loaded[module_name]
            end
        end
        local ret = require(module_name, ...)
        if reloading then
            loaded[module_name] = ret
            loaded[ret] = module_name
        end
        return ret
    end

    return {
        enter = enter,
        exit = exit,
        load = load,
        require = require,
        is_loaded = is_loaded,
    }
end

local sandbox = make_sandbox()

local function merge_objects(module_res)
    for _, m in ipairs(module_res) do
        assert(m.old_module)
        for index, v in ipairs(m.values) do
            for name, value_map in pairs(v) do
                for old, new in pairs(value_map) do
                    if old == nil and not sandbox.is_loaded(new) then
                        m.old_module[name] = new
                    elseif type(new) == "table" and not sandbox.is_loaded(new) then
                        print("COPY", tostring(new), tostring(old))
                        for k, nv in pairs(new) do
                            old[k] = nv
                        end
                    elseif type(new) == "function" then
                        local i = 1
                        while true do
                            local name,v = debug.getupvalue(new, i)
                            if name == nil or name == "" then
                                break
                            end
                            local id = debug.upvalueid(new, i)
                            local uv = m.upvalue_map[id]
                            if uv then
                                print("SET UV :", tostring(new), name, tostring(uv.replaced_upvalue))
                                debug.setupvalue(new, i, uv.replaced_upvalue)
                            end
                        i = i + 1
                        end
                    end
                end			
            end
        end
    end
end

--- 枚举出module中所有函数的upvalue，Lua在不同的闭包间访问相同的外部local变量时，使用的是同样的upvalue
---@param moudule table
local function collect_module_upvalues(moudule)
    local function collect_function_upvalues(func, upvalues)
        assert(type(func) == "function")
        local i = 1
        while true do
            local name, value = debug.getupvalue(func, i)
            if name == nil or name == "" then
                break
            else
                if not name:find("^[_%w]") then
                    error("Invalid upvalue : " .. table.concat(path, "."))
                end
                if not upvalues[name] then
                    upvalues[name] = value
                    if type(value) == "function" then
                        collect_function_upvalues(value, upvalues)
                    end
                end
            end
            i = i + 1
        end
    end

    local ret = {}
    for _, v in pairs(moudule) do
        if type(v) == "function" then
            collect_function_upvalues(v, ret)
        end
    end
    return ret
end

local function collect_module_info(module)
    local ret = {}
    if sandbox.is_loaded(module) then
        return ret
    end
    for k, v in pairs(module) do
        if sandbox.is_loaded(v) then
        elseif type(v) == "function" then
            table.insert(ret, {name = k, value = v})
        end
    end
    return ret
end

---@param new_module_info table
---@param old_module table
---@return table
local function match_module(new_module_info, old_module)
    local ret = {}
    for _, v in ipairs(new_module_info) do
        local oldFun = rawget(old_module, v.name)
        if oldFun and oldFun ~= v.value then
            table.insert(ret, { [v.name] = { [oldFun] = v.value } } )
        end
    end

    return ret
end

---@param value_info_map table
---@param old_upvalues table
---@return table
local function match_upvalues(value_info_map, old_upvalues)
    local ret = {}

    for index, v in ipairs(value_info_map) do
        for name, value_map in pairs(v) do
            for old, new in pairs(value_map) do
                if type(new) == "function" then
                    local i = 1
                    while true do
                        local name, new_upvalue = debug.getupvalue(new, i)
                        if name == nil or name == "" then
                            break
                        end
                        local id = debug.upvalueid(new, i)
                        if not ret[id] then
                            local replaced_upvalue = nil
                            if old_upvalues[name] ~= nil then
                                replaced_upvalue = old_upvalues[name]
                            else
                                -- 新增的upvalue
                                print("ADD NEW UPVALUE : ", tostring(new), name, tostring(new_upvalue))
                                -- 解决新增的upvalue是当前module
                                if type(new_upvalue) == "table" or type(new_upvalue) == "function" and value_info_map[new_upvalue] ~= nil then
                                    replaced_upvalue = value_info_map[new_upvalue]
                                else
                                    replaced_upvalue = new_upvalue
                                end
                            end
    
                            if replaced_upvalue then
                                ret[id] = { replaced_upvalue = replaced_upvalue }
                            end
                        end
                        i = i + 1
                    end
                end
            end
        end
    end

    return ret
end

local function update_global(value_map)
    local running_state = coroutine.running()
    local exclude = { [debug] = true, [coroutine] = true }
    if io then exclude[io] = true end
    exclude[exclude] = true
    exclude[M] = true
    exclude[sandbox] = true
    exclude[value_map] = true

    exclude[package] = true
    exclude[package.loaded] = true
    exclude[loaded_modules] = true

    local update_table

    local function update_running_stack(co, level)
        -- Luau does not eliminate tail calls: recursion would grow the stack as we inspect it.
        while true do
            local f
            if debug.getinfo then
                local info = debug.getinfo(co, level + 1, "f")
                f = info and info.func
            elseif co then
                f = debug.info(co, level + 1, "f")
            else
                f = debug.info(level + 1, "f")
            end
            if f == nil then
                return
            end
            update_table(f)
            local i = 1
            while true do
                local name, v
                if co then
                    name, v = debug.getlocal(co, level + 1, i)
                else
                    name, v = debug.getlocal(level + 1, i)
                end
                if name == nil then
                    if i > 0 then
                        i = -1
                    else
                        break
                    end
                end
                local nv = value_map[v]
                if nv then
                    if co then
                        debug.setlocal(co, level + 1, i, nv)
                    else
                        debug.setlocal(level + 1, i, nv)
                    end
                    update_table(nv)
                else
                    update_table(v)
                end
                if i > 0 then
                    i = i + 1
                else
                    i = i - 1
                end
            end
            level = level + 1
        end
    end

    function update_table(root)
        local t = type(root)
        if t ~= "table" and t ~= "userdata" and t ~= "function" then
            return
        end
        if root == nil or exclude[root] then
            return
        end
        -- print("update_table : ", dump(root, 2))
        exclude[root] = true
        if t == "table" then
            local mt = getmetatable(root)
            if mt then update_table(mt) end
            local ReplaceK = {}
            for key, value in safe_pairs(root) do
                local nv = value_map[value]
                if nv then
                    rawset(root, key, nv)
                    update_table(nv)
                else
                    update_table(value)
                end
                nv = value_map[key]
                if nv then
                    ReplaceK[key] = nv
                end
            end
            for key, value in pairs(ReplaceK) do
                root[key], root[value] = nil, root[key]
                update_table(value)
            end
        elseif t == "userdata" then
            local mt = getmetatable(root)
            if mt then update_table(mt) end
            local user_value = debug.getuservalue(root)
            if user_value then
                local nv = value_map[user_value]
                if nv then
                    debug.setuservalue(root, user_value)
                    update_table(nv)
                else
                    update_table(user_value)
                end
            end
        elseif t == "function" then
            local i = 1
            while true do
                local name, v = debug.getupvalue(root, i)
                if name == nil then
                    break
                else
                    local nv = value_map[v]
                    if nv then
                        update_table(nv)
                    else
                        update_table(v)
                    end
                end
                i=i+1
            end
        end
    end

    update_running_stack(running_state, 2)
    update_table(_G)
    -- Luau 后端把 debug.getregistry 安装成了「抛错」版本（LuaScriptCompat.cpp），
    -- 而此时 _G / 运行栈的更新已经完成且无法回滚。registry 更新属于"尽力而为"：
    -- 任何异常都只跳过这一步，绝不让整次 reload 因此失败。
    local reg_ok = pcall(function()
        local registry = debug.getregistry()
        if registry ~= nil then
            update_table(registry)
        end
    end)
    if not reg_ok then
        print("update_global: registry unavailable, registry scan skipped")
    end
end

local function update_modules(old_modules, new_modules, new_envs)
    print("HOT RELOAD START")

    local result = {}
    for i, old_module in ipairs(old_modules) do
        local new_module = new_modules[i]
        local new_env = new_envs[i]

        if new_module[HOT_RELOAD_MARK] then
            local moduleres = {
                values = {},
                old_module = old_module
            }
            table.insert(moduleres.values, { [new_module] = { [old_module] = new_module } } )
            print("--------------Print ValueMap--------------")
            print(dump(moduleres.values, 6))
            result[i] = moduleres
        else
            local new_module_info = collect_module_info(new_module)
            print("--------------Print NewModuleInfo--------------")
            print(dump(new_module_info))
            local old_module_upvalues = collect_module_upvalues(old_module)
            print("--------------Print OldModuleUpValue--------------")
            print(dump(old_module_upvalues, 6))
            
            local moduleres = {
                values = {},
                upvalue_map = {},
                old_module = old_module,
            }

            moduleres.values = match_module(new_module_info, old_module)

            for k, v in pairs(new_module) do
                if type(v) == "function" then
                    old_module[k] = v
                end
                if old_module[k] == nil then
                    old_module[k] = v
                end
            end

            print("--------------Print ValueMap--------------")
            print(dump(moduleres.values))

            moduleres.upvalue_map = match_upvalues(moduleres.values, old_module_upvalues)
            print("--------------Print UVMap--------------")
            print(dump(moduleres.upvalue_map, 10))
            
            result[i] = moduleres
        end
    end

    merge_objects(result)
    local all_value_maps = {}
    for _, rv in ipairs(result) do
        for _, v in ipairs(rv.values) do
            for name, value_map in pairs(v) do
                for key, value in pairs(value_map) do
                    all_value_maps[key] = value
                end
            end
        end
    end

    print("--------------Print AllValueMap--------------")
    print(dump(all_value_maps))
    update_global(all_value_maps)
    for name, old_module in pairs(loaded_modules) do
        local replacement = all_value_maps[old_module]
        if replacement then
            loaded_modules[name] = replacement
            if package.loaded[name] == old_module then
                package.loaded[name] = replacement
            end
        end
    end

    print("HOT RELOAD END")
end

--- 重载指定模块（两阶段提交 / failed reload rollback）。
---
--- 阶段 1「计算」：sandbox.enter 之后只加载新代码、构建匹配数据，不写任何旧对象，
---   也不改写 loaded_modules / package.loaded / loaded_module_times。任一模块在加载或
---   计算中失败，立即 sandbox.exit() 并返回 false, err，旧状态原样保留。
--- 阶段 2「提交」：用 xpcall 包裹 update_modules（merge_objects / update_global /
---   回写 loaded_modules 与 package.loaded）。提交前已对本次涉及的每个 name 采集
---   强引用浅快照；提交抛错时把快照 rawset 回去，并恢复三张状态表。
---   module_loaded hook 只在提交成功之后才发。
---
--- ⚠ 回滚边界（这不是事务，只是尽力而为的状态恢复，切勿当作全量回滚）：
---   可回滚：old_module 的顶层键值、package.loaded、loaded_modules、
---           loaded_module_times —— 进入本函数前做了强引用浅快照。
---   不可回滚：update_global 会原地改写 _G、运行栈局部变量、任意 table / upvalue /
---           registry，对它们做全量快照不可行。提交失败后这些地方可能残留指向新函数
---           或新表的引用，调用方不应假设「失败 == 状态完全没变」。
---   另：无旧对象、由 sandbox.require 保底首次加载的模块，其首次加载自带的
---           module_loaded(false) hook 属于 require 既有语义，不在本次事务内。
---
---@param module_names table
---@return boolean ok  成功为 true
---@return string|nil err 失败时的错误信息
local function reload_modules(module_names)
    if not module_names or #module_names == 0 then
        return true
    end

    -- 在任何状态写入之前，采集本次涉及的所有 name 的原状态（用于失败回滚）。
    -- originals 同时持有旧模块对象的强引用，避免提交期间被 loaded_modules 弱值表 GC 掉。
    local originals = {}
    for _, module_name in ipairs(module_names) do
        local old_module = loaded_modules[module_name]
        local entry = {
            old_module = old_module,                 -- 强引用保活（fresh 模块为 nil）
            had_lm = old_module ~= nil,
            had_pl = package.loaded[module_name] ~= nil,
            pl_val = package.loaded[module_name],
            had_mt = loaded_module_times[module_name] ~= nil,
            mt_val = loaded_module_times[module_name],
            old_keys = nil,
        }
        if type(old_module) == "table" then
            local keys = {}
            for k, v in pairs(old_module) do
                keys[k] = v
            end
            entry.old_keys = keys
        end
        originals[module_name] = entry
    end

    local tmp_modules = {}
    for _, module_name in ipairs(module_names) do
        local old_module = originals[module_name].old_module
        if old_module then
            tmp_modules[module_name] = old_module
        end
    end

    sandbox.enter(tmp_modules)

    -- ===== 阶段 1：加载 / 计算（不写旧对象、不改写已注册状态）=====
    local old_modules, new_modules, module_envs = {}, {}, {}
    local pending_hooks = {}   -- 提交成功后才发：{ new_module, module_name, is_reload }

    for _, module_name in ipairs(module_names) do		
        if originals[module_name].had_lm == false then
            -- 无旧对象（弱值表被 GC，或显式传入未加载模块）：沿用 sandbox.require 的
            -- 首次加载语义（含 package.loaded 的 nil -> true 规则与 mtime 记录）。
            -- pcall 保证其失败也能走到 sandbox.exit() 并返回 false。
            local ok, err = pcall(sandbox.require, module_name)
            if not ok then
                sandbox.exit()
                return false, err
            end
        else
            -- sandbox.load 可能因 searcher 抛错（例如语法错误）而失败，用 pcall 兜住，
            -- 保证任何加载 / 计算异常都能干净退出沙盒、旧状态原样保留。
            local ok_load, func, env, loader_data = pcall(sandbox.load, module_name)
            if not ok_load then
                sandbox.exit()
                return false, func
            end
            if func == nil then
                sandbox.exit()
                return false, "module not found: " .. tostring(module_name)
            end
            local ok, new_module = xpcall(func, load_error_handler, module_name, loader_data)
            if not ok then
                sandbox.exit()
                return false, new_module
            end
            if new_module then
                for k, v in pairs(env) do
                    new_module[k] = v
                end
            else
                new_module = env
            end
            old_modules[#old_modules + 1] = originals[module_name].old_module
            new_modules[#new_modules + 1] = new_module
            module_envs[#module_envs + 1] = env
            pending_hooks[#pending_hooks + 1] = { new_module, module_name, true }
        end
    end

    -- ===== 阶段 2：提交（可回滚）=====
    local function rollback()
        for name, o in pairs(originals) do
            -- 恢复 old_module 顶层内容（rawset 绕过 __newindex，浅恢复一层）
            if o.old_keys then
                for k in pairs(o.old_module) do
                    rawset(o.old_module, k, nil)
                end
                for k, v in pairs(o.old_keys) do
                    rawset(o.old_module, k, v)
                end
            end
            if o.had_lm then loaded_modules[name] = o.old_module else loaded_modules[name] = nil end
            if o.had_pl then package.loaded[name] = o.pl_val else package.loaded[name] = nil end
            if o.had_mt then loaded_module_times[name] = o.mt_val else loaded_module_times[name] = nil end
        end
    end

    local commit_ok, commit_err = xpcall(function()
        update_modules(old_modules, new_modules, module_envs)
    end, load_error_handler)
    if not commit_ok then
        rollback()
        sandbox.exit()
        return false, commit_err
    end

    -- 提交成功后才通知外部（call_hook 内部已用 pcall 兜住 hook 异常）。
    for _, h in ipairs(pending_hooks) do
        call_hook("module_loaded", h[1], h[2], h[3])
    end

    sandbox.exit()
    return true
end

--- 无参：扫描 mtime 变更的模块并重载；显式传入 module_names 则强制重载该列表。
--- 返回 ok, err：成功 true（含"无变更"），失败 false, err 且保留旧时间戳以便下次重试。
---@param module_names table|nil
---@return boolean ok
---@return string|nil err
function M.reload(module_names)
    if module_names then
        return reload_modules(module_names)
    end

    local modified_modules = {}
    local current_times = {}
    for module_name, time in pairs(loaded_module_times) do
        if not ignore_modules[module_name] then
            local current_time = get_last_modified_time(module_name)
            if current_time ~= time then
                modified_modules[#modified_modules + 1] = module_name
                current_times[module_name] = current_time
            end
        end
    end
    print("modified modules:", dump(modified_modules))
    if #modified_modules == 0 then
        return true
    end

    local ok, err = reload_modules(modified_modules)
    if not ok then
        -- 失败：绝不推进时间戳，下一次 reload 才能重新发现这次改动并重试。
        return false, err
    end

    -- 只有提交成功才推进时间戳。
    for _, module_name in ipairs(modified_modules) do
        loaded_module_times[module_name] = current_times[module_name]
    end
    return true
end

--- 卸载一个模块的全部缓存。C++ 的全局 unrequire(name) 会先清 package.loaded，再调用
--- 这里清掉 HotReload 自己持有的三份私有状态；没有它，下一次 require 会从
--- loaded_modules 里拿回旧模块，看起来像 unrequire 完全没生效。
---
--- 只清缓存，不清 package.preload / package.aliases —— 那些是 loader / 重定向的
--- "声明"，删掉会让 reloadmodule(name) 失去它刚被要求重载的模块。幂等、不抛错，
--- 返回实际清掉的条目数，让 C++ 侧能区分 "清了东西" 和 "本来就没有"。
---@param module_name string
---@return number removed
function M.unrequire(module_name)
    if type(module_name) ~= "string" then
        return 0
    end

    local removed = 0
    local function clear(cache)
        if type(cache) ~= "table" then return end
        if rawget(cache, module_name) ~= nil then
            removed = removed + 1
        end
        rawset(cache, module_name, nil)
    end

    clear(loaded_modules)       -- __mode="v" 弱值表
    clear(loaded_module_times)  -- 清掉后无参 reload 会重新记录 mtime
    clear(ignore_modules)
    clear(package.loaded)       -- C++ 已清过一次；幂等，也让单独调用本函数有效
    return removed
end

M.require = sandbox.require

return M
