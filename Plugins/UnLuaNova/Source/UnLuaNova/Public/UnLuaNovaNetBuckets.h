// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

// EXPERIMENTAL API - Lua-declared networking. Interfaces in this header may change
// without a deprecation cycle and are not recommended for production use. See
// Docs/CN/LuaNetworking.md for the supported subset.

#include "CoreMinimal.h"
#include "UnLuaNovaNetPayload.h"
#include "UnLuaNovaNetBuckets.generated.h"

/**
 * Storage for every Lua field that shares one replication condition.
 *
 * UE decides whether to send a property from the property's own registration
 * (FDoRepLifetimeParams::Condition), and that registration is cached per class - it cannot
 * depend on an instance or on a value read out of a script. So "field X is OwnerOnly" has to
 * mean "field X lives in a property that was registered COND_OwnerOnly", and the only way to
 * get there is to give each condition its own storage.
 *
 * This is that storage: one typed array per supported type, exactly the shape the unconditional
 * buckets on the component use, so the read/write paths are shared. It is a USTRUCT rather than
 * eleven more UPROPERTYs because a struct is one property to register, one property to dirty and
 * one RepNotify, while still serialising each array natively - including UObject references,
 * which a hand-rolled byte payload would lose.
 *
 * The cost is granularity: a group is dirtied as a whole, not per type. That is why the
 * unconditional fields stay on the component's own per-type arrays, where push model can still
 * be told "only the Floats changed". Conditional fields are the uncommon case and pay for
 * correctness with a coarser dirty flag.
 */
USTRUCT()
struct FLuaNetBuckets
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bools;

	UPROPERTY()
	TArray<int32> Ints;

	UPROPERTY()
	TArray<int64> Int64s;

	UPROPERTY()
	TArray<float> Floats;

	UPROPERTY()
	TArray<double> Doubles;

	UPROPERTY()
	TArray<FString> Strings;

	UPROPERTY()
	TArray<FName> Names;

	UPROPERTY()
	TArray<FVector> Vectors;

	UPROPERTY()
	TArray<FRotator> Rotators;

	UPROPERTY()
	TArray<FTransform> Transforms;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> Objects;

	/**
	 * Canonical text payloads for arbitrary UStruct / container fields. A UPROPERTY cannot be
	 * conjured onto this class at runtime, so all such fields share one replicated string
	 * array; the declaration (covered by SchemaVersion) says how to decode each element.
	 */
	UPROPERTY()
	TArray<FString> Generics;

	/** Size every array to the matching bucket count; counts are indexed by ELuaNetBucket. */
	void SetNum(const int32 (&Counts)[int32(UnLuaNova::ELuaNetBucket::Max)])
	{
		Bools.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Bool)]);
		Ints.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Int)]);
		Int64s.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Int64)]);
		Floats.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Float)]);
		Doubles.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Double)]);
		Strings.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::String)]);
		Names.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Name)]);
		Vectors.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Vector)]);
		Rotators.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Rotator)]);
		Transforms.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Transform)]);
		Objects.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Object)]);
		Generics.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Generic)]);
	}
};

/**
 * A type-erased handle onto one set of buckets, so the read/write paths do not have to know
 * whether they are looking at the component's unconditional arrays or at a conditional group.
 *
 * Pointers, not references: it is built per call from whichever storage a slot's condition
 * selects, and it never owns anything.
 */
struct FLuaNetBucketView
{
	TArray<uint8>* Bools = nullptr;
	TArray<int32>* Ints = nullptr;
	TArray<int64>* Int64s = nullptr;
	TArray<float>* Floats = nullptr;
	TArray<double>* Doubles = nullptr;
	TArray<FString>* Strings = nullptr;
	TArray<FName>* Names = nullptr;
	TArray<FVector>* Vectors = nullptr;
	TArray<FRotator>* Rotators = nullptr;
	TArray<FTransform>* Transforms = nullptr;
	TArray<TObjectPtr<UObject>>* Objects = nullptr;
	TArray<FString>* Generics = nullptr;

	static FLuaNetBucketView From(FLuaNetBuckets& Buckets)
	{
		FLuaNetBucketView View;
		View.Bools = &Buckets.Bools;
		View.Ints = &Buckets.Ints;
		View.Int64s = &Buckets.Int64s;
		View.Floats = &Buckets.Floats;
		View.Doubles = &Buckets.Doubles;
		View.Strings = &Buckets.Strings;
		View.Names = &Buckets.Names;
		View.Vectors = &Buckets.Vectors;
		View.Rotators = &Buckets.Rotators;
		View.Transforms = &Buckets.Transforms;
		View.Objects = &Buckets.Objects;
		View.Generics = &Buckets.Generics;
		return View;
	}

	/** True when every array is present, i.e. the view can be used. */
	bool IsValid() const
	{
		return Bools && Ints && Int64s && Floats && Doubles && Strings && Names
			&& Vectors && Rotators && Transforms && Objects && Generics;
	}
};
