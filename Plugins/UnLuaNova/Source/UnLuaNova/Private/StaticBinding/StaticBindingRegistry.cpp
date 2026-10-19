#include "StaticBinding/StaticBindingRegistry.h"

#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

FStaticBindingRegistry& FStaticBindingRegistry::Get()
{
	static FStaticBindingRegistry Instance;
	return Instance;
}

EStaticPropKind FStaticBindingRegistry::GetPropertyKind(const FProperty* Property)
{
	if (!Property)
		return EStaticPropKind::Unknown;

	if (CastField<FBoolProperty>(Property))					return EStaticPropKind::Bool;
	if (CastField<FInt8Property>(Property))					return EStaticPropKind::Int8;
	if (CastField<FInt16Property>(Property))				return EStaticPropKind::Int16;
	if (CastField<FIntProperty>(Property))					return EStaticPropKind::Int;
	if (CastField<FInt64Property>(Property))				return EStaticPropKind::Int64;
	if (CastField<FByteProperty>(Property))					return EStaticPropKind::Byte;
	if (CastField<FFloatProperty>(Property))				return EStaticPropKind::Float;
	if (CastField<FDoubleProperty>(Property))				return EStaticPropKind::Double;
	if (CastField<FNameProperty>(Property))					return EStaticPropKind::Name;
	if (CastField<FStrProperty>(Property))					return EStaticPropKind::String;
	if (CastField<FTextProperty>(Property))					return EStaticPropKind::Text;
	if (CastField<FClassProperty>(Property))				return EStaticPropKind::Class;
	if (CastField<FSoftClassProperty>(Property))			return EStaticPropKind::SoftObject;
	if (CastField<FSoftObjectProperty>(Property))			return EStaticPropKind::SoftObject;
	if (CastField<FLazyObjectProperty>(Property))			return EStaticPropKind::LazyObject;
	if (CastField<FWeakObjectProperty>(Property))			return EStaticPropKind::WeakObject;
	if (CastField<FInterfaceProperty>(Property))			return EStaticPropKind::Interface;
	if (CastField<FStructProperty>(Property))				return EStaticPropKind::Struct;
	if (CastField<FEnumProperty>(Property))					return EStaticPropKind::Enum;
	if (CastField<FArrayProperty>(Property))				return EStaticPropKind::Array;
	if (CastField<FSetProperty>(Property))					return EStaticPropKind::Set;
	if (CastField<FMapProperty>(Property))					return EStaticPropKind::Map;
	if (CastField<FDelegateProperty>(Property))				return EStaticPropKind::Delegate;
	if (CastField<FMulticastDelegateProperty>(Property))	return EStaticPropKind::MulticastDelegate;
	if (CastField<FFieldPathProperty>(Property))			return EStaticPropKind::FieldPath;
	if (CastField<FObjectProperty>(Property))				return EStaticPropKind::Object;

	return EStaticPropKind::Unknown;
}

static UStruct* ResolveStructByName(const FString& Name)
{
	if (UClass* Class = FindFirstObject<UClass>(*Name))
		return Class;
	if (UScriptStruct* Struct = FindFirstObject<UScriptStruct>(*Name))
		return Struct;
	if (UEnum* Enum = FindFirstObject<UEnum>(*Name))
		return Enum;

	if (UClass* Class = LoadObject<UClass>(nullptr, *Name))
		return Class;
	if (UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, *Name))
		return Struct;
	if (UEnum* Enum = LoadObject<UEnum>(nullptr, *Name))
		return Enum;

	return nullptr;
}

