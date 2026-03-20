#pragma once

// Layer D: IEflAreaRegistry - area registration, unlock, state

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

enum class AreaBackend { Hijacked, Native };

struct AreaDef {
    std::string id;
    std::string displayName;
    AreaBackend backend = AreaBackend::Hijacked;
    std::string hostRoom;      // for hijacked backend
    std::string music;
    std::string entryAnchor;
    std::string unlockTrigger; // trigger ID that gates access (empty = always accessible)

    static std::optional<AreaDef> fromJson(const nlohmann::json& j);
};

class AreaRegistry {
public:
    void registerArea(const AreaDef& def);
    const AreaDef* getArea(const std::string& id) const;
    const std::vector<AreaDef>& allAreas() const;
    std::vector<const AreaDef*> areasByHostRoom(const std::string& roomName) const;

private:
    std::vector<AreaDef> areas_;
    std::unordered_map<std::string, size_t> index_; // id -> index in areas_
};

} // namespace efl
