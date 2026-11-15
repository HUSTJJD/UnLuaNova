---
title: "Tutorials"
description: "Levels, main scripts and capabilities of the 13 runnable tutorials"
---

# Tutorials

This page lists the 13 tutorials under `Content/Tutorials/` by number. Each tutorial is a `.umap`
level with one companion Lua main script. The capability notes and code snippets below are taken
from the scripts that actually exist in the repository; nothing is inferred.

## How to open a tutorial

Double-click the `.umap` under `Content/Tutorials/<NN>_<Name>/` in the editor, then press Play
(PIE). `Screen.Print` writes to the screen and `print` to the Output Log. Some tutorials need key
presses to trigger (03, 04, 06, 11), and tutorial 10 is best run in multiplayer mode.

## Prerequisites

- **The plugin is built**: `Plugins/UnLuaNova` is a C++ plugin, so the project must be generated as a C++ project and compiled first. For integrating into a third-party project from scratch, see the checklist at `examples/minimal/README.md`.
- **The script root is `<Project>/Content/Script/`**: tutorial main scripts live in `Content/Script/Tutorials/`, and module paths use dots without an extension, e.g. `Tutorials.01_HelloWorld`. `.lua` files under `Content/Script/` are not visible in the Content Browser; use the system file explorer.
- **The tutorial levels and Blueprint assets belong to the UnLuaNovaDemo project in this repository**: a third-party project does not get `Content/Tutorials/` automatically; reproducing these levels requires copying it together with `Content/Script/Tutorials/`.

## Dual backends and Luau-only samples

`LuaBackend` in the project `Config/DefaultUnLuaNova.ini` selects the backend (a compile-time
switch; recompile after changing it), with values `Lua` (official Lua) or `Luau` (default).
Luau-only syntax (`continue`, compound assignment, type annotations, vector / buffer, etc.) is a
syntax error at `require` time under the `Lua` backend.

Such samples live in `Content/Script/Luau/` and are reached through the guard entry
`Content/Script/Luau/Index.lua`: `require "Luau.Index"` checks `UnLuaNova.GetBackend()` first and
loads files such as `01_StrictTyping` only when Luau is confirmed, otherwise it returns
`Available = false`. Usage: `local Examples = require "Luau.Index"; if Examples.Available then ... end`.
The 13 tutorial scripts on this page do not go through that guard.

## Tutorial list

For 01 through 12 the main script is `Content/Script/Tutorials/<NN>_<Name>.lua`; tutorial 13 has no
numbered main script, see item 13.

### 1. 01_HelloWorld

- Level: `Content/Tutorials/01_HelloWorld/HelloWorld.umap`
- Main script: `Content/Script/Tutorials/01_HelloWorld.lua`
- Demonstrates: The most basic static binding - a Blueprint implements the `UnLuaNovaInterface` interface and names its script through `GetModuleName`; plus the initialization callback and on-screen output for a bound object.
- Key points: `local M = UnLuaNova.Class()`, `print(msg)` inside `M:Initialize()`, and `Screen.Print(msg)` from `require "Tutorials.Screen"` to show Hello World. Note this lesson uses `Initialize`, not `ReceiveBeginPlay`.

### 2. 02_OverrideBlueprintEvents

- Level: `Content/Tutorials/02_OverrideBlueprintEvents/OverrideBlueprintEvents.umap`
- Main script: `Content/Script/Tutorials/02_OverrideBlueprintEvents.lua`
- Demonstrates: Overriding Blueprint events and Blueprint functions from Lua, and calling the overridden original.
- Key points: Declare `M:ReceiveBeginPlay()` for the event and `M:SayHi(name)` for the function; call the original with `self.Overridden.SayHi(self, name)`. The script warns explicitly that `self.Overridden:SayHi(name)` is wrong.

### 3. 03_BindInputs

