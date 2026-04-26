#include "efl/registries/calendar_registry.h"

namespace efl {

namespace {

// Map FoM season strings to the integer encoding used by gml_Script_season:
// 0=spring, 1=summer, 2=fall, 3=winter. -1 means "any season".
int seasonStringToInt(const std::string& s) {
    if (s == "spring") return 0;
    if (s == "summer") return 1;
    if (s == "fall")   return 2;
    if (s == "winter") return 3;
    if (s == "any" || s.empty()) return -1;
    return -1;
}

} // namespace

std::optional<CalendarEventDef> CalendarEventDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.at("id").is_string()) {
        return std::nullopt;
    }

    CalendarEventDef def;
    def.id = j.at("id").get<std::string>();

    if (j.contains("displayName") && j.at("displayName").is_string()) {
        def.displayName = j.at("displayName").get<std::string>();
    }

    if (j.contains("season")) {
        const auto& s = j.at("season");
        if (s.is_string()) {
            def.season = seasonStringToInt(s.get<std::string>());
        } else if (s.is_number_integer()) {
            // Tolerate raw integer for advanced authors who already speak the
            // game's season encoding.
            int n = s.get<int>();
            def.season = (n >= 0 && n <= 3) ? n : -1;
        }
    }

    if (j.contains("dayOfSeason") && j.at("dayOfSeason").is_number_integer()) {
        int day = j.at("dayOfSeason").get<int>();
        if (day >= 1 && day <= 28) {
            def.dayOfSeason = day;
        }
    }

    if (j.contains("condition") && j.at("condition").is_string()) {
        def.condition = j.at("condition").get<std::string>();
    }

    if (j.contains("onActivate") && j.at("onActivate").is_string()) {
        def.onActivate = j.at("onActivate").get<std::string>();
    }

    if (j.contains("lifecycle") && j.at("lifecycle").is_string()) {
        def.once = (j.at("lifecycle").get<std::string>() == "once");
    } else if (j.contains("once") && j.at("once").is_boolean()) {
        def.once = j.at("once").get<bool>();
    }

    return def;
}

void CalendarRegistry::registerEvent(CalendarEventDef def) {
    auto it = index_.find(def.id);
    if (it != index_.end()) {
        events_[it->second] = std::move(def);
        return;
    }
    index_[def.id] = events_.size();
    events_.push_back(std::move(def));
}

const CalendarEventDef* CalendarRegistry::getEvent(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &events_[it->second];
}

std::vector<const CalendarEventDef*>
CalendarRegistry::activeEventsFor(int season, int dayOfSeason) const {
    std::vector<const CalendarEventDef*> matches;
    matches.reserve(events_.size());
    for (const auto& def : events_) {
        if (def.season != -1 && def.season != season) continue;
        if (def.dayOfSeason != -1 && def.dayOfSeason != dayOfSeason) continue;
        matches.push_back(&def);
    }
    return matches;
}

bool CalendarRegistry::hasFired(const std::string& eventId) const {
    return firedOnce_.find(eventId) != firedOnce_.end();
}

void CalendarRegistry::markFired(const std::string& eventId) {
    firedOnce_.insert(eventId);
}

size_t CalendarRegistry::tickNewDay(int season, int dayOfSeason) {
    size_t fired = 0;
    for (const auto& def : events_) {
        if (def.season != -1 && def.season != season) continue;
        if (def.dayOfSeason != -1 && def.dayOfSeason != dayOfSeason) continue;
        if (def.once && hasFired(def.id)) continue;

        if (onActivate) {
            onActivate(def);
        }

        if (def.once) {
            markFired(def.id);
        }
        ++fired;
    }
    return fired;
}

} // namespace efl
