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

除了上面默认导出的引擎类型，还可以为项目自己的类型生成绑定表，减少部分运行时查找与参数编组成本。生成 thunk 仍调用 `ProcessEvent`；不支持的签名走反射回退，收益需按场景测量。

**默认开启**，且是自动流程 —— 不需要人去跑命令，也不需要改配置。见下面[怎么关 / 怎么调](#默认就开着)。
Commandlet 只是给 CI / 构建机留的手动入口。

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

*注：运行后日志会输出 `selected N types`。若数量大到编译吃力，用 `StaticBindingModules`
收窄到核心模块（如 `/Script/Engine`、`/Script/UMG`）—— 项目类型永远位于自己的模块中，
不会被误伤。*

### 默认就开着

静态绑定是插件的核心性能特性，`bStaticBinding` 默认 `True`，不用写任何配置就会被
自动维护。需要在**工程** `Config/DefaultUnLuaNova.ini` 里动的只有"要更大覆盖面"和
"要关掉"两种情况：

```ini
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
; 关掉（一般不需要；把范围切到 All 且类型增删频繁时才考虑，改由 CI 生成）
;bStaticBinding=False
; 扩大覆盖面，默认 ExportedReflected（十来个类型）通常就够：
;StaticBindingSelection=Modules
;+StaticBindingModules=/Script/Engine
```

### 生成哪些类型

默认是 **`ExportedReflected`**：只生成插件本来就静态导出的那批 —— `UObject`、`UClass`、
`UWorld`，加上 `FVector` / `FVector2D` / `FVector4` / `FQuat` / `FRotator` / `FTransform` /
`FColor` / `FLinearColor` / `FIntPoint` / `FIntVector` 这些数学结构，十来个。

清单是**运行时从导出注册表读的**（`BEGIN_EXPORT_REFLECTED_CLASS` 那批），不是硬编码的一份
名单，所以往插件里加一个静态导出类型，它自动就进来了。

为什么不默认覆盖 `/Script/Engine`：那一个包就是几千个类型，生成出来的翻译单元会让编译
明显变慢，而脚本真正会在循环里反复摸的就是这十来个 —— 静态绑定的价值集中在**每次访问都
走的那些类型**上，不在覆盖面的大小上。

要更大覆盖面再切：

| `StaticBindingSelection` | 覆盖 |
|:---|:---|
| `ExportedReflected`（默认） | 插件默认静态导出的类型 |
| `Modules` | `StaticBindingModules` 列出的包 |
| `All` | 全部类型，只排除 `meta=(UnLuaNovaNoExport)` |

切到 `Modules` 时需要给名单：

```ini
StaticBindingSelection=Modules
+StaticBindingModules=/Script/Engine
+StaticBindingModules=/Script/UMG
```

开着之后剩下的事是自动的：编辑器启动时会比对产物的**指纹**（类型集合的 CRC），
产物缺失或类型有增删就重新生成，日志会写明做了什么：

```
StaticBinding: no generated file yet, generating into .../Generated/UnLuaNovaStaticBinding_Generated.cpp
StaticBinding: wrote ... (312 types, thunk coverage 2211/4078).
It is C++ - rebuild once for it to take effect; until then the reflective path stays in use.
```

**注意第二步**：产物是 `.cpp`，写完盘还要 UBT 编一次才进二进制。所以自动生成之后
thunk **本次启动还不生效**，要下次编译之后。日志里直接写了这句，不用猜。

产物落在 `Source/<工程模块>/Generated/`（在模块目录内才会被 UBT 编译）——
把生成物放在别处等于没有它。生成完之后**把产物一起提交**，团队其他人和 CI 就都不用再生成。

### 关于那一次重编译

产物是 `.cpp`，写完盘还要 UBT 编一次才进二进制 —— 这是编译期绑定的固有代价，不是
配置能绕开的。默认范围下（十来个类型）这次重编译很轻，所以功能默认开、流程全自动。

只有一种情况需要重新考虑：把 `StaticBindingSelection` 切到 `All` 之后，每次类型增删
都会触发一次大重编译。那时把 `bStaticBinding=False` 关掉自动、改由 CI 跑 Commandlet
更合适。

### CI / 构建机

没有编辑器启动可挂，用 Commandlet（退出码 0 = 有产物）：

```
UnrealEditor-Cmd.exe "<工程目录>/<工程>.uproject" -run=UnLuaNovaStaticBindingGenerator -unattended -NoSplash
```

可选参数：

| 参数 | 作用 |
|:---|:---|
| `-OutputDir=<路径>` | 产物目录，默认 `Source/<工程模块>/Generated` |
| `-Selection=ExportedReflected\|Modules\|All` | 生成范围，默认 `ExportedReflected` |
| `-Modules=/Script/Engine,/Script/UMG` | 收窄到指定包，逗号分隔；仅 `-Selection=Modules` 时生效 |

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
