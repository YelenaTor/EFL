#pragma once

// Layer D: IEflNpcRegistry - NPC definition and lifecycle

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

struct NpcDef {
    std::string id;
    std::string displayName;
    std::string kind;          // "local" (v1) or "world" (Phase 8)
    std::string defaultArea;
    std::string spawnAnchor;
    std::string dialogueSet;
    std::string unlockTrigger;

    static std::optional<NpcDef> fromJson(const nlohmann::json& j);
};

class NpcRegistry {
public:
    void registerNpc(const NpcDef& def);
    const NpcDef* getNpc(const std::string& id) const;
    std::vector<const NpcDef*> npcsInArea(const std::string& areaId) const;
    const std::vector<NpcDef>& allNpcs() const;

private:
    std::vector<NpcDef> npcs_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
