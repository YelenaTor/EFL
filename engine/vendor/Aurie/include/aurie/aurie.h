#pragma once
#include <cstdint>
#include <string>
#include <functional>

// Minimal Aurie SDK stubs for EFL compilation
// Replace with real Aurie SDK when vendoring

namespace Aurie {

using AurieStatus = int32_t;
constexpr AurieStatus AURIE_SUCCESS = 0;
constexpr AurieStatus AURIE_MODULE_NOT_FOUND = -1;

struct AurieModule {
    std::string name;
    std::string version;
};

using ModuleInitCallback = std::function<AurieStatus()>;
using ModuleUnloadCallback = std::function<void()>;

#define EXPORTED_AURIE_MODULE(init_fn, unload_fn)

} // namespace Aurie
