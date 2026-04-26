#pragma once

#include "efl/areas/IRoomBackend.h"
#include "efl/core/log_service.h"
#include "efl/core/event_bus.h"
#include "efl/core/diagnostics.h"
#include "efl/registries/npc_registry.h"
#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include <string>

#ifndef EFL_STUB_SDK
namespace efl::bridge {
    class InstanceWalker;
    class RoutineInvoker;
    class RoomTracker;
} // namespace efl::bridge

namespace efl {

// NativeRoomBackend: creates a GML room at runtime via room_add() and navigates to it.
//
// Lifecycle (two-phase):
//   activate()     — called by bootstrap when player enters hostRoom (trigger room).
//                    Creates the room, calls room_goto(), transitions to AwaitingEntry.
//   onRoomChange   — fires when the native room becomes current. Populates instances.
//   cleanupRoom()  — fires when the native room is left. Fires exit event, room_delete().
//
// deactivate() is called by bootstrap when leaving hostRoom (while en route to the native
// room). It is a lifecycle-aware no-op for the Active case — cleanup is self-managed.
class NativeRoomBackend : public IRoomBackend {
public:
    NativeRoomBackend(bridge::InstanceWalker& instanceWalker,
                      bridge::RoutineInvoker& routineInvoker,
                      bridge::RoomTracker& roomTracker,
                      PipeWriter* pipe,
                      LogService& log,
                      EventBus& events,
                      NpcRegistry& npcs,
                      StoryBridge& story,
                      TriggerService& triggers,
                      DiagnosticEmitter& diagnostics);

    void activate(const AreaDef& area) override;
    void deactivate() override;
    bool supportsNativeRooms() const override { return true; }

private:
    enum class State { Idle, AwaitingEntry, Active };

    bridge::InstanceWalker& instanceWalker_;
    bridge::RoutineInvoker& routineInvoker_;
    bridge::RoomTracker&    roomTracker_;
    PipeWriter*             pipe_;
    LogService&             log_;
    EventBus&               events_;
    NpcRegistry&            npcs_;
    StoryBridge&            story_;
    TriggerService&         triggers_;
    DiagnosticEmitter&      diagnostics_;

    State       state_          = State::Idle;
    int         nativeRoomId_   = -1;
    std::string nativeRoomName_;
    AreaDef     pendingArea_;
    std::string pendingExitEvent_;

    void populateRoom();
    void cleanupRoom();
};

} // namespace efl
#endif // EFL_STUB_SDK
