// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "CoreUObject.h"
#include "Runtime/Launch/Resources/Version.h"
#include "LuaBackend.h"      // FUnLuaNovaUserdataDtor, for NewUserdataDtor()

// --- 内置库开关（编译期） ---------------------------------------------------
// UNLUANOVA_HAS_STDLIB_TIMER / _EVENT / _OOP 只有一个来源：UnLuaNova.Build.cs
// 读 Config/DefaultUnLuaNova.ini 的 bTimer / bEvent / bOOP（见 Build.cs 的 loadFeature）。
// 这里刻意不写 #ifndef 默认值 —— 那样等于"同一件事两个答案"，
// 而且宏没定义时会静默变成"库被打开"，与本意相反。没定义就是构建脚本没跑。

UNLUANOVA_API DECLARE_LOG_CATEGORY_EXTERN(LogUnLuaNova, Log, All);
UNLUANOVA_API DECLARE_LOG_CATEGORY_EXTERN(UnLuaNovaDelegate, Log, All);

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION < 25
typedef UProperty FProperty;
#endif

struct lua_State;
struct luaL_Reg;

namespace UnLuaNova
{   
    bool IsUObjectValid(UObjectBase* ObjPtr);

    /**
     * Interface to manage Lua stack for a C++ type
     */
    struct ITypeOps
    {
        ITypeOps() { StaticExported = false; };

        virtual FString GetName() const = 0;

        // Deprecated, replaced with ReadValue_InContainer.
        virtual void Read(lua_State* L, const void* ContainerPtr, bool bCreateCopy) const { ReadValue_InContainer(L, ContainerPtr, bCreateCopy); }

        virtual void ReadValue_InContainer(lua_State* L, const void* ContainerPtr, bool bCreateCopy) const = 0;

        virtual void ReadValue(lua_State* L, const void* ValuePtr, bool bCreateCopy) const = 0;

        // Deprecated, replaced with WriteValue_InContainer.
        virtual void Write(lua_State* L, void* ContainerPtr, int32 IndexInStack) const { WriteValue_InContainer(L, ContainerPtr, IndexInStack); }

        virtual bool WriteValue_InContainer(lua_State* L, void* ContainerPtr, int32 IndexInStack, bool bCreateCopy = true) const = 0;

        virtual bool WriteValue(lua_State* L, void* ValuePtr, int32 IndexInStack, bool bCreateCopy) const = 0;

        bool StaticExported;
    };

    /**
     * Interface to manage a C++ type
     */
    struct ITypeInterface : public ITypeOps
    {
        ITypeInterface() { }
        virtual ~ITypeInterface() {}

        virtual bool IsValid() const = 0;
        virtual bool IsPODType() const = 0;
        virtual bool IsTriviallyDestructible() const = 0;
        virtual int32 GetSize() const = 0;
        virtual int32 GetAlignment() const = 0;
        virtual int32 GetOffset() const = 0;
        virtual uint32 GetValueTypeHash(const void *Src) const = 0;
        virtual void Initialize(void *Dest) const = 0;
        virtual void Destruct(void *Dest) const = 0;
        virtual void Copy(void *Dest, const void *Src) const = 0;
        virtual bool Identical(const void *A, const void *B) const = 0;
        virtual FProperty* GetUProperty() const = 0;
    };

    /**
     * Exported property interface
     */
    struct IExportedProperty : public ITypeOps
    {   
        IExportedProperty() { StaticExported = true;}
        virtual ~IExportedProperty() {}

        virtual void Register(lua_State *L) = 0;

#if WITH_EDITOR
        virtual void GenerateIntelliSense(FString &Buffer) const = 0;
#endif
    };

    /**
     * Exported function interface
     */
    struct IExportedFunction
    {
        virtual ~IExportedFunction() {}

        virtual void Register(lua_State *L) = 0;
        virtual int32 Invoke(lua_State *L) = 0;

#if WITH_EDITOR
        virtual FString GetName() const = 0;
        virtual void GenerateIntelliSense(FString &Buffer) const = 0;
#endif
    };

    /**
     * Exported class interface
     */
    struct IExportedClass
    {
        virtual ~IExportedClass() {}

