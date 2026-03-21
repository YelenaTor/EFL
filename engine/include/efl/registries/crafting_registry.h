#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class TriggerService;

struct Ingredient {
    std::string item;
    int count = 1;
};

struct RecipeDef {
    std::string id;
    std::string output;
    std::string station;
    std::vector<Ingredient> ingredients;
    std::string unlockTrigger;

    static std::optional<RecipeDef> fromJson(const nlohmann::json& j);
};

class CraftingRegistry {
public:
    void registerRecipe(const RecipeDef& def);
    const RecipeDef* getRecipe(const std::string& id) const;
    std::vector<const RecipeDef*> recipesAtStation(const std::string& station) const;
    std::vector<const RecipeDef*> availableRecipes(const TriggerService& triggers) const;
    const std::vector<RecipeDef>& allRecipes() const;

private:
    std::vector<RecipeDef> recipes_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
