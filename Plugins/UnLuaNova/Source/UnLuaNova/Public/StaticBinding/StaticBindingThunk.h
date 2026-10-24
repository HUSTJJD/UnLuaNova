// Compile-time marshalling for statically bound UFunctions.
//
// This is the piece that closes the gap slua-unreal opened with CppBinding. slua's
// CppBinding wins because the marshalling code is written (or generated) per signature,
// so the compiler knows every parameter's C++ type and emits a direct store instead of a
// virtual FProperty call. The generated-thunk path here does the same thing:
//
//   * the *kind* of every parameter and of the return value is a template argument, so
//     the marshalling body is a single `if constexpr` chain that resolves to one store
//     or one push - no FProperty virtual dispatch, no FPropertyDesc, no TWeakFieldPtr
//     validity check, no per-parameter type check;
//   * the *offsets* are still read from the resolved FStaticFuncBinding at run time.
//     That is deliberate. slua bakes offsets into generated code, which is marginally
//     faster but silently corrupts the call if a UPROPERTY is inserted without
//     regenerating. We take the one indirect load and keep the generated file valid
//     across source edits.
//
// A thunk is only installed for functions the static binding generator marked as
// eligible (see UUnLuaNovaStaticBindingGeneratorCommandlet): no net functions, no latent
// functions, no interface functions, no out/reference parameters, and every parameter of
// a kind listed in IsThunkableParamKind(). Everything else keeps the FFunctionDesc path,
// so correctness never depends on the generator being exhaustive.

#pragma once

#include "CoreMinimal.h"
#include "LuaBackend.h"
#include "LuaFunction.h"
#include "UnLuaNovaBase.h"
#include "StaticBinding/StaticBindingTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include <utility>

struct lua_State;

namespace UnLuaNova
{
	/** Kinds whose C++ representation is known well enough to marshal without FProperty. */
	constexpr bool IsThunkableParamKind(const EStaticPropKind Kind)
	{
		return Kind == EStaticPropKind::Bool
			|| Kind == EStaticPropKind::Int8
			|| Kind == EStaticPropKind::Int16
			|| Kind == EStaticPropKind::Int
			|| Kind == EStaticPropKind::Int64
			|| Kind == EStaticPropKind::Byte
			|| Kind == EStaticPropKind::Float
			|| Kind == EStaticPropKind::Double
			|| Kind == EStaticPropKind::Name
			|| Kind == EStaticPropKind::String
			|| Kind == EStaticPropKind::Object
			|| Kind == EStaticPropKind::Class;
	}

	/** Kinds that need a real constructor/destructor rather than zeroed bytes. */
	constexpr bool IsThunkHeapishKind(const EStaticPropKind Kind)
	{
		return Kind == EStaticPropKind::Name || Kind == EStaticPropKind::String;
	}

	// ---------------------------------------------------------------- write ----

