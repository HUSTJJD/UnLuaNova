// Static binding descriptors.
//
// The point of these structs is to move work that UnLuauNova currently does on *every*
// cross-boundary access (reflection lookups, virtual dispatch, flag tests) into a
// pre-resolved, POD, generator-friendly table.
//
// They are POD on purpose: the build-time generator emits them as `static constexpr`
// tables, so there is zero start-up parsing cost and the compiler can keep them in
// read-only memory. Fields that can only be known at runtime (FProperty*/UFunction*)
// are filled in once by FStaticBindingRegistry::Register().

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UFunction;
class UClass;

/** Coarse property category. Mirrors what FPropertyDesc dispatches on at runtime. */
enum class EStaticPropKind : uint8
{
	Unknown,
	Bool,
	Int8,
	Int16,
	Int,
	Int64,
	Byte,
	Float,
	Double,
	Name,
	String,
	Text,
	Object,
	Class,
	SoftObject,
	LazyObject,
	WeakObject,
	Interface,
	Struct,
	Enum,
	Array,
	Set,
	Map,
	Delegate,
	MulticastDelegate,
	FieldPath,
};

/** A pre-resolved UPROPERTY. */
struct FStaticPropBinding
{
	/** UE property name, e.g. TEXT("Health"). */
	const TCHAR* Name = nullptr;

	/** Cached FProperty::GetOffset_ForGC(). */
	int32 Offset = 0;

	EStaticPropKind Kind = EStaticPropKind::Unknown;

	/** Resolved on first registration. */
	FProperty* Property = nullptr;
};

/** A pre-resolved UFUNCTION parameter / return value. */
struct FStaticParamBinding
{
	const TCHAR* Name = nullptr;
	int32 Offset = 0;
	EStaticPropKind Kind = EStaticPropKind::Unknown;
	FProperty* Property = nullptr;
};

/** Behaviour flags hoisted out of the per-call hot path. */
struct FStaticFuncFlags
{
	/** Has FUNC_Net* -> must go through GetFunctionCallspace/CallRemoteFunction. */
	uint8 bIsNet : 1;

	/** Has FUNC_Native -> UObject::ProcessEvent will query the call space itself. */
	uint8 bIsNative : 1;

	/** FUNC_BlueprintEvent / native event -> can be overridden from Lua, so the
	 *  ULuaFunction::Get() redirect lookup must still run. */
	uint8 bOverridable : 1;

	/** BlueprintCallable static function (UFUNCTION(static)). */
	uint8 bStatic : 1;

	/** Declared on an interface -> needs the per-object FindFunctionByName. */
	uint8 bInterface : 1;

	/** Has a LatentInfo parameter -> needs the coroutine path. */
	uint8 bLatent : 1;

	FStaticFuncFlags()
		: bIsNet(0), bIsNative(0), bOverridable(0), bStatic(0), bInterface(0), bLatent(0)
	{
	}
};

/** A pre-resolved UFUNCTION. */
struct FStaticFuncBinding
{
	const TCHAR* Name = nullptr;

	/** Cached UFunction::ParmsSize. */
	int32 ParamsSize = 0;

	/** Cached UFunction::ReturnValueOffset, or MAX_uint16 when there is no return value. */
	uint16 ReturnValueOffset = MAX_uint16;

	/** Number of entries in Params (does not include the return value). */
	int32 NumParams = 0;

	FStaticFuncFlags Flags;

	const FStaticParamBinding* Params = nullptr;

	/** Resolved on first registration. */
	UFunction* Function = nullptr;
};

/** A pre-resolved UCLASS / UScriptStruct. `TypeName` matches UnLuauNova's metatable name. */
struct FStaticTypeBinding
{
	/** Metatable name as UnLuauNova sees it, e.g. TEXT("Actor"), TEXT("Vector"), TEXT("MyEnum"). */
	const TCHAR* TypeName = nullptr;

	const FStaticPropBinding* Props = nullptr;
	int32 NumProps = 0;

	const FStaticFuncBinding* Funcs = nullptr;
	int32 NumFuncs = 0;

	/** Resolved on first registration. */
	UClass* Class = nullptr;

	/** Set by the registry once every Property/Function pointer has been resolved. */
	bool bResolved = false;
};

/** Build-time helper: turn a UPROPERTY/UFUNCTION name into a descriptor array entry. */
#define UnLuauNovaNOVA_STATIC_PROP(NameLiteral, KindValue) \
	{ TEXT(#NameLiteral), 0, EStaticPropKind::KindValue, nullptr }

#define UnLuauNovaNOVA_STATIC_PARAM(NameLiteral, KindValue) \
	{ TEXT(#NameLiteral), 0, EStaticPropKind::KindValue, nullptr }

#define UnLuauNovaNOVA_STATIC_FUNC_BEGIN(NameLiteral) \
	{ TEXT(#NameLiteral), 0, MAX_uint16, 0, FStaticFuncFlags(), nullptr, nullptr }
