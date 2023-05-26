#pragma once

#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_set>

class JSONCompanion {
   private:
    const nlohmann::json& j{};

   public:
    JSONCompanion(const nlohmann::json& t_json);
    bool containsOnlyKeys(const std::unordered_set<std::string>& keySet);
    bool containsNoKeys();
    bool containsKey(const std::string& k);
    std::unordered_set<std::string> differenceKeys(
        std::unordered_set<std::string> set);
};