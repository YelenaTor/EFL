#pragma once

#include "efl/areas/IRoomBackend.h"
#include "efl/core/diagnostics.h"
#include "efl/core/log_service.h"

#ifndef EFL_STUB_SDK
#include "efl/areas/HijackedRoomBackend.h"

namespace efl {

// NativeRoomBackend: placeholder for true custom-room registration (not yet available).
// Emits AREA-H001 on every activation and delegates to HijackedRoomBackend.
class NativeRoomBackend : public IRoomBackend {
public:
    NativeRoomBackend(bridge::InstanceWalker& instanceWalker,
                      bridge::RoutineInvoker& routineInvoker,
                      PipeWriter* pipe,
                      LogService& log,
                      EventBus& events,
                      NpcRegistry& npcs,
                      TriggerService& triggers,
                      DiagnosticEmitter& diagnostics);

    void activate(const AreaDef& area) override;
    void deactivate() override;
    bool supportsNativeRooms() const override;

private:
    LogService& log_;
    DiagnosticEmitter& diagnostics_;
    HijackedRoomBackend fallback_;
};

} // namespace efl
#endif // EFL_STUB_SDK
