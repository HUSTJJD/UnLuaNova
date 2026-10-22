---
title: "C++ API 参考"
description: "UnLuaNova 公开 C++ API 的逐项签名：FLuaEnv、模块接口、定位器、绑定导出、静态绑定注册表、LowLevel、覆写与调试类型"
---

# C++ API 参考

签名**逐条抄自头文件**，不是凭记忆写的。本文只覆盖 C++ 侧；Lua 侧 API 见
[API 参考](API.md)。

命名空间：除特别说明外都在 `namespace UnLuaNova`。

---

## `FLuaEnv` — `Public/LuaEnv.h`

一个 Lua 虚拟机实例。可以同时存在多个，全局变量互不可见。

```cpp
class UNLUANOVA_API FLuaEnv : public FUObjectArray::FUObjectDeleteListener
```

### 委托与静态查找

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCreated, FLuaEnv&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestroyed, FLuaEnv&);
DECLARE_DELEGATE_RetVal_FourParams(bool, FLuaFileLoader, const FLuaEnv& /* Env */,
    const FString& /* FilePath */, TArray<uint8>& /* Data */, FString& /* RealFilePath */);

static FOnCreated   OnCreated;    // 每个环境构造时触发一次
static FOnDestroyed OnDestroyed;  // 每个环境销毁时触发一次

static TMap<lua_State*, FLuaEnv*>& GetAll();
static FLuaEnv* FindEnv(const lua_State* L);
static FLuaEnv& FindEnvChecked(const lua_State* L);
```

### 生命周期与启动

```cpp
FLuaEnv();
virtual ~FLuaEnv() override;

void Start(const TMap<FString, UObject*>& Args = {});
void Start(const FString& StartupModuleName, const TMap<FString, UObject*>& Args);

const FString& GetName();
void SetName(FString InName);

virtual void NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index) override;
virtual void OnUObjectArrayShutdown() override;

virtual bool TryBind(UObject* Object);
virtual bool TryReplaceInputs(UObject* Object);
```

### 执行与状态

```cpp
bool DoString(const FString& Chunk, const FString& ChunkName = "chunk");
virtual void GC();
virtual void HotReload();
FORCEINLINE lua_State* GetMainState() const;
```

`DoString` 返回 `false` 表示失败；失败**不会**把环境搞脏，后续调用照常工作。

### 协程（线程）

```cpp
void  AddThread(lua_State* Thread, int32 ThreadRef);
int32 FindOrAddThread(lua_State* Thread);
int32 FindThread(const lua_State* Thread);
void  ResumeThread(int32 ThreadRef);
```

### 注册表

```cpp
UUnLuaNovaManager* GetManager();
FClassRegistry*     GetClassRegistry() const;
FObjectRegistry*    GetObjectRegistry() const;
FDelegateRegistry*  GetDelegateRegistry() const;
FFunctionRegistry*  GetFunctionRegistry() const;
FContainerRegistry* GetContainerRegistry() const;
FEnumRegistry*      GetEnumRegistry() const;
FPropertyRegistry*  GetPropertyRegistry() const;
FDanglingCheck*     GetDanglingCheck() const;
FDeadLoopCheck*     GetDeadLoopCheck() const;
```

### 加载器与手动引用

```cpp
void AddLoader(const FLuaFileLoader Loader);
void AddBuiltInLoader(const FString InName, lua_CFunction Loader);

void AddManualObjectReference(UObject* Object);
void RemoveManualObjectReference(UObject* Object);
```

> ⚠️ `PushUObject` 只是给脚本一个 userdata，引擎并不知道那是引用。
> **只被 Lua 引用的对象照样会被 GC 回收**；要保活得用 `AddManualObjectReference`
> 或 UPROPERTY。详见 `Lifetime.spec.cpp`。

---

## `IUnLuaNovaModule` — `Public/UnLuaNovaModule.h`

```cpp
class UNLUANOVA_API IUnLuaNovaModule : public IModuleInterface
{
    static FORCEINLINE IUnLuaNovaModule& Get();

