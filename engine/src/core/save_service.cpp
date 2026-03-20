#include "efl/core/save_service.h"

namespace efl {

void SaveService::set(const std::string& modId, const std::string& feature,
                      const std::string& contentId, const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    store_[modId][feature][contentId] = data;
}

std::optional<nlohmann::json> SaveService::get(const std::string& modId,
                                                const std::string& feature,
                                                const std::string& contentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto modIt = store_.find(modId);
    if (modIt == store_.end()) return std::nullopt;

    auto featureIt = modIt->find(feature);
    if (featureIt == modIt->end()) return std::nullopt;

    auto contentIt = featureIt->find(contentId);
    if (contentIt == featureIt->end()) return std::nullopt;

    return *contentIt;
}

void SaveService::remove(const std::string& modId, const std::string& feature,
                         const std::string& contentId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto modIt = store_.find(modId);
    if (modIt == store_.end()) return;

    auto featureIt = modIt->find(feature);
    if (featureIt == modIt->end()) return;

    featureIt->erase(contentId);
}

nlohmann::json SaveService::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_;
}

void SaveService::deserialize(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    store_ = j;
}

void SaveService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    store_ = nlohmann::json::object();
}

} // namespace efl
