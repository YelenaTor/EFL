# Content Types

EFL Packs define content through JSON files organized in subdirectories. Each content type has its own schema, file location, and set of required/optional fields. All files are validated against the schemas in `schemas/` at load time.

---

## Areas

Custom locations created by hijacking existing game rooms.

**File location**: `areas/*.json`
**Schema**: `schemas/area.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique area identifier |
| `displayName` | string | yes | Human-readable area name |
| `backend` | string | yes | `"hijacked"` (v1) or `"native"` (v2) |
| `hostRoom` | string | no | Game room to hijack (required when backend is `"hijacked"`) |
| `locationId` | string | no | Location identifier for map integration |
| `music` | string | no | Music track to play (empty string = no change) |
| `tileProfile` | string | no | Tile profile for ground appearance |
| `spawnTable` | string | no | Reference to a resource spawn table |
| `entryAnchor` | string | no | Default entry point anchor name |
| `unlockTrigger` | string | no | Trigger ID that gates area access (empty = always accessible) |
| `saveNamespace` | string | no | Override save namespace for this area |

```json
{
    "id": "crystal_cave",
    "displayName": "Crystal Cave",
    "backend": "hijacked",
    "hostRoom": "rm_mine_04",
    "music": "",
    "entryAnchor": "cave_entrance",
    "unlockTrigger": "has_pickaxe_trigger"
}
```

**V1 limitation**: Only the `"hijacked"` backend is available. You must specify a `hostRoom` that exists in the base game.

---

## Warps

Transition points that connect two areas.

**File location**: `warps/*.json`
**Schema**: `schemas/warp.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique warp identifier |
| `sourceArea` | string | yes | Area the player warps from |
| `targetArea` | string | yes | Area the player warps to |
| `sourceAnchor` | string | no | Position in the source area |
| `targetAnchor` | string | no | Position in the target area where the player arrives |
| `transition` | string | no | Transition animation type |
| `requireTrigger` | string | no | Trigger ID that gates this warp (empty = always usable) |
| `failureText` | string | no | Text shown when the trigger condition isn't met |

```json
{
    "id": "town_to_cave",
    "sourceArea": "town",
    "sourceAnchor": "cave_door",
    "targetArea": "crystal_cave",
    "targetAnchor": "cave_entrance",
    "requireTrigger": "has_pickaxe_trigger",
    "failureText": "The entrance is blocked by rubble. Maybe a pickaxe would help."
}
```

---

## Resources

Harvestable, breakable, and forageable nodes that spawn in areas.

**File location**: `resources/*.json`
**Schema**: `schemas/resource.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique resource identifier |
| `kind` | string | yes | `"forageable_node"`, `"breakable_node"`, `"harvestable_node"`, or `"special_interactable"` |
| `sprite` | string | no | Sprite sheet reference |
| `yieldTable` | array | no | Items dropped when harvested (each: `item`, `min`, `max`) |
| `spawnRules` | object | no | Where and when this resource appears |
| `spawnRules.areas` | array | no | Area IDs where this resource can spawn |
| `spawnRules.respawnPolicy` | string | no | How the resource respawns (e.g., `"daily"`, `"weekly"`) |
| `spawnRules.seasonal` | array | no | Seasons when this resource is available |
| `spawnRules.dungeonVotes` | array | no | Vote entries for FoM dungeon floor spawn pools (see below) |
| `interaction` | object | no | How the player interacts with this resource |
| `interaction.tool` | string | no | Required tool (e.g., `"pickaxe"`, `"hoe"`) |
| `interaction.scriptMode` | string | no | Script behavior mode |

### Dungeon Vote Spawn (`spawnRules.dungeonVotes`)

Each entry in `dungeonVotes` adds this resource to a FoM dungeon floor spawn pool. EFL calls `register_node@Anchor@Anchor` once per entry when node prototypes are initialized.

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `biome` | string | yes | Dungeon biome (see table below) |
| `pool` | string | yes | Spawn pool within the biome (see table below) |
| `weight` | number | yes | Relative spawn weight (higher = more frequent) |

**Biome strings** (from `dungeons/dungeons/biomes/<index>` in FoM data):

| Biome string | Index | Floor range |
|:-------------|:------|:------------|
| `"upper_mines"` | 0 | Floors 1–19 |
| `"tide_caverns"` | 1 | Floors 20–39 |
| `"deep_earth"` | 2 | Floors 40–59 |
| `"lava_caves"` | 3 | Floors 60–79 |
| `"ancient_ruins"` | 4 | Floors 80+ |

**Pool strings**:

| Pool | Type |
|:-----|:-----|
| `ore_rock` | Primary ore nodes |
| `seam_rock` | Seam ore nodes |
| `small_rock` | Small rock nodes |
| `large_rock` | Large rock obstacles |
| `enemy` | Enemy spawn slots |
| `junk` | Junk/debris nodes |
| `breakable` | Breakable objects |
| `chest` | Treasure chests |
| `fish` | Fishing spots |
| `bug` | Bug catch spots |
| `forageable` | Forageable plants |
| `void_rock` | Void-biome rock nodes |
| `void_herb` | Void-biome herb nodes |
| `void_pearl` | Void-biome pearl nodes |

> **Note**: Dungeon vote injection requires `gml_Script_register_node@Anchor@Anchor`. The struct argument layout is pending a runtime probe session. This feature is implemented as a stub in the current release (votes are registered but injection call is deferred until struct fields are confirmed). See RESOURCE-W001 in diagnostic codes.

```json
{
    "id": "mythril_ore",
    "kind": "breakable_node",
    "sprite": "sprites/mythril.png",
    "yieldTable": [
        { "item": "mythril_chunk", "min": 1, "max": 3 }
    ],
    "spawnRules": {
        "areas": ["crystal_cave"],
        "respawnPolicy": "weekly",
        "seasonal": ["winter", "spring"],
        "dungeonVotes": [
            { "biome": "deep_earth", "pool": "ore_rock", "weight": 10 },
            { "biome": "lava_caves", "pool": "seam_rock", "weight": 5 }
        ]
    },
    "interaction": {
        "tool": "pickaxe"
    }
}
```

---

## Recipes

Crafting recipes at specific stations.

**File location**: `recipes/*.json`
**Schema**: `schemas/recipe.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique recipe identifier |
| `output` | string | yes | Item produced |
| `station` | string | no | Crafting station required (e.g., `"anvil"`, `"kitchen"`) |
| `ingredients` | array | no | List of required items (each: `item`, `count`) |
| `unlockTrigger` | string | no | Trigger ID that makes this recipe available |

```json
{
    "id": "mythril_ring",
    "output": "mythril_ring",
    "station": "anvil",
    "ingredients": [
        { "item": "mythril_chunk", "count": 5 },
        { "item": "gold_bar", "count": 1 }
    ],
    "unlockTrigger": "found_mythril_trigger"
}
```

---

## NPCs

Characters that appear in EFL areas.

**File location**: `npcs/*.json`
**Schema**: `schemas/npc.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique NPC identifier |
| `displayName` | string | yes | Name shown in dialogue |
| `kind` | string | yes | `"local"` (v1) or `"world"` (v2) |
| `defaultArea` | string | no | Area where this NPC spawns |
| `spawnAnchor` | string | no | Position within the area |
| `portraitPack` | string | no | Portrait set to use for dialogue |
| `dialogueSet` | string | no | ID of the dialogue definition to use |
| `schedule` | object | no | Time-based position changes (map of day → array of `{time, anchor}`) |
| `unlockTrigger` | string | no | Trigger ID that controls NPC visibility |
| `saveNamespace` | string | no | Override save namespace for this NPC |

```json
{
    "id": "cave_hermit",
    "displayName": "Old Hermit",
    "kind": "local",
    "defaultArea": "crystal_cave",
    "spawnAnchor": "npc_spot_01",
    "portraitPack": "hayden",
    "dialogueSet": "hermit_dialogue",
    "unlockTrigger": ""
}
```

**V1 limitation**: Only `"local"` NPCs are supported. They exist only within EFL areas. World NPCs with global schedules, hearts, and gift systems are planned for v2.

---

## Dialogue

Conditional dialogue entries for NPCs.

**File location**: `dialogue/*.json`
**Schema**: `schemas/dialogue.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique dialogue set identifier |
| `npc` | string | no | NPC this dialogue belongs to |
| `entries` | array | no | List of dialogue entries |
| `entries[].id` | string | no | Entry identifier |
| `entries[].text` | string | no | The dialogue text shown to the player |
| `entries[].portrait` | string | no | Portrait expression (e.g., `"happy"`, `"sad"`, `"angry"`) |
| `entries[].condition` | string | no | Condition that must be met for this entry to show |

```json
{
    "id": "hermit_dialogue",
    "npc": "cave_hermit",
    "entries": [
        {
            "id": "greeting",
            "text": "Ah, a visitor! It's been ages since anyone found this cave.",
            "portrait": "happy",
            "condition": ""
        },
        {
            "id": "return_visit",
            "text": "Back again? You must like crystals as much as I do.",
            "portrait": "wink",
            "condition": "flag:visited_cave"
        }
    ]
}
```

Conditions use the shorthand format `"flag:<flag_name>"` to check if a flag has been set. An empty condition means the entry always shows. Entries are evaluated in order; the first matching set of entries is used.

---

## Quests

Multi-stage quest chains with objectives and rewards.

**File location**: `quests/*.json`
**Schema**: `schemas/quest.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique quest identifier |
| `title` | string | yes | Quest title shown to the player |
| `stages` | array | yes | Ordered list of quest stages |
| `stages[].id` | string | yes | Stage identifier |
| `stages[].objectives` | array | no | What the player must do (each has a `type`) |
| `stages[].onComplete` | array | no | Actions taken when the stage is completed |
| `rewards` | array | no | Items or effects granted on quest completion |

```json
{
    "id": "crystal_collection",
    "title": "Crystal Collection",
    "stages": [
        {
            "id": "gather",
            "objectives": [
                { "type": "collect", "item": "mythril_chunk", "count": 3 }
            ],
            "onComplete": [
                { "type": "setFlag", "flag": "found_mythril" }
            ]
        },
        {
            "id": "return",
            "objectives": [
                { "type": "talkTo", "npc": "cave_hermit" }
            ]
        }
    ],
    "rewards": [
        { "type": "item", "item": "hermit_blessing", "count": 1 }
    ]
}
```

---

## Triggers

Reusable boolean conditions that gate content across the framework.

**File location**: `triggers/*.json`
**Schema**: `schemas/trigger.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique trigger identifier (referenced by other content) |
| `type` | string | yes | `"flagSet"`, `"questComplete"`, `"allOf"`, or `"anyOf"` |
| `conditions` | array | no | Child conditions (for `allOf`/`anyOf` types) |

```json
{
    "id": "found_mythril_trigger",
    "type": "flagSet",
    "flagName": "found_mythril"
}
```

See [Triggers and Conditions](triggers-and-conditions.md) for compound triggers and nesting.

---

## Events

Story events and cutscene bridges.

**File location**: `events/*.json`
**Schema**: `schemas/event.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique event identifier |
| `mode` | string | no | `"nativeBridge"` (uses FoM's StoryExecutor) or `"customOverlay"` |
| `trigger` | string | no | Trigger ID that activates this event (empty = manual activation) |
| `commands` | array | no | Sequence of commands to execute |

```json
{
    "id": "hermit_intro_cutscene",
    "mode": "nativeBridge",
    "trigger": "first_cave_visit",
    "commands": [
        { "type": "dialogue", "npc": "cave_hermit", "line": "Welcome to my domain, young one." },
        { "type": "setFlag", "flag": "met_hermit" }
    ]
}
```

**V1 note**: The `"nativeBridge"` mode reuses the game's built-in StoryExecutor and cutscene system. EFL does not implement its own cutscene engine.
