#include <aurie/aurie.h>
#include "efl/core/bootstrap.h"

static efl::EflBootstrap g_efl;

static Aurie::AurieStatus EflInit() {
    if (!g_efl.initialize(".")) {
        return Aurie::AURIE_MODULE_NOT_FOUND;
    }
    return Aurie::AURIE_SUCCESS;
}

static void EflUnload() {
    g_efl.shutdown();
}

EXPORTED_AURIE_MODULE(EflInit, EflUnload)
