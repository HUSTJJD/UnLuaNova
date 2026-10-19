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

#include "UnLuaNovaEx.h"
#include "LuaLib_Math.h"

static int32 FLinearColor_New(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    void* Userdata = NewTypedUserdata(L, FLinearColor);

    switch (NumParams)
    {
    case 1:
        {
            new(Userdata) FLinearColor(ForceInitToZero);
            break;
        }
    case 4:
        {
            const float& R = lua_tonumber(L, 2);
            const float& G = lua_tonumber(L, 3);
            const float& B = lua_tonumber(L, 4);
            new(Userdata) FLinearColor(R, G, B);
            break;
        }
    case 5:
        {
            const float& R = lua_tonumber(L, 2);
            const float& G = lua_tonumber(L, 3);
            const float& B = lua_tonumber(L, 4);
            const float& A = lua_tonumber(L, 5);
            new(Userdata) FLinearColor(R, G, B, A);
            break;
        }
    default:
        {
            return luaL_error(L, "invalid parameters");
        }
    }
    return 1;
}

static int32 FLinearColor_Set(lua_State* L)
{
    const int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
        return luaL_error(L, "invalid parameters");

    FLinearColor* V = (FLinearColor*)GetCppInstanceFast(L, 1);
    if (!V)
        return luaL_error(L, "invalid FLinearColor");

    UnLuaNova::TFieldSetter4<float>::Set(L, NumParams, &V->R);
    return 0;
}

static const luaL_Reg FLinearColorLib[] =
{
    {"Set", FLinearColor_Set},
    {"Add", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TAdd<float>, true>::Calculate},
    {"Sub", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TSub<float>, true>::Calculate},
    {"Mul", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TMul<float>, true>::Calculate},
    {"Div", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TDiv<float>, true>::Calculate},
    {"__add", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TAdd<float>>::Calculate},
    {"__sub", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TSub<float>>::Calculate},
    {"__mul", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TMul<float>>::Calculate},
    {"__div", UnLuaNova::TMathCalculation<FLinearColor, UnLuaNova::TDiv<float>>::Calculate},
    {"__tostring", UnLuaNova::TMathUtils<FLinearColor>::ToString},
    {"__call", FLinearColor_New},
    {nullptr, nullptr}
};

BEGIN_EXPORT_REFLECTED_CLASS(FLinearColor)
    ADD_FUNCTION(ToFColor)
    ADD_NAMED_FUNCTION("Clamp", GetClamped)
    ADD_LIB(FLinearColorLib)
    ADD_STATIC_PROPERTY(Black)
    ADD_STATIC_PROPERTY(White)
END_EXPORT_CLASS()

IMPLEMENT_EXPORTED_CLASS(FLinearColor)
