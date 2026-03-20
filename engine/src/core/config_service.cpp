#include "efl/core/config_service.h"

namespace efl {

std::string ConfigService::makeKey(const std::string& modId, const std::string& key) const {
    return modId + "/" + key;
}

void ConfigService::set(const std::string& modId, const std::string& key, const std::string& value) {
    values_[makeKey(modId, key)] = value;
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
