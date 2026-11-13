// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/mit-license.php
//
// Unless required by applicable law or agreed to in writing, software distributed under
// the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the specific language governing
// permissions and limitations under the License.

#include "UnLuaNovaNetTypeSpec.h"

#include "LuaBackend.h"
#include "UObject/UnrealType.h"

namespace UnLuaNova
{
    namespace
    {
        constexpr int32 MaxSpecDepth = 4;
        constexpr int32 MaxValueDepth = 32;

        void SplitFirst(const FString& Text, FString& OutHead, FString& OutTail)
        {
            int32 Colon = INDEX_NONE;
            if (Text.FindChar(TEXT(':'), Colon))
            {
                OutHead = Text.Left(Colon);
                OutTail = Text.Mid(Colon + 1);
            }
            else
            {
                OutHead = Text;
                OutTail.Empty();
            }
        }

        /** Canonical form has to be re-parsable: it is what gets stored on the declaration. */
        const TCHAR* BuiltinName(ELuaNetParamType Type)
        {
            switch (Type)
            {
            case ELuaNetParamType::Bool:      return TEXT("Bool");
            case ELuaNetParamType::Int:       return TEXT("Int");
            case ELuaNetParamType::Int64:     return TEXT("Int64");
            case ELuaNetParamType::Float:     return TEXT("Float");
            case ELuaNetParamType::Double:    return TEXT("Double");
            case ELuaNetParamType::String:    return TEXT("String");
            case ELuaNetParamType::Name:      return TEXT("Name");
            case ELuaNetParamType::Vector:    return TEXT("Vector");
            case ELuaNetParamType::Rotator:   return TEXT("Rotator");
            case ELuaNetParamType::Transform: return TEXT("Transform");
            case ELuaNetParamType::Object:    return TEXT("Object");
            default:                          return TEXT("Generic");
            }
        }

        UScriptStruct* ResolveStruct(const FString& Path)
        {
            if (Path.IsEmpty() || !Path.StartsWith(TEXT("/")))
                return nullptr;
            if (UScriptStruct* Found = FindObject<UScriptStruct>(nullptr, *Path))
                return Found;
            return LoadObject<UScriptStruct>(nullptr, *Path);
        }

