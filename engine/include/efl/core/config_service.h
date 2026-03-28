#pragma once

// Layer C: Per-mod configuration

#include <string>
#include <unordered_map>

namespace efl {

class ConfigService {
public:
    // Load config from <contentDir>/config.json. Silent no-op if file missing.
    void loadFromFile(const std::string& path);

    // Write current config to the path supplied by loadFromFile (or the explicit path).
    void saveToFile(const std::string& path) const;

    // set() writes through to disk if a config path is loaded.
    void set(const std::string& modId, const std::string& key, const std::string& value);

    std::string getString(const std::string& modId, const std::string& key,
                          const std::string& defaultValue = "") const;
    bool getBool(const std::string& modId, const std::string& key, bool defaultValue = false) const;
    int getInt(const std::string& modId, const std::string& key, int defaultValue = 0) const;

private:
    // Key is "modId/key"
    std::unordered_map<std::string, std::string> values_;
    std::string configPath_; // set by loadFromFile; empty = no write-through
    std::string makeKey(const std::string& modId, const std::string& key) const;
};

} // namespace efl