- Level: `Content/Tutorials/03_BindInputs/BindInputs.umap`
- Main script: `Content/Script/Tutorials/03_BindInputs.lua`
- Demonstrates: Key and axis input binding, plus fine-grained binding with modifier keys.
- Key points: Declaring `{KeyName}_Pressed` / `{KeyName}_Released` receives key events and an axis mapping uses a same-named function; the script generates `M[key_name .. "_Pressed"]` in a loop and uses `UnLuaNova.Input.BindKey(M, "C", "Pressed", fn, { Ctrl = true })` for combinations such as Ctrl+C.

### 4. 04_DynamicBinding

- Level: `Content/Tutorials/04_DynamicBinding/DynamicBinding.umap`
- Main script: `Content/Script/Tutorials/04_DynamicBinding.lua`
- Demonstrates: Binding objects to Lua dynamically at runtime, without the Blueprint implementing the interface.
- Key points: Left mouse button calls `World:SpawnActor(SpawnClass, Transform, AlwaysSpawn, self, self, "Tutorials.GravitySphereActor")`; right mouse button calls `NewObject(WidgetClass, self, nil, "Tutorials.IconWidget")`. The last argument is a module path relative to `Content/Script/`, matching `GravitySphereActor.lua` and `IconWidget.lua`.

### 5. 05_BindDelegates

- Level: `Content/Tutorials/05_BindDelegates/BindDelegates.umap`
- Main script: `Content/Script/Tutorials/05_BindDelegates.lua`
- Demonstrates: Binding and unbinding UMG widget delegates, and timer delegates.
- Key points: `Construct` uses `self.ClickMeButton.OnClicked:Add(self, self.OnButtonClicked)` and `UE.UKismetSystemLibrary.K2_SetTimerDelegate({ self, self.OnTimer }, 1, true)`; `Destruct` pairs `Remove` with `K2_ClearAndInvalidateTimerHandle`.

### 6. 06_NativeContainers

- Level: `Content/Tutorials/06_NativeContainers/NativeContainers.umap`
- Main script: `Content/Script/Tutorials/06_NativeContainers.lua`
- Demonstrates: Creating and using UE native containers `TArray` / `TSet` / `TMap` from Lua.
- Key points: `UE.TArray(0)`, `UE.TSet(0)`, `UE.TMap(0, true)` fix the element types from sample values; number keys 1 / 2 / 3 trigger `One_Pressed` / `Two_Pressed` / `Three_Pressed`, which exercise `Add`, `AddUnique`, `RemoveItem`, `Contains`, `Find`, `Clear` and print to the console.

### 7. 07_CallLatentFunction

- Level: `Content/Tutorials/07_CallLatentFunction/CallLatentFunction.umap`
- Main script: `Content/Script/Tutorials/07_CallLatentFunction.lua`
- Demonstrates: Calling UE Latent functions from Lua coroutines.
- Key points: The coroutine function `run` loops on `UE.UKismetSystemLibrary.Delay(self, 1)`; `ReceiveBeginPlay` starts two parallel coroutines A and B with `coroutine.resume(coroutine.create(run), self, "A")`, each printing five times.

### 8. 08_CppCallLua

- Level: `Content/Tutorials/08_CppCallLua/CppCallLua.umap`
- Main script: `Content/Script/Tutorials/08_CppCallLua.lua`
- Demonstrates: Calling Lua functions from C++, through the global table and through `FLuaTable`.
- Key points: `ReceiveBeginPlay` calls `UE.UTutorialBlueprintFunctionLibrary.CallLuaByGlobalTable()` and `CallLuaByFLuaTable()`; both end up calling the Lua-side `M.CallMe(a, b)` and reading its return value. C++ source: `Source/UnLuaNovaDemo/TutorialBlueprintFunctionLibrary.cpp`; the project `*.Build.cs` needs an `UnLuaNova` dependency (and `Lua` for native Lua API access).

### 9. 09_StaticExport

