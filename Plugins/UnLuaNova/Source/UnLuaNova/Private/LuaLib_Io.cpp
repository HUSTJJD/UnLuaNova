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

#include "LuaLib_Io.h"
#include "UnLuaNovaCompatibility.h"

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
// io.tmpfile() has no disk path; flushing it must be a no-op rather than an attempt to
// write an empty filename through ResolvePath. Kept as a field instead of a path sentinel
// so a script cannot confuse a real file with a temporary one by editing __path.
static const char IoTmpField[] = "__tmp";

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
     * Hard cap on how much a single file may be read or buffered. The io implementation
     * keeps the whole file in a Lua string, so without this a script can turn one
     * io.open / repeated file:write into an unbounded allocation. 0 disables the cap.
     */
    int64 IoMaxBytes()
    {
        static auto CVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.iomaxbytes"),
            16 * 1024 * 1024,
            TEXT("Maximum bytes a lua.io file may read or buffer in one handle. "
                 "0 = unlimited. Applies on open, write and flush."),
            ECVF_Default);
        return int64(CVar->GetInt());
    }

    /** Fail before allocating a buffer that would cross the cap. */
    void CheckBufferSize(lua_State* L, size_t Size)
    {
        const int64 Max = IoMaxBytes();
        if (Max > 0 && int64(Size) > Max)
        {
            // lua_pushfstring only understands a small format set; build the numbers here.
            const FString Message = FString::Printf(
                TEXT("io: buffer would be %lld bytes, exceeding lua.iomaxbytes (%lld)"),
                (long long)Size, (long long)Max);
            luaL_error(L, "%s", TCHAR_TO_UTF8(*Message));
        }
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

    void ReadCount(lua_State* L, const char* Data, size_t Len, int64& Pos, int64 Count);
    void ReadFormat(lua_State* L, const char* Data, size_t Len, int64& Pos, const char* RawFormat);

    int FileRead(lua_State* L);
    int FileWrite(lua_State* L);
    int FileLines(lua_State* L);
    int FileSeek(lua_State* L);
    int FileFlush(lua_State* L);
    int FileClose(lua_State* L);
    int FileToString(lua_State* L);

    int IoTmpFile(lua_State* L);
    int IoPopen(lua_State* L);

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

        // Upvalue 1 is the format list passed to file:lines(...) / io.lines(...).
        // An empty list means the default "one line per iteration".
        lua_pushvalue(L, lua_upvalueindex(1));
        const int Formats = lua_gettop(L);
        const int NumFormats = int(lua_rawlen(L, Formats));

        int Results = 0;
        if (NumFormats == 0)
        {
            ReadFormat(L, Data, Len, Pos, "l");
            Results = 1;
        }
        else
        {
            for (int i = 1; i <= NumFormats; ++i)
            {
                lua_rawgeti(L, Formats, i);
                const int FmtIdx = lua_gettop(L);
                if (lua_type(L, FmtIdx) == LUA_TNUMBER)
                    ReadCount(L, Data, Len, Pos, int64(lua_tointeger(L, FmtIdx)));
                else
                {
                    const char* Fmt = lua_tostring(L, FmtIdx);
                    if (!Fmt)
                        return luaL_error(L, "io: invalid read format");
                    ReadFormat(L, Data, Len, Pos, Fmt);
                }
                lua_remove(L, FmtIdx);
                ++Results;
            }
        }
        lua_remove(L, Formats);

        // generic-for stops on the first nil, so an exhausted format list ends the loop.
        lua_pushinteger(L, lua_Integer(Pos));
        lua_setfield(L, 1, IoPosField);
        return Results;
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

    // One read format -> exactly one pushed result, with Pos advanced. Shared by
    // file:read and the lines() iterator so both accept the same format list instead
    // of the iterator silently ignoring its arguments.
    void ReadCount(lua_State* L, const char* Data, size_t Len, int64& Pos, int64 Count)
    {
        if (Count < 0)
        {
            luaL_error(L, "io: read count must be non-negative");
            return;
        }
        if (Count == 0)
        {
            // Reading zero bytes is the documented end-of-file test: an empty string
            // when there is more to read, nil at the end.
            if (!Data || Pos >= int64(Len))
                lua_pushnil(L);
            else
                lua_pushliteral(L, "");
            return;
        }

        if (!Data || Pos >= int64(Len))
        {
            lua_pushnil(L);
            return;
        }

        const int64 Available = int64(Len) - Pos;
        const int64 Take = FMath::Min(Count, Available);
        lua_pushlstring(L, Data + Pos, size_t(Take));
        Pos += Take;
    }

    void ReadFormat(lua_State* L, const char* Data, size_t Len, int64& Pos, const char* RawFormat)
    {
        const char* Format = RawFormat;
        if (Format[0] == '*')
            ++Format;
        if (Format[0] == '\0' || Format[1] != '\0')
        {
            luaL_error(L, "io: invalid read format '%s'", RawFormat);
            return;
        }

        if (Format[0] == 'a')
        {
            if (!Data || Pos >= int64(Len))
                lua_pushliteral(L, "");
            else
                lua_pushlstring(L, Data + Pos, size_t(int64(Len) - Pos));
            Pos = int64(Len);
            return;
        }

        if (Format[0] == 'l' || Format[0] == 'L')
        {
            if (!Data || Pos >= int64(Len))
            {
                lua_pushnil(L);
                return;
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
            return;
        }

        if (Format[0] == 'n')
        {
            // Skip whitespace, then let the number parser decide where it stops.
            while (Data && Pos < int64(Len) && isspace(static_cast<unsigned char>(Data[Pos])))
                ++Pos;

            if (!Data || Pos >= int64(Len))
            {
                lua_pushnil(L);
                return;
            }

            const char* Start = Data + Pos;
            char* End = nullptr;
            const double Value = strtod(Start, &End);
            if (End == Start)
            {
                lua_pushnil(L);
                return;
            }

            lua_pushnumber(L, Value);
            Pos += int64(End - Start);
            return;
        }

        luaL_error(L, "io: invalid read format '%s'", RawFormat);
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
                    ReadCount(L, Data, Len, Pos, int64(luaL_checkinteger(L, i)));
                else
                {
                    const char* Format = lua_tostring(L, i);
                    if (!Format)
                        return luaL_error(L, "io: invalid read format");
                    ReadFormat(L, Data, Len, Pos, Format);
                }
                ++Results;
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

        const int32 Mode = GetModeField(L, 1);
        if ((Mode & IoModeWrite) == 0)
            return luaL_error(L, "io: file was not opened for writing");

        const int NumArgs = lua_gettop(L);

        // A write starts at __pos and overwrites, it does not append. The old code always
        // concatenated the whole buffer with the arguments, which made every "r+" seek
        // useless: the data landed at the end regardless of where the position was.
        // No luaL_Buffer: it keeps its own bookkeeping value on the stack, which makes
        // every index below it stale - and no C++ buffer either, since Luau has no __gc.
        lua_getfield(L, 1, IoDataField);              // [file, args..., buffer]
        size_t OldLen = 0;
        const char* OldData = lua_tolstring(L, -1, &OldLen);

        lua_getfield(L, 1, IoPosField);
        int64 Pos = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);
        // Lua's append mode always writes at the end even after a seek.
        if ((Mode & IoModeAppend) != 0)
            Pos = int64(OldLen);
        else
            Pos = FMath::Clamp(Pos, int64(0), int64(OldLen));

        lua_pushlstring(L, OldData ? OldData : "", size_t(Pos)); // prefix

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

        // No arguments means an empty write; do it explicitly rather than relying on the
        // exact behaviour of lua_concat(L, 0) across the two backends.
        if (NumArgs > 1)
            lua_concat(L, NumArgs - 1);               // [.., buffer, prefix, text]
        else
            lua_pushliteral(L, "");
        size_t TextLen = 0;
        lua_tolstring(L, -1, &TextLen);

        // Bytes past the overwritten region survive, exactly as they would on disk.
        const size_t OverwriteEnd = size_t(Pos) + TextLen;
        const size_t Tail = OverwriteEnd < OldLen ? OldLen - OverwriteEnd : 0;
        if (Tail > 0)
            lua_pushlstring(L, OldData + OverwriteEnd, Tail);
        else
            lua_pushliteral(L, "");

        lua_concat(L, 3);                             // [.., buffer, result]

        size_t NewLen = 0;
        lua_tolstring(L, -1, &NewLen);
        CheckBufferSize(L, NewLen);

        lua_pushvalue(L, -1);
        lua_setfield(L, 1, IoDataField);

        // The position advances by what was written, not by the resulting file length.
        lua_pushinteger(L, lua_Integer(Pos + int64(TextLen)));
        lua_setfield(L, 1, IoPosField);

        // Lua's file:write returns the file, so calls can be chained.
        lua_pushvalue(L, 1);
        return 1;
    }

    int FileSeek(lua_State* L)
    {
        CheckOpen(L, 1);

        const char* Whence = luaL_optstring(L, 2, "cur");
        const int64 Offset = int64(luaL_optinteger(L, 3, 0));

        // Lua accepts exactly "set", "cur" and "end". Matching only the first character
        // used to turn "nonsense" into a silent relative seek.
        const bool bSet = FCStringAnsi::Strcmp(Whence, "set") == 0;
        const bool bCur = FCStringAnsi::Strcmp(Whence, "cur") == 0;
        const bool bEnd = FCStringAnsi::Strcmp(Whence, "end") == 0;
        if (!bSet && !bCur && !bEnd)
            return luaL_error(L, "io: invalid seek option '%s' (expected set, cur or end)", Whence);

        lua_getfield(L, 1, IoDataField);
        size_t Len = 0;
        lua_tolstring(L, -1, &Len);
        lua_pop(L, 1);

        lua_getfield(L, 1, IoPosField);
        int64 Pos = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);

        const int64 Base = bSet ? int64(0) : (bEnd ? int64(Len) : Pos);
        // This is an in-memory file: there is no sparse hole to seek into, so positions
        // outside the buffer are clamped instead of returning a position that every
        // subsequent read would report as EOF anyway.
        const int64 Next = FMath::Clamp(Base + Offset, int64(0), int64(Len));
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
        CheckBufferSize(L, Len);

        // io.tmpfile() lives only in the Lua string on this handle. Lua says the file is
        // removed when closed; here it was never on disk, so flushing is simply a no-op.
        lua_getfield(L, 1, IoTmpField);
        const bool bTemporary = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (bTemporary)
        {
            lua_pushboolean(L, 1);
            return 1;
        }

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
        const int NumFormats = lua_gettop(L) - 1;

        // Formats travel as a single upvalue so the iterator knows how many values to
        // return per line: file:lines("l", "n") is a multi-value iterator in Lua.
        lua_newtable(L);
        const int Formats = lua_gettop(L);
        for (int i = 2; i <= NumFormats + 1; ++i)
        {
            lua_pushvalue(L, i);
            lua_rawseti(L, Formats, i - 1);
        }
        lua_pushcclosure(L, &LinesIterator, 1);
        lua_pushvalue(L, 1);
        return 2;
    }

    int IoTmpFile(lua_State* L)
    {
        // An in-memory, read/write handle. There is no OS temp file and no path: the
        // buffer is the file, and closing it discards the buffer. This is the honest
        // subset of io.tmpfile() the rooted library can offer without a writable
        // scratch directory or a destructor to remove a real file.
        lua_newtable(L);
        const int File = lua_gettop(L);

        lua_pushliteral(L, "");
        lua_setfield(L, File, IoPathField);
        lua_pushinteger(L, IoModeRead | IoModeWrite);
        lua_setfield(L, File, IoModeField);
        lua_pushboolean(L, 1);
        lua_setfield(L, File, IoOpenField);
        lua_pushliteral(L, "");
        lua_setfield(L, File, IoDataField);
        lua_pushinteger(L, 0);
        lua_setfield(L, File, IoPosField);
        lua_pushboolean(L, 1);
        lua_setfield(L, File, IoTmpField);

        luaL_getmetatable(L, IoFileMeta);
        lua_setmetatable(L, File);
        return 1;
    }

    int IoPopen(lua_State* L)
    {
        // Process execution is deliberately outside this library's capability set.
        // Saying so is better than an empty stub that scripts mistake for a working pipe.
        return luaL_error(L,
            "io.popen is not supported by UnLuaNova's rooted io library: "
            "scripts cannot start processes");
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
            else if (*C == 'b')
            {
                // Binary mode is a no-op here: the buffer is bytes, there is no text
                // translation to disable. Accepting the flag keeps "rb"/"wb" scripts
                // working instead of silently treating them as a different mode.
            }
            else
                return luaL_error(L, "io: invalid mode '%s' (expected r, w, a, + or b)", ModeArg);
        }

        // Silently defaulting an unrecognised mode to "r" used to make a typo open a
        // readable handle where the script expected to write.
        if ((Mode & (IoModeRead | IoModeWrite)) == 0)
            return luaL_error(L, "io: invalid mode '%s' (no read or write access)", ModeArg);

        FString Full;
        if (!ResolvePath(L, UTF8_TO_TCHAR(PathArg), Full))
            return luaL_error(L, "%s", lua_tostring(L, -1));

        const int64 Max = IoMaxBytes();
        const int64 FileSize = IFileManager::Get().FileSize(*Full);
        if (Max > 0 && FileSize > Max)
        {
            const FString Message = FString::Printf(
                TEXT("io: '%s' is %lld bytes, exceeding lua.iomaxbytes (%lld)"),
                UTF8_TO_TCHAR(PathArg), (long long)FileSize, (long long)Max);
            return luaL_error(L, "%s", TCHAR_TO_UTF8(*Message));
        }

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
        const int NumFormats = Top - 1;

        lua_pushcfunction(L, &IoOpen);
        lua_pushvalue(L, 1);
        lua_pushliteral(L, "r");
        lua_call(L, 2, 2); // file, or nil + error

        if (lua_isnil(L, -2))
            return luaL_error(L, "%s", lua_tostring(L, -1));

        lua_pop(L, 1); // the second result is always nil on success
        // Stack is now [filename, formats..., file]; the original indices are unchanged.

        lua_newtable(L);
        const int Formats = lua_gettop(L);
        for (int i = 2; i <= NumFormats + 1; ++i)
        {
            lua_pushvalue(L, i);
            lua_rawseti(L, Formats, i - 1);
        }
        lua_pushcclosure(L, &LinesIterator, 1);
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

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s"), *Out);

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
        lua_pushcfunction(L, &IoTmpFile);
        lua_setfield(L, -2, "tmpfile");
        lua_pushcfunction(L, &IoPopen);
        lua_setfield(L, -2, "popen");
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

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("io library installed, rooted at lua.ioroot (default: the project directory)."));
    }
}
