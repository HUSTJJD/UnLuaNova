---
title: "Showcase Samples"
description: "Runnable samples: binding, overriding, containers and UMG"
---

# Showcase Samples

See the [capability matrix](../feature-matrix.md) for source and execution evidence. Existing images
are illustrative, were not regenerated in this audit, and do not certify the current build.

New executable samples: `Examples.RuntimeContracts` exposes `.Run()` for Event.Once reentry,
explicit unsubscribe and OOP inheritance on both backends. `Luau.Index.BufferInterop:Run()`
demonstrates buffer copy ownership. Both are connected to automation source and await execution.

This page collects the **runnable samples**: a short piece of real code plus a screenshot of it
running, to answer "how is this actually used" rather than describing an API signature.

Example sources live under `Source/UnLuaNovaDemo/`, their Lua scripts under the project's
`Content/Script/`. The screenshots on this page are produced by the automation suite, not by
hand.

## Why the screenshots are automated

Hand-taken pictures go stale: the code moves on and the picture still claims to be current. So
they are regenerated whenever `Tools/RunTests.ps1 -Publish` runs - the same run that produces
[the test reports](../test-luau-report.md). See [Testing and Automation](Testing.md).

Regenerating just the images (fast):

```powershell
.\Tools\RunTests.ps1 -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

Four specs, each opening one tutorial level and taking one shot; the files land in
`Docs/Images/tests-luau/`:

| Spec | Level | Image |
|---|---|---|
| `UnLuaNova.Showcase.Binding` | `Content/Tutorials/01_HelloWorld` | `Showcase_Binding.png` |
| `UnLuaNova.Showcase.Override` | `Content/Tutorials/02_OverrideBlueprintEvents` | `Showcase_Override.png` |
| `UnLuaNova.Showcase.Container` | `Content/Tutorials/06_NativeContainers` | `Showcase_Container.png` |
| `UnLuaNova.Showcase.UMG` | `Content/Tutorials/11_ReleaseUMG` | `Showcase_UMG.png` |

!!! warning "A viewport is required"
    Screenshots need real rendering, so a `-NullRHI` run drops these four entirely (they are
    not counted as failures). Do not pass `-NullRHI` when refreshing images.

## The samples

### 1. Binding: UObject and Lua calling each other

`UTutorialObject` shows the basic path in both directions - how a C++ class is reached from
Lua, and how a Lua function is called back from C++.

This example exercises binding and object lifetime. It does not establish a speed advantage or
startup cost independent of the number of UObjects.

![Binding sample](../Images/tests-luau/Showcase_Binding.png)

### 2. Overriding: replacing Blueprint events from Lua

`UTutorialBlueprintFunctionLibrary.CallLuaByGlobalTable` walks the whole path: entering Lua from
a Blueprint, then overriding events such as `ReceiveBeginPlay` from Lua.

Point of interest: overriding **replaces** - the original implementation does not run unless you
call it explicitly through `Overridden`.

![Override sample](../Images/tests-luau/Showcase_Override.png)

### 3. Containers: TArray / TMap / TSet from Lua

`LuaLib_Array`, `LuaLib_Map` and `LuaLib_Set` show how containers are read and written from Lua,
and that they are reference semantics there (the container itself is not copied). `TSet` can now
be iterated with `pairs`.

![Container sample](../Images/tests-luau/Showcase_Container.png)

### 4. UMG: Lua takes over the UI and its lifetime

`11_ReleaseUMG` shows how parent/child widget creation, release and references settle down once
Lua owns the UI - the scenario where dangling references are easiest to write.

![UMG sample](../Images/tests-luau/Showcase_UMG.png)

Debugging features are on the [Debugging](Debugging.md) page; memory statistics come from the
`lua.memstats` console command, which breaks Lua-side allocation down by memory category.

---

The image comments above become real pictures after the next `-Publish` run - the shots are
produced by `UnLuaNova.Showcase.*`, they just have to be published into the repository first.
To add a sample: add the sample class and its Lua script, then one screenshot command in
`Private/Tests/ShowcaseTest.cpp`.

---

## Further reading

- [Testing and Automation](Testing.md) - where the screenshots come from
- [Test report (Lua backend)](../test-lua-report.md) · [Test report (Luau backend)](../test-luau-report.md)
- [Programming Guide](UnLuaNova_Programming_Guide.md)
