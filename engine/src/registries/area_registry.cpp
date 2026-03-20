#include "efl/registries/area_registry.h"

namespace efl {

std::optional<AreaDef> AreaDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("displayName")) {
        return std::nullopt;
    }

    AreaDef def;
    def.id          = j.at("id").get<std::string>();
    def.displayName = j.at("displayName").get<std::string>();

    if (j.contains("backend")) {
        std::string b = j.at("backend").get<std::string>();
        def.backend = (b == "native") ? AreaBackend::Native : AreaBackend::Hijacked;
    }

    if (j.contains("hostRoom"))      def.hostRoom      = j.at("hostRoom").get<std::string>();
    if (j.contains("music"))         def.music         = j.at("music").get<std::string>();
    if (j.contains("entryAnchor"))   def.entryAnchor   = j.at("entryAnchor").get<std::string>();
    if (j.contains("unlockTrigger")) def.unlockTrigger = j.at("unlockTrigger").get<std::string>();

    return def;
}

void AreaRegistry::registerArea(const AreaDef& def) {
    if (index_.count(def.id)) {
        // Overwrite existing entry
        areas_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = areas_.size();
    areas_.push_back(def);
}

const AreaDef* AreaRegistry::getArea(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &areas_[it->second];
}

const std::vector<AreaDef>& AreaRegistry::allAreas() const {
    return areas_;
}

std::vector<const AreaDef*> AreaRegistry::areasByHostRoom(const std::string& roomName) const {
    std::vector<const AreaDef*> result;
    for (const auto& area : areas_) {
        if (area.hostRoom == roomName) {
            result.push_back(&area);
        }
    }
    return result;
}

} // namespace efl
