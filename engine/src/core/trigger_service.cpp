#include "efl/core/trigger_service.h"
#include <stdexcept>

namespace efl {

void TriggerService::registerTrigger(const TriggerDef& def) {
    std::lock_guard<std::mutex> lock(mutex_);
    triggers_[def.id] = def;
}

void TriggerService::registerFromJson(const nlohmann::json& j) {
    TriggerDef def;
    def.id = j.at("id").get<std::string>();

    std::string typeStr = j.at("type").get<std::string>();
    if (typeStr == "allOf")           def.type = TriggerType::AllOf;
    else if (typeStr == "anyOf")      def.type = TriggerType::AnyOf;
    else if (typeStr == "questComplete") def.type = TriggerType::QuestComplete;
    else                              def.type = TriggerType::FlagSet;

    if (j.contains("flagName"))   def.flagName  = j["flagName"].get<std::string>();
    if (j.contains("questId"))    def.questId   = j["questId"].get<std::string>();
    if (j.contains("conditions")) def.conditions = j["conditions"].get<std::vector<std::string>>();

    registerTrigger(def);
}

bool TriggerService::evaluate(const std::string& triggerId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = triggers_.find(triggerId);
    if (it == triggers_.end()) return false;

    const TriggerDef& def = it->second;

    switch (def.type) {
        case TriggerType::FlagSet: {
            auto fi = flags_.find(def.flagName);
            return fi != flags_.end() && fi->second;
        }
        case TriggerType::QuestComplete:
            return completedQuests_.count(def.questId) > 0;
        case TriggerType::AllOf:
            for (const auto& cond : def.conditions) {
                if (!evaluateCondition(cond)) return false;
            }
            return true;
        case TriggerType::AnyOf:
            for (const auto& cond : def.conditions) {
                if (evaluateCondition(cond)) return true;
            }
            return false;
    }
    return false;
}

// Must be called with mutex_ already held.
bool TriggerService::evaluateCondition(const std::string& cond) const {
    constexpr std::string_view flagPrefix = "flag:";
    if (cond.starts_with(flagPrefix)) {
        std::string name = cond.substr(flagPrefix.size());
        auto fi = flags_.find(name);
        return fi != flags_.end() && fi->second;
    }
    // Otherwise treat as a trigger id — look it up (mutex already held, call inner logic directly)
    auto it = triggers_.find(cond);
    if (it == triggers_.end()) return false;

    const TriggerDef& def = it->second;
    switch (def.type) {
        case TriggerType::FlagSet: {
            auto fi = flags_.find(def.flagName);
            return fi != flags_.end() && fi->second;
        }
        case TriggerType::QuestComplete:
            return completedQuests_.count(def.questId) > 0;
        case TriggerType::AllOf:
            for (const auto& c : def.conditions) {
                if (!evaluateCondition(c)) return false;
            }
            return true;
        case TriggerType::AnyOf:
            for (const auto& c : def.conditions) {
                if (evaluateCondition(c)) return true;
            }
            return false;
    }
    return false;
}

void TriggerService::setFlag(const std::string& name, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    flags_[name] = value;
}

bool TriggerService::getFlag(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = flags_.find(name);
    return it != flags_.end() && it->second;
}

void TriggerService::markQuestComplete(const std::string& questId) {
    std::lock_guard<std::mutex> lock(mutex_);
    completedQuests_.insert(questId);
}

bool TriggerService::isQuestComplete(const std::string& questId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completedQuests_.count(questId) > 0;
}

nlohmann::json TriggerService::serializeFlags() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [name, val] : flags_) {
        j[name] = val;
    }
    return j;
}

void TriggerService::deserializeFlags(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, val] : j.items()) {
        flags_[key] = val.get<bool>();
    }
}

} // namespace efl
