#include "efl/areas/NativeRoomBackend.h"

#ifndef EFL_STUB_SDK
#include "efl/bridge/instance_walker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/bridge/room_tracker.h"
#include "efl/ipc/pipe_writer.h"
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl {

NativeRoomBackend::NativeRoomBackend(bridge::InstanceWalker& instanceWalker,
                                     bridge::RoutineInvoker& routineInvoker,
                                     bridge::RoomTracker& roomTracker,
                                     PipeWriter* pipe,
                                     LogService& log,
                                     EventBus& events,
                                     NpcRegistry& npcs,
                                     StoryBridge& story,
                                     TriggerService& triggers,
                                     DiagnosticEmitter& diagnostics)
    : instanceWalker_(instanceWalker)
    , routineInvoker_(routineInvoker)
    , roomTracker_(roomTracker)
    , pipe_(pipe)
    , log_(log)
    , events_(events)
    , npcs_(npcs)
    , story_(story)
    , triggers_(triggers)
    , diagnostics_(diagnostics)
{
    roomTracker_.onRoomChange([this](const std::string& oldRoom, const std::string& newRoom) {
        if (state_ == State::AwaitingEntry && newRoom == nativeRoomName_) {
            populateRoom();
        } else if (state_ == State::Active && oldRoom == nativeRoomName_) {
            cleanupRoom();
        }
    });
}

void NativeRoomBackend::activate(const AreaDef& area) {
    if (state_ != State::Idle) {
        log_.warn("AREA", "NativeRoomBackend::activate called while not idle (state=" +
                  std::to_string(static_cast<int>(state_)) + ") — ignoring");
        return;
    }

    try {
        YYTK::RValue roomIdVal = routineInvoker_.callBuiltin("room_add", {});
        nativeRoomId_ = static_cast<int>(roomIdVal.m_Real);

        routineInvoker_.callBuiltin("room_set_width",
            {YYTK::RValue(static_cast<double>(nativeRoomId_)),
             YYTK::RValue(static_cast<double>(area.roomWidth))});
        routineInvoker_.callBuiltin("room_set_height",
            {YYTK::RValue(static_cast<double>(nativeRoomId_)),
             YYTK::RValue(static_cast<double>(area.roomHeight))});
        routineInvoker_.callBuiltin("room_set_persistent",
            {YYTK::RValue(static_cast<double>(nativeRoomId_)),
             YYTK::RValue(0.0)});

        YYTK::RValue nameVal = routineInvoker_.callBuiltin("room_get_name",
            {YYTK::RValue(static_cast<double>(nativeRoomId_))});
        nativeRoomName_ = nameVal.ToString();

        pendingArea_      = area;
        pendingExitEvent_ = area.exitEvent;
        state_            = State::AwaitingEntry;

        log_.info("AREA", "NativeRoomBackend created room '" + nativeRoomName_ +
                  "' (id=" + std::to_string(nativeRoomId_) + ") for area '" + area.id + "'" +
                  " [" + std::to_string(area.roomWidth) + "x" + std::to_string(area.roomHeight) + "]");

        if (pipe_) {
            pipe_->write("area.creating", nlohmann::json{
                {"areaId", area.id}, {"nativeRoomId", nativeRoomId_},
                {"nativeRoomName", nativeRoomName_}});
        }

        routineInvoker_.callBuiltin("room_goto",
            {YYTK::RValue(static_cast<double>(nativeRoomId_))});

    } catch (const std::exception& ex) {
        diagnostics_.emit("AREA-E001", Severity::Error, "AREA",
                          "Failed to create native room for area '" + area.id + "': " +
                          std::string(ex.what()),
                          "Check that room_add() is available and the area roomWidth/roomHeight are valid");
        log_.error("AREA", "NativeRoomBackend room creation failed: " + std::string(ex.what()));
        nativeRoomId_   = -1;
        nativeRoomName_.clear();
        state_ = State::Idle;
    }
}

void NativeRoomBackend::deactivate() {
    // Bootstrap calls this when the player leaves hostRoom (the trigger room).
    // That transition is what initiates navigation to the native room, so in the
    // normal flow we arrive here in AwaitingEntry — navigation is already in flight.
    // The Active case is handled internally by cleanupRoom() via the room tracker.
    if (state_ == State::Idle) {
        return;
    }
    if (state_ == State::AwaitingEntry) {
        // Navigation was cancelled before we arrived (unusual — e.g., another room change
        // happened before room_goto completed). Clean up the dangling room.
        log_.warn("AREA", "NativeRoomBackend: navigation to '" + nativeRoomName_ +
                  "' cancelled before arrival — deleting room");
        try {
            routineInvoker_.callBuiltin("room_delete",
                {YYTK::RValue(static_cast<double>(nativeRoomId_))});
        } catch (...) {}
        nativeRoomId_   = -1;
        nativeRoomName_.clear();
        state_ = State::Idle;
    }
    // State::Active: departure detected via room-tracker callback → cleanupRoom().
    // No action needed here; bootstrap's deactivate call for Active would be a logic
    // error (no area has hostRoom == nativeRoomName_), but we don't crash on it.
}

void NativeRoomBackend::populateRoom() {
    state_ = State::Active;

    log_.info("AREA", "NativeRoomBackend populating area '" + pendingArea_.id +
              "' in room '" + nativeRoomName_ + "'");

    // Clear any default instances in the new room (defensive — native rooms should be empty)
    try {
        auto defaultInstances = instanceWalker_.getAll("obj_interactable");
        for (auto* inst : defaultInstances) {
            routineInvoker_.callBuiltin("instance_destroy", {YYTK::RValue(inst)});
        }
    } catch (const std::exception& ex) {
        log_.warn("AREA", "Failed to clear default instances: " + std::string(ex.what()));
    }

    // Set music
    if (!pendingArea_.music.empty()) {
        try {
            YYTK::RValue musicAsset = routineInvoker_.callBuiltin("asset_get_index",
                {YYTK::RValue(pendingArea_.music.c_str())});
            routineInvoker_.callBuiltin("audio_play_music", {musicAsset});
            log_.info("AREA", "Set music: " + pendingArea_.music);
        } catch (const std::exception& ex) {
            log_.warn("AREA", "Failed to set music '" + pendingArea_.music + "': " +
                      std::string(ex.what()));
        }
    }

    // Spawn NPCs
    auto npcs = npcs_.npcsInArea(pendingArea_.id);
    for (const auto* npc : npcs) {
        if (!npc->unlockTrigger.empty() && !triggers_.evaluate(npc->unlockTrigger)) {
            log_.info("NPC", "NPC '" + npc->id + "' locked (trigger: " +
                      npc->unlockTrigger + ")");
            continue;
        }

        try {
            auto commaPos = npc->spawnAnchor.find(',');
            if (commaPos == std::string::npos) {
                log_.warn("NPC", "Invalid spawnAnchor for NPC '" + npc->id + "': " +
                          npc->spawnAnchor);
                continue;
            }
            double x = std::stod(npc->spawnAnchor.substr(0, commaPos));
            double y = std::stod(npc->spawnAnchor.substr(commaPos + 1));

            YYTK::RValue npcObj = routineInvoker_.callBuiltin("asset_get_index",
                {YYTK::RValue("par_NPC")});
            YYTK::RValue layerName(YYTK::RValue("Instances"));
            routineInvoker_.callBuiltin("instance_create_layer",
                {YYTK::RValue(x), YYTK::RValue(y), layerName, npcObj});

            log_.info("NPC", "Spawned NPC '" + npc->id + "' at (" +
                      std::to_string(x) + ", " + std::to_string(y) + ")");

            if (pipe_) {
                pipe_->write("npc.spawned", nlohmann::json{
                    {"npcId", npc->id}, {"areaId", pendingArea_.id},
                    {"x", x}, {"y", y}});
            }
        } catch (const std::exception& ex) {
            log_.warn("NPC", "Failed to spawn NPC '" + npc->id + "': " +
                      std::string(ex.what()));
        }
    }

    // TODO: teleport player to entryAnchor (needs YYTK SetBuiltin on obj_player)

    events_.publish("area.activated", nlohmann::json{
        {"areaId", pendingArea_.id}, {"nativeRoomName", nativeRoomName_}});

    if (pipe_) {
        pipe_->write("area.activated", nlohmann::json{
            {"areaId", pendingArea_.id}, {"nativeRoomId", nativeRoomId_},
            {"nativeRoomName", nativeRoomName_}});
    }

    if (!pendingArea_.entryEvent.empty()) {
        story_.fireEffects(pendingArea_.entryEvent, triggers_);
    }
}

void NativeRoomBackend::cleanupRoom() {
    log_.info("AREA", "NativeRoomBackend deactivating area '" + pendingArea_.id +
              "' — leaving room '" + nativeRoomName_ + "'");

    if (!pendingExitEvent_.empty()) {
        story_.fireEffects(pendingExitEvent_, triggers_);
        pendingExitEvent_.clear();
    }

    if (pipe_) {
        pipe_->write("area.deactivated", nlohmann::json{
            {"areaId", pendingArea_.id}, {"nativeRoomId", nativeRoomId_}});
    }

    try {
        routineInvoker_.callBuiltin("room_delete",
            {YYTK::RValue(static_cast<double>(nativeRoomId_))});
        log_.info("AREA", "Deleted native room '" + nativeRoomName_ +
                  "' (id=" + std::to_string(nativeRoomId_) + ")");
    } catch (const std::exception& ex) {
        log_.warn("AREA", "room_delete failed for '" + nativeRoomName_ + "': " +
                  std::string(ex.what()));
    }

    nativeRoomId_   = -1;
    nativeRoomName_.clear();
    state_ = State::Idle;
}

} // namespace efl
#endif // EFL_STUB_SDK
