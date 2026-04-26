#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include "efl/ipc/pipe_writer.h"

namespace efl {

// ── CutsceneDef::fromJson ──────────────────────────────────────────────────
//
// Pack event JSON schema:
//
//   {
//     "id":      "crystal_cave_reveal",   // must match __mist__.json function key
//     "trigger": "has_cave_key",          // EFL trigger id
//     "once":    true,                    // optional, default true
//     "onFire": {                         // optional EFL-side effects
//       "setFlags":    ["cave_revealed"],
//       "clearFlags":  [],
//       "startQuest":  "find_crystals",
//       "advanceQuest": ""
//     }
//   }
//
// Dialogue, animations, item grants, and FoM quest effects belong in the
// Mist script (__mist__.json) authored via MOMI — not here.

std::optional<CutsceneDef> CutsceneDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id"))
        return std::nullopt;

    CutsceneDef def;
    def.id      = j.at("id").get<std::string>();
    def.trigger = j.value("trigger", "");
    def.once    = j.value("once", true);

    if (j.contains("onFire")) {
        const auto& f = j.at("onFire");
        if (f.contains("setFlags"))
            def.onFire.setFlags = f.at("setFlags").get<std::vector<std::string>>();
        if (f.contains("clearFlags"))
            def.onFire.clearFlags = f.at("clearFlags").get<std::vector<std::string>>();
        def.onFire.startQuest   = f.value("startQuest", "");
        def.onFire.advanceQuest = f.value("advanceQuest", "");
        def.onFire.grantItemId  = f.value("grantItemId",  0);
        def.onFire.grantItemQty = f.value("grantItemQty", 1);
    }

    return def;
}

// ── Registration ───────────────────────────────────────────────────────────

void StoryBridge::setPipeWriter(PipeWriter* pipe) {
    pipe_ = pipe;
}

void StoryBridge::registerCutscene(const CutsceneDef& def) {
    auto it = index_.find(def.id);
    if (it != index_.end()) {
        cutscenes_[it->second] = def;
        return;
    }
    index_[def.id] = cutscenes_.size();
    cutscenes_.push_back(def);
}

const CutsceneDef* StoryBridge::getCutscene(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &cutscenes_[it->second];
}

const std::vector<CutsceneDef>& StoryBridge::allCutscenes() const {
    return cutscenes_;
}

// ── Boot hook ──────────────────────────────────────────────────────────────

void StoryBridge::onLoadCutscenes() {
    if (!pipe_) return;
    nlohmann::json keys = nlohmann::json::array();
    for (const auto& c : cutscenes_)
        keys.push_back(c.id);
    pipe_->write("story.cutscenes_registered", nlohmann::json{{"keys", keys}});
}

// ── Eligibility gate ───────────────────────────────────────────────────────

bool StoryBridge::evaluateEligibility(const std::string& key, TriggerService& triggers) {
    auto it = index_.find(key);
    if (it == index_.end())
        return false; // not an EFL cutscene — let FoM handle it

    const CutsceneDef& def = cutscenes_[it->second];

    if (def.once && seen_.count(key))
        return false;

    if (!def.trigger.empty() && !triggers.evaluate(def.trigger))
        return false;

    // Commit: apply EFL-side effects now, before returning true.
    for (const auto& f : def.onFire.setFlags)
        triggers.setFlag(f, true);
    for (const auto& f : def.onFire.clearFlags)
        triggers.setFlag(f, false);
    if (!def.onFire.startQuest.empty() && onQuestStart)
        onQuestStart(def.onFire.startQuest);
    if (!def.onFire.advanceQuest.empty() && onQuestAdvance)
        onQuestAdvance(def.onFire.advanceQuest);
    if (def.onFire.grantItemId > 0 && onItemGrant)
        onItemGrant(def.onFire.grantItemId, def.onFire.grantItemQty);

    if (def.once)
        seen_[key] = true;

    if (pipe_) {
        pipe_->write("story.cutscene_eligible", nlohmann::json{
            {"key", key},
            {"trigger", def.trigger}
        });
    }

    return true;
}

// ── EFL-driven effects (area entry/exit) ──────────────────────────────────

void StoryBridge::fireEffects(const std::string& id, TriggerService& triggers) {
    if (id.empty()) return;
    auto it = index_.find(id);
    if (it == index_.end()) return;

    const CutsceneDef& def = cutscenes_[it->second];

    if (!def.trigger.empty() && !triggers.evaluate(def.trigger))
        return;

    for (const auto& f : def.onFire.setFlags)
        triggers.setFlag(f, true);
    for (const auto& f : def.onFire.clearFlags)
        triggers.setFlag(f, false);
    if (!def.onFire.startQuest.empty() && onQuestStart)
        onQuestStart(def.onFire.startQuest);
    if (!def.onFire.advanceQuest.empty() && onQuestAdvance)
        onQuestAdvance(def.onFire.advanceQuest);
    if (def.onFire.grantItemId > 0 && onItemGrant)
        onItemGrant(def.onFire.grantItemId, def.onFire.grantItemQty);

    if (pipe_) {
        pipe_->write("story.effects_fired", nlohmann::json{
            {"id", id}, {"trigger", def.trigger}
        });
    }
}

} // namespace efl
