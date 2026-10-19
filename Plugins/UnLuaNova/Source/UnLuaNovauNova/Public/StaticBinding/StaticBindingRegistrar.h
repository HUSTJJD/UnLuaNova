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

#define UnLuaNovaNOVA_SB_CONCAT_INNER(A, B) A##B
#define UnLuaNovaNOVA_SB_CONCAT(A, B) UnLuaNovaNOVA_SB_CONCAT_INNER(A, B)

/** Emitted at the end of each generated binding table. */
#define UnLuaNovaNOVA_REGISTER_STATIC_BINDING(TypeName) \
	static FStaticBindingAutoRegistrar UnLuaNovaNOVA_SB_CONCAT(GStaticBindingRegistrar_, TypeName)(&TypeBinding_##TypeName)
