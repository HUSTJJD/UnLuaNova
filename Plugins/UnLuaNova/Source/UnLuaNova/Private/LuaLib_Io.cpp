// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
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

#include "LuaLib_Io.h"

#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
// Misc/FileHelper.h, not FileHelpers.h: the latter is editor-only, and this is a runtime
// module that has to build for a packaged game.
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnLuaNovaBase.h"

// Field names of the file object. The object is a Lua table rather than a userdata so
// that nothing in it needs a destructor: Luau has no __gc (see luau.org/sandbox), so a
// userdata holding an FString or a TArray would leak whatever that buffer is holding the
// moment a script dropped the handle without closing it. Strings and integers in a table
// are collected with the table itself.
static const char IoDataField[] = "__data";
static const char IoPosField[] = "__pos";
static const char IoPathField[] = "__path";
static const char IoModeField[] = "__mode";
static const char IoOpenField[] = "__open";

namespace
{
    const char* const IoFileMeta = "UnLuaNovaIoFile";

    // Mode bits, kept in a plain integer field so the Lua side can inspect them too.
    // Prefixed: the io.write / io.read C functions below share the file-global namespace,
    // and an enumerator named IoWrite would make the function name a redefinition.
    enum EIoMode : uint8
    {
        IoModeRead = 1 << 0,
        IoModeWrite = 1 << 1,
        IoModeAppend = 1 << 2,
    };

    bool IoEnabled()
    {
        static auto CVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.io"),
            0,
            TEXT("Install an io library rooted at lua.ioroot. 0=off (default), 1=on. "
                 "Only installed on backends that have no io of their own (Luau). Read once per environment."),
            ECVF_Default);

