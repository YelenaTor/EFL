# Manifest Reference

Every EFL Pack must contain a `manifest.efl` file at its root. This file declares the pack's identity, dependencies, features, and settings. It is a JSON file validated against `schemas/efl-manifest.schema.json`.

## Full Annotated Manifest

```json
{
    "schemaVersion": 1,
    "modId": "com.author.my-mod",
    "name": "My Awesome Mod",
    "version": "1.0.0-pre.3",
    "eflVersion": "1.0.0-pre.3",
    "dependencies": {
        "required": ["com.other.dependency"],
        "optional": ["com.other.nice-to-have"]
    },
    "features": {
        "areas": true,
        "warps": true,
        "resources": false,
        "crafting": false,
        "npcs": true,
        "quests": false,
        "triggers": true,
        "dialogue": true,
        "story": false,
        "ipcPublish": false,
        "ipcConsume": false,
        "migrations": false
    },
    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked",
        "saveScope": "EFL/com.author.my-mod"
    }
}
```

## Field Reference

### schemaVersion

- **Type**: integer
- **Required**: yes
- **Description**: Manifest schema version. Always `1` for the current format. Future EFL versions may increment this if the manifest format changes.

### modId

- **Type**: string
- **Required**: yes
- **Description**: Globally unique identifier in reverse-domain notation (e.g., `com.author.modname`). Used for save namespacing, dependency resolution, and IPC channel ownership. Cannot contain spaces.

### name

- **Type**: string
- **Required**: yes
- **Description**: Human-readable display name. Shown in logs and the TUI monitor.

### version

- **Type**: string
- **Required**: yes
- **Description**: The pack's own version, following [semver](https://semver.org/) (e.g., `"1.0.0-pre.3"`, `"0.3.1-beta"`).

### eflVersion

- **Type**: string
- **Required**: yes
- **Description**: Minimum required EFL version. EFL checks this at boot and rejects packs that require a newer version than what's installed.

### dependencies

- **Type**: object
- **Required**: no

#### dependencies.required

- **Type**: array of strings (modIds)
- **Description**: Packs that must be loaded before this one. EFL emits `MANIFEST-E002` if a required dependency is missing.

#### dependencies.optional

- **Type**: array of strings (modIds)
- **Description**: Packs that enhance this one but aren't strictly necessary. EFL emits a warning if optional dependencies are missing.

### features

- **Type**: object
- **Required**: no
- **Description**: Boolean flags declaring which EFL subsystems this pack uses. In strict mode, accessing an undeclared subsystem is an error; in non-strict mode, it's a warning.

| Feature | Description |
|:--------|:------------|
| `areas` | Register custom areas (hijacked rooms) |
| `warps` | Define warp/transition points between areas |
| `resources` | Register resource nodes (forageables, breakables, harvestables) |
| `crafting` | Define crafting recipes |
| `npcs` | Register NPCs (local to EFL areas in v1) |
| `quests` | Define quest chains with stages and objectives |
| `triggers` | Define reusable boolean trigger conditions |
| `dialogue` | Register dialogue sets with conditional entries |
| `story` | Define story events and cutscene bridges |
| `ipcPublish` | Publish messages to cross-mod IPC channels |
| `ipcConsume` | Subscribe to cross-mod IPC channels |
| `migrations` | Enable save data migration system |

### settings

- **Type**: object
- **Required**: no

#### settings.strictMode

- **Type**: boolean
- **Description**: When `true`, EFL rejects attempts to use features not declared in the `features` map. When `false`, undeclared access emits a warning but proceeds. Recommended: `true` during development.

#### settings.areaBackend

- **Type**: string (`"hijacked"` or `"native"`)
- **Description**: Which area backend to use. V1 only supports `"hijacked"` (repopulates existing game rooms). `"native"` (true custom rooms) is reserved for v2.

#### settings.saveScope

- **Type**: string
- **Description**: Namespace prefix for this pack's save data. Convention: `"EFL/<modId>"`. All persisted state (flags, quest progress, NPC state) is stored under this prefix.