	/**
	 * Store the Lua value at StackIndex into the parameter slot described by Param.
	 *
	 * StackIndex may be past the top of the stack: a script is allowed to omit trailing
	 * arguments, and the frame is zeroed beforehand, which is a correct default for every
	 * kind except the two that need a constructor - those get an empty one.
	 */
	template <EStaticPropKind Kind>
	FORCEINLINE void WriteThunkParam(lua_State* L, int32 StackIndex, uint8* Params, const FStaticParamBinding& Param)
	{
		uint8* const Dest = Params + Param.Offset;
		const bool bSupplied = StackIndex <= lua_gettop(L) && !lua_isnil(L, StackIndex);

		if constexpr (Kind == EStaticPropKind::Bool)
		{
			// A bool UPROPERTY lives in a bitfield, so it cannot be stored with a plain
			// write. FBoolProperty::SetPropertyValue is inline and non-virtual, which is
			// the point of going through the typed subclass rather than FProperty.
			static_cast<FBoolProperty*>(Param.Property)->SetPropertyValue(Dest, bSupplied && lua_toboolean(L, StackIndex) != 0);
		}
		else if constexpr (Kind == EStaticPropKind::Int)
		{
			*reinterpret_cast<int32*>(Dest) = bSupplied ? (int32)lua_tointeger(L, StackIndex) : 0;
		}
		else if constexpr (Kind == EStaticPropKind::Int8)
		{
			*reinterpret_cast<int8*>(Dest) = bSupplied ? (int8)lua_tointeger(L, StackIndex) : 0;
		}
		else if constexpr (Kind == EStaticPropKind::Int16)
		{
			*reinterpret_cast<int16*>(Dest) = bSupplied ? (int16)lua_tointeger(L, StackIndex) : 0;
		}
		else if constexpr (Kind == EStaticPropKind::Int64)
		{
			*reinterpret_cast<int64*>(Dest) = bSupplied ? (int64)lua_tointeger(L, StackIndex) : 0;
		}
		else if constexpr (Kind == EStaticPropKind::Byte)
		{
			*reinterpret_cast<uint8*>(Dest) = bSupplied ? (uint8)lua_tointeger(L, StackIndex) : 0;
		}
		else if constexpr (Kind == EStaticPropKind::Float)
		{
			*reinterpret_cast<float*>(Dest) = bSupplied ? (float)lua_tonumber(L, StackIndex) : 0.f;
		}
		else if constexpr (Kind == EStaticPropKind::Double)
		{
			*reinterpret_cast<double*>(Dest) = bSupplied ? (double)lua_tonumber(L, StackIndex) : 0.0;
		}
		else if constexpr (Kind == EStaticPropKind::Name)
		{
			const char* const Raw = bSupplied ? lua_tostring(L, StackIndex) : nullptr;
			new (Dest) FName(Raw ? UTF8_TO_TCHAR(Raw) : TEXT(""));
		}
		else if constexpr (Kind == EStaticPropKind::String)
		{
			const char* const Raw = bSupplied ? lua_tostring(L, StackIndex) : nullptr;
			new (Dest) FString(Raw ? UTF8_TO_TCHAR(Raw) : TEXT(""));
		}
		else if constexpr (Kind == EStaticPropKind::Object || Kind == EStaticPropKind::Class)
		{
			static_cast<FObjectPropertyBase*>(Param.Property)->SetObjectPropertyValue(
				Dest, bSupplied ? UnLuaNova::GetUObject(L, StackIndex) : nullptr);
		}
		else
		{
			// Only reachable if a thunk is emitted for a kind IsThunkableParamKind()
			// rejects; the generator is the only emitter and it checks first.
			static_assert(Kind != Kind, "StaticBindingThunk: parameter kind is not thunkable");
		}
	}

	template <EStaticPropKind Kind>
	FORCEINLINE void DestroyThunkParam(uint8* Params, const FStaticParamBinding& Param)
	{
		if constexpr (Kind == EStaticPropKind::String)
		{
			reinterpret_cast<FString*>(Params + Param.Offset)->~FString();
		}
		else if constexpr (Kind == EStaticPropKind::Name)
		{
			reinterpret_cast<FName*>(Params + Param.Offset)->~FName();
		}
	}

	// ----------------------------------------------------------------- read ----

