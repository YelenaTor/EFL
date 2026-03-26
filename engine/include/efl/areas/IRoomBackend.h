#pragma once

#include "efl/registries/area_registry.h"

namespace efl {

class IRoomBackend {
public:
    virtual ~IRoomBackend() = default;
    virtual void activate(const AreaDef& area) = 0;
    virtual void deactivate() = 0;
    virtual bool supportsNativeRooms() const = 0;
};

} // namespace efl
