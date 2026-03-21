# Examples

This page walks through the `examples/hello_adventurer/` pack — a complete, minimal EFL Pack that demonstrates areas, warps, NPCs, dialogue, triggers, and events working together.

## Pack Overview

Hello Adventurer creates a single room with an NPC whose dialogue escalates the more you talk to them. It uses flags to track how many times you've visited, producing increasingly exasperated responses.

```
examples/hello_adventurer/
├── manifest.efl
├── areas/
│   └── example_room.json
├── warps/
│   └── town_to_example.json
├── npcs/
│   └── auri_clone.json
├── dialogue/
│   └── auri_clone_dialogue.json
├── triggers/
│   └── talked_triggers.json
└── events/
    └── welcome_event.json
```

## File-by-File Walkthrough

### manifest.efl

```json
{
    "schemaVersion": 1,
    "modId": "com.efl.example.hello-adventurer",
    "name": "Hello, Adventurer!",
    "version": "1.0.0",
    "eflVersion": "1.0.0-pre.1",
    "features": {
        "areas": true,
        "warps": true,
        "npcs": true,
        "triggers": true,
        "dialogue": true
    },
    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked",
        "saveScope": "EFL/com.efl.example.hello-adventurer"
    }
}
```

The manifest declares exactly which subsystems the pack uses. With `strictMode: true`, EFL would reject any attempt to register content for undeclared features (e.g., trying to add a quest without `"quests": true`).

Only the features actually used are set to `true` — resources, crafting, quests, story, and IPC are all `false` (omitted above for brevity; the actual file includes all 12 booleans).

### areas/example_room.json

```json
{
    "id": "example_room",
    "displayName": "Strange Empty Room",
    "backend": "hijacked",
    "hostRoom": "rm_water_seal_0",
    "music": "",
    "entryAnchor": "door_south",
    "unlockTrigger": ""
}
```

This hijacks the game room `rm_water_seal_0` and repopulates it as "Strange Empty Room." The empty `music` field means the room keeps whatever music was playing. No `unlockTrigger` means the area is always accessible.

### warps/town_to_example.json

```json
{
    "id": "town_to_example_room",
    "sourceArea": "town",
    "sourceAnchor": "example_door",
    "targetArea": "example_room",
    "targetAnchor": "door_south",
    "requireTrigger": "",
    "failureText": ""
}
```

A simple warp from the town to the example room. No trigger requirement — the player can always use this warp.

### npcs/auri_clone.json

```json
{
    "id": "auri_clone",
    "displayName": "???",
    "kind": "local",
    "defaultArea": "example_room",
    "spawnAnchor": "center",
    "portraitPack": "celine",
    "dialogueSet": "auri_clone_dialogue",
    "unlockTrigger": ""
}
```

A local NPC named "???" that spawns in the center of the example room. The `portraitPack` references an existing game portrait set (`celine`) so the NPC has facial expressions in dialogue. The `dialogueSet` links to the dialogue definition below.

### dialogue/auri_clone_dialogue.json

This is the heart of the example. The dialogue set contains 14 entries across 5 escalation tiers:

| Tier | Condition | Tone |
|:-----|:----------|:-----|
| First visit | (none) | Welcoming, cheerful |
| Second visit | `flag:talked_once` | Surprised you're still here |
| Third visit | `flag:talked_twice` | Annoyed, tells you to leave |
| Fourth visit | `flag:talked_three` | Existential crisis about being a demo NPC |
| Fifth visit | `flag:talked_four` | Passive-aggressive resignation |
| Sixth+ visit | `flag:talked_five` | Silent treatment (`"..."`) |

Each entry has a `condition` field using the `"flag:<name>"` shorthand. Entries with no condition (`""`) show on the first visit. As flags are set by the game tracking conversation count, later entries take priority.

The portrait expressions match the tone — `"happy"` and `"wink"` early on, transitioning through `"annoyed"`, `"mad"`, `"sad"`, and finally `"ugh"`.

### triggers/talked_triggers.json

```json
[
    { "id": "talked_once_trigger",  "type": "flagSet", "flagName": "talked_once" },
    { "id": "talked_twice_trigger", "type": "flagSet", "flagName": "talked_twice" },
    { "id": "talked_three_trigger", "type": "flagSet", "flagName": "talked_three" },
    { "id": "talked_four_trigger",  "type": "flagSet", "flagName": "talked_four" },
    { "id": "talked_five_trigger",  "type": "flagSet", "flagName": "talked_five" }
]
```

Five simple `flagSet` triggers, one per conversation tier. Note this file contains a JSON array — triggers can be defined individually or as arrays in a single file.

### events/welcome_event.json

```json
{
    "id": "example_room_welcome",
    "mode": "nativeBridge",
    "trigger": "",
    "commands": [
        { "type": "dialogue", "npc": "auri_clone", "line": "Huh? How did you get in here? ...Oh well. Welcome, I guess!" },
        { "type": "setFlag", "flag": "example_room_visited" }
    ]
}
```

A one-time welcome event that fires when the player first enters the room. It uses `nativeBridge` mode to play through the game's built-in dialogue system, then sets a flag to track that the room has been visited.

## How the Flag Pattern Works

The escalating dialogue is driven entirely by flags:

1. Player enters the room → `welcome_event` fires, sets `example_room_visited`
2. Player talks to NPC → first-visit dialogue shows (no condition)
3. Game internally sets `talked_once` after first conversation
4. Player talks again → `flag:talked_once` entries match, showing surprised dialogue
5. Game sets `talked_twice` → next conversation shows annoyed dialogue
6. Pattern continues through all tiers

This demonstrates how EFL's flag + condition system creates dynamic, stateful content without any scripting.

## Try Modifying It

Some ideas for experimenting with this pack:

- **Add a new dialogue tier**: Create entries with a `flag:talked_six` condition and add a matching trigger
- **Gate the warp**: Set `requireTrigger` on the warp to a quest completion trigger, so players must complete something before entering
- **Add a second NPC**: Create another NPC definition in `npcs/` with its own dialogue set
- **Add resources**: Enable `"resources": true` in the manifest and create resource nodes in `resources/`
- **Change the host room**: Try a different `hostRoom` value to see how the area looks in a different game room
