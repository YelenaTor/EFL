#pragma once
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class TriggerService;
class PipeWriter;

// EFL-side effects that apply the moment a cutscene is committed to play.
// Mist/MOMI owns all cutscene content (dialogue, animations, item grants via
// __mist__.json). This struct only carries state changes that EFL itself tracks.
struct CutsceneOnFire {
    std::vector<std::string> setFlags;
    std::vector<std::string> clearFlags;
    std::string startQuest;
    std::string advanceQuest;
    int grantItemId  = 0; // numeric item ID from t2_input.json items list (0 = no grant)
    int grantItemQty = 1;
};

// Describes one EFL-managed cutscene trigger.
// The `id` is the Mist function key registered in __mist__.json by MOMI
// (e.g. "crystal_cave_reveal" maps to "crystal_cave_reveal.mist").
// EFL owns when it plays; MOMI/Mist owns what it plays.
struct CutsceneDef {
    std::string    id;
    std::string    trigger;    // EFL trigger id — evaluated at check_cutscene_eligible
    bool           once = true; // mark seen after committing, never replay
    CutsceneOnFire onFire;

    static std::optional<CutsceneDef> fromJson(const nlohmann::json& j);
};

class StoryBridge {
public:
    void setPipeWriter(PipeWriter* pipe);

    void registerCutscene(const CutsceneDef& def);
    const CutsceneDef* getCutscene(const std::string& id) const;
    const std::vector<CutsceneDef>& allCutscenes() const;

    // Called from the load_cutscenes hook (fires once at boot).
    // Logs registered cutscene keys to IPC so DevKit can show them.
    void onLoadCutscenes();

    // Called from the check_cutscene_eligible hook (FoM-driven).
    // Returns true if EFL owns this key AND trigger conditions are met AND
    // (if once=true) it hasn't been seen yet. Applies onFire effects and marks
    // as seen before returning true.
    bool evaluateEligibility(const std::string& key, TriggerService& triggers);

    // EFL-driven path (area entry/exit, not gated by FoM's cutscene system).
    // Evaluates the trigger for a named CutsceneDef and applies its onFire
    // effects if conditions are met. Does not interact with check_cutscene_eligible.
    void fireEffects(const std::string& id, TriggerService& triggers);

    // Wired by bootstrap to forward EFL quest state changes.
    std::function<void(const std::string& questId)> onQuestStart;
    std::function<void(const std::string& questId)> onQuestAdvance;

    // Wired by bootstrap to call give_item@Ari@Ari via RoutineInvoker.
    std::function<void(int itemId, int qty)> onItemGrant;

private:
    PipeWriter* pipe_ = nullptr;
    std::vector<CutsceneDef> cutscenes_;
    std::unordered_map<std::string, size_t> index_;
    std::unordered_map<std::string, bool>   seen_;
};

} // namespace efl
