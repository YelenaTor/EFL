#include "efl/core/event_bus.h"

#include <algorithm>

namespace efl {

SubscriptionId EventBus::subscribe(const std::string& eventName, EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    SubscriptionId id = nextId_++;
    subscribers_[eventName].push_back({id, std::move(callback)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [eventName, subs] : subscribers_) {
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [id](const Subscription& s) { return s.id == id; }),
            subs.end());
    }
}

void EventBus::publish(const std::string& eventName, const nlohmann::json& data) {
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(eventName);
        if (it != subscribers_.end()) {
            for (const auto& sub : it->second) {
                callbacks.push_back(sub.callback);
            }
        }
    }
    // Invoke callbacks outside the lock to avoid deadlocks if a callback
    // tries to subscribe/publish.
    for (auto& cb : callbacks) {
        cb(data);
    }
}

void EventBus::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
}

} // namespace efl
