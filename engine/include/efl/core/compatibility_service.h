#pragma once

// Layer C: Version compatibility checks

#include <string>

namespace efl {

class CompatibilityService {
public:
    // Check if runningVersion satisfies requiredVersion
    // Same major version required, running >= required
    static bool isCompatible(const std::string& runningVersion, const std::string& requiredVersion);

    // Check if an external mod (by modId) is currently loaded in the Aurie module system.
    // In EFL_STUB_SDK builds, always returns false (no game runtime present).
    static bool isExternalModLoaded(const std::string& modId);

private:
    struct SemVer {
        int major = 0, minor = 0, patch = 0;
    };
    static SemVer parse(const std::string& version);
};

} // namespace efl
