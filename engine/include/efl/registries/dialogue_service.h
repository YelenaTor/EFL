#pragma once

// Layer D: IEflDialogueService - dialogue binding and conversations

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace efl {

struct DialogueEntry {
    std::string id;
    std::string text;
    std::string portrait;
    std::string condition;  // empty = always shown, or "flag:name" for conditional
};

struct DialogueDef {
    std::string id;
    std::string npc;
    std::vector<DialogueEntry> entries;

    static std::optional<DialogueDef> fromJson(const nlohmann::json& j);
};

using ConditionEvaluator = std::function<bool(const std::string& condition)>;

class DialogueService {
public:
    void registerDialogue(const DialogueDef& def);
    const DialogueDef* getDialogue(const std::string& id) const;
    std::vector<const DialogueEntry*> availableEntries(const std::string& dialogueId,
                                                       ConditionEvaluator evaluator) const;

private:
    std::vector<DialogueDef> dialogues_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
