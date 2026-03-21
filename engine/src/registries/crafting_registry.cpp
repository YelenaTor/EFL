#include "efl/registries/crafting_registry.h"
#include "efl/core/trigger_service.h"

namespace efl {

std::optional<RecipeDef> RecipeDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("output")) {
        return std::nullopt;
    }

    RecipeDef def;
    def.id = j.at("id").get<std::string>();
    def.output = j.at("output").get<std::string>();

    if (j.contains("station")) {
        def.station = j.at("station").get<std::string>();
    }

    if (j.contains("unlockTrigger")) {
        def.unlockTrigger = j.at("unlockTrigger").get<std::string>();
    }

    if (j.contains("ingredients")) {
        for (const auto& ingJson : j.at("ingredients")) {
            Ingredient ing;
            ing.item = ingJson.at("item").get<std::string>();
            if (ingJson.contains("count")) {
                ing.count = ingJson.at("count").get<int>();
            }
            def.ingredients.push_back(ing);
        }
    }

    return def;
}

void CraftingRegistry::registerRecipe(const RecipeDef& def) {
    if (index_.count(def.id)) {
        recipes_[index_[def.id]] = def;
        return;
    }
    index_[def.id] = recipes_.size();
    recipes_.push_back(def);
}

const RecipeDef* CraftingRegistry::getRecipe(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &recipes_[it->second];
}

std::vector<const RecipeDef*> CraftingRegistry::recipesAtStation(const std::string& station) const {
    std::vector<const RecipeDef*> result;
    for (const auto& recipe : recipes_) {
        if (recipe.station == station) {
            result.push_back(&recipe);
        }
    }
    return result;
}

std::vector<const RecipeDef*> CraftingRegistry::availableRecipes(const TriggerService& triggers) const {
    std::vector<const RecipeDef*> result;
    for (const auto& recipe : recipes_) {
        if (recipe.unlockTrigger.empty() || triggers.evaluate(recipe.unlockTrigger)) {
            result.push_back(&recipe);
        }
    }
    return result;
}

const std::vector<RecipeDef>& CraftingRegistry::allRecipes() const {
    return recipes_;
}

} // namespace efl
