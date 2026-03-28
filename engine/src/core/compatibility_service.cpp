#include "efl/core/compatibility_service.h"

#ifndef EFL_STUB_SDK
#include <Aurie/shared.hpp>
#endif

#include <sstream>

namespace efl {

CompatibilityService::SemVer CompatibilityService::parse(const std::string& version) {
    SemVer v;
    char dot1, dot2;
    std::istringstream ss(version);
    if (!(ss >> v.major >> dot1 >> v.minor >> dot2 >> v.patch))
        return {0, 0, 0};
    if (dot1 != '.' || dot2 != '.')
        return {0, 0, 0};
    return v;
}

bool CompatibilityService::isCompatible(const std::string& runningVersion,
                                        const std::string& requiredVersion) {
    SemVer running = parse(runningVersion);
    SemVer required = parse(requiredVersion);

    // Major version must match
    if (running.major != required.major)
        return false;

    // Running version must be >= required version
    if (running.minor > required.minor)
        return true;
    if (running.minor < required.minor)
        return false;

    return running.patch >= required.patch;
}

bool CompatibilityService::isExternalModLoaded(const std::string& modId) {
#ifndef EFL_STUB_SDK
    // Convention: mods that want to be discoverable as EFL dependencies must call
    //   Aurie::ObCreateInterface(module, &myInterface, modId.c_str())
    // at init time. EFL checks for that interface by name.
    return Aurie::ObInterfaceExists(modId.c_str());
#else
    // In stub/test mode there is no game runtime — treat all external deps as absent.
    (void)modId;
    return false;
#endif
}

} // namespace efl
