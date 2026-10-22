---
title: "C++ API Reference"
description: "Every public UnLuaNova C++ signature: FLuaEnv, the module interface, locators, binding exports, the static binding registry, LowLevel, overriding and debug types"
---

# C++ API Reference

Signatures are **copied from the headers**, not written from memory. This page covers the C++ side
only; the Lua-side API is in [API Reference](API.md).

Unless stated otherwise everything is in `namespace UnLuaNova`.

---

## `FLuaEnv` - `Public/LuaEnv.h`

One Lua VM instance. Several can exist at once, and their globals are invisible to each other.

```cpp
class UNLUANOVA_API FLuaEnv : public FUObjectArray::FUObjectDeleteListener
```

### Delegates and static lookup

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCreated, FLuaEnv&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestroyed, FLuaEnv&);
DECLARE_DELEGATE_RetVal_FourParams(bool, FLuaFileLoader, const FLuaEnv& /* Env */,
    const FString& /* FilePath */, TArray<uint8>& /* Data */, FString& /* RealFilePath */);

static FOnCreated   OnCreated;    // fired once per env, on construction
static FOnDestroyed OnDestroyed;  // fired once per env, on destruction

static TMap<lua_State*, FLuaEnv*>& GetAll();
static FLuaEnv* FindEnv(const lua_State* L);
static FLuaEnv& FindEnvChecked(const lua_State* L);
```

### Lifetime and startup

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

### Execution and state

```cpp
bool DoString(const FString& Chunk, const FString& ChunkName = "chunk");
virtual void GC();
virtual void HotReload();
FORCEINLINE lua_State* GetMainState() const;
```

`DoString` returns `false` on failure. A failure does **not** leave the env dirty - later calls keep
working.

### Coroutines (threads)

```cpp
void  AddThread(lua_State* Thread, int32 ThreadRef);
int32 FindOrAddThread(lua_State* Thread);
int32 FindThread(const lua_State* Thread);
void  ResumeThread(int32 ThreadRef);
```

### Registries

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

### Loaders and manual references

```cpp
void AddLoader(const FLuaFileLoader Loader);
void AddBuiltInLoader(const FString InName, lua_CFunction Loader);

void AddManualObjectReference(UObject* Object);
void RemoveManualObjectReference(UObject* Object);
```

> Warning: `PushUObject` only hands the script a userdata - the engine has no idea that userdata is a
> reference. **An object referenced only from Lua is still collected.** Keep it alive with
> `AddManualObjectReference` or a UPROPERTY. See `Lifetime.spec.cpp`.

---

## `IUnLuaNovaModule` - `Public/UnLuaNovaModule.h`

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

`GetEnv()` takes an optional `UObject*` - the locator decides which env the object belongs to; pass
nothing to get the main env. The rest of the module lifecycle (`StartupModule` / `ShutdownModule`)
comes from `IModuleInterface`.

---

## Locators - `Public/LuaEnvLocator.h`, `Public/LuaModuleLocator.h`

They decide "which env does this object belong to" and "which Lua module does this object use".
Both are replaceable extension points.

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

## `UUnLuaNovaFunctionLibrary` - `Public/UnLuaNovaFunctionLibrary.h`

Three functions exposed to Blueprint:

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

## Sandbox - `Public/UnLuaNovaSandbox.h`

```cpp
UNLUANOVA_API void RestoreTablesAfterSandbox(lua_State* L);
```

Undoes the part of `luaL_sandbox` that UnLuaNova cannot live with: it re-enables writes to
`UE.<Type>`, `Lua.<Module>` and `package.loaded`. Call it **immediately after** sandboxing. It is a
no-op on backends without read-only tables.

---

## Exports and registration - `Public/Binding.h`

`Binding.h` declares functions only - **no macros**; the `EXPORT_*` / `BEGIN_EXPORT_*` / `ADD_*`
family lives in `Public/UnLuaNovaEx.h`.

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

`UnExportClass` **withdraws** a static export: call it before the owning helper deletes its
`IExportedClass` at module teardown (hot reload / leaving PIE), otherwise the registry keeps a
dangling pointer.

Common macros (`UnLuaNovaEx.h`):

| Macro | Purpose |
|---|---|
| `BEGIN_EXPORT_CLASS(Type, ...)` / `BEGIN_EXPORT_REFLECTED_CLASS(Type, ...)` | start exporting a class |
| `BEGIN_EXPORT_CLASS_WITH_SUPER(Type, SuperType, ...)` | with super-class information |
| `ADD_PROPERTY(Property)` / `ADD_STATIC_PROPERTY(Property)` / `ADD_BITFIELD_BOOL_PROPERTY(Property)` | export a property |
| `ADD_FUNCTION(Function)` / `ADD_NAMED_FUNCTION(Name, Function)` / `ADD_FUNCTION_EX(Name, RetType, Function, ...)` | export a member function |
| `ADD_STATIC_FUNCTION(Function)` / `ADD_STATIC_FUNCTION_EX(...)` | export a static function |
| `ADD_EXTERNAL_FUNCTION(RetType, Function, ...)` | export a free function |
| `ADD_LIB(Lib)` | attach the `luaL_Reg` table (operators, `__tostring`, …) |
| `END_EXPORT_CLASS(...)` / `IMPLEMENT_EXPORTED_CLASS(Name)` | finish and emit the static registrar instance |
| `BEGIN_EXPORT_ENUM(Enum)` / `ADD_ENUM_VALUE(Value)` / `ADD_SCOPED_ENUM_VALUE(Value)` / `END_EXPORT_ENUM(Enum)` | export an enum |
| `ADD_TYPE_INTERFACE(Type)` / `ADD_NAMED_TYPE_INTERFACE(Name, Type)` | register a type interface |

The full list is at `Public/UnLuaNovaEx.h:427-623`.

---

## Static binding registry - `Public/StaticBinding/`

```cpp
class UNLUANOVA_API FStaticBindingRegistry
{
    static FStaticBindingRegistry& Get();

