#pragma once

// Layer C: Boolean condition evaluation for unlocks

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace efl {

enum class TriggerType { AllOf, AnyOf, QuestComplete, FlagSet };

struct TriggerDef {
    std::string id;
    TriggerType type = TriggerType::FlagSet;
    std::string flagName;                    // for FlagSet
    std::string questId;                     // for QuestComplete
    std::vector<std::string> conditions;     // for AllOf/AnyOf — refs to other triggers or "flag:name"
};

class TriggerService {
public:
    void registerTrigger(const TriggerDef& def);
    void registerFromJson(const nlohmann::json& j);

    // Returns true if the trigger references form a cycle. Called automatically
    // during registerTrigger(); exposed for testing.
    bool hasCycle(const std::string& triggerId) const;

    bool evaluate(const std::string& triggerId) const;

    void setFlag(const std::string& name, bool value);
    bool getFlag(const std::string& name) const;

    void markQuestComplete(const std::string& questId);
    bool isQuestComplete(const std::string& questId) const;

    nlohmann::json serializeFlags() const;
    void deserializeFlags(const nlohmann::json& j);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TriggerDef> triggers_;
    std::unordered_map<std::string, bool> flags_;
    std::unordered_set<std::string> completedQuests_;

    bool evaluateCondition(const std::string& cond) const;
    bool hasCycleLocked(const std::string& id) const;
    bool detectCycle(const std::string& id, std::unordered_set<std::string>& visited) const;
};

} // namespace efl