    virtual bool  IsActive() = 0;
    virtual void  SetActive(bool bActive) = 0;
    virtual UnLuaNova::FLuaEnv* GetEnv(UObject* Object = nullptr) = 0;
    virtual void  HotReload() = 0;
};
```

`GetEnv()` 带一个可选的 `UObject*`——由定位器决定该对象属于哪个环境，
传空则取主环境。其余模块生命周期方法（`StartupModule` / `ShutdownModule`）来自
`IModuleInterface`。

---

## 定位器 — `Public/LuaEnvLocator.h`、`Public/LuaModuleLocator.h`

决定"哪个对象属于哪个环境"和"哪个对象用哪个 Lua 模块"，都是可替换的扩展点。

```cpp
class UNLUANOVA_API ULuaEnvLocator : public UObject
{
    virtual UnLuaNova::FLuaEnv* Locate(const UObject* Object);
    virtual void HotReload();
    virtual void Reset();
    TSharedPtr<UnLuaNova::FLuaEnv, ESPMode::ThreadSafe> Env;
};

class UNLUANOVA_API ULuaEnvLocator_ByGameInstance : public ULuaEnvLocator
{
    virtual UnLuaNova::FLuaEnv* Locate(const UObject* Object) override;
    virtual void HotReload() override;
    virtual void Reset() override;
    UnLuaNova::FLuaEnv* GetDefault();
    TMap<TWeakObjectPtr<UGameInstance>, TSharedPtr<UnLuaNova::FLuaEnv, ESPMode::ThreadSafe>> Envs;
};
```

```cpp
class UNLUANOVA_API ULuaModuleLocator : public UObject
{
    virtual FString Locate(const UObject* Object);
};

class UNLUANOVA_API ULuaModuleLocator_ByPackage : public ULuaModuleLocator
{
    virtual FString Locate(const UObject* Object) override;
    TMap<FName, FString> Cache;   // private
};
```

---

## `UUnLuaNovaFunctionLibrary` — `Public/UnLuaNovaFunctionLibrary.h`

给蓝图用的三个函数：

```cpp
UCLASS()
class UNLUANOVA_API UUnLuaNovaFunctionLibrary : public UBlueprintFunctionLibrary
{
    UFUNCTION(BlueprintCallable) static FString GetScriptRootPath();
    UFUNCTION(BlueprintCallable) static int64   GetFileLastModifiedTimestamp(FString Path);
    UFUNCTION(BlueprintCallable) static void    HotReload();
};
```

---

## 沙盒 — `Public/UnLuaNovaSandbox.h`

```cpp
UNLUANOVA_API void RestoreTablesAfterSandbox(lua_State* L);
```

撤销 `luaL_sandbox` 里 UnLuaNova 活不下去的那部分：重新放开对 `UE.<Type>`、
`Lua.<Module>`、`package.loaded` 的写权限。**紧跟在 sandbox 之后调用**。
在不支持只读表的后端上是空操作。

---

## 导出与注册 — `Public/Binding.h`

`Binding.h` 只声明函数，**没有宏**；`EXPORT_*` / `BEGIN_EXPORT_*` / `ADD_*` 系列宏在
`Public/UnLuaNovaEx.h`。

```cpp
UNLUANOVA_API void ExportClass(IExportedClass* Class);
UNLUANOVA_API void UnExportClass(IExportedClass* Class);
UNLUANOVA_API void ExportEnum(IExportedEnum* Enum);
UNLUANOVA_API void ExportFunction(IExportedFunction* Function);
UNLUANOVA_API void AddType(FString Name, TSharedPtr<ITypeInterface> TypeInterface);

UNLUANOVA_API TMap<FString, IExportedClass*> GetExportedReflectedClasses();
UNLUANOVA_API TMap<FString, IExportedClass*> GetExportedNonReflectedClasses();
UNLUANOVA_API TArray<IExportedEnum*>         GetExportedEnums();
UNLUANOVA_API TArray<IExportedFunction*>     GetExportedFunctions();

