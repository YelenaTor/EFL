#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include "efl/ipc/pipe_writer.h"

namespace efl {

std::optional<EventDef> EventDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id")) {
        return std::nullopt;
    }

    EventDef def;
    def.id = j.at("id").get<std::string>();

    if (j.contains("mode")) {
        const std::string& mode = j.at("mode").get<std::string>();
        if (mode == "custom") {
            def.mode = EventMode::Custom;
        } else {
            def.mode = EventMode::NativeBridge;
        }
    }

    if (j.contains("trigger")) {
        def.trigger = j.at("trigger").get<std::string>();
    }

    if (j.contains("commands")) {
        for (const auto& cmdJson : j.at("commands")) {
            EventCommand cmd;
            cmd.type = cmdJson.at("type").get<std::string>();
            if (cmdJson.contains("npc"))  cmd.npc  = cmdJson.at("npc").get<std::string>();
            if (cmdJson.contains("line")) cmd.line = cmdJson.at("line").get<std::string>();
            if (cmdJson.contains("flag")) cmd.flag = cmdJson.at("flag").get<std::string>();
            def.commands.push_back(cmd);
        }
    }

    return def;
}

void StoryBridge::registerEvent(const EventDef& def) {
    if (index_.count(def.id)) {
        events_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = events_.size();
    events_.push_back(def);
}

const EventDef* StoryBridge::getEvent(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &events_[it->second];
}

bool StoryBridge::canFire(const std::string& eventId, const TriggerService& triggers) const {
    auto it = index_.find(eventId);
    if (it == index_.end()) return false;

    const EventDef& def = events_[it->second];
    return def.trigger.empty() || triggers.evaluate(def.trigger);
}

const std::vector<EventDef>& StoryBridge::allEvents() const {
    return events_;
}

void StoryBridge::setPipeWriter(PipeWriter* pipe) {
    pipe_ = pipe;
}

void StoryBridge::fireEvent(const std::string& eventId, const TriggerService& triggers) {
    if (!canFire(eventId, triggers))
        return;

    // Emit IPC so the TUI monitor shows the fired event.
    if (pipe_) {
        pipe_->write("story.fired", nlohmann::json{
            {"eventId", eventId},
            {"status", "stub"}
        });
    }

    // TODO(v2.3+): call RoutineInvoker::invoke("gml_Script_story_start", {eventId})
    // once the FoM script name is confirmed via discover_scripts.py output.
    // Until then the event is acknowledged by EFL but not fired in the game engine.
}

} // namespace efl
