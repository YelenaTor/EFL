#include <gtest/gtest.h>
#include "efl/registries/calendar_registry.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {

efl::CalendarEventDef makeEvent(const std::string& id,
                                 const std::string& season,
                                 int day,
                                 bool once = false,
                                 const std::string& condition = "",
                                 const std::string& onActivate = "") {
    nlohmann::json j;
    j["id"]          = id;
    j["season"]      = season;
    j["dayOfSeason"] = day;
    if (once)                  j["lifecycle"]   = "once";
    if (!condition.empty())    j["condition"]   = condition;
    if (!onActivate.empty())   j["onActivate"]  = onActivate;
    auto def = efl::CalendarEventDef::fromJson(j);
    EXPECT_TRUE(def.has_value());
    return *def;
}

} // namespace

TEST(CalendarRegistry, ParsesAllFields) {
    nlohmann::json j = {
        {"id", "summer_kickoff"},
        {"displayName", "Summer Kickoff"},
        {"season", "summer"},
        {"dayOfSeason", 1},
        {"condition", "town_unlocked"},
        {"onActivate", "story_summer_intro"},
        {"lifecycle", "once"},
    };
    auto def = efl::CalendarEventDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->id, "summer_kickoff");
    EXPECT_EQ(def->displayName, "Summer Kickoff");
    EXPECT_EQ(def->season, 1);
    EXPECT_EQ(def->dayOfSeason, 1);
    EXPECT_EQ(def->condition, "town_unlocked");
    EXPECT_EQ(def->onActivate, "story_summer_intro");
    EXPECT_TRUE(def->once);
}

TEST(CalendarRegistry, MissingIdRejected) {
    nlohmann::json j = {{"season", "spring"}};
    auto def = efl::CalendarEventDef::fromJson(j);
    EXPECT_FALSE(def.has_value());
}

TEST(CalendarRegistry, AnySeasonByDefault) {
    nlohmann::json j = {{"id", "loose"}};
    auto def = efl::CalendarEventDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->season, -1);
    EXPECT_EQ(def->dayOfSeason, -1);
}

TEST(CalendarRegistry, IntegerSeasonAccepted) {
    nlohmann::json j = {{"id", "raw"}, {"season", 2}};
    auto def = efl::CalendarEventDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->season, 2);
}

TEST(CalendarRegistry, OutOfRangeDayIgnored) {
    // Day-of-season above FoM's 28-day cap is treated as "any day" rather
    // than an error so authors can stub future calendars.
    nlohmann::json j = {{"id", "weird"}, {"dayOfSeason", 99}};
    auto def = efl::CalendarEventDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->dayOfSeason, -1);
}

TEST(CalendarRegistry, ActiveEventsForFiltersBySeasonAndDay) {
    efl::CalendarRegistry reg;
    reg.registerEvent(makeEvent("a", "spring", 1));
    reg.registerEvent(makeEvent("b", "spring", 14));
    reg.registerEvent(makeEvent("c", "summer", 1));
    reg.registerEvent(makeEvent("d", "any", 1));

    auto matches = reg.activeEventsFor(/*season=*/0, /*day=*/1);
    std::vector<std::string> ids;
    ids.reserve(matches.size());
    for (auto* m : matches) ids.push_back(m->id);
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "a");
    EXPECT_EQ(ids[1], "d");
}

TEST(CalendarRegistry, TickFiresOnceAndStops) {
    efl::CalendarRegistry reg;
    reg.registerEvent(makeEvent("rare", "summer", 1, /*once=*/true));

    int fires = 0;
    reg.onActivate = [&](const efl::CalendarEventDef&) { ++fires; };

    EXPECT_EQ(reg.tickNewDay(1, 1), 1u);
    EXPECT_EQ(fires, 1);

    // Second tick of the same day is a no-op for once-only events.
    EXPECT_EQ(reg.tickNewDay(1, 1), 0u);
    EXPECT_EQ(fires, 1);
    EXPECT_TRUE(reg.hasFired("rare"));
}

TEST(CalendarRegistry, DailyEventsRefireEachMatchingDay) {
    efl::CalendarRegistry reg;
    reg.registerEvent(makeEvent("daily_check", "spring", 5, /*once=*/false));

    int fires = 0;
    reg.onActivate = [&](const efl::CalendarEventDef&) { ++fires; };

    reg.tickNewDay(0, 5);
    reg.tickNewDay(0, 6); // wrong day — no fire
    reg.tickNewDay(0, 5);
    EXPECT_EQ(fires, 2);
}

TEST(CalendarRegistry, RegisterReplacesExistingEvent) {
    efl::CalendarRegistry reg;
    reg.registerEvent(makeEvent("e", "spring", 1));
    reg.registerEvent(makeEvent("e", "summer", 2)); // overwrite

    EXPECT_EQ(reg.allEvents().size(), 1u);
    auto* def = reg.getEvent("e");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->season, 1);
    EXPECT_EQ(def->dayOfSeason, 2);
}