        return CVar->GetInt() != 0;
    }

    /**
     * Resolve a script-supplied path under the configured root.
     *
     * Returns false - and pushes an error - when the result would land outside the root.
     * That is the whole point of the switch: the root is the boundary, and both absolute
     * paths and ".." have to be turned back rather than merely discouraged.
     */
    bool ResolvePath(lua_State* L, const FString& InPath, FString& OutFull)
    {
        static auto RootCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.ioroot"),
            TEXT(""),
            TEXT("Directory that lua.io is confined to. Empty means the project directory. "
                 "Paths outside it are rejected."),
            ECVF_Default);

        FString Root(RootCVar->GetString());
        if (Root.IsEmpty())
            Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        else
            Root = FPaths::ConvertRelativePathToFull(Root);

        FPaths::NormalizeFilename(Root);
        Root = FPaths::ConvertRelativePathToFull(Root);
        if (!Root.EndsWith(TEXT("/")) && !Root.EndsWith(TEXT("\\")))
            Root += TEXT("/");

        FString Relative = InPath;
        FPaths::NormalizeFilename(Relative);
        if (Relative.IsEmpty() || !FPaths::IsRelative(Relative) || Relative.Contains(TEXT(":")))
        {
            lua_pushliteral(L, "io: a relative path without a drive or stream name is required");
            return false;
        }

        // FPaths::CollapseRelativeDirectories removes "." and ".." without touching the
        // filesystem, so "a/../../b" is resolved - and then shown to leave the root -
        // before anything is opened.
        FString Combined = Root + Relative;
        if (!FPaths::CollapseRelativeDirectories(Combined))
        {
            lua_pushliteral(L, "io: invalid relative path");
            return false;
        }
        Combined = FPaths::ConvertRelativePathToFull(Combined);

        if (!Combined.StartsWith(Root, ESearchCase::CaseSensitive))
        {
            lua_pushfstring(L, "io: '%s' is outside lua.ioroot ('%s')", TCHAR_TO_UTF8(*InPath), TCHAR_TO_UTF8(*Root));
            return false;
        }

        OutFull = MoveTemp(Combined);
        return true;
    }

    /** Push the file object's mode field, or 0 when the shape is not ours. */
    int32 GetModeField(lua_State* L, int32 Idx)
    {
        lua_getfield(L, Idx, IoModeField);
        const int32 Mode = int32(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return Mode;
    }

    /** Check that the argument is an open file object of ours. */
    void CheckOpen(lua_State* L, int32 Idx)
    {
        lua_getfield(L, Idx, IoOpenField);
        const bool bOpen = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        if (!bOpen)
            luaL_error(L, "io: attempt to use a closed file");

        lua_getfield(L, Idx, IoPosField);
        const bool bValidPosition = lua_type(L, -1) == LUA_TNUMBER
            && lua_tonumber(L, -1) >= 0 && lua_tonumber(L, -1) <= MAX_int32;
        lua_pop(L, 1);
        if (!bValidPosition)
            luaL_error(L, "io: invalid file position");
    }

    int FileRead(lua_State* L);
    int FileWrite(lua_State* L);
    int FileLines(lua_State* L);
    int FileSeek(lua_State* L);
    int FileFlush(lua_State* L);
    int FileClose(lua_State* L);
    int FileToString(lua_State* L);

    int LinesIterator(lua_State* L)
    {
        CheckOpen(L, 1);
        lua_getfield(L, 1, IoDataField);
        size_t Len = 0;
        const char* Data = lua_tolstring(L, -1, &Len);
        lua_pop(L, 1);

        lua_getfield(L, 1, IoPosField);
        int64 Pos = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);

        if (!Data || Pos >= int64(Len))
            return 0; // no more lines; the iterator returning nothing ends the loop

        const char* Start = Data + Pos;
        const char* Newline = static_cast<const char*>(memchr(Start, '\n', size_t(int64(Len) - Pos)));

        size_t LineLen = Newline ? size_t(Newline - Start) : size_t(int64(Len) - Pos);
        // Strip a trailing \r so CRLF files do not hand scripts a stray byte.
        if (LineLen > 0 && Start[LineLen - 1] == '\r')
            --LineLen;

        lua_pushlstring(L, Start, LineLen);

        const int64 Next = Newline ? int64(Newline - Data) + 1 : int64(Len);
        lua_pushinteger(L, lua_Integer(Next));
        lua_setfield(L, 1, IoPosField);

        return 1;
    }

    /** Slice one line out of the buffer, advancing Pos past it. */
    bool TakeLine(const char* Data, size_t Len, int64& Pos, const char*& OutStart, size_t& OutLen)
    {
        if (!Data || Pos >= int64(Len))
            return false;

        const char* Start = Data + Pos;
        const char* Newline = static_cast<const char*>(memchr(Start, '\n', size_t(int64(Len) - Pos)));
        size_t LineLen = Newline ? size_t(Newline - Start) : size_t(int64(Len) - Pos);

        // CRLF files would otherwise hand the script a stray \r on every line.
        if (LineLen > 0 && Start[LineLen - 1] == '\r')
            --LineLen;

        OutStart = Start;
        OutLen = LineLen;
        Pos = Newline ? int64(Newline - Data) + 1 : int64(Len);
        return true;
    }

    int FileRead(lua_State* L)
    {
        CheckOpen(L, 1);

        // Taken before anything is pushed: from here on the stack holds the buffer and
        // the results, and there is no way to tell them apart by index alone.
        const int NumArgs = lua_gettop(L);

        lua_getfield(L, 1, IoDataField);
        size_t Len = 0;
        const char* Data = lua_tolstring(L, -1, &Len);
        const int DataIdx = lua_gettop(L);

        lua_getfield(L, 1, IoPosField);
        int64 Pos = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);

        int Results = 0;

        if (NumArgs == 1)
        {
            // No format means "the next line", which is what Lua does.
            const char* Start = nullptr;
            size_t LineLen = 0;
            if (TakeLine(Data, Len, Pos, Start, LineLen))
                lua_pushlstring(L, Start, LineLen);
            else
                lua_pushnil(L);
            ++Results;
        }
        else
        {
        for (int i = 2; i <= NumArgs; ++i)
        {
            if (lua_type(L, i) == LUA_TNUMBER)
            {
                const int64 Count = int64(luaL_checkinteger(L, i));
                if (Count < 0)
                    return luaL_error(L, "io: read count must be non-negative");
                if (Count == 0)
                {
                    // Reading zero bytes is the documented end-of-file test: an empty
                    // string when there is more to read, nil at the end.
                    if (!Data || Pos >= int64(Len))
                    {
                        lua_pushnil(L);
                        ++Results;
                    }
                    else
                    {
                        lua_pushliteral(L, "");
                        ++Results;
                    }
                    continue;
                }

                if (!Data || Pos >= int64(Len))
                {
                    lua_pushnil(L);
                    ++Results;
                    continue;
                }

                const int64 Available = int64(Len) - Pos;
                const int64 Take = FMath::Min(Count, Available);
                lua_pushlstring(L, Data + Pos, size_t(Take));
                Pos += Take;
                ++Results;
                continue;
            }

            const char* Format = lua_tostring(L, i);
            if (!Format)
                return luaL_error(L, "io: invalid read format");
            if (Format[0] == '*')
                ++Format;
            if (Format[0] == '\0' || Format[1] != '\0')
                return luaL_error(L, "io: invalid read format");

            if (Format[0] == 'a')
            {
                if (!Data || Pos >= int64(Len))
                    lua_pushliteral(L, "");
                else
                    lua_pushlstring(L, Data + Pos, size_t(int64(Len) - Pos));
                Pos = int64(Len);
                ++Results;
                continue;
            }

            if (Format[0] == 'l' || Format[0] == 'L')
            {
                if (!Data || Pos >= int64(Len))
                {
                    lua_pushnil(L);
                    ++Results;
                    continue;
                }

                const char* Start = Data + Pos;
                const char* Newline = static_cast<const char*>(memchr(Start, '\n', size_t(int64(Len) - Pos)));
                size_t LineLen = Newline ? size_t(Newline - Start) : size_t(int64(Len) - Pos);

                // "L" keeps the newline, "l" does not - that is the only difference.
                if (Format[0] == 'l' && LineLen > 0 && Start[LineLen - 1] == '\r')
                    --LineLen;
                if (Format[0] == 'L' && Newline)
                    ++LineLen;

                lua_pushlstring(L, Start, LineLen);
                Pos = Newline ? int64(Newline - Data) + 1 : int64(Len);
                ++Results;
                continue;
            }

            if (Format[0] == 'n')
            {
                // Skip whitespace, then let the number parser decide where it stops.
                while (Pos < int64(Len) && isspace(static_cast<unsigned char>(Data[Pos])))
                    ++Pos;

                if (Pos >= int64(Len))
                {
                    lua_pushnil(L);
                    ++Results;
                    continue;
                }

                const char* Start = Data + Pos;
                char* End = nullptr;
                const double Value = strtod(Start, &End);
                if (End == Start)
                {
                    lua_pushnil(L);
                    ++Results;
                    continue;
                }

                lua_pushnumber(L, Value);
                Pos += int64(End - Start);
                ++Results;
                continue;
            }

            return luaL_error(L, "io: invalid read format");
        }
        }

        lua_pushinteger(L, lua_Integer(Pos));
        lua_setfield(L, 1, IoPosField);

        lua_remove(L, DataIdx);
        return Results;
    }

    int FileWrite(lua_State* L)
    {
        CheckOpen(L, 1);

        if ((GetModeField(L, 1) & IoModeWrite) == 0)
            return luaL_error(L, "io: file was not opened for writing");

        const int NumArgs = lua_gettop(L);

        // The existing buffer plus every argument, concatenated in one go. No luaL_Buffer:
        // it keeps its own bookkeeping value on the stack, which makes every index below
        // it stale - and no C++ buffer either, since Luau cannot call a destructor.
        lua_getfield(L, 1, IoDataField);

        for (int i = 2; i <= NumArgs; ++i)
        {
            if (lua_type(L, i) == LUA_TNUMBER)
            {
                // "%.14g" is what tostring() uses, so 1 comes back as "1" and not "1.0".
                lua_pushfstring(L, "%.14g", double(lua_tonumber(L, i)));
                continue;
            }

            size_t Len = 0;
            const char* Text = luaL_checklstring(L, i, &Len);
            lua_pushlstring(L, Text, Len);
        }

        lua_concat(L, NumArgs);

        lua_pushvalue(L, -1);
        lua_setfield(L, 1, IoDataField);

        // Lua's file:write returns the file, so calls can be chained.
        lua_pushvalue(L, 1);
        return 1;
    }

    int FileSeek(lua_State* L)
    {
        CheckOpen(L, 1);

        const char* Whence = luaL_optstring(L, 2, "cur");
        const int64 Offset = int64(luaL_optinteger(L, 3, 0));

        lua_getfield(L, 1, IoDataField);
        size_t Len = 0;
        lua_tolstring(L, -1, &Len);
        lua_pop(L, 1);

        lua_getfield(L, 1, IoPosField);
        int64 Pos = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);

        int64 Base = Pos;
        if (Whence[0] == 's')
            Base = 0;
        else if (Whence[0] == 'e')
            Base = int64(Len);

        const int64 Next = FMath::Max(int64(0), Base + Offset);
        lua_pushinteger(L, lua_Integer(Next));
        lua_setfield(L, 1, IoPosField);

        lua_pushinteger(L, lua_Integer(Next));
        return 1;
    }

    int FileFlush(lua_State* L)
    {
        CheckOpen(L, 1);

        if ((GetModeField(L, 1) & IoModeWrite) == 0)
        {
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_getfield(L, 1, IoDataField);
        size_t Len = 0;
        const char* Data = lua_tolstring(L, -1, &Len);
        lua_pop(L, 1);

        lua_getfield(L, 1, IoPathField);
        const FString RelativePath = UTF8_TO_TCHAR(luaL_checkstring(L, -1));
        lua_pop(L, 1);
        FString Path;
        // File objects are tables: validate again because scripts can edit their fields.
        if (!ResolvePath(L, RelativePath, Path))
            return luaL_error(L, "%s", lua_tostring(L, -1));

        TArray<uint8> Bytes;
        Bytes.SetNumUninitialized(int32(Len));
        if (Len > 0 && Data)
            FMemory::Memcpy(Bytes.GetData(), Data, Len);

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        if (!FFileHelper::SaveArrayToFile(Bytes, *Path))
            return luaL_error(L, "io: failed to write '%s'", TCHAR_TO_UTF8(*Path));

        lua_pushboolean(L, 1);
        return 1;
    }

    int FileClose(lua_State* L)
    {
        lua_getfield(L, 1, IoOpenField);
        const bool bOpen = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        if (!bOpen)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        if ((GetModeField(L, 1) & IoModeWrite) != 0)
        {
            // Reuse flush rather than duplicating the write path.
            lua_pushcfunction(L, &FileFlush);
            lua_pushvalue(L, 1);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                const char* Message = lua_tostring(L, -1);
                lua_pop(L, 1);
                lua_pushboolean(L, 0);
                lua_pushstring(L, Message ? Message : "io: close failed");
                return 2;
            }
        }

        lua_pushboolean(L, 0);
        lua_setfield(L, 1, IoOpenField);

        lua_pushboolean(L, 1);
        return 1;
    }

    int FileToString(lua_State* L)
    {
        lua_getfield(L, 1, IoOpenField);
        const bool bOpen = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        if (!bOpen)
        {
            lua_pushliteral(L, "file (closed)");
            return 1;
        }

        lua_getfield(L, 1, IoPathField);
        const char* Path = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_pushfstring(L, "file (%s)", Path ? Path : "");
        return 1;
    }

    int FileLines(lua_State* L)
    {
        CheckOpen(L, 1);
        lua_pushcfunction(L, &LinesIterator);
        lua_pushvalue(L, 1);
        return 2;
    }

    int IoOpen(lua_State* L)
    {
        const char* PathArg = luaL_checkstring(L, 1);
        const char* ModeArg = luaL_optstring(L, 2, "r");

        uint8 Mode = 0;
        for (const char* C = ModeArg; *C; ++C)
        {
            if (*C == 'r')
                Mode |= IoModeRead;
            else if (*C == 'w')
                Mode |= IoModeWrite;
            else if (*C == 'a')
                Mode |= IoModeWrite | IoModeAppend;
            else if (*C == '+')
                Mode |= IoModeRead | IoModeWrite;
        }

        if (Mode == 0)
            Mode = IoModeRead;

        FString Full;
        if (!ResolvePath(L, UTF8_TO_TCHAR(PathArg), Full))
            return luaL_error(L, "%s", lua_tostring(L, -1));

        lua_newtable(L);
        const int File = lua_gettop(L);

        lua_pushstring(L, PathArg);
        lua_setfield(L, File, IoPathField);
        lua_pushinteger(L, Mode);
        lua_setfield(L, File, IoModeField);
        lua_pushboolean(L, 1);
        lua_setfield(L, File, IoOpenField);

        bool bExists = false;
        TArray<uint8> Existing;
        if ((Mode & IoModeRead) != 0 || (Mode & IoModeAppend) != 0)
            bExists = FFileHelper::LoadFileToArray(Existing, *Full);

        if (bExists && (Mode & IoModeAppend) != 0)
        {
            lua_pushlstring(L, reinterpret_cast<const char*>(Existing.GetData()), Existing.Num());
        }
        else if ((Mode & IoModeWrite) != 0 && (Mode & IoModeRead) == 0)
        {
            lua_pushliteral(L, "");
        }
        else if (bExists)
        {
            lua_pushlstring(L, reinterpret_cast<const char*>(Existing.GetData()), Existing.Num());
        }
        else
        {
            lua_pushnil(L);
            lua_pushfstring(L, "io: cannot open '%s'", PathArg);
            return 2;
        }

        lua_setfield(L, File, IoDataField);

        lua_pushinteger(L, (Mode & IoModeAppend) != 0 ? lua_Integer(Existing.Num()) : 0);
        lua_setfield(L, File, IoPosField);

        luaL_getmetatable(L, IoFileMeta);
        lua_setmetatable(L, File);

        return 1;
    }

    int IoLines(lua_State* L)
    {
        const int Top = lua_gettop(L);

        lua_pushcfunction(L, &IoOpen);
        lua_pushvalue(L, 1);
        lua_pushliteral(L, "r");
        lua_call(L, 2, 2); // file, or nil + error

        if (lua_isnil(L, -2))
            return luaL_error(L, "%s", lua_tostring(L, -1));

        lua_pop(L, 1); // the second result is always nil on success

        lua_pushcfunction(L, &LinesIterator);
        lua_insert(L, -2); // iterator below the file
        return 2;
    }

    int IoWrite(lua_State* L)
    {
        // There is no stdout to write to. The console log is the closest thing a script
        // can meaningfully target, and it is where a script author will look.
        FString Out;
        for (int i = 1; i <= lua_gettop(L); ++i)
        {
            if (lua_type(L, i) == LUA_TNUMBER)
                Out += FString::SanitizeFloat(lua_tonumber(L, i));
            else
                Out += UTF8_TO_TCHAR(luaL_checkstring(L, i));
        }

        UE_LOG(LogUnLuaNova, Log, TEXT("%s"), *Out);

        lua_pushboolean(L, 1);
        return 1;
    }

    int IoRead(lua_State* L)
    {
        return luaL_error(L, "io.read: there is no stdin in UnLuaNova; use io.open and read the file");
    }

    int IoClose(lua_State* L)
    {
        lua_pushcfunction(L, &FileClose);
        lua_pushvalue(L, 1);
        lua_call(L, 1, LUA_MULTRET);
        return lua_gettop(L) - 1;
    }

    int IoType(lua_State* L)
    {
        if (lua_getmetatable(L, 1) == 0)
        {
            lua_pushnil(L);
            return 1;
        }

        luaL_getmetatable(L, IoFileMeta);
        const bool bIsFile = lua_rawequal(L, -1, -2) != 0;
        lua_pop(L, 2);

        if (!bIsFile)
        {
            lua_pushnil(L);
            return 1;
        }

        lua_getfield(L, 1, IoOpenField);
        const bool bOpen = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        lua_pushstring(L, bOpen ? "file" : "closed file");
        return 1;
    }
}

