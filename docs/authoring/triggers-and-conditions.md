# Triggers and Conditions

Triggers are EFL's unified boolean condition system. They gate access to content across the entire framework — areas, warps, NPCs, recipes, quests, and events all use the same trigger system.

## Where Triggers Are Used

| Content Type | Field | Effect |
|:-------------|:------|:-------|
| Area | `unlockTrigger` | Controls whether the area is accessible |
| Warp | `requireTrigger` | Controls whether the player can use the warp |
| NPC | `unlockTrigger` | Controls whether the NPC is visible and interactable |
| Recipe | `unlockTrigger` | Controls whether the recipe appears in crafting menus |
| Event | `trigger` | Controls when the event/cutscene activates |
| Dialogue | `condition` | Controls which dialogue entries are shown (uses shorthand) |

## Trigger Types

### flagSet

Checks whether a named flag has been set in the pack's save data.

```json
{
    "id": "talked_to_hermit",
    "type": "flagSet",
    "flagName": "met_hermit"
}
```

Flags are set by quest `onComplete` actions, cutscene `onFire.setFlags` declarations, or other game state changes.

### questComplete

Checks whether a specific quest has been completed.

```json
{
    "id": "finished_crystal_quest",
    "type": "questComplete",
    "questId": "crystal_collection"
}
```

### allOf

Requires **all** child conditions to be true (logical AND).

```json
{
    "id": "ready_for_deep_cave",
    "type": "allOf",
    "conditions": [
        { "type": "flagSet", "flagName": "met_hermit" },
        { "type": "questComplete", "questId": "crystal_collection" }
    ]
}
```

### anyOf

Requires **at least one** child condition to be true (logical OR).

```json
{
    "id": "has_any_tool",
    "type": "anyOf",
    "conditions": [
        { "type": "flagSet", "flagName": "has_pickaxe" },
        { "type": "flagSet", "flagName": "has_drill" }
    ]
}
```

## Nesting for Complex Logic

`allOf` and `anyOf` can be nested to express complex boolean conditions:

```json
{
    "id": "endgame_access",
    "type": "allOf",
    "conditions": [
        { "type": "questComplete", "questId": "main_quest_line" },
        {
            "type": "anyOf",
            "conditions": [
                { "type": "flagSet", "flagName": "befriended_hermit" },
                { "type": "flagSet", "flagName": "found_secret_key" }
            ]
        }
    ]
}
```

This reads as: "main quest is complete AND (befriended hermit OR found secret key)."

## Dialogue Condition Shorthand

Dialogue entries use a simplified condition string instead of full trigger references:

```json
{
    "id": "return_greeting",
    "text": "Oh, you're back!",
    "portrait": "happy",
    "condition": "flag:talked_once"
}
```

The `"flag:<name>"` shorthand checks whether the named flag is set. An empty string (`""`) means the entry has no condition and always matches.

## Walkthrough: Building a Compound Trigger

Suppose you want an area that unlocks only after the player:
1. Completes the "Crystal Collection" quest, AND
2. Has either talked to the hermit OR found the map

**Step 1**: Define the simple triggers in `triggers/`:

```json
[
    {
        "id": "crystal_quest_done",
        "type": "questComplete",
        "questId": "crystal_collection"
    },
    {
        "id": "knows_location",
        "type": "anyOf",
        "conditions": [
            { "type": "flagSet", "flagName": "hermit_told_location" },
            { "type": "flagSet", "flagName": "found_map" }
        ]
    }
]
```

**Step 2**: Combine them:

```json
{
    "id": "deep_cave_unlock",
    "type": "allOf",
    "conditions": [
        { "type": "questComplete", "questId": "crystal_collection" },
        {
            "type": "anyOf",
            "conditions": [
                { "type": "flagSet", "flagName": "hermit_told_location" },
                { "type": "flagSet", "flagName": "found_map" }
            ]
        }
    ]
}
```

**Step 3**: Reference it in your area definition:

```json
{
    "id": "deep_cave",
    "displayName": "Deep Crystal Cave",
    "backend": "hijacked",
    "hostRoom": "rm_mine_05",
    "unlockTrigger": "deep_cave_unlock"
}
```

## Runtime Evaluation

Triggers are evaluated at runtime, not at load time. This means:

- Flags set during gameplay immediately affect trigger results
- Quest completion during a session unlocks gated content without restarting
- Trigger state is not cached — each check reads the current save data
