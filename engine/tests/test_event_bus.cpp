#include <gtest/gtest.h>
#include "efl/core/event_bus.h"

TEST(EventBus, SubscribeAndPublish) {
    efl::EventBus bus;
    std::string received;
    bus.subscribe("test.event", [&](const nlohmann::json& data) {
        received = data["msg"].get<std::string>();
    });
    bus.publish("test.event", {{"msg", "hello"}});
    EXPECT_EQ(received, "hello");
}

TEST(EventBus, MultipleSubscribers) {
    efl::EventBus bus;
    int count = 0;
    bus.subscribe("evt", [&](auto&) { count++; });
    bus.subscribe("evt", [&](auto&) { count++; });
    bus.publish("evt", {});
    EXPECT_EQ(count, 2);
}

TEST(EventBus, UnsubscribeById) {
    efl::EventBus bus;
    int count = 0;
    auto id = bus.subscribe("evt", [&](auto&) { count++; });
    bus.publish("evt", {});
    EXPECT_EQ(count, 1);
    bus.unsubscribe(id);
    bus.publish("evt", {});
    EXPECT_EQ(count, 1); // no change
}