UNLUANOVA_API IExportedClass*          FindExportedClass(FString Name);
UNLUANOVA_API IExportedClass*          FindExportedReflectedClass(FString Name);
UNLUANOVA_API IExportedClass*          FindExportedNonReflectedClass(FString Name);
UNLUANOVA_API TSharedPtr<ITypeInterface> FindTypeInterface(FString Name);
```

`UnExportClass` 用于**撤销**一个静态导出：在持有它的 helper 于模块卸载（热重载 / 退出 PIE）
删掉 `IExportedClass` **之前**调用，否则注册表里会留下悬垂指针。

常用宏（`UnLuaNovaEx.h`）：

| 宏 | 用途 |
|---|---|
| `BEGIN_EXPORT_CLASS(Type, ...)` / `BEGIN_EXPORT_REFLECTED_CLASS(Type, ...)` | 开始导出一个类 |
| `BEGIN_EXPORT_CLASS_WITH_SUPER(Type, SuperType, ...)` | 带父类信息 |
| `ADD_PROPERTY(Property)` / `ADD_STATIC_PROPERTY(Property)` / `ADD_BITFIELD_BOOL_PROPERTY(Property)` | 导出属性 |
| `ADD_FUNCTION(Function)` / `ADD_NAMED_FUNCTION(Name, Function)` / `ADD_FUNCTION_EX(Name, RetType, Function, ...)` | 导出成员函数 |
| `ADD_STATIC_FUNCTION(Function)` / `ADD_STATIC_FUNCTION_EX(...)` | 导出静态函数 |
| `ADD_EXTERNAL_FUNCTION(RetType, Function, ...)` | 导出外部函数 |
| `ADD_LIB(Lib)` | 挂上 `luaL_Reg` 表（运算符、`__tostring` 等） |
| `END_EXPORT_CLASS(...)` / `IMPLEMENT_EXPORTED_CLASS(Name)` | 收尾并生成静态注册实例 |
| `BEGIN_EXPORT_ENUM(Enum)` / `ADD_ENUM_VALUE(Value)` / `ADD_SCOPED_ENUM_VALUE(Value)` / `END_EXPORT_ENUM(Enum)` | 导出枚举 |
| `ADD_TYPE_INTERFACE(Type)` / `ADD_NAMED_TYPE_INTERFACE(Name, Type)` | 注册类型接口 |

完整清单见 `Public/UnLuaNovaEx.h:427-623`。

---

## 静态绑定注册表 — `Public/StaticBinding/`

```cpp
class UNLUANOVA_API FStaticBindingRegistry
{
    static FStaticBindingRegistry& Get();

    void Register(const FStaticTypeBinding* Type);    // 幂等，只入队
    void Unregister(const FStaticTypeBinding* Type);

    const FStaticTypeBinding* Find(const FName TypeName) const;   // 按 UnLuaNova metatable 名
    const FStaticTypeBinding* Find(const UStruct* Type) const;    // 按 UObject 类（会走父类链）

    const FStaticPropBinding* FindProp(const FStaticTypeBinding* Type, const FName Name) const;
    const FStaticFuncBinding* FindFunc(const FStaticTypeBinding* Type, const FName Name) const;

