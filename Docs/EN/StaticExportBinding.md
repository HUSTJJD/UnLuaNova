---
title: "Static Export and Static Binding"
description: "Manual static export, build-time static binding code generation and opt-out type selection"
---

For customisation and performance reasons, UnLuaNova exports these common engine types by
default (see the code for the full list):

#### Basic types
 * UObject
 * UClass
 * UWorld

#### Containers
 * TArray
 * TSet
 * TMap

#### Math
 * FVector
 * FVector2D
 * FVector4
 * FQuat
 * FRotator
 * FTransform
 * FColor
 * FLinearColor
 * FIntPoint
 * FIntVector

## Static binding code generation

Binding tables can also be generated for project types, reducing some lookup and marshalling work.
Generated thunks still call `ProcessEvent`; unsupported signatures fall back to reflection. Measure
the benefit for the actual workload.

Produced by a Commandlet at build time:

```
UnrealEditor-Cmd.exe "<ProjectDir>/<Project>.uproject" -run=UnLuaNovaStaticBindingGenerator -unattended -NoSplash
```

The generated code lands in **UnLuaNova's own module directory**
`<UnLuaNova>/Source/UnLuaNova/Generated/` - inside a module directory, because that is the
only place UBT compiles it. A generated file written anywhere else is not compiled at all,
and the reflection cost you meant to save is still being paid.

It deliberately does *not* default to the host project's `Source/<ProjectModule>/`: the project
name is not necessarily the main module directory (the Lyra host is named `LyraStarterGame`
while its main module lives in `Source/LyraGame`), so guessing from the project name can drop
the file into a directory with no `.Build.cs` that UBT never looks at.

This is a build-time step, not a runtime switch: rerun it when the set of types changes. The
output is **generated code and is git-ignored** - it depends on `StaticBindingSelection` and on
the host project's type set, so committing it into the plugin repository would leak across
projects. Teams and CI rely on the automatic generation at editor startup, or run the
Commandlet explicitly.

### Type selection (opt-out)

Everything is exported by default; a project only declares what it does **not** want:

| Order | Rule |
|---|---|
| 1 | `meta=(UnLuaNovaExport)` on the type or any ancestor -> force export, overriding every filter below |
| 2 | exclude the `SKEL_` / `REINST_` / `TRASHCLASS_` / `HOTRELOADED_` / `PLACEHOLDER-CLASS` prefixes |
| 3 | exclude `CLASS_Deprecated` / `CLASS_Transient` |
| 4 | exclude `meta=(UnLuaNovaNoExport)` |

```cpp
// opt-out: it would be exported by default, here it is explicitly not
UCLASS(meta=(UnLuaNovaNoExport))
class UMyDebugOnlyType : public UObject
{
    GENERATED_BODY()
};

// Tagging an ancestor once pulls a whole hierarchy back in
UCLASS(meta=(UnLuaNovaExport))
class UMyGameplayBase : public UObject
{
    GENERATED_BODY()
};
```

*The run logs `selected N types`. If that number makes compilation painful, narrow it to the
core modules (for example `/Script/Engine`, `/Script/UMG`); project types always live in their
own module and are never caught by that.*

## Static export

UnLuaNova also offers a straightforward way to export types, member variables and enums by hand.

### Classes
* Non-reflected class
```
BEGIN_EXPORT_CLASS(ClassType, ...)
```
or
```
BEGIN_EXPORT_NAMED_CLASS(ClassName, ClassType, ...)
```
**'...'** is the constructor parameter list.

* Reflected class
```
BEGIN_EXPORT_REFLECTED_CLASS(UObjectType)
```
or
```
BEGIN_EXPORT_REFLECTED_CLASS(NonUObjectType, ...)
```
**'...'** is the constructor parameter list.

#### Member variables
```
ADD_PROPERTY(Property)
```
or, for bitfield bools:
```
ADD_BITFIELD_BOOL_PROPERTY(Property)
```

#### Member functions
##### Non-static member functions
* shorthand
```
ADD_FUNCTION(Function)
```
or
```
ADD_NAMED_FUNCTION(Name, Function)
```

* full form
```
ADD_FUNCTION_EX(Name, RetType, Function, ...)
```
or
```
ADD_CONST_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** is the parameter type list.

##### Static member functions
```
ADD_STATIC_FUNCTION(Function)
```
or
```
ADD_STATIC_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** is the parameter type list.

#### Example
```
struct Vec3
{
	Vec3() : x(0), y(0), z(0) {}
	Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	void Set(const Vec3 &V) { *this = V; }
	Vec3& Get() { return *this; }
	void Get(Vec3 &V) const { V = *this; }

	bool operator==(const Vec3 &V) const { return x == V.x && y == V.y && z == V.z; }

	static Vec3 Cross(const Vec3 &A, const Vec3 &B) { return Vec3(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x); }
	static Vec3 Multiply(const Vec3 &A, float B) { return Vec3(A.x * B, A.y * B, A.z * B); }
	static Vec3 Multiply(const Vec3 &A, const Vec3 &B) { return Vec3(A.x * B.x, A.y * B.y, A.z * B.z); }

	float x, y, z;
};

BEGIN_EXPORT_CLASS(Vec3, float, float, float)
	ADD_PROPERTY(x)
	ADD_PROPERTY(y)
	ADD_PROPERTY(z)
	ADD_FUNCTION(Set)
	ADD_NAMED_FUNCTION("Equals", operator==)
	ADD_FUNCTION_EX("Get", Vec3&, Get)
	ADD_CONST_FUNCTION_EX("GetCopy", void, Get, Vec3&)
	ADD_STATIC_FUNCTION(Cross)
	ADD_STATIC_FUNCTION_EX("MulScalar", Vec3, Multiply, const Vec3&, float)
	ADD_STATIC_FUNCTION_EX("MulVec", Vec3, Multiply, const Vec3&, const Vec3&)
END_EXPORT_CLASS()
IMPLEMENT_EXPORTED_CLASS(Vec3)
```

### Global functions
```
EXPORT_FUNCTION(RetType, Function, ...)
```
or
```
EXPORT_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** is the parameter type list.

#### Example
```
void GetEngineVersion(int32 &MajorVer, int32 &MinorVer, int32 &PatchVer)
{
	MajorVer = ENGINE_MAJOR_VERSION;
	MinorVer = ENGINE_MINOR_VERSION;
	PatchVer = ENGINE_PATCH_VERSION;
}

EXPORT_FUNCTION(void, GetEngineVersion, int32&, int32&, int32&)
```

### Enums
* Unscoped enum
```
enum EHand
{
	LeftHand,
	RightHand
};

BEGIN_EXPORT_ENUM(EHand)
	ADD_ENUM_VALUE(LeftHand)
	ADD_ENUM_VALUE(RightHand)
END_EXPORT_ENUM(EHand)
```

* Scoped enum
```
enum class EEye
{
	LeftEye,
	RightEye
};

BEGIN_EXPORT_ENUM(EEye)
	ADD_SCOPED_ENUM_VALUE(LeftEye)
	ADD_SCOPED_ENUM_VALUE(RightEye)
END_EXPORT_ENUM(EEye)
```

---

## Further reading

- [API Reference](API.md) - the C++ side of the export interface
- [Programming Guide](UnLuaNova_Programming_Guide.md) - reaching engine types from Lua
- [Settings](Settings.md) - every setting
