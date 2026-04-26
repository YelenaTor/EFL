#pragma once

// Layer D: CalendarRegistry — minimal V3 pilot
//
// Holds pack-declared calendar/world events keyed by season + day-of-season
// (e.g. "summer day 14"). The registry is intentionally small for the V3
// pilot: it answers "which events are active right now?" and tracks
// once-only fire state so future ticks don't re-fire them.
//
// Conditions and onActivate dispatch are wired by `bootstrap.cpp` against the
// existing TriggerService and StoryBridge; this module only owns the
// definitions and the firing-history bookkeeping.

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace efl {

struct CalendarEventDef {
    std::string id;
    std::string displayName;

    // -1 means "any season". 0=spring, 1=summer, 2=fall, 3=winter.
    int season = -1;

    // -1 means "any day of the season". Otherwise 1..28 in FoM's calendar.
    int dayOfSeason = -1;

    // Optional trigger id; bootstrap evaluates this through TriggerService
    // before calling onActivate. Empty string means "always satisfied".
    std::string condition;

    // Optional story event id to fire when this calendar event activates.
    // Bootstrap looks this up through StoryBridge / cutscene eligibility.
    std::string onActivate;

    // If true, the registry remembers it has fired and never activates the
    // event again for this game session. Persistence across save/load is a
    // V3 follow-up — pilot keeps it in-memory.
    bool once = false;

    static std::optional<CalendarEventDef> fromJson(const nlohmann::json& j);
};

class CalendarRegistry {
public:
    void registerEvent(CalendarEventDef def);
    const CalendarEventDef* getEvent(const std::string& id) const;

    // All registered events whose season/day filters match the given date.
    // Includes once-only events that have already fired; callers filter
    // those out via hasFired() if they want only "ready to activate" events.
    std::vector<const CalendarEventDef*> activeEventsFor(int season,
                                                         int dayOfSeason) const;

    bool hasFired(const std::string& eventId) const;
    void markFired(const std::string& eventId);

    const std::vector<CalendarEventDef>& allEvents() const { return events_; }

    // Bootstrap wires this to: evaluate condition -> fire story event ->
    // markFired (if once). Pilot registry just tracks state and lets the
    // callback do the dispatch.
    std::function<void(const CalendarEventDef& def)> onActivate;

    // Drive a "new day" check. For every event whose season/day matches and
    // (for once-only events) hasn't already fired, invoke onActivate. Returns
    // the number of events fired.
    size_t tickNewDay(int season, int dayOfSeason);

private:
    std::vector<CalendarEventDef> events_;
    std::unordered_map<std::string, size_t> index_;
    std::unordered_set<std::string> firedOnce_;
};

} // namespace efl
