#include "efl/core/config_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace efl {

std::string ConfigService::makeKey(const std::string& modId, const std::string& key) const {
    return modId + "/" + key;
}

void ConfigService::loadFromFile(const std::string& path) {
    configPath_ = path;
    std::ifstream f(path);
    if (!f.is_open())
        return; // Missing file is not an error — use defaults

    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return;
    }

    if (!j.is_object()) return;
    for (auto& [modId, modVals] : j.items()) {
        if (!modVals.is_object()) continue;
        for (auto& [k, v] : modVals.items()) {
            if (v.is_string())
                values_[makeKey(modId, k)] = v.get<std::string>();
        }
    }
}

void ConfigService::saveToFile(const std::string& path) const {
    // Reconstruct per-modId structure from flat values_ map.
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [flatKey, value] : values_) {
        auto sep = flatKey.find('/');
        if (sep == std::string::npos) continue;
        std::string modId = flatKey.substr(0, sep);
        std::string k     = flatKey.substr(sep + 1);
        j[modId][k] = value;
    }

    std::ofstream f(path);
    if (f.is_open())
        f << j.dump(4);
}

void ConfigService::set(const std::string& modId, const std::string& key, const std::string& value) {
    values_[makeKey(modId, key)] = value;
    if (!configPath_.empty())
        saveToFile(configPath_);
}

std::string ConfigService::getString(const std::string& modId, const std::string& key,
                                     const std::string& defaultValue) const {
    auto it = values_.find(makeKey(modId, key));
    if (it != values_.end())
        return it->second;
    return defaultValue;
}

bool ConfigService::getBool(const std::string& modId, const std::string& key, bool defaultValue) const {
    auto it = values_.find(makeKey(modId, key));
    if (it != values_.end())
        return it->second == "true" || it->second == "1";
    return defaultValue;
}

int ConfigService::getInt(const std::string& modId, const std::string& key, int defaultValue) const {
    auto it = values_.find(makeKey(modId, key));
    if (it != values_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

} // namespace efl
