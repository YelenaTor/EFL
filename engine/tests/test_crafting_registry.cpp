#include <gtest/gtest.h>
#include "efl/registries/crafting_registry.h"
#include "efl/core/trigger_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(CraftingRegistry, RegisterAndLookup) {
    efl::CraftingRegistry reg;
    auto recipe = efl::RecipeDef::fromJson(loadFixture("sample_recipe.json"));
    ASSERT_TRUE(recipe.has_value());
    reg.registerRecipe(*recipe);
    auto found = reg.getRecipe("crystal_pickaxe");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->output, "crystal_pickaxe");
    EXPECT_EQ(found->ingredients.size(), 2);
}

TEST(CraftingRegistry, RecipesByStation) {
    efl::CraftingRegistry reg;
    reg.registerRecipe({.id = "r1", .output = "a", .station = "anvil"});
    reg.registerRecipe({.id = "r2", .output = "b", .station = "cauldron"});
    reg.registerRecipe({.id = "r3", .output = "c", .station = "anvil"});
    EXPECT_EQ(reg.recipesAtStation("anvil").size(), 2);
    EXPECT_EQ(reg.recipesAtStation("cauldron").size(), 1);
}

TEST(CraftingRegistry, UnlockFiltering) {
    efl::CraftingRegistry reg;
    efl::TriggerService triggers;
    reg.registerRecipe({.id = "locked", .output = "x", .unlockTrigger = "unlock_x"});
    triggers.registerTrigger({.id = "unlock_x", .type = efl::TriggerType::FlagSet,
                              .flagName = "flag_x"});
    EXPECT_EQ(reg.availableRecipes(triggers).size(), 0);
    triggers.setFlag("flag_x", true);
    EXPECT_EQ(reg.availableRecipes(triggers).size(), 1);
}