    void Register(const FStaticTypeBinding* Type);    // idempotent, only enqueues
    void Unregister(const FStaticTypeBinding* Type);

    const FStaticTypeBinding* Find(const FName TypeName) const;   // by UnLuaNova metatable name
    const FStaticTypeBinding* Find(const UStruct* Type) const;    // by UObject class (walks the super chain)

    const FStaticPropBinding* FindProp(const FStaticTypeBinding* Type, const FName Name) const;
    const FStaticFuncBinding* FindFunc(const FStaticTypeBinding* Type, const FName Name) const;

    static EStaticPropKind GetPropertyKind(const FProperty* Property);
    int32 NumRegisteredTypes() const;
    void Clear();     // called on module shutdown
};
```

The registration macro (`StaticBindingRegistrar.h:23`):

```cpp
#define UNLUANOVA_REGISTER_STATIC_BINDING(TypeName) \
    static FStaticBindingAutoRegistrar UNLUANOVA_SB_CONCAT(GStaticBindingRegistrar_, TypeName)(&TypeBinding_##TypeName)
```

`Register()` touches no UObject, so running it at DLL load time is safe; the real
`FProperty`/`UFunction` resolution is deferred to `ResolvePending()` and triggered lazily by readers.

Descriptor types live in `StaticBindingTypes.h`: `EStaticPropKind` (`:21`), `FStaticPropBinding`
(`:52`), `FStaticParamBinding` (`:67`), `FStaticFuncFlags` (`:76`), `FStaticFuncBinding` (`:104`),
`FStaticTypeBinding` (`:126`), plus the builders `UNLUANOVA_STATIC_PROP`,
`UNLUANOVA_STATIC_PARAM` and `UNLUANOVA_STATIC_FUNC_BEGIN` (`:145-151`).

---

## `UnLuaNova::LowLevel` - `Public/LowLevel.h`

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

`IsReleasedPtr` recognises only the single sentinel `ReleasedPtr` - a released userdata is *stamped*,
not nulled, so a script that touches it afterwards gets a Lua error rather than a read of freed
memory.

> `PushUObject` / `RunChunk` / `Call` are **not** in `LowLevel`; they sit directly in
> `namespace UnLuaNova`: `UnLuaNovaBase.h:238`, `:203` and `UnLuaNovaLegacy.h:693`.

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
struct UNLUANOVA_API FAutoStack;   // restores the Lua stack automatically

// Public/UnLuaNovaLegacy.h
template <typename... T>
FLuaRetValues Call(lua_State* L, const char* FuncName, T&&... Args);          // call a global function
template <typename... T>
FLuaRetValues CallTableFunc(lua_State* L, const char* TableName,
                            const char* FuncName, T&&... Args);               // call a function in a global table
```

`CreateState()` at `UnLuaNovaBase.h:154` is marked `UE_DEPRECATED(4.20)` - use `FLuaEnv` instead.

---

## Overriding - `Public/LuaFunction.h`, `Public/LuaOverridesClass.h`

```cpp
class UNLUANOVA_API ULuaFunction : public UFunction
{
    static bool IsOverridable(const UFunction* Function);
    static bool Override(UFunction* Function, UClass* Outer, FName NewName);
    static ULuaFunction* Get(UFunction* Function);

    static void RestoreOverrides(UClass* Class);   // on leaving PIE: restore every override
    static void SuspendOverrides(UClass* Class);   // before saving a UPackage: suspend so they are not serialised
    static void ResumeOverrides(UClass* Class);    // resume once the save is done
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
class UNLUANOVA_API ULuaOverridesClass : public UClass   // carries every ULuaFunction created by a runtime override
{
    static ULuaOverridesClass* Create(UClass* Class);
    void Restore();
    void SetActive(const bool bActive);
    UClass* GetOwner() const;
    virtual void BeginDestroy() override;
};
```

When writing tests, remember to call `ULuaFunction::RestoreOverrides(Class)` in `AfterEach`, or later
cases get polluted.

---

## Debug types - `Public/UnLuaNovaDebugBase.h`

```cpp
#define MAX_LUA_VALUE_DEPTH 4

struct UNLUANOVA_API FLuaDebugValue      // holds one Lua value
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

struct FLuaVariable                      // Lua variable wrapper
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

## Further reading

- [Architecture](Architecture.md) - layout, the dual-backend abstraction, the static binding chain
- [API Reference](API.md) - the Lua-side API
- [Static Export and Binding](StaticExportBinding.md) - hand-written exports and the macro system above
- [Debugging](Debugging.md) - the headless debug service and `lua.dbg`
- [Testing and Automation](Testing.md) - test conventions and the test list
