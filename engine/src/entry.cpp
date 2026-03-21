#ifdef EFL_STUB_SDK

// Stub entry point — no-op when building tests

#else

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>
#include "efl/core/bootstrap.h"

static efl::EflBootstrap g_efl;
static YYTK::YYTKInterface* g_yytk = nullptr;

// Aurie calls ModuleInitialize after __AurieFrameworkInit sets up globals.
// g_ArSelfModule is already populated by the time this runs.
EXPORTED Aurie::AurieStatus ModuleInitialize(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    // Acquire YYTK interface
    Aurie::AurieStatus status = Aurie::ObGetInterface(
        "YYTK_Main",
        reinterpret_cast<Aurie::AurieInterfaceBase*&>(g_yytk)
    );

    if (!Aurie::AurieSuccess(status) || !g_yytk) {
        return Aurie::AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }

    // Module lives at <game>/mods/aurie/EFL.dll
    // Content packs live at <game>/mods/efl/
    auto contentDir = ModulePath.parent_path().parent_path() / "efl";

    if (!g_efl.initialize(contentDir.string(), Module, g_yytk)) {
        return Aurie::AURIE_MODULE_INITIALIZATION_FAILED;
    }

    return Aurie::AURIE_SUCCESS;
}

EXPORTED Aurie::AurieStatus ModuleUnload(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    g_efl.shutdown();
    return Aurie::AURIE_SUCCESS;
}

#endif // EFL_STUB_SDK
