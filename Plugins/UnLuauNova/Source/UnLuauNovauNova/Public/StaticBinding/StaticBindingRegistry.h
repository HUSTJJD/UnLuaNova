#pragma once

#include "CoreMinimal.h"
#include "StaticBinding/StaticBindingTypes.h"

/**
 * Registry of pre-resolved (static) bindings.
 *
 * Reflective access stays the default: the registry is consulted first and, on a miss,
 * UnLuauNova falls back to its existing FPropertyDesc / FFunctionDesc machinery. A declared
 * type therefore gets the fast path while every other type keeps working exactly as before.
 */
class UNLUANOVA_API FStaticBindingRegistry
{
public:
	static FStaticBindingRegistry& Get();

	/**
	 * Resolve every FProperty*/UFunction* inside the descriptor.
	 * Safe to call repeatedly; a type is only resolved once.
	 */
	void Register(const FStaticTypeBinding* Type);

	void Unregister(const FStaticTypeBinding* Type);

	/** Look up a declared type by its UnLuauNova metatable name. */
	const FStaticTypeBinding* Find(const FName TypeName) const;

	/** Look up a declared type by UObject class (walks the super chain). */
	const FStaticTypeBinding* Find(const UStruct* Type) const;

	const FStaticPropBinding* FindProp(const FStaticTypeBinding* Type, const FName Name) const;
	const FStaticFuncBinding* FindFunc(const FStaticTypeBinding* Type, const FName Name) const;

	/** Coarse classification shared by the generator and the runtime. */
	static EStaticPropKind GetPropertyKind(const FProperty* Property);

	int32 NumRegisteredTypes() const { return Types.Num(); }

	/** Called by FUnLuauNovaModule on shutdown. */
	void Clear();

private:
	FStaticBindingRegistry() = default;

	struct FEntry
	{
		const FStaticTypeBinding* Type = nullptr;
		TMap<FName, const FStaticPropBinding*> Props;
		TMap<FName, const FStaticFuncBinding*> Funcs;
	};

	void Resolve(const FStaticTypeBinding* Type, FEntry& Entry);

	/** Resolves everything queued by Register(). Deferred until the UObject system is up. */
	void ResolvePending();

	TMap<FName, FEntry> Types;
	TMap<const UStruct*, const FStaticTypeBinding*> StructToType;
	TArray<const FStaticTypeBinding*> Pending;
	mutable FCriticalSection CriticalSection;
};
