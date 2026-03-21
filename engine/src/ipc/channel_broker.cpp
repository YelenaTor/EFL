#include "efl/ipc/channel_broker.h"

namespace efl {

void ChannelBroker::declareChannel(const std::string& modId, const std::string& channelName,
                                   const std::string& version) {
    channels_[channelName] = ChannelInfo{modId, version};
}

SubscribeResult ChannelBroker::subscribe(const std::string& channelName,
                                         ChannelCallback callback,
                                         const std::string& expectedVersion) {
    auto it = channels_.find(channelName);
    if (it == channels_.end()) {
        return SubscribeResult{false, false, ""};
    }

    SubscribeResult result;
    result.success = true;

    if (!expectedVersion.empty() && expectedVersion != it->second.version) {
        result.hasWarning = true;
        result.warningMessage = "Version mismatch: channel declared as " +
                                it->second.version + ", subscriber expects " + expectedVersion;
    }

    subscribers_[channelName].push_back(ChannelSubscriber{std::move(callback), expectedVersion});
    return result;
}

void ChannelBroker::publish(const std::string& modId, const std::string& channelName,
                             const nlohmann::json& data) {
    auto chanIt = channels_.find(channelName);
    if (chanIt == channels_.end()) {
        // Undeclared channel — reject silently
        return;
    }

    messageCount_++;

    auto subIt = subscribers_.find(channelName);
    if (subIt == subscribers_.end()) return;

    for (const auto& sub : subIt->second) {
        sub.callback(data);
    }
}

size_t ChannelBroker::messageCount() const {
    return messageCount_;
}

void ChannelBroker::clear() {
    channels_.clear();
    subscribers_.clear();
    messageCount_ = 0;
}

} // namespace efl
