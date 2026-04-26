#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

struct YieldEntry {
    std::string item;
    int itemId = -1;  // numeric FoM item index; -1 = not set (harvest logged, no grant)
    int min = 0;
    int max = 1;
};

struct DungeonVoteEntry {
    // biome name as it appears in __fiddle__.json dungeons/dungeons/biomes[N].name (lowercase/underscore):
    //   "upper_mines" (floor 1) | "tide_caverns" (20) | "deep_earth" (40)
    //   "lava_caves" (60) | "ancient_ruins" (80)
    std::string biome;
    // pool key inside biome.votes: "ore_rock"|"small_rock"|"seam_rock"|"large_rock"|"enemy"|...
    std::string pool = "ore_rock";
    // GML object name to spawn (matches __fiddle__.json item/object IDs, e.g. "node_copper")
    std::string objectId;
    // vote weight — higher = more frequent relative to other pool entries
    int weight = 1;
};

struct SpawnRules {
    std::vector<std::string> areas;
    std::unordered_map<std::string, std::pair<int,int>> anchors; // area_id -> {grid_x, grid_y}
    std::vector<DungeonVoteEntry> dungeonVotes;
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
    std::string objectName;   // optional: FoM GML object name for instance_create_layer spawn
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
    std::vector<const ResourceDef*> resourcesWithDungeonVotes() const;
    const std::vector<ResourceDef>& allResources() const;

private:
    std::vector<ResourceDef> resources_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
