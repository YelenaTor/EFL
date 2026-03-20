#include "efl/registries/quest_registry.h"

namespace efl {

std::optional<QuestDef> QuestDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("stages")) {
        return std::nullopt;
    }

    QuestDef def;
    def.id = j.at("id").get<std::string>();

    if (j.contains("title")) {
        def.title = j.at("title").get<std::string>();
    }

    if (j.contains("unlockTrigger")) {
        def.unlockTrigger = j.at("unlockTrigger").get<std::string>();
    }

    for (const auto& stageJson : j.at("stages")) {
        QuestStage stage;
        stage.id = stageJson.at("id").get<std::string>();

        if (stageJson.contains("objectives")) {
            for (const auto& objJson : stageJson.at("objectives")) {
                QuestObjective obj;
                obj.type = objJson.at("type").get<std::string>();
                if (objJson.contains("item"))  obj.item  = objJson.at("item").get<std::string>();
                if (objJson.contains("npc"))   obj.npc   = objJson.at("npc").get<std::string>();
                if (objJson.contains("count")) obj.count = objJson.at("count").get<int>();
                stage.objectives.push_back(obj);
            }
        }

        if (stageJson.contains("onComplete")) {
            for (const auto& actionJson : stageJson.at("onComplete")) {
                QuestAction action;
                action.type = actionJson.at("type").get<std::string>();
                if (actionJson.contains("flag")) action.flag = actionJson.at("flag").get<std::string>();
                stage.onComplete.push_back(action);
            }
        }

        def.stages.push_back(stage);
    }

    if (j.contains("rewards")) {
        for (const auto& rewardJson : j.at("rewards")) {
            QuestReward reward;
            reward.type = rewardJson.at("type").get<std::string>();
            if (rewardJson.contains("item"))  reward.item  = rewardJson.at("item").get<std::string>();
            if (rewardJson.contains("count")) reward.count = rewardJson.at("count").get<int>();
            def.rewards.push_back(reward);
        }
    }

    return def;
}

void QuestRegistry::registerQuest(const QuestDef& def) {
    if (index_.count(def.id)) {
        quests_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = quests_.size();
    quests_.push_back(def);
}

const QuestDef* QuestRegistry::getQuest(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &quests_[it->second];
}

void QuestRegistry::startQuest(const std::string& questId) {
    if (!index_.count(questId)) return;
    auto& prog = progress_[questId];
    if (prog.state == QuestState::NotStarted) {
        prog.state = QuestState::Active;
        prog.currentStageIdx = 0;
    }
}

void QuestRegistry::completeStage(const std::string& questId, const std::string& stageId) {
    auto questIt = index_.find(questId);
    if (questIt == index_.end()) return;

    auto progIt = progress_.find(questId);
    if (progIt == progress_.end()) return;

    auto& prog = progIt->second;
    if (prog.state != QuestState::Active) return;

    const QuestDef& def = quests_[questIt->second];
    if (prog.currentStageIdx >= def.stages.size()) return;
    if (def.stages[prog.currentStageIdx].id != stageId) return;

    prog.currentStageIdx++;
    if (prog.currentStageIdx >= def.stages.size()) {
        prog.state = QuestState::Completed;
    }
}

QuestState QuestRegistry::getQuestState(const std::string& questId) const {
    auto it = progress_.find(questId);
    if (it == progress_.end()) return QuestState::NotStarted;
    return it->second.state;
}

std::string QuestRegistry::getCurrentStage(const std::string& questId) const {
    auto questIt = index_.find(questId);
    if (questIt == index_.end()) return {};

    auto progIt = progress_.find(questId);
    if (progIt == progress_.end()) return {};

    const auto& prog = progIt->second;
    if (prog.state != QuestState::Active) return {};

    const QuestDef& def = quests_[questIt->second];
    if (prog.currentStageIdx >= def.stages.size()) return {};
    return def.stages[prog.currentStageIdx].id;
}

std::vector<std::string> QuestRegistry::activeQuestIds() const {
    std::vector<std::string> result;
    for (const auto& [id, prog] : progress_) {
        if (prog.state == QuestState::Active) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> QuestRegistry::completedQuestIds() const {
    std::vector<std::string> result;
    for (const auto& [id, prog] : progress_) {
        if (prog.state == QuestState::Completed) {
            result.push_back(id);
        }
    }
    return result;
}

} // namespace efl
