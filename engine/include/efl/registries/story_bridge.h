#pragma once
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
    // Emits story.fired IPC. Actual game engine call is stubbed until
    // the FoM story_start script name is confirmed via discovery tools.
    void fireEvent(const std::string& eventId, const TriggerService& triggers);

private:
    PipeWriter* pipe_ = nullptr;
    std::vector<EventDef> events_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
