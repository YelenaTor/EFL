#include "efl/registries/dialogue_service.h"

namespace efl {

std::optional<DialogueDef> DialogueDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("npc")) {
        return std::nullopt;
    }

    DialogueDef def;
    def.id  = j.at("id").get<std::string>();
    def.npc = j.at("npc").get<std::string>();

    if (j.contains("entries")) {
        for (const auto& e : j.at("entries")) {
            DialogueEntry entry;
            if (e.contains("id"))        entry.id        = e.at("id").get<std::string>();
            if (e.contains("text"))      entry.text      = e.at("text").get<std::string>();
            if (e.contains("portrait"))  entry.portrait  = e.at("portrait").get<std::string>();
            if (e.contains("condition")) entry.condition = e.at("condition").get<std::string>();
            def.entries.push_back(std::move(entry));
        }
    }

    return def;
}

void DialogueService::registerDialogue(const DialogueDef& def) {
    if (index_.count(def.id)) {
        dialogues_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = dialogues_.size();
    dialogues_.push_back(def);
}

const DialogueDef* DialogueService::getDialogue(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &dialogues_[it->second];
}

std::vector<const DialogueEntry*> DialogueService::availableEntries(
    const std::string& dialogueId,
    ConditionEvaluator evaluator) const
{
    std::vector<const DialogueEntry*> result;
    const DialogueDef* def = getDialogue(dialogueId);
    if (!def) return result;

    for (const auto& entry : def->entries) {
        if (entry.condition.empty() || evaluator(entry.condition)) {
            result.push_back(&entry);
        }
    }
    return result;
}

} // namespace efl