- Level: `Content/Tutorials/09_StaticExport/StaticExport.umap`
- Main script: `Content/Script/Tutorials/09_StaticExport.lua`
- Demonstrates: Using a statically exported C++ type that is not a UObject.
- Key points: `local tutorial = UE.FTutorialObject(...)` followed by `tutorial:GetTitle()`. The type is exported in `Source/UnLuaNovaDemo/TutorialObject.cpp` with `BEGIN_EXPORT_CLASS(FTutorialObject)` / `ADD_FUNCTION(GetTitle)`.

### 10. 10_Replications

- Level: `Content/Tutorials/10_Replications/Replications.umap`
- Main script: `Content/Script/Tutorials/10_Replications.lua`
- Demonstrates: Replication / RPC overrides; run it in multiplayer mode. The level also contains `ChatCharacter.uasset`.
- Key points: The numbered main script only prints a hint on the server when `self:HasAuthority()` is true; the actual RPC and replication demo is the companion character script `Content/Script/Tutorials/ChatCharacter.lua`, where `Say_Server_RPC`, `Say_Multicast_RPC` and `OnRep_Name` correspond to overriding RPCs via `{FunctionName}_RPC` and replication callbacks via `OnRep_{VariableName}`.

### 11. 11_ReleaseUMG

- Level: `Content/Tutorials/11_ReleaseUMG/ReleaseUMG.umap`
- Main script: `Content/Script/Tutorials/11_ReleaseUMG.lua`
- Demonstrates: The release and reference-settling flow once UMG widgets are owned by Lua, avoiding dangling references.
- Key points: `ReceiveBeginPlay` builds the root widget with `UE.UClass.Load("/Game/Tutorials/11_ReleaseUMG/ReleaseUMG_Root.ReleaseUMG_Root_C")` and `NewObject`; pressing `L` runs `collectgarbage("collect")` and `C` runs `UE.UKismetSystemLibrary.CollectGarbage()`. The companion `ReleaseUMG_Root.lua`, `ReleaseUMG_ItemParent.lua` and `ReleaseUMG_ItemChild.lua` call `self:Release()` in `Destruct`. Use `Obj List Class=...` / `Obj Refs Name=...` to inspect references.

### 12. 12_CustomLoader

- Level: `Content/Tutorials/12_CustomLoader/CustomLoader.umap`
- Main script: `Content/Script/Tutorials/12_CustomLoader.lua`
- Demonstrates: A custom Lua file loader via the `FUnLuaNovaDelegates::CustomLoadLuaFile` delegate.
- Key points: `ReceiveBeginPlay` calls `SetupCustomLoader(1)` (fixed-path lookup), `SetupCustomLoader(2)` (lookup via `package.path`, after `package.loaded["Tutorials"] = nil` and appending `./?/Index.lua`), then `SetupCustomLoader(0)` to unbind. The two loader implementations live in `TutorialBlueprintFunctionLibrary.cpp`.

### 13. 13_AnimNotify

- Level: `Content/Tutorials/13_AnimNotify/AnimNotify.umap` (the folder also has `AN_FootStep.uasset`, `AM_Walk.uasset`, `BP_AnimNotifyTutorialCharacter.uasset`)
- Main script: `Content/Script/Tutorials/AN_FootStep.lua`
- Naming difference: For 01 through 12 the main script is a numbered `<NN>_<Name>.lua` matching the level folder; tutorial 13 has **no numbered main script**. Its script is `AN_FootStep.lua`, named after the anim-notify asset `AN_FootStep.uasset` in the level. This difference is kept as-is; there is no `13_AnimNotify.lua`.
- Demonstrates: Implementing an animation notify (AnimNotify) callback in Lua.
- Key points: The script is annotated `---@type AN_FootStep_C : UAnimNotify`, uses `local M = UnLuaNova.Class()`, and `function M:Received_Notify(MeshComp, Animation)` prints `foot step` and returns `true`.

## Further reading

- [Showcase Samples](Showcase.md) - runnable samples and automated screenshots
- [Quickstart for UE Newcomers](Quickstart_For_UE_Newbie.md) - creating a Blueprint, binding Lua, generating templates
- [Programming Guide](UnLuaNova_Programming_Guide.md) - writing real logic
