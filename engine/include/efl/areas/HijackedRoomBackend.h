#pragma once

#include "efl/areas/IRoomBackend.h"
#include "efl/core/log_service.h"
#include "efl/core/event_bus.h"
#include "efl/core/diagnostics.h"
#include "efl/registries/npc_registry.h"
#include "efl/core/trigger_service.h"

#ifndef EFL_STUB_SDK
namespace efl::bridge {
    class InstanceWalker;
    class RoutineInvoker;
} // namespace efl::bridge

namespace efl {

class HijackedRoomBackend : public IRoomBackend {
public:
    HijackedRoomBackend(bridge::InstanceWalker& instanceWalker,
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
    bridge::InstanceWalker& instanceWalker_;
    bridge::RoutineInvoker& routineInvoker_;
    PipeWriter* pipe_;
    LogService& log_;
    EventBus& events_;
    NpcRegistry& npcs_;
    TriggerService& triggers_;
    DiagnosticEmitter& diagnostics_;
};

} // namespace efl
#endif // EFL_STUB_SDK
