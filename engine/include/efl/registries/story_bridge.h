#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class TriggerService;

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
    void registerEvent(const EventDef& def);
    const EventDef* getEvent(const std::string& id) const;
    bool canFire(const std::string& eventId, const TriggerService& triggers) const;
    const std::vector<EventDef>& allEvents() const;

private:
    std::vector<EventDef> events_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
