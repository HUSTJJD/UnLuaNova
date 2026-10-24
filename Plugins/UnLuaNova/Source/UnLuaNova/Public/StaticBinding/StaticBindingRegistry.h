#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeRWLock.h"
#include "StaticBinding/StaticBindingTypes.h"

#include <atomic>

/**
 * Registry of pre-resolved (static) bindings.
 *
 * Reflective access stays the default: the registry is consulted first and, on a miss,
 * UnLuaNova falls back to its existing FPropertyDesc / FFunctionDesc machinery. A declared
 * type therefore gets the fast path while every other type keeps working exactly as before.
 */
class UNLUANOVA_API FStaticBindingRegistry
{
public:
	static FStaticBindingRegistry& Get();

	/**
	 * Queue a writable descriptor for resolution on first lookup.
	 * The descriptor and its nested tables must outlive the registration and Lua users.
	 * Safe to call repeatedly; a type is only resolved once.
	 */
	void Register(FStaticTypeBinding* Type);

	void Unregister(const FStaticTypeBinding* Type);

	/** Look up a declared type by its UnLuaNova metatable name. */
	const FStaticTypeBinding* Find(const FName TypeName) const;

	/** Look up a declared type by UObject class (walks the super chain). */
	const FStaticTypeBinding* Find(const UStruct* Type) const;

	const FStaticPropBinding* FindProp(const FStaticTypeBinding* Type, const FName Name) const;
	const FStaticFuncBinding* FindFunc(const FStaticTypeBinding* Type, const FName Name) const;

	/** Coarse classification shared by the generator and the runtime. */
	static EStaticPropKind GetPropertyKind(const FProperty* Property);

	int32 NumRegisteredTypes() const { return Types.Num(); }

	/**
	 * How much of the registered surface actually got a generated thunk.
	 *
	 * A thunk is what removes the reflective call entirely, so this - not the number of
	 * registered types - is the figure that says whether running the generator bought
	 * anything. Reported by the benchmark spec so it lands next to the timings instead of
	 * only in the generator's log.
	 */
	void GetThunkCoverage(int32& OutTypes, int32& OutFuncs, int32& OutThunks) const;

	/** Called by FUnLuaNovaModule on shutdown. */
	void Clear();

private:
	FStaticBindingRegistry() = default;

	struct FEntry
	{
		const FStaticTypeBinding* Type = nullptr;
		TMap<FName, const FStaticPropBinding*> Props;
		TMap<FName, const FStaticFuncBinding*> Funcs;
	};

	void Resolve(FStaticTypeBinding* Type, FEntry& Entry);

	/** Resolves everything queued by Register(). Deferred until the UObject system is up. */
	void ResolvePending();

	TMap<FName, FEntry> Types;
	TMap<const UStruct*, const FStaticTypeBinding*> StructToType;
	TArray<FStaticTypeBinding*> Pending;

	// Large projects register thousands of types and then hit the read path from Lua on
	// every property and function access. An exclusive lock serialised all of that, so
	// readers now take a shared lock instead. PendingCount lets a reader skip the resolve
	// step outright when nothing is queued - the steady state after startup.
	mutable FRWLock RwLock;
	std::atomic<int32> PendingCount{0};
};
