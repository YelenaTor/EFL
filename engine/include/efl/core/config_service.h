#pragma once

// Layer C: Per-mod configuration

#include <string>
#include <unordered_map>

namespace efl {

class ConfigService {
public:
    void set(const std::string& modId, const std::string& key, const std::string& value);
    std::string getString(const std::string& modId, const std::string& key,
                          const std::string& defaultValue = "") const;
    bool getBool(const std::string& modId, const std::string& key, bool defaultValue = false) const;
    int getInt(const std::string& modId, const std::string& key, int defaultValue = 0) const;

private:
    // Key is "modId/key"
    std::unordered_map<std::string, std::string> values_;
    std::string makeKey(const std::string& modId, const std::string& key) const;
};

} // namespace efl
