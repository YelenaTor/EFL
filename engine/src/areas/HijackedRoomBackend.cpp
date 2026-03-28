#include "efl/areas/HijackedRoomBackend.h"

#ifndef EFL_STUB_SDK
#include "efl/bridge/instance_walker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/ipc/pipe_writer.h"
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl {

HijackedRoomBackend::HijackedRoomBackend(bridge::InstanceWalker& instanceWalker,
                                         bridge::RoutineInvoker& routineInvoker,
                                         PipeWriter* pipe,
                                         LogService& log,
                                         EventBus& events,
                                         NpcRegistry& npcs,
                                         StoryBridge& story,
                                         TriggerService& triggers,
                                         DiagnosticEmitter& diagnostics)
    : instanceWalker_(instanceWalker)
    , routineInvoker_(routineInvoker)
    , pipe_(pipe)
    , log_(log)
    , events_(events)
    , npcs_(npcs)
    , story_(story)
    , triggers_(triggers)
    , diagnostics_(diagnostics)
{}

void HijackedRoomBackend::activate(const AreaDef& area) {
    log_.info("AREA", "Activating EFL area '" + area.id +
              "' in hijacked room: " + area.hostRoom);

    try {
        auto defaultInstances = instanceWalker_.getAll("obj_interactable");
        for (auto* inst : defaultInstances) {
            routineInvoker_.callBuiltin("instance_destroy", {YYTK::RValue(inst)});
        }
        log_.info("AREA", "Cleared " + std::to_string(defaultInstances.size()) +
                  " default instances in " + area.hostRoom);
    } catch (const std::exception& ex) {
        log_.warn("AREA", "Failed to clear default instances: " + std::string(ex.what()));
    }

    if (!area.music.empty()) {
        try {
            YYTK::RValue musicAsset = routineInvoker_.callBuiltin("asset_get_index",
                {YYTK::RValue(area.music.c_str())});
            routineInvoker_.callBuiltin("audio_play_music", {musicAsset});
            log_.info("AREA", "Set music: " + area.music);
        } catch (const std::exception& ex) {
            log_.warn("AREA", "Failed to set music '" + area.music + "': " + std::string(ex.what()));
        }
    }

    events_.publish("area.activated", nlohmann::json{
        {"areaId", area.id}, {"hostRoom", area.hostRoom}});

    if (pipe_) {
        pipe_->write("area.activated", nlohmann::json{
            {"areaId", area.id}, {"hostRoom", area.hostRoom}});
    }

    auto npcs = npcs_.npcsInArea(area.id);
    for (const auto* npc : npcs) {
        // Check unlock trigger — skip NPCs that aren't visible yet
        if (!npc->unlockTrigger.empty() &&
            !triggers_.evaluate(npc->unlockTrigger)) {
            log_.info("NPC", "NPC '" + npc->id + "' locked (trigger: " +
                      npc->unlockTrigger + ")");
            continue;
        }

        // Parse spawn anchor "x,y" format
        try {
            auto commaPos = npc->spawnAnchor.find(',');
            if (commaPos == std::string::npos) {
                log_.warn("NPC", "Invalid spawnAnchor for NPC '" + npc->id +
                          "': " + npc->spawnAnchor);
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
                    {"npcId", npc->id}, {"areaId", area.id},
                    {"x", x}, {"y", y}});
            }
        } catch (const std::exception& ex) {
            log_.warn("NPC", "Failed to spawn NPC '" + npc->id + "': " +
                      std::string(ex.what()));
        }
    }

    if (!area.entryEvent.empty()) {
        story_.fireEvent(area.entryEvent, triggers_);
    }
    pendingExitEvent_ = area.exitEvent;
}

void HijackedRoomBackend::deactivate() {
    if (!pendingExitEvent_.empty()) {
        story_.fireEvent(pendingExitEvent_, triggers_);
        pendingExitEvent_.clear();
    }
}

bool HijackedRoomBackend::supportsNativeRooms() const {
    return false;
}

} // namespace efl
#endif // EFL_STUB_SDK
