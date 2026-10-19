// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#include "UnLuauNovaCompatibility.h"
#include "UnLuauNovaEx.h"
#include "LuaLib_Math.h"

static int32 FVector2D_New(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    void* Userdata = NewTypedUserdata(L, FVector2D);

    switch (NumParams)
    {
    case 1:
        {
            new(Userdata) FVector2D(ForceInitToZero);
            break;
        }
    case 2:
        {
            const UnLuauNovaReal& XY = lua_tonumber(L, 2);
            new(Userdata) FVector2D(XY);
            break;
        }
    case 3:
        {
            const UnLuauNovaReal& X = lua_tonumber(L, 2);
            const UnLuauNovaReal& Y = lua_tonumber(L, 3);
            new(Userdata) FVector2D(X, Y);
            break;
        }
    default:
        {
            return luaL_error(L, "invalid parameters");
        }
    }

    return 1;
}

static int32 FVector2D_Set(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    FVector2D* V = (FVector2D*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector2D");

    UnLuauNova::TFieldSetter2<UnLuauNovaReal>::Set(L, NumParams, &V->X);
    return 0;
}

static int32 FVector2D_Normalize(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    FVector2D* V = (FVector2D*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector2D");

    if (NumParams == 1)
    {
        V->Normalize();
    }
    else if (NumParams == 2)
    {
        const float& Tolerance = lua_tonumber(L, 2);
        V->Normalize(Tolerance);
    }
    else
    {
        return luaL_error(L, "invalid parameters");
    }
    
    return 0;
}

static int32 FVector2D_IsNormalized(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams != 1)
        return luaL_error(L, "invalid parameters");

    FVector2D* V = (FVector2D*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector2D");

    lua_pushboolean(L, FMath::Abs(1.f - V->SizeSquared()) < THRESH_VECTOR_NORMALIZED);
    return 1;
}

static int32 FVector2D_UNM(lua_State* L)
{
    FVector2D* V = (FVector2D*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FVector2D");

    void* Userdata = NewTypedUserdata(L, FVector2D);
    new(Userdata) FVector2D(-(*V));
    return 1;
}

static const luaL_Reg FVector2DLib[] =
{
    {"Set", FVector2D_Set},
    {"Normalize", FVector2D_Normalize},
    {"IsNormalized", FVector2D_IsNormalized},
    {"Add", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TAdd<UnLuauNovaReal>, true>::Calculate},
    {"Sub", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TSub<UnLuauNovaReal>, true>::Calculate},
    {"Mul", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TMul<UnLuauNovaReal>, true>::Calculate},
    {"Div", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TDiv<UnLuauNovaReal>, true>::Calculate},
    {"__add", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TAdd<UnLuauNovaReal>>::Calculate},
    {"__sub", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TSub<UnLuauNovaReal>>::Calculate},
    {"__mul", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TMul<UnLuauNovaReal>>::Calculate},
    {"__div", UnLuauNova::TMathCalculation<FVector2D, UnLuauNova::TDiv<UnLuauNovaReal>>::Calculate},
    {"__tostring", UnLuauNova::TMathUtils<FVector2D>::ToString},
    {"__unm", FVector2D_UNM},
    {"__call", FVector2D_New},
    {nullptr, nullptr}
};

BEGIN_EXPORT_REFLECTED_CLASS(FVector2D)
    ADD_CONST_FUNCTION_EX("Dot", UnLuauNovaReal, operator|, const FVector2D&)
    ADD_CONST_FUNCTION_EX("Cross", UnLuauNovaReal, operator^, const FVector2D&)
    ADD_FUNCTION(Size)
    ADD_FUNCTION(SizeSquared)
    ADD_STATIC_FUNCTION_EX("Dist", UnLuauNovaReal, Distance, const FVector2D&, const FVector2D&)
    ADD_STATIC_FUNCTION(DistSquared)
    ADD_STATIC_PROPERTY(ZeroVector)
    ADD_LIB(FVector2DLib)
END_EXPORT_CLASS()

IMPLEMENT_EXPORTED_CLASS(FVector2D)
