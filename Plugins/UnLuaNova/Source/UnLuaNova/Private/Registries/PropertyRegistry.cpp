#include "Misc/EngineVersionComparison.h"
#include "UnLuaNovaCompatibility.h"
#include "PropertyRegistry.h"
#include "Binding.h"
#include "ClassRegistry.h"
#include "EnumRegistry.h"
#include "LowLevel.h"
#include "LuaEnv.h"
#include "ReflectionUtils/PropertyDesc.h"

namespace UnLuaNova
{
    FPropertyRegistry::FPropertyRegistry(FLuaEnv* Env)
        : Env(Env)
    {
        PropertyCollector = FindFirstObject<UScriptStruct>(TEXT("PropertyCollector"));
        check(PropertyCollector);
    }

    void FPropertyRegistry::NotifyUObjectDeleted(UObject* Object)
    {
        FieldProperties.Remove(static_cast<UField*>(Object));
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::CreateTypeInterface(lua_State* L, int32 Index)
    {
        Index = LowLevel::AbsIndex(L, Index);

        TSharedPtr<ITypeInterface> TypeInterface;
        int32 Type = lua_type(L, Index);
        switch (Type)
        {
        case LUA_TBOOLEAN:
            TypeInterface = GetBoolProperty();
            break;
        case LUA_TNUMBER:
            // Not lua_isinteger(): on Luau that is only true for the separate LUA_TINTEGER
            // tag, so `UE.TArray(0)` - and every other literal a script writes - was
            // inferred as float, and TArray<int32> came back holding 1.0f bit patterns.
            //
            // int32, not int64: an integer literal means a UE int32, and callers (and the
            // specs) read the array back as TArray<int32>. Widening it to int64 during the
            // Luau port misaligned every element past the first.
            TypeInterface = UnLuaNovaIsIntegralNumber(L, Index) ? GetIntProperty() : GetFloatProperty();
            break;
        case LUA_TSTRING:
            TypeInterface = GetStringProperty();
            break;
        case LUA_TTABLE:
            {
                lua_pushstring(L, "__name");
                Type = lua_rawget(L, Index);
                if (Type == LUA_TSTRING)
                {
                    const char* Name = lua_tostring(L, -1);
                    auto ClassDesc = Env->GetClassRegistry()->Find(Name);
                    if (ClassDesc)
                    {
                        TypeInterface = GetFieldProperty(ClassDesc->AsStruct());
                    }
                    else
                    {
                        auto EnumDesc = Env->GetEnumRegistry()->Find(Name);
                        if (EnumDesc)
                            TypeInterface = GetFieldProperty(EnumDesc->GetEnum());
                        else
                            TypeInterface = FindTypeInterface(lua_tostring(L, -1));
                    }
                }
                lua_pop(L, 1);
            }
            break;
        case LUA_TUSERDATA:
            {
                // mt/nil
                lua_getmetatable(L, Index);
                if (lua_istable(L, -1))
                {
                    // mt,mt.__name/nil
                    lua_getfield(L, -1, "__name");
                    if (lua_isstring(L, -1))
                    {
                        const char* Name = lua_tostring(L, -1);
                        FClassDesc* ClassDesc = Env->GetClassRegistry()->Find(Name);
                        if (ClassDesc)
                            TypeInterface = GetFieldProperty(ClassDesc->AsStruct());
                    }
                    // mt
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            break;
        default:
            break;
        }

        return TypeInterface;
    }

    // UE 5.8 dropped EObjectFlags ObjectFlags from UE_PROPERTY_PARAMS_BASE_FIELDS (the
    // FProperty constructors taking InObjectFlags were deprecated in the same release),
    // so every UECodeGen_Private::F*PropertyParams below omits it from 5.8 on. Each
    // list is positional, so getting that wrong shifts every field after it.
    TSharedPtr<ITypeInterface> FPropertyRegistry::GetBoolProperty()
    {
        if (!BoolProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FBoolProperty(PropertyCollector, NAME_None, RF_Transient, 0, (EPropertyFlags)0, 0xFF, 1, true);
#else
            constexpr auto Params = UECodeGen_Private::FBoolPropertyParams
            {
                nullptr,
                nullptr,
                CPF_None,
                UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                sizeof(bool),
                sizeof(FPropertyCollector),
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FBoolProperty(PropertyCollector, Params);
#endif
            BoolProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return BoolProperty;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetIntProperty()
    {
        if (!IntProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FIntProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FIntPropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Int,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FIntProperty(PropertyCollector, Params);
#endif
            IntProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return IntProperty;
    }
    
    TSharedPtr<ITypeInterface> FPropertyRegistry::GetInt64Property()
    {
        if (!Int64Property)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FInt64Property(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FInt64PropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Int64,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FInt64Property(PropertyCollector, Params);
#endif
            Int64Property = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return Int64Property;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetFloatProperty()
    {
        if (!FloatProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FFloatProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FFloatPropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Float,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FFloatProperty(PropertyCollector, Params);
#endif
            FloatProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return FloatProperty;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetStringProperty()
    {
        if (!StringProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FStrProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FStrPropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Str,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FStrProperty(PropertyCollector, Params);
#endif
            StringProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return StringProperty;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetNameProperty()
    {
        if (!NameProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FNameProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FNamePropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Name,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto Property = new FNameProperty(PropertyCollector, Params);
#endif
            NameProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return NameProperty;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetTextProperty()
    {
        if (!TextProperty)
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            const auto Property = new FTextProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash);
#else
            constexpr auto Params = UECodeGen_Private::FTextPropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Text,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
#if UE_VERSION_NEWER_THAN(5, 2, 1)
            const auto Property = new FTextProperty(PropertyCollector, "", RF_Transient);
#else
            const auto Property = new FTextProperty(PropertyCollector, Params);
#endif
#endif
            TextProperty = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        }
        return TextProperty;
    }

    TSharedPtr<ITypeInterface> FPropertyRegistry::GetFieldProperty(UField* Field)
    {
        if (const auto Exists = FieldProperties.Find(Field))
            return *Exists;

        FProperty* Property;
        if (const auto Class = Cast<UClass>(Field))
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            Property = new FObjectProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash, Class);
#else
            constexpr auto Params = UECodeGen_Private::FObjectPropertyParams
            {
                nullptr,
                nullptr,
                CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Object,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto ObjectProperty = new FObjectProperty(PropertyCollector, Params);
            ObjectProperty->PropertyClass = Class;
            Property = ObjectProperty;
#endif
        }
        else if (const auto ScriptStruct = Cast<UScriptStruct>(Field))
        {
#if UE_VERSION_OLDER_THAN(5, 1, 0)
            Property = new FStructProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash, ScriptStruct);
#else
            const auto Params = UECodeGen_Private::FStructPropertyParams
            {
                nullptr,
                nullptr,
                ScriptStruct->GetCppStructOps()
                    ? ScriptStruct->GetCppStructOps()->GetComputedPropertyFlags() | CPF_HasGetValueTypeHash
                    : CPF_HasGetValueTypeHash,
                UECodeGen_Private::EPropertyGenFlags::Struct,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
                RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
                1,
#endif
                nullptr,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                1,
#endif
                0,
                nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
                METADATA_PARAMS(0, nullptr)
#else
                METADATA_PARAMS(nullptr, 0)
#endif
            };
            const auto StructProperty = new FStructProperty(PropertyCollector, Params);
            StructProperty->Struct = ScriptStruct;
#if UE_VERSION_OLDER_THAN(5, 3, 0)
            StructProperty->ElementSize = ScriptStruct->PropertiesSize;
#else
            StructProperty->SetElementSize(ScriptStruct->PropertiesSize);
#endif
            Property = StructProperty;
#endif
        }
        else if (const auto Enum = Cast<UEnum>(Field))
        {
#if UE_VERSION_OLDER_THAN(5, 5, 0)
        	const auto EnumProperty = new FEnumProperty(PropertyCollector, NAME_None, RF_Transient, 0, CPF_HasGetValueTypeHash, Enum);
#else
        	const auto Params = UECodeGen_Private::FEnumPropertyParams{
        		nullptr,
				nullptr,
				CPF_HasGetValueTypeHash,
				UECodeGen_Private::EPropertyGenFlags::Enum,
#if UE_VERSION_OLDER_THAN(5, 8, 0)
				RF_Transient,
#endif
#if UE_VERSION_OLDER_THAN(5, 3, 0)
				1,
#endif
				nullptr,
				nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
				1,
#endif
				0,
				nullptr,
#if UE_VERSION_NEWER_THAN(5, 2, 1)
				METADATA_PARAMS(0, nullptr)
#else
				METADATA_PARAMS(nullptr, 0)
#endif
			};
        	const auto EnumProperty = new FEnumProperty(PropertyCollector, Params);
#endif
            const auto UnderlyingProperty = new FByteProperty(EnumProperty, TEXT("UnderlyingType"), RF_Transient);
            Property = EnumProperty;
            Property->AddCppProperty(UnderlyingProperty);
#if UE_VERSION_OLDER_THAN(5, 3, 0)
            Property->ElementSize = UnderlyingProperty->ElementSize;
#else
            Property->SetElementSize(UnderlyingProperty->GetElementSize());
#endif
            Property->PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor;
        }
        else
        {
            Property = nullptr;
        }

        const auto Ret = TSharedPtr<ITypeInterface>(FPropertyDesc::Create(Property));
        FieldProperties.Add(Field, Ret);
        return Ret;
    }
}
