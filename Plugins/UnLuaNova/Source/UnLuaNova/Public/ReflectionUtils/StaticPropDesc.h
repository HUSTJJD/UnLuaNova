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

#include "ReflectionUtils/PropertyDesc.h"
#include "StaticBinding/StaticBindingTypes.h"

/**
 * Property descriptor for a UPROPERTY that a static binding already resolved.
 *
 * FPropertyDesc::ReadValue_InContainer() costs, on every single access: a
 * TWeakFieldPtr::IsValid() (a weak-handle resolve, not a pointer test), a released-ptr
 * check, a ContainerPtrToValuePtr() and then a *virtual* GetValueInternal(). All four are
 * pure lookup overhead - none of them depends on the value being read.
 *
 * A FStaticPropBinding already answers two of them at registration time (the offset and
 * the kind), and the remaining two are invariants of the descriptor being alive at all.
 * This class therefore resolves the kind once, in its constructor, and turns the read into
 * "add an offset, push by type".
 *
 * It derives from FPropertyDesc rather than replacing it: Class_Index/Class_NewIndex and
 * every container path keep working through ITypeOps unchanged. Callers must use Supports()
 * before constructing this descriptor; other kinds require FPropertyDesc::Create() or the
 * normal class registry lookup to obtain their specialized descriptor.
 */
class FStaticPropDesc : public FPropertyDesc
{
public:
	static bool Supports(EStaticPropKind Kind);
	FStaticPropDesc(FProperty* InProperty, const FStaticPropBinding* InBinding);

	virtual bool IsValid() const override;

	virtual void ReadValue_InContainer(lua_State* L, const void* ContainerPtr, bool bCreateCopy) const override;
	virtual bool WriteValue_InContainer(lua_State* L, void* ContainerPtr, int32 IndexInStack, bool bCreateCopy) const override;

	virtual void ReadValue(lua_State* L, const void* ValuePtr, bool bCreateCopy) const override;
	virtual bool WriteValue(lua_State* L, void* ValuePtr, int32 IndexInStack, bool bCopyValue) const override;

protected:
	virtual void GetValueInternal(lua_State* L, const void* ValuePtr, bool bCreateCopy) const override;
	virtual bool SetValueInternal(lua_State* L, void* ValuePtr, int32 IndexInStack, bool bCopyValue) const override;

private:
	/** True when Kind is one this descriptor marshals itself; otherwise it defers to FPropertyDesc. */
	FORCEINLINE bool IsHandledKind() const { return bHandledKind; }

	const FStaticPropBinding* Binding;
	int32 Offset;
	EStaticPropKind Kind;
	bool bHandledKind;
};
