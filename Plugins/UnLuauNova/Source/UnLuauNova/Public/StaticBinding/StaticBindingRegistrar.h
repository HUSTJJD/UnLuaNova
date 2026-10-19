#pragma once

#include "StaticBinding/StaticBindingRegistry.h"

/**
 * Registers a generated binding table from a static initialiser.
 *
 * Registration itself is free of UObject work; the registry defers all resolution until
 * the first lookup, so this is safe to run at DLL load time.
 */
struct FStaticBindingAutoRegistrar
{
	explicit FStaticBindingAutoRegistrar(const FStaticTypeBinding* Type)
	{
		FStaticBindingRegistry::Get().Register(Type);
	}
};

#define UNLUA_SB_CONCAT_INNER(A, B) A##B
#define UNLUA_SB_CONCAT(A, B) UNLUA_SB_CONCAT_INNER(A, B)

/** Emitted at the end of each generated binding table. */
#define UNLUA_REGISTER_STATIC_BINDING(TypeName) \
	static FStaticBindingAutoRegistrar UNLUA_SB_CONCAT(GStaticBindingRegistrar_, TypeName)(&TypeBinding_##TypeName)
