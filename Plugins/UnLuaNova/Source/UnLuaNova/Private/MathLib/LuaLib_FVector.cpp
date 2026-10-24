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

#include "UnLuaNovaCompatibility.h"
#include "UnLuaNovaEx.h"
#include "LuaBackend.h"
#include "MathLib/LuaLib_Math.h"

static int32 FVector_New(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    void* Userdata = NewTypedUserdata(L, FVector);

    switch (NumParams)
    {
    case 1:
        {
            new(Userdata) FVector(ForceInitToZero);
            break;
        }
    case 2:
        {
#if UNLUANOVA_HAS_NATIVE_VECTOR
            // A Luau vector arrives as three floats already sitting in the TValue, so
            // UE.FVector(v) can build the FVector without going through three
            // lua_tonumber calls and without allocating anything for the argument.
            if (UNLUANOVA_ISVECTOR(L, 2))
            {
                const LUA_VECTOR_TYPE* Components = UNLUANOVA_TOVECTOR(L, 2);
                new(Userdata) FVector((UnLuaNovaReal)Components[0], (UnLuaNovaReal)Components[1], (UnLuaNovaReal)Components[2]);
                break;
            }
#endif
            const UnLuaNovaReal& XYZ = lua_tonumber(L, 2);
            new(Userdata) FVector(XYZ);
            break;
        }
    case 4:
        {
            const UnLuaNovaReal& X = lua_tonumber(L, 2);
            const UnLuaNovaReal& Y = lua_tonumber(L, 3);
            const UnLuaNovaReal& Z = lua_tonumber(L, 4);
            new(Userdata) FVector(X, Y, Z);
            break;
        }
    default:
        {
            return luaL_error(L, "invalid parameters");
        }
    }
    return 1;
}

static int32 FVector_Set(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    FVector* V = (FVector*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector");

#if UNLUANOVA_HAS_NATIVE_VECTOR
    if (NumParams == 2 && UNLUANOVA_ISVECTOR(L, 2))
    {
        const LUA_VECTOR_TYPE* Components = UNLUANOVA_TOVECTOR(L, 2);
        V->X = (UnLuaNovaReal)Components[0];
        V->Y = (UnLuaNovaReal)Components[1];
        V->Z = (UnLuaNovaReal)Components[2];
        return 0;
    }
#endif

    UnLuaNova::TFieldSetter3<UnLuaNovaReal>::Set(L, NumParams, &V->X);
    return 0;
}

static int32 FVector_Normalize(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    FVector* V = (FVector*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector");

    if (NumParams == 1)
    {
        lua_pushboolean(L, V->Normalize());
    }
    else if (NumParams == 2)
    {
        const float& Tolerance = lua_tonumber(L, 2);
        lua_pushboolean(L, V->Normalize(Tolerance));
    }
    else
    {
        return luaL_error(L, "invalid parameters");
    }

    return 1;
}

static int32 FVector_UNM(lua_State* L)
{
    FVector* V = (FVector*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector");

    void* Userdata = NewTypedUserdata(L, FVector);
    new(Userdata) FVector(-(*V));
    return 1;
}

// Luau only: hand the value back as a native vector. That is the whole point of the
// type - arithmetic on it allocates nothing, where every FVector operation on the
// userdata path pays for a new userdata. On other backends there is no such type, so
// this answers nil and callers keep using the userdata.
static int32 FVector_ToVector(lua_State* L)
{
    FVector* V = (FVector*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector");

#if UNLUANOVA_HAS_NATIVE_VECTOR
    UNLUANOVA_PUSHVECTOR3(L, V->X, V->Y, V->Z);
    return 1;
#else
    // A bare nil used to be the answer here, and it is the worst possible one: a script
    // that does `local v = fv:ToVector()` gets nil, carries on, and fails three lines later
    // with "attempt to index a nil value" - nowhere near the call that caused it. Say it
    // here, once, where the reason is. Scripts that want to branch instead should ask
    // UnLuaNova.HasFeature('vector') rather than poke the return value.
    UE_LOG(LogUnLuaNova, Warning,
        TEXT("FVector:ToVector() is only available on the Luau backend - there is no native vector type to hand back here. It returns nil; guard the call with UnLuaNova.HasFeature('vector')."));
    lua_pushnil(L);
    return 1;
#endif
}

static const luaL_Reg FVectorLib[] =
{
    {"Set", FVector_Set},
    {"Normalize", FVector_Normalize},
    {"ToVector", FVector_ToVector},
    {"Add", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TAdd<UnLuaNovaReal>, true>::Calculate},
    {"Sub", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TSub<UnLuaNovaReal>, true>::Calculate},
    {"Mul", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TMul<UnLuaNovaReal>, true>::Calculate},
    {"Div", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TDiv<UnLuaNovaReal>, true>::Calculate},
    {"__add", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TAdd<UnLuaNovaReal>>::Calculate},
    {"__sub", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TSub<UnLuaNovaReal>>::Calculate},
    {"__mul", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TMul<UnLuaNovaReal>>::Calculate},
    {"__div", UnLuaNova::TMathCalculation<FVector, UnLuaNova::TDiv<UnLuaNovaReal>>::Calculate},
    {"__tostring", UnLuaNova::TMathUtils<FVector>::ToString},
    {"__unm", FVector_UNM},
    {"__call", FVector_New},
    {nullptr, nullptr}
};

BEGIN_EXPORT_REFLECTED_CLASS(FVector)
    ADD_CONST_FUNCTION_EX("Dot", UnLuaNovaReal, operator|, const FVector&)
    ADD_CONST_FUNCTION_EX("Cross", FVector, operator^, const FVector&)
    ADD_FUNCTION(Size)
    ADD_FUNCTION(Size2D)
    ADD_FUNCTION(SizeSquared)
    ADD_FUNCTION(SizeSquared2D)
    ADD_STATIC_FUNCTION(Dist)
    ADD_STATIC_FUNCTION(Dist2D)
    ADD_STATIC_FUNCTION(DistSquared)
    ADD_STATIC_FUNCTION(DistSquared2D)
    ADD_FUNCTION(IsNormalized)
    ADD_FUNCTION(CosineAngle2D)
    ADD_FUNCTION(RotateAngleAxis)
    ADD_NAMED_FUNCTION("ToRotator", ToOrientationRotator)
    ADD_NAMED_FUNCTION("ToQuat", ToOrientationQuat)
    ADD_LIB(FVectorLib)
END_EXPORT_CLASS()

IMPLEMENT_EXPORTED_CLASS(FVector)
