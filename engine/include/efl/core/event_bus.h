#pragma once

// Layer C: Publish/subscribe event system

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace efl {

using EventCallback = std::function<void(const nlohmann::json& data)>;
using SubscriptionId = uint64_t;

class EventBus {
public:
    SubscriptionId subscribe(const std::string& eventName, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    void publish(const std::string& eventName, const nlohmann::json& data);
    void clear();

private:
    struct Subscription {
        SubscriptionId id;
        EventCallback callback;
    };
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Subscription>> subscribers_;
    SubscriptionId nextId_ = 1;
};

} // namespace efl