void FStaticBindingRegistry::Resolve(const FStaticTypeBinding* Type, FEntry& Entry)
{
	const FString TypeName = Type->TypeName ? Type->TypeName : TEXT("");

	UStruct* Struct = ResolveStructByName(TypeName);
	if (!Struct)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[StaticBinding] type '%s' could not be resolved; reflective fallback stays in effect."), *TypeName);
		return;
	}

	StructToType.Add(Struct, Type);

	// --- properties -------------------------------------------------------
	for (int32 i = 0; i < Type->NumProps; ++i)
	{
		const FStaticPropBinding* Source = &Type->Props[i];
		if (!Source->Name)
			continue;

		const FName PropName = Source->Name;

		FProperty* Prop = Struct->FindPropertyByName(PropName);
		if (!Prop)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[StaticBinding] %s: property '%s' not found."), *TypeName, *PropName.ToString());
			continue;
		}

		const_cast<FStaticPropBinding*>(Source)->Property = Prop;
		const_cast<FStaticPropBinding*>(Source)->Offset = Prop->GetOffset_ForGC();
		if (Source->Kind == EStaticPropKind::Unknown)
			const_cast<FStaticPropBinding*>(Source)->Kind = GetPropertyKind(Prop);

		Entry.Props.Add(PropName, Source);
	}

	// --- functions --------------------------------------------------------
	const UClass* AsClass = Cast<UClass>(Struct);
	for (int32 i = 0; i < Type->NumFuncs; ++i)
	{
		const FStaticFuncBinding* Source = &Type->Funcs[i];
		if (!Source->Name)
			continue;

		const FName FuncName = Source->Name;

		UFunction* Function = AsClass ? AsClass->FindFunctionByName(FuncName, EIncludeSuperFlag::IncludeSuper) : nullptr;
		if (!Function)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[StaticBinding] %s: function '%s' not found."), *TypeName, *FuncName.ToString());
			continue;
		}

		const_cast<FStaticFuncBinding*>(Source)->Function = Function;
		const_cast<FStaticFuncBinding*>(Source)->ParamsSize = Function->ParmsSize;
		const_cast<FStaticFuncBinding*>(Source)->ReturnValueOffset = Function->ReturnValueOffset;

		const_cast<FStaticFuncBinding*>(Source)->Flags.bIsNet = Function->HasAnyFunctionFlags(FUNC_Net) != 0;
		const_cast<FStaticFuncBinding*>(Source)->Flags.bIsNative = Function->HasAnyFunctionFlags(FUNC_Native) != 0;
		const_cast<FStaticFuncBinding*>(Source)->Flags.bStatic = Function->HasAnyFunctionFlags(FUNC_Static) != 0;
		const_cast<FStaticFuncBinding*>(Source)->Flags.bInterface =
			Function->GetOwnerClass() != nullptr && Function->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface);

		// Mirrors ULuaFunction::IsOverridable() without pulling in the override machinery.
		constexpr uint32 OverridableMask = FUNC_Native | FUNC_Event | FUNC_Net;
		constexpr uint32 OverridableResult = FUNC_Native | FUNC_Event;
		const_cast<FStaticFuncBinding*>(Source)->Flags.bOverridable =
			(Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) != 0)
			|| ((Function->FunctionFlags & OverridableMask) == OverridableResult);

		for (int32 p = 0; p < Source->NumParams; ++p)
		{
			const FStaticParamBinding* ParamSource = &Source->Params[p];
			if (!ParamSource->Name)
				continue;

			const FName ParamName = ParamSource->Name;

			if (ParamName == TEXT("LatentInfo"))
				const_cast<FStaticFuncBinding*>(Source)->Flags.bLatent = 1;

			FProperty* Param = nullptr;
			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				if (It->GetFName() == ParamName)
				{
					Param = *It;
					break;
				}
			}

			if (!Param || Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("[StaticBinding] %s.%s: parameter '%s' not found."), *TypeName, *FuncName.ToString(), *ParamName.ToString());
				continue;
			}

			const_cast<FStaticParamBinding*>(ParamSource)->Property = Param;
			const_cast<FStaticParamBinding*>(ParamSource)->Offset = Param->GetOffset_ForGC();
			if (ParamSource->Kind == EStaticPropKind::Unknown)
				const_cast<FStaticParamBinding*>(ParamSource)->Kind = GetPropertyKind(Param);
		}

		Entry.Funcs.Add(FuncName, Source);
	}

	const_cast<FStaticTypeBinding*>(Type)->bResolved = true;
}

/**
 * Resolution is deferred on purpose: generated tables register from static initialisers,
 * which run long before the UObject system is usable. Nothing touches UClass/UFunction
 * until the first lookup.
 */
void FStaticBindingRegistry::ResolvePending()
{
	// Cheap gate: readers consult this before taking any lock at all.
	if (PendingCount.load(std::memory_order_acquire) == 0)
		return;

	FRWScopeLock Write(RwLock, SLT_Write);

	// Re-check under the lock - another thread may have drained the queue meanwhile.
	if (Pending.Num() == 0)
		return;

	for (const FStaticTypeBinding* Type : Pending)
	{
		const FName Key = Type->TypeName;
		FEntry& Entry = Types.FindOrAdd(Key);
		Entry.Type = Type;
		Resolve(Type, Entry);
	}

	Pending.Empty();
	PendingCount.store(0, std::memory_order_release);
}