        virtual void Register(lua_State *L) = 0;
        virtual void AddLib(const luaL_Reg *Lib) = 0;
        virtual bool IsReflected() const = 0;
        virtual FString GetName() const = 0;
        virtual FString GetSuperClassName() const = 0;
        virtual void GetProperties(TArray<TSharedPtr<IExportedProperty>>& InArray) const = 0;
        virtual void GetFunctions(TArray<IExportedFunction*>& InArray) const = 0;

#if WITH_EDITOR
        virtual void GenerateIntelliSense(FString &Buffer) const = 0;
#endif
    };

    /**
     * Exported enum interface
     */
    struct IExportedEnum
    {
        virtual ~IExportedEnum() {}

        virtual void Register(lua_State *L) = 0;

#if WITH_EDITOR
        virtual FString GetName() const = 0;
        virtual void GenerateIntelliSense(FString &Buffer) const = 0;
#endif
    };

    /**
     * Create Lua state
     *
     * @return - created Lua state
     */
    UE_DEPRECATED(4.20, "Use FLuaEnv to create lua vm instead.")
    UNLUANOVA_API lua_State* CreateState();

    /**
     * @return - Lua state
     */
    UNLUANOVA_API lua_State* GetState();

    /**
     * Start up UnLuaNova
     *
     * @return - true if UnLuaNova is started successfully, false otherwise
     */
    UNLUANOVA_API bool Startup();

    /**
     * Shut down UnLuaNova
     */
    UNLUANOVA_API void Shutdown();

    UNLUANOVA_API bool IsEnabled();

    /**
     * Load a Lua file without running it
     *
     * @param RelativeFilePath - the relative (to project's content dir) Lua file path
     * @param Mode - mode of the chunk, it may be the string "b" (only binary chunks), "t" (only text chunks), or "bt" (both binary and text)
     * @param Env - Lua stack index of the 'Env'
     * @return - true if Lua file is loaded successfully, false otherwise
     */
    UNLUANOVA_API bool LoadFile(lua_State *L, const FString &RelativeFilePath, const char *Mode = "bt", int32 Env = 0);
 
    /**
     * Load a Lua chunk without running it
     *
     * @param Chunk - Lua chunk
     * @param ChunkSize - chunk size (in bytes)
     * @param ChunkName - name of the chunk, which is used for error messages and in debug information
     * @param Mode - mode of the chunk, it may be the string "b" (only binary chunks), "t" (only text chunks), or "bt" (both binary and text)
     * @param Env - Lua stack index of the 'Env'
     * @return - true if Lua chunk is loaded successfully, false otherwise
     */
    UNLUANOVA_API bool LoadChunk(lua_State *L, const char *Chunk, int32 ChunkSize, const char *ChunkName = "", const char *Mode = "bt", int32 Env = 0);

    /**
     * Run a Lua chunk
     *
     * @param Chunk - Lua chunk
     * @return - true if the Lua chunk runs successfully, false otherwise
     */
    UNLUANOVA_API bool RunChunk(lua_State *L, const char *Chunk);

    /**
     * Report Lua error
     *
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 ReportLuaCallError(lua_State *L);

    /**
     * Push a pointer with the name of meta table
     *
     * @param Value - pointer address
     * @param MetatableName - name of the meta table
     * @param bAlwaysCreate - always create Lua instance for this pointer
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 PushPointer(lua_State *L, void *Value, const char *MetatableName, bool bAlwaysCreate = false);

    /**
     * Get the address of user data at the given stack index
     *
     * @param Index - Lua stack index
     * @param[out] OutTwoLvlPtr - whether the address is a two level pointer
     * @return - the address of user data
     */
    UNLUANOVA_API void* GetPointer(lua_State *L, int32 Index, bool *OutTwoLvlPtr = nullptr);

    /**
     * Push a UObject
     *
     * @param Object - UObject instance
     * @param bAddRef - whether to add reference for this object
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 PushUObject(lua_State *L, UObjectBaseUtility *Object, bool bAddRef = true);

    /**
     * Get a UObject at the given stack index
     *
     * @param Index - Lua stack index
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API UObject* GetUObject(lua_State *L, int32 Index, bool bReturnNullIfInvalid = true);

    /**
     * Allocate user data for smart pointer
     *
     * @param Size - user data size (in bytes)
     * @param MetatableName - name of the meta table
     * @return - the address of new created user data
     */
    UNLUANOVA_API void* NewSmartPointer(lua_State *L, int32 Size, const char *MetatableName);