	/** Push the value living at Params + Offset. Unknown means `void`, so nothing is pushed. */
	template <EStaticPropKind Kind>
	FORCEINLINE int32 PushThunkValue(lua_State* L, const uint8* Params, int32 Offset, FProperty* Property)
	{
		const uint8* const Source = Params + Offset;

		if constexpr (Kind == EStaticPropKind::Bool)
		{
			lua_pushboolean(L, static_cast<FBoolProperty*>(Property)->GetPropertyValue(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Int)
		{
			UNLUANOVA_PUSHINT(L, *reinterpret_cast<const int32*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Int8)
		{
			UNLUANOVA_PUSHINT(L, *reinterpret_cast<const int8*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Int16)
		{
			UNLUANOVA_PUSHINT(L, *reinterpret_cast<const int16*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Int64)
		{
			UNLUANOVA_PUSHINT64(L, *reinterpret_cast<const int64*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Byte)
		{
			UNLUANOVA_PUSHINT(L, *reinterpret_cast<const uint8*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Float)
		{
			lua_pushnumber(L, *reinterpret_cast<const float*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Double)
		{
			lua_pushnumber(L, *reinterpret_cast<const double*>(Source));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Name)
		{
			lua_pushstring(L, TCHAR_TO_UTF8(*reinterpret_cast<const FName*>(Source)->ToString()));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::String)
		{
			lua_pushstring(L, TCHAR_TO_UTF8(**reinterpret_cast<const FString*>(Source)));
			return 1;
		}
		else if constexpr (Kind == EStaticPropKind::Object || Kind == EStaticPropKind::Class)
		{
			UnLuaNova::PushUObject(L, static_cast<FObjectPropertyBase*>(Property)->GetObjectPropertyValue(Source), false);
			return 1;
		}
		else
		{
			static_assert(Kind != Kind, "StaticBindingThunk: return kind is not thunkable");
			return 0;
		}
	}

	// ---------------------------------------------------------------- thunk ----

	template <EStaticPropKind... Args, size_t... Indices>
	FORCEINLINE void WriteThunkParams(lua_State* L, uint8* Params, const FStaticFuncBinding* Binding, std::index_sequence<Indices...>)
	{
		// +2 because slot 1 is `self`, the UObject the method was called on.
		(WriteThunkParam<Args>(L, 2 + (int32)Indices, Params, Binding->Params[Indices]), ...);
	}

	template <EStaticPropKind... Args, size_t... Indices>
	FORCEINLINE void DestroyThunkParams(uint8* Params, const FStaticFuncBinding* Binding, std::index_sequence<Indices...>)
	{
		(DestroyThunkParam<Args>(Params, Binding->Params[Indices]), ...);
	}

	/**
	 * The function the static binding generator hangs off a metatable.
	 *
	 * Upvalue 1 is the resolved FStaticFuncBinding as a light userdata. Nothing else is
	 * captured, so the closure is built once per class per function and from then on is
	 * served straight out of the metatable cache.
	 */
	template <EStaticPropKind Ret, EStaticPropKind... Args>
	int32 StaticFuncThunk(lua_State* L)
	{
		const FStaticFuncBinding* Binding = (const FStaticFuncBinding*)lua_touserdata(L, lua_upvalueindex(1));
		if (UNLIKELY(!Binding || !Binding->Function))
			return luaL_error(L, "static binding thunk has no resolved UFunction");

		UObject* Object = UnLuaNova::GetUObject(L, 1, false);
		if (UNLIKELY(!Object))
			return luaL_error(L, "attempt to call UFunction '%s' on NULL object (check the usage of ':' and '.')",
				TCHAR_TO_UTF8(Binding->Name ? Binding->Name : TEXT("")));

		// The parameter frame is small for every signature we thunk, so it stays on the
		// stack: the reflective path's per-call buffer round trip is one of the costs
		// this exists to remove.
		constexpr int32 InlineCapacity = 256;
		uint8 InlineFrame[InlineCapacity];
		uint8* Params = Binding->ParamsSize <= InlineCapacity
			? InlineFrame
			: (uint8*)FMemory::Malloc(Binding->ParamsSize);

		// Memzero is the initialisation step for every thunkable kind except FName and
		// FString, which are placement-constructed by WriteThunkParam(). Doing it in one
		// block is what keeps the per-parameter path down to a single store.
		FMemory::Memzero(Params, Binding->ParamsSize);

		WriteThunkParams<Args...>(L, Params, Binding, std::make_index_sequence<sizeof...(Args)>{});

		UFunction* FinalFunction = Binding->Function;

		// Not gated on ENABLE_CALL_OVERRIDDEN_FUNCTION here: this template is instantiated
		// in the project's module by the generated file, which does not see UnLuaNova's
		// private defines, so a #if would silently drop the redirect on one side of the
		// boundary only. The flag carries the decision instead - the registry leaves
		// bOverridable at false when the feature is compiled out.
		if (Binding->Flags.bOverridable)
		{
			const ULuaFunction* LuaFunction = ULuaFunction::Get(FinalFunction);
			if (LuaFunction && LuaFunction->GetOverridden())
				FinalFunction = LuaFunction->GetOverridden();
		}

		// Non-net: ProcessEvent queries the call space itself for FUNC_Native and never
		// needs it otherwise - the same reasoning as FFunctionDesc::CallUEFast.
		Object->UObject::ProcessEvent(FinalFunction, Params);

		int32 NumResults = 0;
		if constexpr (Ret != EStaticPropKind::Unknown)
		{
			NumResults = PushThunkValue<Ret>(L, Params, Binding->ReturnValueOffset, Binding->ReturnProperty);
		}

		DestroyThunkParams<Args...>(Params, Binding, std::make_index_sequence<sizeof...(Args)>{});

		if (UNLIKELY(Params != InlineFrame))
			FMemory::Free(Params);

		return NumResults;
	}
}