        bool ParseSpec(const FString& Raw, FLuaNetTypeSpec& Out, FString& Error, int32 Depth)
        {
            if (Depth > MaxSpecDepth)
            {
                Error = TEXT("Type spec is nested too deeply.");
                return false;
            }

            FString Text = Raw;
            Text.TrimStartAndEndInline();

            // The builtin names keep working unchanged: a declaration that says "Int" is still
            // an Int bucket, not a generic payload.
            ELuaNetParamType Builtin = ELuaNetParamType::Int;
            if (ParseLuaNetParamType(Text, Builtin))
            {
                Out.Kind = ELuaNetSpecKind::Builtin;
                Out.Builtin = Builtin;
                Out.Canonical = BuiltinName(Builtin);
                return true;
            }

            FString Head, Tail;
            SplitFirst(Text, Head, Tail);
            const FString Keyword = Head.ToLower();

            if (Keyword == TEXT("struct") || Text.StartsWith(TEXT("/")))
            {
                const FString Path = Keyword == TEXT("struct") ? Tail : Text;
                if (!ResolveStruct(Path))
                {
                    Error = FString::Printf(TEXT("UScriptStruct '%s' could not be resolved."), *Path);
                    return false;
                }
                Out.Kind = ELuaNetSpecKind::Struct;
                Out.StructPath = Path;
                Out.Canonical = TEXT("Struct:") + Path;
                return true;
            }

            const bool bArray = Keyword == TEXT("array");
            const bool bSet = Keyword == TEXT("set");
            const bool bMap = Keyword == TEXT("map");
            if (!bArray && !bSet && !bMap)
            {
                Error = FString::Printf(TEXT("Unknown network type '%s'."), *Text);
                return false;
            }
            if (Tail.IsEmpty())
            {
                Error = TEXT("Container type spec is missing its element type.");
                return false;
            }

            Out.Kind = bArray ? ELuaNetSpecKind::Array : bSet ? ELuaNetSpecKind::Set : ELuaNetSpecKind::Map;

            if (bMap)
            {
                FString KeyText, ValueText;
                SplitFirst(Tail, KeyText, ValueText);
                if (ValueText.IsEmpty())
                {
                    Error = TEXT("Map type spec requires Key:Value.");
                    return false;
                }
                FLuaNetTypeSpec KeySpec, ValueSpec;
                if (!ParseSpec(KeyText, KeySpec, Error, Depth + 1) || !ParseSpec(ValueText, ValueSpec, Error, Depth + 1))
                    return false;
                // A text payload has no NetGUID and no stable hash for composite keys.
                if (KeySpec.Kind != ELuaNetSpecKind::Builtin
                    || KeySpec.Builtin == ELuaNetParamType::Object
                    || KeySpec.Builtin == ELuaNetParamType::Vector
                    || KeySpec.Builtin == ELuaNetParamType::Rotator
                    || KeySpec.Builtin == ELuaNetParamType::Transform)
                {
                    Error = TEXT("Map keys must be a scalar builtin type.");
                    return false;
                }
                Out.Children.Add(MoveTemp(KeySpec));
                Out.Children.Add(MoveTemp(ValueSpec));
                Out.Canonical = TEXT("Map:") + Out.Children[0].Canonical + TEXT(":") + Out.Children[1].Canonical;
                return true;
            }

            FLuaNetTypeSpec Element;
            if (!ParseSpec(Tail, Element, Error, Depth + 1))
                return false;
            Out.Children.Add(MoveTemp(Element));
            Out.Canonical = (bArray ? TEXT("Array:") : TEXT("Set:")) + Out.Children[0].Canonical;
            return true;
        }

        bool ValidateSpec(const FLuaNetTypeSpec& Spec, FString& Error, int32 Depth)
        {
            if (Depth > MaxSpecDepth || !Spec.IsValid())
            {
                Error = TEXT("Invalid or over-nested network type spec.");
                return false;
            }
            switch (Spec.Kind)
            {
            case ELuaNetSpecKind::Builtin:
                return Spec.Builtin != ELuaNetParamType::Object;
            case ELuaNetSpecKind::Struct:
                if (ResolveStruct(Spec.StructPath))
                    return true;
                Error = FString::Printf(TEXT("UScriptStruct '%s' could not be resolved."), *Spec.StructPath);
                return false;
            case ELuaNetSpecKind::Array:
            case ELuaNetSpecKind::Set:
                return Spec.Children.Num() == 1 && ValidateSpec(Spec.Children[0], Error, Depth + 1);
            case ELuaNetSpecKind::Map:
                return Spec.Children.Num() == 2
                    && Spec.Children[0].Kind == ELuaNetSpecKind::Builtin
                    && ValidateSpec(Spec.Children[1], Error, Depth + 1);
            default:
                return false;
            }
        }

        // --- RPC parameter properties --------------------------------------

        FProperty* BuildBuiltin(FFieldVariant Outer, FName Name, ELuaNetParamType Type)
        {
            switch (Type)
            {
            case ELuaNetParamType::Bool:
            {
                auto* Property = new FBoolProperty(Outer, Name);
                Property->SetBoolSize(sizeof(bool), true);
                return Property;
            }
            case ELuaNetParamType::Int:       return new FIntProperty(Outer, Name);
            case ELuaNetParamType::Int64:     return new FInt64Property(Outer, Name);
            case ELuaNetParamType::Float:     return new FFloatProperty(Outer, Name);
            case ELuaNetParamType::Double:    return new FDoubleProperty(Outer, Name);
            case ELuaNetParamType::String:    return new FStrProperty(Outer, Name);
            case ELuaNetParamType::Name:      return new FNameProperty(Outer, Name);
            case ELuaNetParamType::Vector:    return nullptr; // handled as a Struct below
            case ELuaNetParamType::Rotator:   return nullptr;
            case ELuaNetParamType::Transform: return nullptr;
            case ELuaNetParamType::Object:
            {
                auto* Property = new FObjectProperty(Outer, Name);
                Property->SetPropertyClass(UObject::StaticClass());
                return Property;
            }
            }
            return nullptr;
        }

