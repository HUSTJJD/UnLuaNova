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

#include "ReflectionUtils/StaticPropDesc.h"
#include "StaticBinding/StaticBindingThunk.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Kinds this descriptor marshals itself. Everything else defers to FPropertyDesc. */
	bool IsHandled(EStaticPropKind Kind)
	{
		switch (Kind)
		{
		case EStaticPropKind::Bool:
		case EStaticPropKind::Int8:
		case EStaticPropKind::Int16:
		case EStaticPropKind::Int:
		case EStaticPropKind::Int64:
		case EStaticPropKind::Byte:
		case EStaticPropKind::Float:
		case EStaticPropKind::Double:
		case EStaticPropKind::Name:
		case EStaticPropKind::String:
		case EStaticPropKind::Object:
		case EStaticPropKind::Class:
			return true;
		default:
			return false;
		}
	}

	// The dispatch tables below are the only place the runtime kind is turned into a
	// compile-time one. Each entry instantiates the same template the generated thunks
	// use, so a property read and a function return of the same type share one marshaller.

	int32 PushByKind(lua_State* L, const uint8* ValuePtr, FProperty* Property, EStaticPropKind Kind)
	{
		switch (Kind)
		{
#define UNLUANOVA_STATIC_PROP_PUSH(KindName) \
	case EStaticPropKind::KindName: return UnLuaNova::PushThunkValue<EStaticPropKind::KindName>(L, ValuePtr, 0, Property);
			UNLUANOVA_STATIC_PROP_PUSH(Bool)
			UNLUANOVA_STATIC_PROP_PUSH(Int8)
			UNLUANOVA_STATIC_PROP_PUSH(Int16)
			UNLUANOVA_STATIC_PROP_PUSH(Int)
			UNLUANOVA_STATIC_PROP_PUSH(Int64)
			UNLUANOVA_STATIC_PROP_PUSH(Byte)
			UNLUANOVA_STATIC_PROP_PUSH(Float)
			UNLUANOVA_STATIC_PROP_PUSH(Double)
			UNLUANOVA_STATIC_PROP_PUSH(Name)
			UNLUANOVA_STATIC_PROP_PUSH(String)
			UNLUANOVA_STATIC_PROP_PUSH(Object)
			UNLUANOVA_STATIC_PROP_PUSH(Class)
#undef UNLUANOVA_STATIC_PROP_PUSH
		default:
			return -1;
		}
	}

	void WriteByKind(lua_State* L, int32 IndexInStack, uint8* ValuePtr, FProperty* Property, EStaticPropKind Kind)
	{
		// WriteThunkParam<> wants a descriptor because that is what a function parameter
		// is; a property is the same thing with a zero offset from its own value pointer.
		FStaticParamBinding Slot;
		Slot.Offset = 0;
		Slot.Kind = Kind;
		Slot.Property = Property;

		switch (Kind)
		{
#define UNLUANOVA_STATIC_PROP_WRITE(KindName) \
	case EStaticPropKind::KindName: UnLuaNova::WriteThunkParam<EStaticPropKind::KindName>(L, IndexInStack, ValuePtr, Slot); return;
			UNLUANOVA_STATIC_PROP_WRITE(Bool)
			UNLUANOVA_STATIC_PROP_WRITE(Int8)
			UNLUANOVA_STATIC_PROP_WRITE(Int16)
			UNLUANOVA_STATIC_PROP_WRITE(Int)
			UNLUANOVA_STATIC_PROP_WRITE(Int64)
			UNLUANOVA_STATIC_PROP_WRITE(Byte)
			UNLUANOVA_STATIC_PROP_WRITE(Float)
			UNLUANOVA_STATIC_PROP_WRITE(Double)
			UNLUANOVA_STATIC_PROP_WRITE(Name)
			UNLUANOVA_STATIC_PROP_WRITE(String)
			UNLUANOVA_STATIC_PROP_WRITE(Object)
			UNLUANOVA_STATIC_PROP_WRITE(Class)
#undef UNLUANOVA_STATIC_PROP_WRITE
		default:
			return;
		}
	}
}

