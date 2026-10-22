---
title: "静态导出与静态绑定"
description: "手动静态导出、构建期静态绑定代码生成与 opt-out 类型选择"
---

鉴于定制化和性能方面的考虑，UnLuaNova 默认导出了以下引擎常用的类（详细的可以查阅代码）：

#### 基础类型
 * UObject
 * UClass
 * UWorld
 
#### 常用容器
 * TArray
 * TSet
 * TMap

#### 数学库
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

## 静态绑定代码生成

除了上面默认导出的引擎类型，还可以为项目自己的类型生成静态绑定表，绕开反射热路径。

由 Commandlet 在构建期生成：

```
UnrealEditor-Cmd.exe <工程>.uproject -run=UnLuaNovaStaticBindingGenerator
```

生成的代码落在 `Source/<工程模块>/Generated/` 下（在模块目录内才会被 UBT 编译）。

### 类型选择（opt-out）

默认导出全部类型，项目只需要声明**不要什么**：

| 顺序 | 规则 |
|---|---|
| 1 | `meta=(UnLuaNovaExport)` 在本类或任一祖先 → 强制导出，覆盖以下全部过滤 |
| 2 | 排除 `SKEL_` / `REINST_` / `TRASHCLASS_` / `HOTRELOADED_` / `PLACEHOLDER-CLASS` 前缀 |
| 3 | 排除 `CLASS_Deprecated` / `CLASS_Transient` |
| 4 | 排除 `meta=(UnLuaNovaNoExport)` |

在基类上标记一次 `meta=(UnLuaNovaExport)`，它的整个派生体系都会被导出 —— 这对类型数量庞大的项目尤其省事。

```cpp
// opt-out：默认会被导出，这里显式不要
UCLASS(meta=(UnLuaNovaNoExport))
class UMyDebugOnlyType : public UObject
{
    GENERATED_BODY()
};

// 祖先上标一次，整个派生体系都拉回来
UCLASS(meta=(UnLuaNovaExport))
class UMyGameplayBase : public UObject
{
    GENERATED_BODY()
};
```

*注：运行后日志会输出 `selected N types`。若数量大到编译吃力，可以改为只绑定核心模块（如 `/Script/Engine`、`/Script/UMG`）来收窄，项目类型永远位于自己的模块中，不会被误伤。*

### 怎么跑

```
UnrealEditor-Cmd.exe "<工程目录>/<工程>.uproject" -run=UnLuaNovaStaticBindingGenerator -unattended -NoSplash
```

生成的代码落在 `Source/<工程模块>/Generated/` 下（在模块目录内才会被 UBT 编译）——
把生成物放在别处等于没有它：文件不会被编译，构建期省下的反射开销也就没省到。

它是构建期步骤，不是运行期开关：类型集合变了就重跑一次，然后把生成物一起提交。

## 静态导出

UnLuaNova 提供了一种简单的方案来静态导出类型/成员变量/枚举等。

### 类
* 非反射类
```
BEGIN_EXPORT_CLASS(ClassType, ...)
```
 或者
```
BEGIN_EXPORT_NAMED_CLASS(ClassName, ClassType, ...)
```
 **'...'** 表示构造方法里的参数列表。

* 反射类
```
BEGIN_EXPORT_REFLECTED_CLASS(UObjectType)
```
 或者
```
BEGIN_EXPORT_REFLECTED_CLASS(NonUObjectType, ...)
```
 **'...'** 表示构造方法里的参数列表。

#### 成员变量
```
ADD_PROPERTY(Property)
```
或（位域布尔类型的属性）
```
ADD_BITFIELD_BOOL_PROPERTY(Property)
```

#### 成员函数
##### 非静态成员函数
* 简写风格
```
ADD_FUNCTION(Function)
```
或
```
ADD_NAMED_FUNCTION(Name, Function)
```

* 完整风格
```
ADD_FUNCTION_EX(Name, RetType, Function, ...)
```
或
```
ADD_CONST_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** 表示参数类型列表。

##### 静态成员函数
```
ADD_STATIC_FUNCTION(Function)
```
或
```
ADD_STATIC_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** 表示参数类型列表。

#### 示例
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

### 全局函数
```
EXPORT_FUNCTION(RetType, Function, ...)
```
Or
```
EXPORT_FUNCTION_EX(Name, RetType, Function, ...)
```
**'...'** 表示参数类型列表。

#### 示例
```
void GetEngineVersion(int32 &MajorVer, int32 &MinorVer, int32 &PatchVer)
{
	MajorVer = ENGINE_MAJOR_VERSION;
	MinorVer = ENGINE_MINOR_VERSION;
	PatchVer = ENGINE_PATCH_VERSION;
}

EXPORT_FUNCTION(void, GetEngineVersion, int32&, int32&, int32&)
```

### 枚举
* 不带作用域的枚举
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

* 带作用域的枚举
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

## 延伸阅读

- [API 参考](API.md) — C++ 侧的导出接口
- [编程指南](UnLuaNova_Programming_Guide.md) — 从 Lua 访问引擎类型
- [设置项](Settings.md) — 设置项一览
