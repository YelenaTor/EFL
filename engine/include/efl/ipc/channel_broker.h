#pragma once

// Layer F: Cross-mod IPC channel broker

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

namespace efl {

using ChannelCallback = std::function<void(const nlohmann::json& data)>;

struct SubscribeResult {
    bool success = false;
    bool hasWarning = false;
    std::string warningMessage;
};

class ChannelBroker {
public:
    void declareChannel(const std::string& modId, const std::string& channelName,
                        const std::string& version);
    SubscribeResult subscribe(const std::string& channelName, ChannelCallback callback,
                              const std::string& expectedVersion = "");
    void publish(const std::string& modId, const std::string& channelName,
                 const nlohmann::json& data);
    size_t messageCount() const;
    void clear();

private:
    struct ChannelInfo {
        std::string ownerMod;
        std::string version;
    };
    struct ChannelSubscriber {
        ChannelCallback callback;
        std::string expectedVersion;
    };
    std::unordered_map<std::string, ChannelInfo> channels_;
    std::unordered_map<std::string, std::vector<ChannelSubscriber>> subscribers_;
    size_t messageCount_ = 0;
};

} // namespace efl
