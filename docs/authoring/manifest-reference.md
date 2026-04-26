# Manifest Reference

Every EFL pack must contain a `manifest.efl` file at its root. This file declares the pack's identity, dependencies, features, and settings. It is a JSON file validated against `schemas/efl-manifest.schema.json`.

**Current schema version: 2**

If you are upgrading an older pack, use DevKit **Packs -> Migrate...** first. The migration wizard runs a dry analysis and creates a backup before applying changes.

## Full Annotated Example

```json
{
    "schemaVersion": 2,
    "modId": "com.author.my-mod",
    "name": "My Mod",
    "version": "1.1.0",
    "eflVersion": "1.1.0",
    "author": "Your Name",
    "description": "A short description of what this mod does.",

    "dependencies": {
        "required": [
            { "modId": "com.other.required-mod", "versionRange": "^1.0" }
        ],
        "optional": [
            { "modId": "com.other.nice-to-have", "versionRange": ">=0.5" }
        ],
        "conflicts": [
            { "modId": "com.clash.mod", "reason": "Both register spr_MyNPC — use a compat shim instead." }
        ]
    },

    "features": ["areas", "warps", "npcs", "triggers", "dialogue", "story"],

    "assets": {
        "sprites": ["spr_MyNPC", "spr_MyNPC_idle"],
        "sounds": ["snd_MyNPC_greeting"]
    },

    "ipc": {
        "publish": ["com.author.my-mod:npc.greeted"],
        "consume": ["com.other.mod:quest.completed"]
    },

    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked"
    },

    "scriptHooks": [
        { "target": "gml_Script_hoe_node", "handler": "efl_resource_despawn" }
    ]
}
```

---

## Field Reference

### schemaVersion

- **Type**: integer
- **Required**: yes
- **Value**: always `2`
- **Description**: Manifest schema version. Must be `2`. EFL rejects manifests with an unrecognised schema version.

---

### modId

- **Type**: string
- **Required**: yes
- **Pattern**: reverse domain notation, lowercase, no spaces (e.g. `com.author.modname`)
- **Description**: Globally unique identifier. Used for save namespacing, dependency resolution, and IPC channel ownership. Cannot be changed after first release without breaking save compatibility.

---

### name

- **Type**: string
- **Required**: yes
- **Description**: Human-readable display name. Shown in logs and the DevKit.

---

### version