    /**
     * Get the address of smart pointer at the given stack index
     *
     * @param Index - Lua stack index
     * @return - the address of the smart pointer
     */
    UNLUANOVA_API void* GetSmartPointer(lua_State *L, int32 Index);

    /**
     * Allocate user data
     *
     * @param Size - memory size (in bytes)
     * @param MetatableName - name of the meta table
     * @param Alignment - alignment (in bytes)
     * @return - the address of new created user data
     */
    UNLUANOVA_API void* NewUserdata(lua_State *L, int32 Size, const char *MetatableName, int32 Alignment);

    /**
     * Same, for a payload whose C++ destructor has to run when the VM collects it.
     *
     * Plain Lua reaches that destructor through the metatable's __gc; Luau has no __gc and
     * needs the finalizer handed to the VM at allocation time. 'Dtor' is ignored on the
     * former, so passing it unconditionally is correct on both - see UNLUANOVA_NEWDATA_DTOR
     * in LuaBackend.h.
     */
    UNLUANOVA_API void* NewUserdataDtor(lua_State *L, int32 Size, const char *MetatableName, int32 Alignment, FUnLuaNovaUserdataDtor Dtor);

    /**
     * Payload address of a userdata block, or null if UnLuaNova did not create the block.
     *
     * 'Block' is what lua_touserdata() returns - and what a Lua finalizer receives. The
     * payload sits behind [FUserdataDesc][padding], so handing the block straight to a C++
     * destructor would run it over the descriptor bytes instead of the object.
     *
     * Declared here as well as in LuaCore.h so public headers can build finalizers without
     * reaching into the private tree; both declarations are the same function.
     */
    UNLUANOVA_API void* GetUserdataPayloadFromBlock(void* Block);

    /** Finalizer for a payload of type T - hand it to NewUserdataDtor. */
    template <typename T>
    void DestroyUserdataPayload(lua_State*, void* Block)
    {
        if (void* Payload = GetUserdataPayloadFromBlock(Block))
        {
            static_cast<T*>(Payload)->~T();
        }
    }

    /**
     * Push an untyped dynamic array (same memory layout with TArray)
     *
     * @param ScriptArray - untyped dynamic array
     * @param TypeInterface - type info for the dynamic array
     * @param bCreateCopy - whether to copy the dynamic array
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 PushArray(lua_State *L, const FScriptArray *ScriptArray, TSharedPtr<ITypeInterface> TypeInterface, bool bCreateCopy = false);

    /**
     * Push an untyped set (same memory layout with TSet)
     *
     * @param ScriptSet - untyped set
     * @param TypeInterface - type info for the set
     * @param bCreateCopy - whether to copy the set
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 PushSet(lua_State *L, const FScriptSet *ScriptSet, TSharedPtr<ITypeInterface> TypeInterface, bool bCreateCopy = false);

    /**
     * Push an untyped map (same memory layout with TMap)
     *
     * @param ScriptMap - untyped map
     * @param TypeInterface - type info for the map
     * @param bCreateCopy - whether to copy the map
     * @return - the number of results on Lua stack
     */
    UNLUANOVA_API int32 PushMap(lua_State *L, const FScriptMap *ScriptMap, TSharedPtr<ITypeInterface> KeyInterface, TSharedPtr<ITypeInterface> ValueInterface, bool bCreateCopy = false);

    /**
     * Get an untyped dynamic array at the given stack index
     *
     * @param Index - Lua stack index
     * @return - the untyped dynamic array
     */
    UNLUANOVA_API FScriptArray* GetArray(lua_State *L, int32 Index);

    /**
     * Get an untyped set at the given stack index
     *
     * @param Index - Lua stack index
     * @return - the untyped set
     */
    UNLUANOVA_API FScriptSet* GetSet(lua_State *L, int32 Index);

    /**
     * Get an untyped map at the given stack index
     *
     * @param Index - Lua stack index
     * @return - the untyped map
     */
    UNLUANOVA_API FScriptMap* GetMap(lua_State *L, int32 Index);


    /**
     * Helper to recover Lua stack automatically
     */
    struct UNLUANOVA_API FAutoStack
    {
        FAutoStack(lua_State *L);
        ~FAutoStack();

    private:
        lua_State* L;
        int32 OldTop;
    };
} // namespace UnLuaNova