        FProperty* BuildStruct(FFieldVariant Outer, FName Name, const TCHAR* Path)
        {
            UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, Path);
            if (!Struct)
                return nullptr;
            auto* Property = new FStructProperty(Outer, Name);
            Property->Struct = Struct;
            return Property;
        }

        FProperty* BuildProperty(FFieldVariant Outer, FName Name, const FLuaNetTypeSpec& Spec)
        {
            if (Spec.Kind == ELuaNetSpecKind::Builtin)
            {
                switch (Spec.Builtin)
                {
                case ELuaNetParamType::Vector:    return BuildStruct(Outer, Name, TEXT("/Script/CoreUObject.Vector"));
                case ELuaNetParamType::Rotator:   return BuildStruct(Outer, Name, TEXT("/Script/CoreUObject.Rotator"));
                case ELuaNetParamType::Transform: return BuildStruct(Outer, Name, TEXT("/Script/CoreUObject.Transform"));
                default:                          return BuildBuiltin(Outer, Name, Spec.Builtin);
                }
            }
            if (Spec.Kind == ELuaNetSpecKind::Struct)
                return BuildStruct(Outer, Name, *Spec.StructPath);

            if (Spec.Kind == ELuaNetSpecKind::Array)
            {
                auto* Property = new FArrayProperty(Outer, Name);
                FProperty* Inner = BuildProperty(FFieldVariant(Property), NAME_None, Spec.Children[0]);
                if (!Inner)
                {
                    delete Property;
                    return nullptr;
                }
                Property->AddCppProperty(Inner);
                return Property;
            }
            if (Spec.Kind == ELuaNetSpecKind::Set)
            {
                auto* Property = new FSetProperty(Outer, Name);
                FProperty* Inner = BuildProperty(FFieldVariant(Property), NAME_None, Spec.Children[0]);
                if (!Inner)
                {
                    delete Property;
                    return nullptr;
                }
                Inner->SetPropertyFlags(CPF_HasGetValueTypeHash);
                Property->AddCppProperty(Inner);
                return Property;
            }
            if (Spec.Kind == ELuaNetSpecKind::Map)
            {
                auto* Property = new FMapProperty(Outer, Name);
                FProperty* Key = BuildProperty(FFieldVariant(Property), NAME_None, Spec.Children[0]);
                FProperty* Value = BuildProperty(FFieldVariant(Property), NAME_None, Spec.Children[1]);
                if (!Key || !Value)
                {
                    delete Key;
                    delete Value;
                    delete Property;
                    return nullptr;
                }
                Key->SetPropertyFlags(CPF_HasGetValueTypeHash);
                Property->AddCppProperty(Key);
                Property->AddCppProperty(Value);
                return Property;
            }
            return nullptr;
        }

        // --- canonical wire text -------------------------------------------
        //
        // Self-describing tags, every primitive terminated by ';'. Strings carry their UTF-8
        // bytes in hex so NUL bytes and non-ASCII text cannot corrupt the framing. Object keys
        // are sorted before writing; Lua table iteration order is not a wire protocol.

        void AppendHexString(FString& Out, const char* Bytes, size_t Length)
        {
            static const TCHAR Hex[] = TEXT("0123456789ABCDEF");
            Out += FString::Printf(TEXT("s%d:"), int32(Length));
            for (size_t Index = 0; Index < Length; ++Index)
            {
                const uint8 Byte = static_cast<uint8>(Bytes[Index]);
                Out.AppendChar(Hex[Byte >> 4]);
                Out.AppendChar(Hex[Byte & 0xF]);
            }
            Out.AppendChar(TEXT(';'));
        }