    static EStaticPropKind GetPropertyKind(const FProperty* Property);
    int32 NumRegisteredTypes() const;
    void Clear();     // 模块关闭时调用
};
```

注册宏（`StaticBindingRegistrar.h:23`）：

```cpp
#define UNLUANOVA_REGISTER_STATIC_BINDING(TypeName) \
    static FStaticBindingAutoRegistrar UNLUANOVA_SB_CONCAT(GStaticBindingRegistrar_, TypeName)(&TypeBinding_##TypeName)
```

`Register()` 本身不碰 UObject，所以在 DLL 加载期执行是安全的；真正的 `FProperty`/`UFunction`
解析推迟到 `ResolvePending()`，由读路径惰性触发。

描述符类型在 `StaticBindingTypes.h`：`EStaticPropKind`（`:21`）、`FStaticPropBinding`（`:52`）、
`FStaticParamBinding`（`:67`）、`FStaticFuncFlags`（`:76`）、`FStaticFuncBinding`（`:104`）、
`FStaticTypeBinding`（`:126`），以及构造它们的 `UNLUANOVA_STATIC_PROP` /
`UNLUANOVA_STATIC_PARAM` / `UNLUANOVA_STATIC_FUNC_BEGIN`（`:145-151`）。

---

## `UnLuaNova::LowLevel` — `Public/LowLevel.h`

```cpp
FORCEINLINE int AbsIndex(lua_State* L, int Index);
int EmptyIterator(lua_State* L);
FORCEINLINE int PushEmptyIterator(lua_State* L);

const static UObject* ReleasedPtr = (UObject*)0xDEAD;
FORCEINLINE bool IsReleasedPtr(const void* Ptr);

void CreateWeakKeyTable(lua_State* L);
void CreateWeakValueTable(lua_State* L);

FString GetMetatableName(const UObject* Object);
FString GetMetatableName(const UStruct* Struct);

void GetFunctionNames(lua_State* L, int TableRef, TSet<FName>& FunctionNames);
int   GetLoadedModule(lua_State* L, const char* ModuleName);   // package.loaded[ModuleName]

bool  CheckPropertyOwner(lua_State* L, UnLuaNova::ITypeOps* InProperty, void* InContainerPtr);
void* GetUserdata(lua_State* L, int32 Index, bool* OutTwoLvlPtr = nullptr, bool* OutClassMetatable = nullptr);
uint8 CalculateUserdataPadding(UStruct* Struct);
```

`IsReleasedPtr` 只认 `ReleasedPtr` 那一个哨兵值——对象被释放后 userdata 是被"盖章"而不是置空，
这样脚本再碰它拿到的是一条 Lua 错误，而不是一次对已释放内存的读。

> `PushUObject` / `RunChunk` / `Call` **不在** `LowLevel` 里，它们直接在
> `namespace UnLuaNova` 下：`UnLuaNovaBase.h:238`、`:203` 与 `UnLuaNovaLegacy.h:693`。

```cpp
// Public/UnLuaNovaBase.h
UNLUANOVA_API int32  PushUObject(lua_State* L, UObjectBaseUtility* Object, bool bAddRef = true);
UNLUANOVA_API UObject* GetUObject(lua_State* L, int32 Index, bool bReturnNullIfInvalid = true);
UNLUANOVA_API bool   RunChunk(lua_State* L, const char* Chunk);
UNLUANOVA_API bool   LoadChunk(lua_State* L, const char* Chunk, int32 ChunkSize,
                               const char* ChunkName = "", const char* Mode = "bt", int32 Env = 0);
UNLUANOVA_API bool   LoadFile(lua_State* L, const FString& RelativeFilePath,
                              const char* Mode = "bt", int32 Env = 0);
UNLUANOVA_API int32  PushPointer(lua_State* L, void* Value, const char* MetatableName, bool bAlwaysCreate = false);
UNLUANOVA_API void*  GetPointer(lua_State* L, int32 Index, bool* OutTwoLvlPtr = nullptr);
UNLUANOVA_API int32  ReportLuaCallError(lua_State* L);
UNLUANOVA_API void*  NewSmartPointer(lua_State* L, int32 Size, const char* MetatableName);
UNLUANOVA_API void*  GetSmartPointer(lua_State* L, int32 Index);
UNLUANOVA_API void*  NewUserdata(lua_State* L, int32 Size, const char* MetatableName, int32 Alignment);
UNLUANOVA_API int32  PushArray(lua_State* L, const FScriptArray* ScriptArray,
                               TSharedPtr<ITypeInterface> TypeInterface, bool bCreateCopy = false);
UNLUANOVA_API int32  PushSet(lua_State* L, const FScriptSet* ScriptSet,
                             TSharedPtr<ITypeInterface> TypeInterface, bool bCreateCopy = false);
UNLUANOVA_API int32  PushMap(lua_State* L, const FScriptMap* ScriptMap,
                             TSharedPtr<ITypeInterface> KeyInterface,
                             TSharedPtr<ITypeInterface> ValueInterface, bool bCreateCopy = false);
UNLUANOVA_API FScriptArray* GetArray(lua_State* L, int32 Index);
UNLUANOVA_API FScriptSet*   GetSet(lua_State* L, int32 Index);
UNLUANOVA_API FScriptMap*   GetMap(lua_State* L, int32 Index);
UNLUANOVA_API bool IsUObjectValid(UObjectBase* ObjPtr);
UNLUANOVA_API bool Startup();
UNLUANOVA_API void Shutdown();
UNLUANOVA_API bool IsEnabled();
struct UNLUANOVA_API FAutoStack;   // 自动恢复 Lua 栈

// Public/UnLuaNovaLegacy.h
template <typename... T>
FLuaRetValues Call(lua_State* L, const char* FuncName, T&&... Args);          // 调全局函数
template <typename... T>
FLuaRetValues CallTableFunc(lua_State* L, const char* TableName,
                            const char* FuncName, T&&... Args);               // 调全局表里的函数
```

`UnLuaNovaBase.h:154` 的 `CreateState()` 已标 `UE_DEPRECATED(4.20)`——用 `FLuaEnv` 代替。

---

## 覆写 — `Public/LuaFunction.h`、`Public/LuaOverridesClass.h`

```cpp
class UNLUANOVA_API ULuaFunction : public UFunction
{
    static bool IsOverridable(const UFunction* Function);
    static bool Override(UFunction* Function, UClass* Outer, FName NewName);
    static ULuaFunction* Get(UFunction* Function);

    static void RestoreOverrides(UClass* Class);   // 退出 PIE 时：还原全部覆写
    static void SuspendOverrides(UClass* Class);   // 保存 UPackage 前：临时挂起，避免被序列化
    static void ResumeOverrides(UClass* Class);    // 保存完成后恢复
    static void GetOverridableFunctions(UClass* Class, TMap<FName, UFunction*>& Functions);

    DECLARE_FUNCTION(execCallLua);
    DECLARE_FUNCTION(execScriptCallLua);

    void Initialize();
    void Override(UFunction* Function, UClass* Class, bool bAddNew);
    void Restore();
    UClass*   GetOverriddenUClass() const;
    UFunction* GetOverridden() const;
    virtual void Bind() override;
    void SetActive(const bool bActive);
    virtual void FinishDestroy() override;
};
```

```cpp
UCLASS(Transient)
class UNLUANOVA_API ULuaOverridesClass : public UClass   // 承载运行时覆写创建的所有 ULuaFunction
{
    static ULuaOverridesClass* Create(UClass* Class);
    void Restore();
    void SetActive(const bool bActive);
    UClass* GetOwner() const;
    virtual void BeginDestroy() override;
};
```

写完用例记得 `AfterEach` 里 `ULuaFunction::RestoreOverrides(Class)`，否则会污染后续用例。

---

## 调试类型 — `Public/UnLuaNovaDebugBase.h`

```cpp
#define MAX_LUA_VALUE_DEPTH 4

struct UNLUANOVA_API FLuaDebugValue      // 承载一个 Lua 值
{
    FLuaDebugValue();
    FString ToString() const;
    void Build(lua_State* L, int32 Index, int32 Level = MAX_int32);

    FString ReadableValue;
    FString Type;
    int32   Depth;
    bool    bAlreadyBuilt;
    TArray<FLuaDebugValue> Keys;
    TArray<FLuaDebugValue> Values;
};

struct FLuaVariable                      // Lua 变量包装
{
    FString Key;
    FLuaDebugValue Value;
};

UNLUANOVA_API bool GetStackVariables(lua_State* L, int32 StackLevel,
                                     TArray<FLuaVariable>& LocalVariables,
                                     TArray<FLuaVariable>& Upvalues, int32 Level = MAX_int32);
UNLUANOVA_API FString GetLuaCallStack(lua_State* L);
void PrintCallStack(lua_State* L);
```

---

## 延伸阅读

- [架构](Architecture.md) — 目录结构、双后端抽象、静态绑定全链路
- [API 参考](API.md) — Lua 侧 API
- [静态导出绑定](StaticExportBinding.md) — 手写导出与上面这套宏
- [调试](Debugging.md) — headless 调试服务与 `lua.dbg`
- [测试与自动化](Testing.md) — 用例约定与清单