FStaticPropDesc::FStaticPropDesc(FProperty* InProperty, const FStaticPropBinding* InBinding)
	: FPropertyDesc(InProperty)
	, Binding(InBinding)
	, Offset(InBinding ? InBinding->Offset : 0)
	, Kind(InBinding ? InBinding->Kind : EStaticPropKind::Unknown)
	, bHandledKind(InBinding != nullptr && IsHandled(Kind))
{
}

bool FStaticPropDesc::IsValid() const
{
	// The base class walks a switch on PropertyType calling IsUObjectValid() on the
	// property's inner types - an object property re-checks its PropertyClass every time.
	// A live FProperty implies its class/struct/signature are live, and TWeakFieldPtr is
	// what actually tells us whether the property survived, so that is all we need.
	return PropertyPtr.IsValid();
}

void FStaticPropDesc::ReadValue_InContainer(lua_State* L, const void* ContainerPtr, bool bCreateCopy) const
{
	if (UNLIKELY(!IsHandledKind()))
	{
		FPropertyDesc::ReadValue_InContainer(L, ContainerPtr, bCreateCopy);
		return;
	}

	if (UNLIKELY(UnLuaNova::LowLevel::IsReleasedPtr(ContainerPtr)))
	{
		UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to read property '%s' on released object"), *Name);
		lua_pushnil(L);
		return;
	}

	int32 NumPushed = PushByKind(L, (const uint8*)ContainerPtr + Offset, Property, Kind);
	if (UNLIKELY(NumPushed < 0))
	{
		FPropertyDesc::ReadValue_InContainer(L, ContainerPtr, bCreateCopy);
		return;
	}
}

bool FStaticPropDesc::WriteValue_InContainer(lua_State* L, void* ContainerPtr, int32 IndexInStack, bool bCreateCopy) const
{
	if (UNLIKELY(!IsHandledKind()))
		return FPropertyDesc::WriteValue_InContainer(L, ContainerPtr, IndexInStack, bCreateCopy);

	if (UNLIKELY(UnLuaNova::LowLevel::IsReleasedPtr(ContainerPtr)))
	{
		UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to write property '%s' on released object"), *Name);
		return false;
	}

	WriteByKind(L, IndexInStack, (uint8*)ContainerPtr + Offset, Property, Kind);
	return true;
}

void FStaticPropDesc::ReadValue(lua_State* L, const void* ValuePtr, bool bCreateCopy) const
{
	if (UNLIKELY(!IsHandledKind()))
	{
		FPropertyDesc::ReadValue(L, ValuePtr, bCreateCopy);
		return;
	}

	if (UNLIKELY(UnLuaNova::LowLevel::IsReleasedPtr(ValuePtr)))
	{
		UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to read property '%s' on released object"), *Name);
		lua_pushnil(L);
		return;
	}

	int32 NumPushed = PushByKind(L, (const uint8*)ValuePtr, Property, Kind);
	if (UNLIKELY(NumPushed < 0))
	{
		FPropertyDesc::ReadValue(L, ValuePtr, bCreateCopy);
		return;
	}
}

bool FStaticPropDesc::WriteValue(lua_State* L, void* ValuePtr, int32 IndexInStack, bool bCopyValue) const
{
	if (UNLIKELY(!IsHandledKind()))
		return FPropertyDesc::WriteValue(L, ValuePtr, IndexInStack, bCopyValue);

	if (UNLIKELY(UnLuaNova::LowLevel::IsReleasedPtr(ValuePtr)))
	{
		UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to write property '%s' on released object"), *Name);
		return false;
	}

	WriteByKind(L, IndexInStack, (uint8*)ValuePtr, Property, Kind);
	return true;
}

void FStaticPropDesc::GetValueInternal(lua_State* L, const void* ValuePtr, bool bCreateCopy) const
{
	// Reached only through paths that go straight to the value pointer (containers,
	// out-parameter copy-back). Same marshalling, no offset - ValuePtr already is it.
	if (UNLIKELY(!IsHandledKind() || PushByKind(L, (const uint8*)ValuePtr, Property, Kind) < 0))
	{
		lua_pushnil(L);
	}
}

bool FStaticPropDesc::SetValueInternal(lua_State* L, void* ValuePtr, int32 IndexInStack, bool bCopyValue) const
{
	if (UNLIKELY(!IsHandledKind()))
		return false;

	WriteByKind(L, IndexInStack, (uint8*)ValuePtr, Property, Kind);
	return true;
}
