#pragma once
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class TriggerService;
class PipeWriter;

enum class EventMode { NativeBridge, Custom };

struct EventCommand {
    std::string type;
    std::string npc;
    std::string line;
    std::string flag;
    std::string quest;
};

struct EventDef {
    std::string id;
    EventMode mode = EventMode::NativeBridge;
    std::string trigger;
    std::vector<EventCommand> commands;

    static std::optional<EventDef> fromJson(const nlohmann::json& j);
};

class StoryBridge {
public:
    void setPipeWriter(PipeWriter* pipe);

    void registerEvent(const EventDef& def);
    const EventDef* getEvent(const std::string& id) const;
    bool canFire(const std::string& eventId, const TriggerService& triggers) const;
    const std::vector<EventDef>& allEvents() const;

    // Fire an event if its trigger conditions are met.
    // Processes set_flag/clear_flag commands via TriggerService, emits IPC for
    // dialogue/quest commands. NativeBridge FoM StoryExecutor call deferred until
    // gml_Script_story_start name is confirmed.
    void fireEvent(const std::string& eventId, TriggerService& triggers);

    // Callbacks wired by bootstrap to forward quest commands to QuestRegistry.
    std::function<void(const std::string& questId)> onQuestStart;
    std::function<void(const std::string& questId)> onQuestAdvance;

private:
    PipeWriter* pipe_ = nullptr;
    std::vector<EventDef> events_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
