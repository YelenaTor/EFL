#pragma once

// Layer C: Namespaced save/load persistence
// Path schema: EFL/<modId>/<feature>/<contentId>

#include <string>
#include <optional>
#include <mutex>
#include <nlohmann/json.hpp>

namespace efl {

class SaveService {
public:
    /// Store data at namespaced path EFL/<modId>/<feature>/<contentId>.
    void set(const std::string& modId, const std::string& feature,
             const std::string& contentId, const nlohmann::json& data);

    /// Retrieve data at the namespaced path. Returns nullopt if not found.
    std::optional<nlohmann::json> get(const std::string& modId, const std::string& feature,
                                      const std::string& contentId) const;

    /// Delete the entry at the namespaced path (no-op if absent).
    void remove(const std::string& modId, const std::string& feature,
                const std::string& contentId);

    /// Return the full nested store as JSON (for writing to disk / pipe).
    nlohmann::json serialize() const;

    /// Restore state from a previously serialized JSON object.
    void deserialize(const nlohmann::json& j);

    /// Wipe all stored data.
    void clear();

private:
    mutable std::mutex mutex_;
    // store_[modId][feature][contentId] = data
    nlohmann::json store_ = nlohmann::json::object();
};

} // namespace efl
