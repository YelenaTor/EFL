#include "efl/registries/npc_registry.h"

namespace efl {

std::optional<NpcDef> NpcDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("displayName")) {
        return std::nullopt;
    }

    NpcDef def;
    def.id          = j.at("id").get<std::string>();
    def.displayName = j.at("displayName").get<std::string>();

    if (j.contains("kind"))          def.kind          = j.at("kind").get<std::string>();
    if (j.contains("defaultArea"))   def.defaultArea   = j.at("defaultArea").get<std::string>();
    if (j.contains("spawnAnchor"))   def.spawnAnchor   = j.at("spawnAnchor").get<std::string>();
    if (j.contains("dialogueSet"))   def.dialogueSet   = j.at("dialogueSet").get<std::string>();
    if (j.contains("unlockTrigger")) def.unlockTrigger = j.at("unlockTrigger").get<std::string>();

    return def;
}

void NpcRegistry::registerNpc(const NpcDef& def) {
    if (index_.count(def.id)) {
        npcs_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = npcs_.size();
    npcs_.push_back(def);
}

const NpcDef* NpcRegistry::getNpc(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &npcs_[it->second];
}

std::vector<const NpcDef*> NpcRegistry::npcsInArea(const std::string& areaId) const {
    std::vector<const NpcDef*> result;
    for (const auto& npc : npcs_) {
        if (npc.defaultArea == areaId) {
            result.push_back(&npc);
        }
    }
    return result;
}

const std::vector<NpcDef>& NpcRegistry::allNpcs() const {
    return npcs_;
}

} // namespace efl
