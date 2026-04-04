#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include "efl/ipc/pipe_writer.h"
#include <stdexcept>

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
            if (cmdJson.contains("npc"))   cmd.npc   = cmdJson.at("npc").get<std::string>();
            if (cmdJson.contains("line"))  cmd.line  = cmdJson.at("line").get<std::string>();
            if (cmdJson.contains("flag"))  cmd.flag  = cmdJson.at("flag").get<std::string>();
            if (cmdJson.contains("quest")) cmd.quest = cmdJson.at("quest").get<std::string>();
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

void StoryBridge::fireEvent(const std::string& eventId, TriggerService& triggers) {
    if (!canFire(eventId, triggers))
        return;

    const EventDef& def = events_[index_.at(eventId)];

    // Process each command in sequence.
    for (const auto& cmd : def.commands) {
        if (cmd.type == "set_flag") {
            if (!cmd.flag.empty())
                triggers.setFlag(cmd.flag, true);
        } else if (cmd.type == "clear_flag") {
            if (!cmd.flag.empty())
                triggers.setFlag(cmd.flag, false);
        } else if (cmd.type == "dialogue") {
            // Actual in-game dialogue open is deferred (no confirmed FoM script name yet).
            // Emit IPC so the DevKit monitor can show dialogue activity.
            if (pipe_) {
                pipe_->write("dialogue.open", nlohmann::json{
                    {"eventId", eventId},
                    {"npcId",   cmd.npc},
                    {"lineId",  cmd.line}
                });
            }
        } else if (cmd.type == "start_quest") {
            if (!cmd.quest.empty() && onQuestStart)
                onQuestStart(cmd.quest);
            if (pipe_) {
                pipe_->write("quest.updated", nlohmann::json{
                    {"questId", cmd.quest}, {"action", "start"}});
            }
        } else if (cmd.type == "advance_quest") {
            if (!cmd.quest.empty() && onQuestAdvance)
                onQuestAdvance(cmd.quest);
            if (pipe_) {
                pipe_->write("quest.updated", nlohmann::json{
                    {"questId", cmd.quest}, {"action", "advance"}});
            }
        } else {
            // Unknown command type — emit a warning so DevKit can surface it.
            if (pipe_) {
                pipe_->write("story.warning", nlohmann::json{
                    {"eventId", eventId},
                    {"message", "Unknown command type: " + cmd.type}
                });
            }
        }
    }

    // NativeBridge: would invoke FoM's StoryExecutor here once script name confirmed.
    // Emit IPC trace so modders can see the event needs native wiring.
    if (def.mode == EventMode::NativeBridge) {
        if (pipe_) {
            pipe_->write("story.native_pending", nlohmann::json{
                {"eventId", eventId},
                {"note", "FoM StoryExecutor call deferred — gml_Script_story_start unconfirmed"}
            });
        }
    }

    if (pipe_) {
        pipe_->write("story.fired", nlohmann::json{
            {"eventId", eventId},
            {"commandCount", static_cast<int>(def.commands.size())},
            {"status", "ok"}
        });
    }
}

} // namespace efl