namespace UnLuaNova
{
    void OpenIoLibrary(lua_State* L)
    {
        if (!L)
            return;

        // Plain Lua already has io, and its io is better than this one (it streams, it has
        // popen, it has no root). Never replace a backend's own library.
        lua_getglobal(L, "io");
        const bool bHasIo = lua_istable(L, -1) != 0;
        lua_pop(L, 1);
        if (bHasIo)
            return;

        if (!IoEnabled())
            return;

        // The metatable has to exist before io.open hands out a file object.
        luaL_newmetatable(L, IoFileMeta);

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index"); // methods live on the metatable

        lua_pushcfunction(L, &FileRead);
        lua_setfield(L, -2, "read");
        lua_pushcfunction(L, &FileWrite);
        lua_setfield(L, -2, "write");
        lua_pushcfunction(L, &FileLines);
        lua_setfield(L, -2, "lines");
        lua_pushcfunction(L, &FileSeek);
        lua_setfield(L, -2, "seek");
        lua_pushcfunction(L, &FileFlush);
        lua_setfield(L, -2, "flush");
        lua_pushcfunction(L, &FileClose);
        lua_setfield(L, -2, "close");
        lua_pushcfunction(L, &FileToString);
        lua_setfield(L, -2, "__tostring");

        lua_pop(L, 1); // the metatable

        lua_newtable(L);
        lua_pushcfunction(L, &IoOpen);
        lua_setfield(L, -2, "open");
        lua_pushcfunction(L, &IoLines);
        lua_setfield(L, -2, "lines");
        lua_pushcfunction(L, &IoWrite);
        lua_setfield(L, -2, "write");
        lua_pushcfunction(L, &IoRead);
        lua_setfield(L, -2, "read");
        lua_pushcfunction(L, &IoClose);
        lua_setfield(L, -2, "close");
        lua_pushcfunction(L, &IoType);
        lua_setfield(L, -2, "type");
        lua_setglobal(L, "io");

        UE_LOG(LogUnLuaNova, Log, TEXT("io library installed, rooted at lua.ioroot (default: the project directory)."));
    }
}
