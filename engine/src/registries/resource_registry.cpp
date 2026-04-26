#include "efl/registries/resource_registry.h"
#include <algorithm>

namespace efl {

std::optional<ResourceDef> ResourceDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("kind")) {
        return std::nullopt;
    }

    ResourceDef def;
    def.id   = j.at("id").get<std::string>();
    def.kind = j.at("kind").get<std::string>();

    if (j.contains("sprite")) {
        def.sprite = j.at("sprite").get<std::string>();
    }

    if (j.contains("objectName")) {
        def.objectName = j.at("objectName").get<std::string>();
    }

    if (j.contains("yieldTable")) {
        for (const auto& entry : j.at("yieldTable")) {
            YieldEntry ye;
            ye.item = entry.at("item").get<std::string>();
            if (entry.contains("itemId")) ye.itemId = entry.at("itemId").get<int>();
            if (entry.contains("min")) ye.min = entry.at("min").get<int>();
            if (entry.contains("max")) ye.max = entry.at("max").get<int>();
            def.yieldTable.push_back(ye);
        }
    }

    if (j.contains("spawnRules")) {
        const auto& sr = j.at("spawnRules");
        if (sr.contains("areas")) {
            def.spawnRules.areas = sr.at("areas").get<std::vector<std::string>>();
        }
        if (sr.contains("anchors")) {
            for (auto& [areaId, val] : sr.at("anchors").items()) {
                std::string xy = val.get<std::string>();
                auto comma = xy.find(',');
                if (comma != std::string::npos) {
                    int gx = std::stoi(xy.substr(0, comma));
                    int gy = std::stoi(xy.substr(comma + 1));
                    def.spawnRules.anchors[areaId] = {gx, gy};
                    // Also ensure the area is in the areas list
                    if (std::find(def.spawnRules.areas.begin(),
                                  def.spawnRules.areas.end(), areaId)
                        == def.spawnRules.areas.end()) {
                        def.spawnRules.areas.push_back(areaId);
                    }
                }
            }
        }
        if (sr.contains("dungeonVotes")) {
            for (const auto& entry : sr.at("dungeonVotes")) {
                DungeonVoteEntry dv;
                dv.biome    = entry.at("biome").get<std::string>();
                dv.pool     = entry.value("pool", std::string("ore_rock"));
                dv.objectId = entry.value("objectId", std::string());
                dv.weight   = entry.value("weight", 1);
                def.spawnRules.dungeonVotes.push_back(dv);
            }
        }
        if (sr.contains("respawnPolicy")) {
            def.spawnRules.respawnPolicy = sr.at("respawnPolicy").get<std::string>();
        }
        if (sr.contains("seasonal")) {
            def.spawnRules.seasonal = sr.at("seasonal").get<std::vector<std::string>>();
        }
    }

    if (j.contains("interaction")) {
        const auto& ia = j.at("interaction");
        if (ia.contains("tool"))       def.interaction.tool       = ia.at("tool").get<std::string>();
        if (ia.contains("scriptMode")) def.interaction.scriptMode = ia.at("scriptMode").get<std::string>();
    }

    return def;
}

void ResourceRegistry::registerResource(const ResourceDef& def) {
    if (index_.count(def.id)) {
        resources_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = resources_.size();
    resources_.push_back(def);
}

const ResourceDef* ResourceRegistry::getResource(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &resources_[it->second];
}

std::vector<const ResourceDef*> ResourceRegistry::resourcesInArea(const std::string& areaId) const {
    std::vector<const ResourceDef*> result;
    for (const auto& res : resources_) {
        const auto& areas = res.spawnRules.areas;
        if (std::find(areas.begin(), areas.end(), areaId) != areas.end()) {
            result.push_back(&res);
        }
    }
    return result;
}

std::vector<const ResourceDef*> ResourceRegistry::resourcesByKind(const std::string& kind) const {
    std::vector<const ResourceDef*> result;
    for (const auto& res : resources_) {
        if (res.kind == kind) {
            result.push_back(&res);
        }
    }
    return result;
}

std::vector<const ResourceDef*> ResourceRegistry::resourcesWithDungeonVotes() const {
    std::vector<const ResourceDef*> result;
    for (const auto& res : resources_) {
        if (!res.spawnRules.dungeonVotes.empty()) {
            result.push_back(&res);
        }
    }
    return result;
}

const std::vector<ResourceDef>& ResourceRegistry::allResources() const {
    return resources_;
}

} // namespace efl