        bool DenseArrayLength(lua_State* L, int32 Idx, int32& OutCount)
        {
            const int32 Abs = lua_absindex(L, Idx);
            const size_t Raw = lua_objlen(L, Abs);
            if (Raw > MAX_int32)
                return false;
            OutCount = int32(Raw);
            int32 Seen = 0;
            lua_pushnil(L);
            while (lua_next(L, Abs) != 0)
            {
                // Key at -2, value at -1. Only 1..N integer keys make a dense array.
                const bool bInteger = lua_type(L, -2) == LUA_TNUMBER;
                const lua_Number Key = bInteger ? lua_tonumber(L, -2) : 0.0;
                const bool bInRange = bInteger && Key >= 1.0 && Key <= lua_Number(OutCount)
                    && Key == lua_Number(lua_tointeger(L, -2));
                lua_pop(L, 1);
                if (!bInRange)
                {
                    lua_pop(L, 1); // the key
                    return false;
                }
                ++Seen;
            }
            return Seen == OutCount;
        }

        bool EncodeTagged(lua_State* L, int32 Idx, FString& Out, FString& Error, int32 Depth)
        {
            if (Depth > MaxValueDepth)
            {
                Error = TEXT("Network value is nested too deeply.");
                return false;
            }
            const int32 Abs = lua_absindex(L, Idx);
            switch (lua_type(L, Abs))
            {
            case LUA_TNIL:
                Out += TEXT("x;");
                return true;
            case LUA_TBOOLEAN:
                Out += lua_toboolean(L, Abs) ? TEXT("b1;") : TEXT("b0;");
                return true;
            case LUA_TNUMBER:
            {
                const lua_Number Number = lua_tonumber(L, Abs);
                const lua_Integer Integer = lua_tointeger(L, Abs);
                if (lua_Number(Integer) == Number)
                    Out += FString::Printf(TEXT("i%lld;"), static_cast<long long>(Integer));
                else
                    Out += FString::Printf(TEXT("n%.17g;"), double(Number));
                return true;
            }
            case LUA_TSTRING:
            {
                size_t Length = 0;
                const char* Bytes = lua_tolstring(L, Abs, &Length);
                AppendHexString(Out, Bytes, Length);
                return true;
            }
            case LUA_TTABLE:
            {
                int32 Count = 0;
                if (DenseArrayLength(L, Abs, Count))
                {
                    Out += FString::Printf(TEXT("a%d:"), Count);
                    for (int32 Index = 1; Index <= Count; ++Index)
                    {
                        lua_rawgeti(L, Abs, Index);
                        const bool bOk = EncodeTagged(L, -1, Out, Error, Depth + 1);
                        lua_pop(L, 1);
                        if (!bOk)
                            return false;
                    }
                    return true;
                }

                TArray<TPair<FString, FString>> Pairs;
                lua_pushnil(L);
                while (lua_next(L, Abs) != 0)
                {
                    FString KeyText, ValueText;
                    const int32 KeyType = lua_type(L, -2);
                    if (KeyType == LUA_TSTRING)
                    {
                        size_t Length = 0;
                        const char* Bytes = lua_tolstring(L, -2, &Length);
                        AppendHexString(KeyText, Bytes, Length);
                    }
                    else if (KeyType == LUA_TNUMBER)
                    {
                        const lua_Integer Integer = lua_tointeger(L, -2);
                        if (lua_Number(Integer) != lua_tonumber(L, -2))
                        {
                            lua_pop(L, 2);
                            Error = TEXT("Non-integer table keys cannot be sent in a generic payload.");
                            return false;
                        }
                        KeyText = FString::Printf(TEXT("i%lld;"), static_cast<long long>(Integer));
                    }
                    else
                    {
                        lua_pop(L, 2);
                        Error = TEXT("Table keys must be strings or integers.");
                        return false;
                    }

                    const bool bOk = EncodeTagged(L, -1, ValueText, Error, Depth + 1);
                    lua_pop(L, 1); // the value; the key stays for lua_next
                    if (!bOk)
                    {
                        lua_pop(L, 1); // abort iteration: pop the key
                        return false;
                    }
                    Pairs.Emplace(MoveTemp(KeyText), MoveTemp(ValueText));
                }
                Pairs.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
                {
                    return A.Key < B.Key;
                });
                Out += FString::Printf(TEXT("o%d:"), Pairs.Num());
                for (const auto& Pair : Pairs)
                    Out += Pair.Key + Pair.Value;
                return true;
            }
            default:
                Error = TEXT("Unsupported Lua value type in a generic network field.");
                return false;
            }
        }

        struct FTextCursor
        {
            const FString& Text;
            int32 Pos = 0;

            explicit FTextCursor(const FString& InText) : Text(InText) {}

            bool AtEnd() const { return Pos >= Text.Len(); }

            bool Peek(TCHAR& Out) const
            {
                if (AtEnd())
                    return false;
                Out = Text[Pos];
                return true;
            }

            bool Take(TCHAR& Out)
            {
                if (!Peek(Out))
                    return false;
                ++Pos;
                return true;
            }

            bool Expect(TCHAR Char)
            {
                if (AtEnd() || Text[Pos] != Char)
                    return false;
                ++Pos;
                return true;
            }

            bool ReadUntil(TCHAR Delimiter, FString& Out)
            {
                const int32 Start = Pos;
                while (!AtEnd() && Text[Pos] != Delimiter)
                    ++Pos;
                if (AtEnd())
                    return false;
                Out = Text.Mid(Start, Pos - Start);
                ++Pos;
                return true;
            }
        };

        int32 HexDigit(TCHAR Char)
        {
            if (Char >= TEXT('0') && Char <= TEXT('9')) return Char - TEXT('0');
            if (Char >= TEXT('A') && Char <= TEXT('F')) return Char - TEXT('A') + 10;
            if (Char >= TEXT('a') && Char <= TEXT('f')) return Char - TEXT('a') + 10;
            return -1;
        }

        bool DecodeString(FTextCursor& Cursor, FString& Error, TArray<ANSICHAR>& OutBytes)
        {
            FString CountText;
            if (!Cursor.ReadUntil(TEXT(':'), CountText) || !CountText.IsNumeric())
            {
                Error = TEXT("Malformed string length in generic payload.");
                return false;
            }
            const int32 Count = FCString::Atoi(*CountText);
            if (Count < 0 || Count > 16 * 1024 * 1024 || Cursor.Pos + Count * 2 > Cursor.Text.Len())
            {
                Error = TEXT("Generic payload string length is out of range.");
                return false;
            }
            OutBytes.SetNumUninitialized(Count);
            for (int32 Index = 0; Index < Count; ++Index)
            {
                TCHAR High, Low;
                if (!Cursor.Take(High) || !Cursor.Take(Low))
                {
                    Error = TEXT("Truncated string in generic payload.");
                    return false;
                }
                const int32 Hi = HexDigit(High), Lo = HexDigit(Low);
                if (Hi < 0 || Lo < 0)
                {
                    Error = TEXT("Invalid hex in generic payload.");
                    return false;
                }
                OutBytes[Index] = static_cast<ANSICHAR>((Hi << 4) | Lo);
            }
            if (!Cursor.Expect(TEXT(';')))
            {
                Error = TEXT("Unterminated string in generic payload.");
                return false;
            }
            return true;
        }

        bool DecodeTagged(lua_State* L, FTextCursor& Cursor, FString& Error, int32 Depth)
        {
            if (Depth > MaxValueDepth)
            {
                Error = TEXT("Network value is nested too deeply.");
                return false;
            }
            TCHAR Tag = 0;
            if (!Cursor.Take(Tag))
            {
                Error = TEXT("Truncated generic payload.");
                return false;
            }
            switch (Tag)
            {
            case TEXT('x'):
                if (!Cursor.Expect(TEXT(';'))) { Error = TEXT("Malformed nil in generic payload."); return false; }
                lua_pushnil(L);
                return true;
            case TEXT('b'):
            {
                FString Token;
                if (!Cursor.ReadUntil(TEXT(';'), Token) || (Token != TEXT("0") && Token != TEXT("1")))
                {
                    Error = TEXT("Malformed bool in generic payload.");
                    return false;
                }
                lua_pushboolean(L, Token == TEXT("1") ? 1 : 0);
                return true;
            }
            case TEXT('i'):
            {
                FString Token;
                if (!Cursor.ReadUntil(TEXT(';'), Token) || !Token.IsNumeric())
                {
                    Error = TEXT("Malformed integer in generic payload.");
                    return false;
                }
                lua_pushinteger(L, lua_Integer(FCString::Atoi64(*Token)));
                return true;
            }
            case TEXT('n'):
            {
                FString Token;
                if (!Cursor.ReadUntil(TEXT(';'), Token))
                {
                    Error = TEXT("Malformed number in generic payload.");
                    return false;
                }
                lua_pushnumber(L, lua_Number(FCString::Atod(*Token)));
                return true;
            }
            case TEXT('s'):
            {
                TArray<ANSICHAR> Bytes;
                if (!DecodeString(Cursor, Error, Bytes))
                    return false;
                lua_pushlstring(L, Bytes.GetData(), Bytes.Num());
                return true;
            }
            case TEXT('a'):
            {
                FString CountText;
                if (!Cursor.ReadUntil(TEXT(':'), CountText) || !CountText.IsNumeric())
                {
                    Error = TEXT("Malformed array in generic payload.");
                    return false;
                }
                const int32 Count = FCString::Atoi(*CountText);
                if (Count < 0 || Count > 1000000)
                {
                    Error = TEXT("Generic payload array length is out of range.");
                    return false;
                }
                lua_newtable(L);
                const int32 Table = lua_gettop(L);
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    if (!DecodeTagged(L, Cursor, Error, Depth + 1))
                    {
                        lua_pop(L, 1);
                        return false;
                    }
                    lua_rawseti(L, Table, Index + 1);
                }
                return true;
            }
            case TEXT('o'):
            {
                FString CountText;
                if (!Cursor.ReadUntil(TEXT(':'), CountText) || !CountText.IsNumeric())
                {
                    Error = TEXT("Malformed object in generic payload.");
                    return false;
                }
                const int32 Count = FCString::Atoi(*CountText);
                if (Count < 0 || Count > 1000000)
                {
                    Error = TEXT("Generic payload object length is out of range.");
                    return false;
                }
                lua_newtable(L);
                const int32 Table = lua_gettop(L);
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    TCHAR KeyTag = 0;
                    if (!Cursor.Take(KeyTag))
                    {
                        lua_pop(L, 1);
                        Error = TEXT("Truncated object key in generic payload.");
                        return false;
                    }
                    if (KeyTag == TEXT('s'))
                    {
                        TArray<ANSICHAR> Bytes;
                        if (!DecodeString(Cursor, Error, Bytes))
                        {
                            lua_pop(L, 1);
                            return false;
                        }
                        lua_pushlstring(L, Bytes.GetData(), Bytes.Num());
                    }
                    else if (KeyTag == TEXT('i'))
                    {
                        FString Token;
                        if (!Cursor.ReadUntil(TEXT(';'), Token) || !Token.IsNumeric())
                        {
                            lua_pop(L, 1);
                            Error = TEXT("Malformed object key in generic payload.");
                            return false;
                        }
                        lua_pushinteger(L, lua_Integer(FCString::Atoi64(*Token)));
                    }
                    else
                    {
                        lua_pop(L, 1);
                        Error = TEXT("Unsupported object key in generic payload.");
                        return false;
                    }
                    if (!DecodeTagged(L, Cursor, Error, Depth + 1))
                    {
                        lua_pop(L, 2);
                        return false;
                    }
                    lua_settable(L, Table);
                }
                return true;
            }
            default:
                Error = FString::Printf(TEXT("Unknown tag '%c' in generic payload."), Tag);
                return false;
            }
        }

        // --- spec-driven shape validation (encode AND decode) ---------------
        // Used by EncodeLuaNetValue before writing and by DecodeLuaNetValue after
        // reading. Keeping one implementation prevents the two sides from drifting
        // into different ideas of what a declared spec permits.

        bool CheckShape(lua_State* L, int32 Idx, const FLuaNetTypeSpec& Spec, FString& Error, int32 Depth)
        {
            if (Depth > MaxValueDepth)
            {
                Error = TEXT("Network value is nested too deeply.");
                return false;
            }
            const int32 Abs = lua_absindex(L, Idx);
            const int32 LuaType = lua_type(L, Abs);

            if (Spec.Kind == ELuaNetSpecKind::Builtin)
            {
                switch (Spec.Builtin)
                {
                case ELuaNetParamType::Bool:      return LuaType == LUA_TBOOLEAN || LuaType == LUA_TNUMBER;
                case ELuaNetParamType::Int:
                case ELuaNetParamType::Int64:
                case ELuaNetParamType::Float:
                case ELuaNetParamType::Double:     return LuaType == LUA_TNUMBER;
                case ELuaNetParamType::String:
                case ELuaNetParamType::Name:       return LuaType == LUA_TSTRING;
                default:                           return LuaType == LUA_TTABLE; // Vector/Rotator/Transform
                }
            }

            if (LuaType != LUA_TTABLE)
            {
                Error = TEXT("A generic struct/container field needs a Lua table.");
                return false;
            }

            if (Spec.Kind == ELuaNetSpecKind::Struct)
            {
                UScriptStruct* Struct = ResolveStruct(Spec.StructPath);
                if (!Struct)
                {
                    Error = TEXT("The declared UScriptStruct is no longer available.");
                    return false;
                }
                lua_pushnil(L);
                while (lua_next(L, Abs) != 0)
                {
                    const FString KeyName = lua_type(L, -2) == LUA_TSTRING
                        ? FString(UTF8_TO_TCHAR(lua_tostring(L, -2))) : FString();
                    if (KeyName.IsEmpty() || !Struct->FindPropertyByName(FName(*KeyName)))
                    {
                        Error = FString::Printf(TEXT("'%s' is not a field of UScriptStruct '%s'."),
                            *KeyName, *Spec.StructPath);
                        lua_pop(L, 2);
                        return false;
                    }
                    lua_pop(L, 1);
                }
                return true;
            }

            if (Spec.Kind == ELuaNetSpecKind::Array || Spec.Kind == ELuaNetSpecKind::Set)
            {
                int32 Count = 0;
                if (!DenseArrayLength(L, Abs, Count))
                {
                    Error = TEXT("An Array/Set field needs a dense Lua array.");
                    return false;
                }
                for (int32 Index = 1; Index <= Count; ++Index)
                {
                    lua_rawgeti(L, Abs, Index);
                    const bool bOk = CheckShape(L, -1, Spec.Children[0], Error, Depth + 1);
                    lua_pop(L, 1);
                    if (!bOk)
                        return false;
                }
                return true;
            }

            // Map: every key/value pair has to match the declared key/value specs.
            lua_pushnil(L);
            while (lua_next(L, Abs) != 0)
            {
                const int32 ValueIdx = lua_absindex(L, -1);
                const int32 KeyIdx = lua_absindex(L, -2);
                const int32 KeyLuaType = lua_type(L, KeyIdx);
                if (KeyLuaType != LUA_TSTRING && KeyLuaType != LUA_TNUMBER)
                {
                    lua_pop(L, 2);
                    Error = TEXT("Map keys must be strings or integers.");
                    return false;
                }
                const bool bKeyOk = CheckShape(L, KeyIdx, Spec.Children[0], Error, Depth + 1);
                const bool bValueOk = bKeyOk && CheckShape(L, ValueIdx, Spec.Children[1], Error, Depth + 1);
                lua_pop(L, 1);
                if (!bValueOk)
                {
                    lua_pop(L, 1);
                    return false;
                }
            }
            return true;
        }

        TCHAR ExpectedTag(const FLuaNetTypeSpec& Spec)
        {
            switch (Spec.Kind)
            {
            case ELuaNetSpecKind::Array:
            case ELuaNetSpecKind::Set:     return TEXT('a');
            case ELuaNetSpecKind::Struct:
            case ELuaNetSpecKind::Map:     return TEXT('o');
            case ELuaNetSpecKind::Builtin:
                switch (Spec.Builtin)
                {
                case ELuaNetParamType::Bool:      return TEXT('b');
                case ELuaNetParamType::String:
                case ELuaNetParamType::Name:      return TEXT('s');
                default:                          return 0; // i/n both valid for numbers, x for nil
                }
            default: return 0;
            }
        }
    }

    bool ParseLuaNetTypeSpec(const FString& Text, FLuaNetTypeSpec& Out, FString& OutError)
    {
        Out = FLuaNetTypeSpec();
        if (!ParseSpec(Text, Out, OutError, 0))
        {
            Out = FLuaNetTypeSpec();
            return false;
        }
        return ValidateSpec(Out, OutError, 0);
    }

    FProperty* BuildLuaNetTypeProperty(UFunction* Outer, FName Name, const FLuaNetTypeSpec& Spec)
    {
        if (!Outer || !Spec.IsValid())
            return nullptr;
        return BuildProperty(FFieldVariant(Outer), Name, Spec);
    }

    bool EncodeLuaNetValue(lua_State* L, int32 Idx, const FLuaNetTypeSpec& Spec, FString& OutText, FString& OutError)
    {
        if (!L || !Spec.IsValid())
        {
            OutError = TEXT("Invalid generic type spec.");
            return false;
        }
        if (!CheckShape(L, Idx, Spec, OutError, 0))
            return false;
        FString Text;
        Text.Reserve(256);
        if (!EncodeTagged(L, Idx, Text, OutError, 0))
            return false;
        OutText = MoveTemp(Text);
        return true;
    }

    bool DecodeLuaNetValue(lua_State* L, const FString& Text, const FLuaNetTypeSpec& Spec, FString& OutError)
    {
        if (!L || !Spec.IsValid())
        {
            OutError = TEXT("Invalid generic type spec.");
            return false;
        }
        FTextCursor Cursor(Text);
        TCHAR Tag = 0;
        const TCHAR Expected = ExpectedTag(Spec);
        if (!Cursor.Peek(Tag))
        {
            OutError = TEXT("Empty generic payload.");
            return false;
        }
        if (Expected != 0 && Tag != Expected && !(Expected == TEXT('b') && Tag == TEXT('i')))
        {
            OutError = TEXT("Generic payload shape does not match the declared type.");
            return false;
        }

        const int32 Top = lua_gettop(L);
        if (!DecodeTagged(L, Cursor, OutError, 0))
        {
            lua_settop(L, Top);
            return false;
        }
        if (!Cursor.AtEnd())
        {
            lua_settop(L, Top);
            OutError = TEXT("Trailing data in generic payload.");
            return false;
        }

        // B1 fix. The write side validates the Lua value against the declared type
        // spec, but the read side used to trust the self-describing payload: it only
        // compared the TOP-LEVEL tag. A peer declaring Array:Int could therefore
        // receive an object payload and have it pushed into Lua unchecked. Re-run
        // the same recursive shape check on the decoded value before handing it to
        // the script. DecodeTagged only produces nil/boolean/number/string/table,
        // which is exactly the value space CheckShape validates on the write side.
        // An explicit nil ('x') is a legitimate payload value and is allowed.
        if (lua_type(L, -1) != LUA_TNIL && !CheckShape(L, -1, Spec, OutError, 0))
        {
            const FString Reason = OutError;
            lua_settop(L, Top);
            OutError = FString::Printf(
                TEXT("Decoded generic payload violates the declared type spec: %s"), *Reason);
            return false;
        }
        return true;
    }
}
