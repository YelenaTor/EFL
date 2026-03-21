#include <gtest/gtest.h>
#include "efl/ipc/channel_broker.h"

TEST(ChannelBroker, PublishAndConsume) {
    efl::ChannelBroker broker;
    broker.declareChannel("mod_a", "items.added", "1.0");
    std::string received;
    broker.subscribe("items.added", [&](const nlohmann::json& data) {
        received = data["item"].get<std::string>();
    });
    broker.publish("mod_a", "items.added", {{"item", "crystal"}});
    EXPECT_EQ(received, "crystal");
}

TEST(ChannelBroker, VersionMismatchWarns) {
    efl::ChannelBroker broker;
    broker.declareChannel("mod_a", "events", "2.0");
    auto result = broker.subscribe("events", [](auto&) {}, "1.0");
    EXPECT_TRUE(result.hasWarning);
}

TEST(ChannelBroker, UndeclaredChannelRejects) {
    efl::ChannelBroker broker;
    broker.publish("mod_a", "undeclared", {{"foo", "bar"}});
    EXPECT_EQ(broker.messageCount(), 0u);
}

TEST(ChannelBroker, MessageCountIncrements) {
    efl::ChannelBroker broker;
    broker.declareChannel("mod_a", "ch", "1.0");
    broker.publish("mod_a", "ch", {});
    broker.publish("mod_a", "ch", {});
    EXPECT_EQ(broker.messageCount(), 2u);
}

TEST(ChannelBroker, ClearResetsState) {
    efl::ChannelBroker broker;
    broker.declareChannel("mod_a", "ch", "1.0");
    broker.publish("mod_a", "ch", {});
    broker.clear();
    EXPECT_EQ(broker.messageCount(), 0u);
    // After clear, channel is gone — publish should not count
    broker.publish("mod_a", "ch", {});
    EXPECT_EQ(broker.messageCount(), 0u);
}

TEST(ChannelBroker, SubscribeToUndeclaredChannelFails) {
    efl::ChannelBroker broker;
    auto result = broker.subscribe("nonexistent", [](auto&) {});
    EXPECT_FALSE(result.success);
}