void FStaticBindingRegistry::Register(const FStaticTypeBinding* Type)
{
	if (!Type || !Type->TypeName)
		return;

	FRWScopeLock Write(RwLock, SLT_Write);

	if (Types.Contains(Type->TypeName) || Pending.Contains(Type))
		return;

	Pending.Add(Type);
	PendingCount.store(Pending.Num(), std::memory_order_release);
}

void FStaticBindingRegistry::Unregister(const FStaticTypeBinding* Type)
{
	if (!Type || !Type->TypeName)
		return;

	// ResolvePending takes the write lock itself, so let it run before we acquire ours -
	// FRWLock is not recursive.
	ResolvePending();

	FRWScopeLock Write(RwLock, SLT_Write);

	const FName Key = Type->TypeName;

	TArray<const UStruct*> KeysToRemove;
	for (const TPair<const UStruct*, const FStaticTypeBinding*>& Pair : StructToType)
	{
		if (Pair.Value == Type)
			KeysToRemove.Add(Pair.Key);
	}
	for (const UStruct* KeyToRemove : KeysToRemove)
		StructToType.Remove(KeyToRemove);

	Types.Remove(Key);
	Pending.Remove(Type);
}

const FStaticTypeBinding* FStaticBindingRegistry::Find(const FName TypeName) const
{
	FRWScopeLock Read(RwLock, SLT_ReadOnly);
	if (const FEntry* Entry = Types.Find(TypeName))
		return Entry->Type;
	return nullptr;
}

const FStaticTypeBinding* FStaticBindingRegistry::Find(const UStruct* Type) const
{
	// Only pay for a resolve when something is actually queued. After startup PendingCount
	// is 0, so this is a no-op and the lookup stays entirely on the shared read path.
	if (PendingCount.load(std::memory_order_acquire) != 0)
		const_cast<FStaticBindingRegistry*>(this)->ResolvePending();

	FRWScopeLock Read(RwLock, SLT_ReadOnly);

	for (const UStruct* Current = Type; Current; Current = Current->GetInheritanceSuper())
	{
		if (const FStaticTypeBinding* const* Found = StructToType.Find(Current))
			return *Found;
	}

	return nullptr;
}

const FStaticPropBinding* FStaticBindingRegistry::FindProp(const FStaticTypeBinding* Type, const FName Name) const
{
	if (!Type || !Type->TypeName)
		return nullptr;

	// Only pay for a resolve when something is actually queued. After startup PendingCount
	// is 0, so this is a no-op and the lookup stays entirely on the shared read path.
	if (PendingCount.load(std::memory_order_acquire) != 0)
		const_cast<FStaticBindingRegistry*>(this)->ResolvePending();

	FRWScopeLock Read(RwLock, SLT_ReadOnly);

	if (const FEntry* Entry = Types.Find(Type->TypeName))
		if (const FStaticPropBinding* const* Found = Entry->Props.Find(Name))
			return *Found;

	return nullptr;
}

const FStaticFuncBinding* FStaticBindingRegistry::FindFunc(const FStaticTypeBinding* Type, const FName Name) const
{
	if (!Type || !Type->TypeName)
		return nullptr;

	// Only pay for a resolve when something is actually queued. After startup PendingCount
	// is 0, so this is a no-op and the lookup stays entirely on the shared read path.
	if (PendingCount.load(std::memory_order_acquire) != 0)
		const_cast<FStaticBindingRegistry*>(this)->ResolvePending();

	FRWScopeLock Read(RwLock, SLT_ReadOnly);

	if (const FEntry* Entry = Types.Find(Type->TypeName))
		if (const FStaticFuncBinding* const* Found = Entry->Funcs.Find(Name))
			return *Found;

	return nullptr;
}

void FStaticBindingRegistry::Clear()
{
	FRWScopeLock Write(RwLock, SLT_Write);
	Types.Empty();
	StructToType.Empty();
	PendingCount.store(0, std::memory_order_release);
	Pending.Empty();
}
