# Examples

This page walks through two reference workspaces under `examples/`:

- [`examples/hello_adventurer/`](#hello-adventurer-efpack) — a complete, minimal `.efpack` covering areas, warps, NPCs, dialogue, triggers, story events, and the V3 calendar pilot.
- [`examples/hello_compat/`](#hello-compat-efdat) — a `.efdat` compatibility artifact that declares `requires` / `optional` / `conflicts` relationships between EFL packs and a MOMI mod. Shipping no content of its own; the canonical reference for the third-party shim model.

Both workspaces are also CI fixtures — every push runs `efl-pack` against them, so the schemas, builder, and validator stay honest against working examples.

## Hello Adventurer (`.efpack`)

This walks through the `examples/hello_adventurer/` pack.

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
├── events/
│   └── welcome_event.json
└── calendar/
    └── spring_first_visit.json
```

## File-by-File Walkthrough

### manifest.efl

```json
{
    "schemaVersion": 2,
    "modId": "com.efl.example.hello-adventurer",
    "name": "Hello, Adventurer!",
    "version": "1.0.0",
    "eflVersion": "1.0.0",
    "author": "EFL Team",
    "description": "Example EFL content pack demonstrating areas, NPCs, dialogue, and story events.",
    "features": ["areas", "warps", "npcs", "triggers", "dialogue", "story", "calendar"],
    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked"
    }
}
```

The manifest declares exactly which subsystems the pack uses via the `features` array. With `strictMode: true`, EFL would reject any attempt to register content for undeclared features (e.g., trying to add a quest without `"quests"` in the array).

Only the features actually used are listed — resources, crafting, quests, and IPC are all omitted.

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
    "trigger": "",
    "once": true,
    "onFire": {
        "setFlags": ["example_room_visited"]
    }
}
```

The EFL eligibility declaration for the welcome cutscene. `trigger` is empty so it is unconditional — the cutscene is always eligible on first entry. `once: true` means EFL blocks it from firing a second time once the flag is set.

The cutscene content itself (Auri's greeting dialogue, camera moves) is authored in your MOMI companion mod's Mist file (`__mist__.json`), not here. EFL's job is only to say "yes, play it now" and set `example_room_visited` when it does.

### calendar/spring_first_visit.json

```json
{
    "id": "hello_adventurer_spring_kickoff",
    "displayName": "Spring Kickoff Reminder",
    "season": "spring",
    "dayOfSeason": 1,
    "lifecycle": "once",
    "onActivate": "example_room_welcome"
}
```

The V3 calendar pilot in action. Once per save, on Spring 1, EFL fires the `example_room_welcome` story event — even if the player never enters the example room. `lifecycle: "once"` keeps it from re-firing every Spring 1, and the empty `condition` field means it's unconditional. Adding `"calendar"` to the manifest's `features` array is what unlocks the `calendar/` folder loader at boot.

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
- **Add resources**: Add `"resources"` to the features array in the manifest and create resource nodes in `resources/`
- **Change the host room**: Try a different `hostRoom` value to see how the area looks in a different game room
- **Add another calendar event**: Drop a second file in `calendar/` for `dayOfSeason: 14` to fire the welcome again at the season's halfway point

## Hello Compat (`.efdat`)

The `examples/hello_compat/` workspace is a tiny `.efdat` reference. It ships **no content** of its own — just a `manifest.efdat` declaring relationships:

```
examples/hello_compat/
└── manifest.efdat
```

```json
{
    "schemaVersion": 1,
    "datId": "com.efl.example.compat.hello-adventurer.x.fluffkin-mod",
    "name": "Hello Adventurer x Fluffkin Compatibility",
    "version": "1.0.0",
    "eflVersion": "1.0.0",
    "author": "EFL Team",
    "description": "Reference .efdat showing how a third party can declare compatibility relationships between an EFL pack and a MOMI mod without shipping any content of their own.",
    "relationships": [
        {
            "type": "requires",
            "target": {
                "kind": "efpack",
                "id": "com.efl.example.hello-adventurer",
                "versionRange": "^1.0.0"
            },
            "reason": "This artifact attaches calendar wiring to the Hello Adventurer pack."
        },
        {
            "type": "requires",
            "target": {
                "kind": "momi",
                "id": "com.example.fluffkin-mod",
                "versionRange": ">=1.0.0"
            },
            "reason": "Fluffkin Mod ships the portrait pack and __mist__ cutscene Hello Adventurer references."
        },
        {
            "type": "optional",
            "target": {
                "kind": "efpack",
                "id": "com.example.seasonal-overhaul",
                "versionRange": "^0.5.0"
            }
        },
        {
            "type": "conflicts",
            "target": {"kind": "efpack", "id": "com.example.silent-room"},
            "reason": "Silent Room replaces the same hostRoom, so the example NPC would never spawn alongside it."
        }
    ]
}
```

### When to ship a `.efdat`

`.efdat` artifacts exist for the case where the **relationship** is the deliverable:

- A third party noticing two packs work better together — they can ship a `.efdat` declaring `optional` and let users opt in.
- A pack author calling out a `conflicts` pair before shipping a hard fix — `.efdat` surfaces a clean error instead of silent breakage.
- A studio bundling `requires` chains for an internal modpack — one `.efdat` proves the loadout, no source code needed.

### Behaviour at load time

EFL evaluates relationships the same way it does manifest dependencies:

- `requires` missing → `MANIFEST-E003` and the artifact stays inactive.
- `conflicts` present → `MANIFEST-E004` with the `reason` text shown to the user.
- `optional` missing → silent (or a notice in the DevKit's relationships view).

When all `requires` resolve, EFL marks the `.efdat` active and the relationship view in the DevKit lights up the matching pack ↔ MOMI link.
