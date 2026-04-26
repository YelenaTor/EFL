#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

enum class QuestState { NotStarted, Active, Completed };

struct QuestObjective {
    std::string type;   // "collect", "talkTo", etc.
    std::string item;
    std::string npc;
    int count = 0;
};

struct QuestAction {
    std::string type;   // "setFlag", etc.
    std::string flag;
};

struct QuestStage {
    std::string id;
    std::vector<QuestObjective> objectives;
    std::vector<QuestAction> onComplete;
};

struct QuestReward {
    std::string type;   // "item"
    std::string item;   // human-readable name for reference
    int itemId = 0;     // numeric ID from t2_input.json items list (0 = unset)
    int count  = 1;
};

struct QuestDef {
    std::string id;
    std::string title;
    std::vector<QuestStage> stages;
    std::vector<QuestReward> rewards;
    std::string unlockTrigger; // optional trigger gating

    static std::optional<QuestDef> fromJson(const nlohmann::json& j);
};

class QuestRegistry {
public:
    void registerQuest(const QuestDef& def);
    const QuestDef* getQuest(const std::string& id) const;

    void startQuest(const std::string& questId);
    void completeStage(const std::string& questId, const std::string& stageId);

    QuestState getQuestState(const std::string& questId) const;
    std::string getCurrentStage(const std::string& questId) const;

    std::vector<std::string> activeQuestIds() const;
    std::vector<std::string> completedQuestIds() const;

    // Wired by bootstrap to call give_item@Ari@Ari via RoutineInvoker.
    std::function<void(int itemId, int qty)> onItemGrant;

private:
    std::vector<QuestDef> quests_;
    std::unordered_map<std::string, size_t> index_;

    struct QuestProgress {
        QuestState state = QuestState::NotStarted;
        size_t currentStageIdx = 0;
    };
    std::unordered_map<std::string, QuestProgress> progress_;
};

} // namespace efl