- **Type**: string
- **Required**: yes
- **Description**: Pack version in [semver](https://semver.org/) format (e.g. `1.1.0`, `0.3.1-beta`).

---

### eflVersion

- **Type**: string
- **Required**: yes
- **Description**: Minimum EFL version required to load this pack. EFL emits `MANIFEST-E002` and refuses to load if the installed EFL is older than this value.

---

### author

- **Type**: string
- **Required**: no
- **Description**: Author name. Shown in the DevKit.

---

### description

- **Type**: string
- **Required**: no
- **Description**: Short explanation of what this pack does. Shown in the DevKit.

---

### dependencies

- **Type**: object
- **Required**: no

#### dependencies.required

- **Type**: array of `{ modId, versionRange }` objects
- **Description**: Packs that must be present and loaded before this one. EFL emits `MANIFEST-E003` and refuses to load if any are missing.

```json
"required": [
    { "modId": "com.other.mod", "versionRange": "^1.0" }
]
```

#### dependencies.optional

- **Type**: array of `{ modId, versionRange }` objects
- **Description**: Packs that enhance this one if present. EFL emits `MANIFEST-W001` if any are missing, but loading continues.

#### dependencies.conflicts

- **Type**: array of `{ modId, reason? }` objects
- **Description**: Packs known to be incompatible with this one. EFL emits `MANIFEST-E004` if a conflicting pack is loaded at the same time. Always include a `reason` to help users understand how to resolve the conflict.

```json
"conflicts": [
    { "modId": "com.clash.mod", "reason": "Both register spr_MyNPC." }
]
```

---

### features

- **Type**: array of strings
- **Required**: no
- **Description**: EFL subsystems this pack uses. **Only declare what you actually use** — subsystems absent from this list are assumed unused. In `strictMode`, accessing an undeclared subsystem is a fatal error; without it, a warning is emitted.

| Value | Subsystem |
|:------|:----------|
| `areas` | Custom areas (hijacked rooms) |
| `warps` | Warp and transition points |
| `resources` | Resource nodes (forageables, breakables, harvestables) |
| `crafting` | Crafting recipes |
| `npcs` | NPCs (local and world) |
| `quests` | Quest chains with stages and objectives |
| `triggers` | Reusable boolean trigger conditions |
| `dialogue` | Dialogue sets with conditional entries |
| `story` | Story events and cutscene bridges |
| `ipc` | Cross-mod IPC channel communication (required to use the `ipc` block) |
| `assets` | Runtime asset injection via YYTK (required to use the `assets` block) |
| `calendar` | Calendar / world-event registry (V3 pilot — required to load files under `calendar/`) |
| `migrations` | Save data migration system |

**Example** — a pack using only areas, NPCs, and dialogue:
```json
"features": ["areas", "npcs", "dialogue"]
```

---

### assets

- **Type**: object
- **Required**: no — only valid when `"assets"` is in `features`
- **Description**: Asset bundles included in this pack for runtime injection via YYTK. EFL's AssetService injects these at boot so the game sees them as native assets. The pack must contain the corresponding files at `assets/sprites/<id>.png` and `assets/sounds/<id>.ogg`.

This is the correct way for a self-contained EFL pack to ship its own sprites and sounds — no MOMI coordination required for asset delivery at runtime.

```json
"assets": {
    "sprites": ["spr_MyNPC", "spr_MyNPC_idle"],
    "sounds": ["snd_MyNPC_greeting"]
}
```

#### assets.sprites

- **Type**: array of strings
- **Description**: Sprite IDs to inject (e.g. `spr_MyNPC`). File must exist at `assets/sprites/spr_MyNPC.png` inside the compiled `.efpack`.

#### assets.sounds

- **Type**: array of strings
- **Description**: Sound IDs to inject (e.g. `snd_MyNPC_greeting`). File must exist at `assets/sounds/snd_MyNPC_greeting.ogg` inside the compiled `.efpack`.

---

### ipc

- **Type**: object
- **Required**: no — only valid when `"ipc"` is in `features`
- **Description**: Cross-mod IPC channel declarations. Publish channels must be owned by this pack (prefixed with `modId`). Consume channels may be owned by other packs.

```json
"ipc": {
    "publish": ["com.author.my-mod:npc.greeted"],
    "consume": ["com.other.mod:quest.completed"]
}
```

#### ipc.publish

- **Type**: array of strings (channel IDs)
- **Description**: Channels this pack will write messages to.

#### ipc.consume

- **Type**: array of strings (channel IDs)
- **Description**: Channels this pack will subscribe to.

---

### settings

- **Type**: object
- **Required**: no

#### settings.strictMode

- **Type**: boolean
- **Default**: `false`
- **Description**: When `true`, accessing any EFL subsystem not declared in `features` is a fatal error. When `false`, it emits a warning but loading continues. Recommended `true` during development to catch undeclared subsystem access early.

#### settings.areaBackend

- **Type**: string — `"hijacked"` or `"native"`
- **Default**: `"hijacked"`
- **Description**: Area backend to use. `hijacked` repopulates existing game rooms dynamically. `native` (true custom room registration) is probe-gated — use `hijacked` unless the native room hook surface has been confirmed by the EFL maintainers.

---

### scriptHooks

- **Type**: array of objects
- **Required**: no
- **Description**: Custom GML script hooks. Each entry patches a FoM script to call an EFL handler when it fires.

```json
"scriptHooks": [
    { "target": "gml_Script_hoe_node", "handler": "efl_resource_despawn" }
]
```

| Property | Type | Required | Description |
|:---------|:-----|:---------|:------------|
| `target` | string | yes | Exact GML script name to hook (e.g. `gml_Script_hoe_node`) |
| `handler` | string | yes | Built-in EFL handler to invoke (e.g. `efl_resource_despawn`) |
| `mode` | string | no | `"callback"` (default) or `"inject"`. `"inject"` is reserved for future GML injection support and emits `HOOK-W002` if used. |

---

## The .efdat Format

`.efdat` files are compatibility artifacts — standalone distributables that declare relationships between EFL packs and MOMI mods without containing any content of their own. They are typically authored by third parties and compiled and checksummed the same way as `.efpack`.

See `schemas/efl-dat.schema.json` for the full schema.

### When to use .efdat

Use an `.efdat` when:
- Your pack works better alongside a specific MOMI mod (e.g. alternate textures)
- Two packs conflict and a shim is needed to inform users
- You want to declare cross-mod compatibility as a versioned, distributable artifact

### Example .efdat

```json
{
    "schemaVersion": 1,
    "datId": "com.hello.compat.fancytextures",
    "name": "HelloAdventurer × FancyNPCTextures Compatibility",
    "version": "1.1.0",
    "eflVersion": "1.1.0",
    "author": "CompatAuthor",
    "description": "Declares compatibility between HelloAdventurer and FancyNPCTextures.",

    "relationships": [
        {
            "type": "requires",
            "target": { "kind": "efpack", "id": "com.example.hello_adventurer", "versionRange": "^1.0" }
        },
        {
            "type": "requires",
            "target": { "kind": "momi", "id": "FancyNPCTextures", "versionRange": ">=2.0" }
        },
        {
            "type": "conflicts",
            "target": { "kind": "efpack", "id": "com.other.npcmod" },
            "reason": "Both override Flora's sprite — install one or the other, not both."
        }
    ]
}
```

### Relationship types

| Type | Meaning |
|:-----|:--------|
| `requires` | The artifact only activates when this target is loaded. Missing target emits `MANIFEST-E003`. |
| `optional` | The artifact can partially activate without this target. Missing target emits `MANIFEST-W001`. |
| `conflicts` | Emit `MANIFEST-E004` if this target is loaded alongside the required targets. |

### Target kinds

| Kind | Description |
|:-----|:------------|
| `efpack` | Another EFL content pack, identified by its `modId` |
| `momi` | A MOMI mod, identified by its MOMI mod ID |

---

## Schema Version History

| Schema version | EFL version | Changes |
|:---------------|:------------|:--------|
| `1` | 1.x | Initial format. `features` as boolean object. `dependencies` as plain string arrays. No assets, ipc, or conflicts blocks. |
| `2` | 2.0 | `features` changed to string array (declare only what you use). `dependencies.required`/`optional` changed to `{modId, versionRange}` objects. `dependencies.conflicts` added. `assets` block added for runtime YYTK injection. `ipc` block added with channel names. `saveScope` removed (auto-derived from `modId`). `author` and `description` added. |
