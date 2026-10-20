---
title: "How To Implement Overriding"
description: "How Blueprint events get replaced by Lua implementations"
---

# Overview
The main feature of UnLuaNova is overriding 'BlueprintEvent', 'AnimNotify', 'RepNotify', 'InputEvent' without helper codes. This document will introduce the two methods to implement overriding.

---

# Thunk Function Replacement

## Thunk function of UFunction

![THUNK_FUNC](../Images/ufunction_thunk_func.png)

'Func' is the **thunk** function.

## UFunction invoking

![UOBJECT_PROCESSEVENT](../Images/uobject_processevent.png)

![UFUNCTION_INVOKE](../Images/ufunction_invoke.png)

If replacing the engine's default thunk function with a customized thunk function that calling Lua function, we can override the UFunction.

---

# Opcode Injection

There is another path to call non-native UFunction:

![UOBJECT_CALLFUNCTION](../Images/uobject_callfunction.png)

Replacing thunk function is useless in this case, and we can inject special Opcodes to UFunction to implement overriding.

## New opcode definition

![NEW_OPCODE](../Images/new_opcode.png)

## New opcode registering

![OPCODE_REGISTRAR](../Images/opcode_registrar.png)

## Injection

![OPCODE_INJECTION](../Images/opcode_injection.png)

![CUSTOMIZED_THUNK_FUNC](../Images/customized_thunk_func.png)

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) — Overriding from the Lua side
- [API Reference](API.md) — API reference
- [FAQ](FAQ.md) — Frequently asked questions
