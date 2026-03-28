#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

struct YieldEntry {
    std::string item;
    int min = 0;
    int max = 1;
};

struct SpawnRules {
    std::vector<std::string> areas;
    std::unordered_map<std::string, std::pair<int,int>> anchors; // area_id -> {grid_x, grid_y}
    std::string respawnPolicy;
    std::vector<std::string> seasonal;
};

struct Interaction {
    std::string tool;
    std::string scriptMode;
};

struct ResourceDef {
    std::string id;
    std::string kind;
    std::string sprite;
    std::vector<YieldEntry> yieldTable;
    SpawnRules spawnRules;
    Interaction interaction;

    static std::optional<ResourceDef> fromJson(const nlohmann::json& j);
};

class ResourceRegistry {
public:
    void registerResource(const ResourceDef& def);
    const ResourceDef* getResource(const std::string& id) const;
    std::vector<const ResourceDef*> resourcesInArea(const std::string& areaId) const;
    std::vector<const ResourceDef*> resourcesByKind(const std::string& kind) const;
    const std::vector<ResourceDef>& allResources() const;

private:
    std::vector<ResourceDef> resources_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
