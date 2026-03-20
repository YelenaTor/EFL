#include "efl/core/compatibility_service.h"

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

} // namespace efl
