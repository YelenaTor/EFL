# Diagnostic Codes

EFL emits structured diagnostic codes during content validation and runtime. These codes appear in log files and in the TUI diagnostics phase.

## Code Format

```
CATEGORY-SEVERITY###
```

- **CATEGORY**: The subsystem that emitted the diagnostic
- **SEVERITY**: Single letter indicating severity
- **###**: Three-digit numeric identifier

Example: `MANIFEST-E001` — an error from the manifest subsystem, code 001.

## Severity Levels

| Prefix | Severity | Meaning |
|:-------|:---------|:--------|
| `E` | Error | Fatal to the subsystem. The affected content will not load. |
| `W` | Warning | Degraded but functional. Content loads with reduced capability. |
| `H` | Hazard | Not currently broken, but likely to cause problems (e.g., potential conflicts with base game or unimplemented stubs). |

## Categories

| Category | Subsystem |
|:---------|:----------|
| `BOOT` | Bootstrap and initialization |
| `MANIFEST` | Manifest parsing and validation |
| `PACK` | `.efpack` archive loading |
| `HOOK` | Engine bridge hook registration |
| `AREA` | Area registration and room hijacking |
| `WARP` | Warp point registration |
| `RESOURCE` | Resource node registration |
| `CRAFT` | Crafting recipe registration |
| `NPC` | NPC registration and lifecycle |
| `QUEST` | Quest registration and stage validation |
| `TRIGGER` | Trigger condition registration |
| `STORY` | Story event and cutscene bridge |
| `DIALOGUE` | Dialogue tree registration |
| `SAVE` | Save namespace and persistence |
| `IPC` | Inter-process and cross-mod communication |

## Codes

### BOOT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `BOOT-W001` | Warning | Content directory does not exist | Create the `EFL/` content directory or verify `contentDir` in your manifest |
| `BOOT-E001` | Error | JSON parse error in a content file during load | Check JSON syntax in the reported file |
| `RELOAD-W001` | Warning | Hot-reload watcher failed to start, or a file change triggered a parse error or reload exception | Check file permissions; inspect the hint text for parse details |

### MANIFEST

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `MANIFEST-E001` | Error | Failed to parse manifest file (invalid JSON or missing required fields) | Check JSON syntax and required fields in your `.efl` manifest |
| `MANIFEST-E002` | Error | Mod's `eflVersion` field requires a newer EFL than is installed | Update EFL via MOMI, or use a mod version compatible with your EFL version |
| `MANIFEST-E003` | Error | A required dependency declared in `dependencies.required` is not loaded | Install the missing dependency via MOMI before loading this pack |
| `MANIFEST-W001` | Warning | An optional dependency declared in `dependencies.optional` is not loaded | Install the optional dependency to enable its related features |

### PACK

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `PACK-E001` | Error | Failed to extract `.efpack` archive | Ensure the file is a valid efpack archive containing a `manifest.efl`; re-pack with `efl-pack` |
| `PACK-E002` | Error | `.efpack` extracted successfully but `manifest.efl` inside failed to parse | Check JSON syntax and required fields in the manifest inside the archive |

### HOOK

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `HOOK-W002` | Warning | Manifest script hook uses `mode: "inject"` which is not yet supported | Remove `mode: "inject"` or use `mode: "callback"`; GML injection is planned for a future release |
| `HOOK-W003` | Warning | A core EFL engine hook failed to register (room_transition, grid_init, frame_update, or tool-node hooks) | This usually means the target script name changed in a FoM update; report to EFL maintainers |
| `HOOK-W004` | Warning | Manifest script hook references an unknown handler name | Check `handler` value against the list of EFL built-in handlers (`efl_resource_despawn`, etc.) |

### AREA

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `AREA-H001` | Hazard | `NativeRoomBackend` activated but native rooms are not yet implemented; falling back to `HijackedRoomBackend` | Set `areaBackend` to `"hijacked"` in manifest settings to suppress this warning |
| `AREA-E001` | Error | Failed to parse an area definition file | Check JSON syntax and required fields in the reported `.json` file |

### WARP

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `WARP-W001` | Warning | A warp transition was suppressed because its `requireTrigger` condition is not met | This is expected during gameplay; fulfil the trigger condition to allow the warp |
| `WARP-E001` | Error | Failed to parse a warp definition file | Check JSON syntax and required fields |

### RESOURCE

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `RESOURCE-H001` | Hazard | `efl_resource_despawn` handler fired but the despawn stub is not yet implemented | Resource spawning integration is pending; this will be resolved in a future EFL release |
| `RESOURCE-W001` | Warning | Failed to register the dungeon vote injection hook (`create_node_prototypes`) | EFL resource nodes with `dungeonVotes` will not appear in FoM dungeon floors |
| `RESOURCE-E001` | Error | Failed to parse a resource definition file | Check JSON syntax and required fields |

### CRAFT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `CRAFT-W001` | Warning | Failed to register the crafting menu hook (`spawn_crafting_menu`) | EFL recipes will not be injected into crafting stations; this may indicate a FoM update changed the script name |
| `CRAFT-E001` | Error | Failed to parse a recipe definition file | Check JSON syntax and required fields |

### NPC

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `NPC-E001` | Error | Failed to parse a local or world NPC definition file | Check JSON syntax and required fields |

### QUEST

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `QUEST-E001` | Error | Failed to parse a quest definition file | Check JSON syntax and required fields |

### TRIGGER

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `TRIGGER-E001` | Error | Failed to register a trigger condition (invalid condition type or circular dependency) | Check trigger syntax and ensure no self-referential or mutually circular conditions |

### DIALOGUE

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `DIALOGUE-E001` | Error | Failed to parse a dialogue definition file | Check JSON syntax and required fields |

### STORY

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `STORY-E001` | Error | Failed to parse a story event definition file | Check JSON syntax and required fields |

## Reading Diagnostics

When a diagnostic is emitted, the log entry includes:

1. The diagnostic code (e.g., `MANIFEST-E001`)
2. A human-readable description of the problem
3. A suggested fix or next step

Example log output:

```
[MANIFEST-E001] Failed to parse manifest: my-mod.efl
  → Check JSON syntax and required fields
```

In the TUI, diagnostics appear as color-coded cards grouped by category during the diagnostics phase.
