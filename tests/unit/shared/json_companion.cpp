#include "json_companion.h"

JSONCompanion::JSONCompanion(const nlohmann::json& t_json) : j(t_json) {}

bool JSONCompanion::containsOnlyKeys(
    const std::unordered_set<std::string>& keySet) {
    for (const auto& [key, value] : this->j.items()) {
        if (keySet.find(key) == keySet.end()) {
            return false;
        }
    }
    return true;
}

bool JSONCompanion::containsNoKeys() {
    for (const auto& [key, value] : this->j.items()) {
        return false;
    }
    return true;
}

bool JSONCompanion::containsKey(const std::string& k) {
    return this->j.find(k) != this->j.end();
}

std::unordered_set<std::string> JSONCompanion::differenceKeys(
    std::unordered_set<std::string> keySet) {
    std::unordered_set<std::string> difference{};
    for (const auto& [key, value] : this->j.items()) {
        if (keySet.find(key) == keySet.end()) {
            difference.insert(key);
        }
    }
    return difference;
}