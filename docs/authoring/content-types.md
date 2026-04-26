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
| `backend` | string | yes | `"hijacked"` or `"native"` |
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
    "id": "forgotten_greenhouse",
    "displayName": "Forgotten Greenhouse",
    "backend": "hijacked",
    "hostRoom": "rm_water_seal_0",
    "music": "",
    "entryAnchor": "greenhouse_gate",
    "unlockTrigger": "greenhouse_unlocked"
}
```

When using `"hijacked"`, you must provide a `hostRoom` that exists in the base game.  
When using `"native"`, EFL creates and owns a runtime room for the area session.

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
    "id": "town_to_greenhouse",
    "sourceArea": "town",
    "sourceAnchor": "greenhouse_door",
    "targetArea": "forgotten_greenhouse",
    "targetAnchor": "greenhouse_gate",
    "requireTrigger": "greenhouse_unlocked",
    "failureText": "The gate is rusted shut. Maybe someone in town knows the key."
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
| `objectName` | string | no | FoM GML object name (e.g. `"obj_ore_node"`). Required for interim spawn via `instance_create_layer`. |
| `yieldTable` | array | no | Items dropped when harvested (each: `item`, `itemId`, `min`, `max`) |
| `yieldTable[].item` | string | no | Human-readable item name (used in logs and IPC events) |
| `yieldTable[].itemId` | integer | no | Numeric FoM item index. Required for actual item granting — see [FoM-Datamined-Insanity](https://github.com/YelenaTor/FoM-Datamined-Insanity) item IDs. Without this, harvest is logged but no item is given. |
| `yieldTable[].min` | integer | no | Minimum yield quantity (default: 1) |
| `yieldTable[].max` | integer | no | Maximum yield quantity (default: 1) |
| `spawnRules` | object | no | Where and when this resource appears |
| `spawnRules.areas` | array | no | Area IDs where this resource can spawn |
| `spawnRules.anchors` | object | no | Per-area grid position as `"area_id": "x,y"` (auto-adds area to `areas`) |
| `spawnRules.respawnPolicy` | string | no | How the resource respawns (`"daily"`, `"seasonal"`, `"none"`) |
| `spawnRules.seasonal` | array | no | Seasons when this resource is available |
| `spawnRules.dungeonVotes` | array | no | Vote entries for FoM dungeon floor spawn pools (see below) |
| `interaction` | object | no | How the player interacts with this resource |
| `interaction.tool` | string | no | Required tool (e.g., `"pickaxe"`, `"hoe"`) |
| `interaction.scriptMode` | string | no | Script behavior mode |

### Dungeon Vote Spawn (`spawnRules.dungeonVotes`)

Each entry in `dungeonVotes` adds this resource to a FoM dungeon floor spawn pool through EFL's runtime dungeon vote hook path.

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

> **Note**: Dungeon vote injection depends on runtime hook compatibility for your current FoM build. If the hook cannot bind, EFL emits `RESOURCE-W001` and falls back without vote injection.

```json
{
    "id": "mythril_ore",
    "kind": "breakable_node",
    "sprite": "sprites/mythril.png",
    "objectName": "obj_ore_node",
    "yieldTable": [
        { "item": "mythril_chunk", "itemId": 312, "min": 1, "max": 3 }
    ],
    "spawnRules": {
        "areas": ["forgotten_greenhouse"],
        "anchors": { "forgotten_greenhouse": "5,12" },
        "respawnPolicy": "daily",
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
| `kind` | string | yes | `"local"` (area-bound) or `"world"` (global schedule — see World NPCs below) |
| `defaultArea` | string | no | Area where this NPC spawns |
| `spawnAnchor` | string | no | Position within the area |
| `portraitPack` | string | no | Portrait set to use for dialogue |
| `dialogueSet` | string | no | ID of the dialogue definition to use |
| `schedule` | object | no | Time-based position changes (map of day → array of `{time, anchor}`) |
| `unlockTrigger` | string | no | Trigger ID that controls NPC visibility |
| `saveNamespace` | string | no | Override save namespace for this NPC |

```json
{
    "id": "town_keeper",
    "displayName": "Town Keeper",
    "kind": "local",
    "defaultArea": "forgotten_greenhouse",
    "spawnAnchor": "npc_spot_01",
    "portraitPack": "hayden",
    "dialogueSet": "keeper_dialogue",
    "unlockTrigger": ""
}
```

Local NPCs exist only within their assigned EFL area and are spawned/despawned when the player enters/leaves the room.

---

## World NPCs

NPCs with global schedules, time-of-day movement, hearts, and gift systems.

**File location**: `world_npcs/*.json`
**Schema**: `schemas/world-npc.schema.json`

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique world NPC identifier |
| `displayName` | string | yes | Name shown in dialogue and the DevKit |
| `objectName` | string | no | FoM GML object name for spawning (e.g. `"obj_npc_merchant"`) |
| `portraitAsset` | string | no | Portrait asset reference |
| `defaultAreaId` | string | no | Area where this NPC appears when no schedule entry matches |
| `defaultAnchorId` | string | no | Anchor position in the default area (format: `"x,y"`) |
| `unlockTrigger` | string | no | Trigger ID that controls NPC visibility |
| `schedule` | array | no | Time-of-day location entries (see below) |
| `heartsPerGift` | integer | no | Hearts gained per gift (default: 1) |
| `giftableItems` | string[] | no | Item IDs this NPC accepts as gifts |

### Schedule Entries

Each schedule entry defines where the NPC should be during a time window. Time values are **seconds since midnight** (e.g. 21600 = 6:00 AM, 43200 = 12:00 PM, 64800 = 6:00 PM).

| Field | Type | Description |
|:------|:-----|:------------|
| `fromSeconds` | integer | Start of the time window (inclusive) |
| `toSeconds` | integer | End of the time window (exclusive) |
| `areaId` | string | EFL area where the NPC should be |
| `anchorId` | string | Position within the area (format: `"x,y"`) |

When EFL detects a schedule boundary crossing (via the per-frame time tick), it despawns the NPC from their old location and spawns them at the new one — if the player is in that room.

```json
{
    "id": "forest_merchant",
    "displayName": "Traveling Merchant",
    "objectName": "obj_npc_merchant",
    "defaultAreaId": "market_square",
    "defaultAnchorId": "320,480",
    "unlockTrigger": "met_merchant_trigger",
    "schedule": [
        { "fromSeconds": 21600, "toSeconds": 43200, "areaId": "market_square", "anchorId": "320,480" },
        { "fromSeconds": 43200, "toSeconds": 64800, "areaId": "forest_clearing", "anchorId": "160,256" }
    ],
    "heartsPerGift": 2,
    "giftableItems": ["rare_gem", "golden_apple"]
}
```

World NPC hearts and gift state are persisted through the `SaveService` and keyed by the pack's `modId`.

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
    "id": "keeper_dialogue",
    "npc": "town_keeper",
    "entries": [
        {
            "id": "greeting",
            "text": "Ah, a visitor! It's been ages since anyone stepped in here.",
            "portrait": "happy",
            "condition": ""
        },
        {
            "id": "return_visit",
            "text": "Back again? Good. This place needs a steady hand.",
            "portrait": "wink",
            "condition": "flag:visited_greenhouse"
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
    "id": "greenhouse_recovery",
    "title": "Greenhouse Recovery",
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
                { "type": "talkTo", "npc": "town_keeper" }
            ]
        }
    ],
    "rewards": [
        { "type": "item", "item": "keeper_token", "count": 1 }
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

Cutscene eligibility declarations and EFL-side side effects.

**File location**: `events/*.json`
**Schema**: `schemas/event.schema.json`

> **Design note**: Cutscene *content* (dialogue, scene steps, camera moves) is authored in MOMI's Mist format (`__mist__.json`) — not in EFL content packs. EFL owns two things only: (1) the eligibility gate (whether FoM should consider the cutscene playable today) and (2) the side effects applied when it fires (flag mutations, quest starts).

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique cutscene identifier. Must match the key in FoM's cutscene registry. |
| `trigger` | string | no | EFL trigger ID. Must be true for the cutscene to be eligible. Empty = unconditional. |
| `once` | boolean | no | If true (default), EFL blocks re-eligibility after first activation. |
| `onFire.setFlags` | string[] | no | EFL flags to set when this cutscene fires. |
| `onFire.clearFlags` | string[] | no | EFL flags to clear when this cutscene fires. |
| `onFire.startQuest` | string | no | EFL quest ID to start when this cutscene fires. |
| `onFire.advanceQuest` | string | no | EFL quest ID to advance when this cutscene fires. |
| `onFire.grantItemId` | integer | no | Numeric FoM item index to grant when this cutscene fires. |
| `onFire.grantItemQty` | integer | no | Quantity to grant (default: 1). |

```json
{
    "id": "greenhouse_reveal",
    "trigger": "has_cave_key",
    "once": true,
    "onFire": {
        "setFlags": ["greenhouse_revealed"],
        "startQuest": "greenhouse_recovery"
    }
}
```

EFL intercepts `gml_Script_check_cutscene_eligible@Mist@Mist` at runtime. If the key matches a registered cutscene and the trigger evaluates true, EFL returns `true` to FoM and applies `onFire` effects. FoM then plays the cutscene using its own Mist interpreter — no EFL command execution is involved.

## Calendar Events

Calendar / world-event registry — V3 pilot.

**File location**: `calendar/*.json`
**Schema**: `schemas/event-calendar.schema.json`
**Required feature tag**: `calendar`

Calendar events fire from EFL's `new_day` hook. Each tick, the engine reads the current season and day-of-season, finds any registered events that match, optionally evaluates a trigger condition, and (if it fires) dispatches a story event through `StoryBridge`. This is intentionally minimal for the V3 pilot — festival scripting, schedule edits, and shop gating come later.

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `id` | string | yes | Unique calendar event id within the pack. |
| `displayName` | string | no | Human-readable name shown in DevKit and tooling. |
| `season` | string \| int | no | One of `spring`, `summer`, `fall`, `winter`, `any` (or 0..=3 if you prefer the raw FoM encoding). Omitted = any. |
| `dayOfSeason` | integer | no | 1..=28. Omitted = every day of the season. |
| `condition` | string | no | Trigger id evaluated before activation. Empty = always satisfied. |
| `onActivate` | string | no | Story event id to fire through `StoryBridge` when the event activates. |
| `lifecycle` | string | no | `daily` (default — fires every matching day) or `once` (fires the first matching day, then stops). |

```json
{
    "id": "summer_kickoff",
    "displayName": "Summer Kickoff",
    "season": "summer",
    "dayOfSeason": 1,
    "condition": "town_unlocked",
    "onActivate": "story_summer_intro",
    "lifecycle": "once"
}
```

EFL keeps fired-once state in memory for the session; persistence across save/load is on the V3 follow-up list.
