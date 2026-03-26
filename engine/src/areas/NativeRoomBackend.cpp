#include "efl/areas/NativeRoomBackend.h"

#ifndef EFL_STUB_SDK
#include "efl/bridge/instance_walker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/ipc/pipe_writer.h"

namespace efl {

NativeRoomBackend::NativeRoomBackend(bridge::InstanceWalker& instanceWalker,
                                     bridge::RoutineInvoker& routineInvoker,
                                     PipeWriter* pipe,
                                     LogService& log,
                                     EventBus& events,
                                     NpcRegistry& npcs,
                                     TriggerService& triggers,
                                     DiagnosticEmitter& diagnostics)
    : log_(log)
    , diagnostics_(diagnostics)
    , fallback_(instanceWalker, routineInvoker, pipe, log, events, npcs, triggers, diagnostics)
{}

void NativeRoomBackend::activate(const AreaDef& area) {
    // Native room registration is not yet implemented; fall back to hijacked backend.
    // Emit once per activation so mod authors know to use areaBackend "hijacked" to suppress.
    diagnostics_.emit("AREA-H001", Severity::Hazard, "AREA",
                      "Native rooms not yet available in this build — falling back to HijackedRoomBackend",
                      "Set areaBackend to \"hijacked\" in manifest settings to suppress this warning");
    log_.warn("AREA", "NativeRoomBackend not yet available — delegating to HijackedRoomBackend");
    fallback_.activate(area);
}

void NativeRoomBackend::deactivate() {
    fallback_.deactivate();
}

bool NativeRoomBackend::supportsNativeRooms() const {
    return false;
}

} // namespace efl
#endif // EFL_STUB_SDK
