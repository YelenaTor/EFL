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
| `H` | Hazard | Not currently broken, but likely to cause problems (e.g., potential conflicts with base game). |

## Categories

| Category | Subsystem |
|:---------|:----------|
| `BOOT` | Bootstrap and initialization |
| `MANIFEST` | Manifest parsing and validation |
| `HOOK` | Engine bridge hook registration |
| `AREA` | Area registration and room hijacking |
| `WARP` | Warp point registration |
| `RESOURCE` | Resource node registration |
| `NPC` | NPC registration and lifecycle |
| `QUEST` | Quest registration and stage validation |
| `TRIGGER` | Trigger condition registration |
| `STORY` | Story event and cutscene bridge |
| `SAVE` | Save namespace and persistence |
| `IPC` | Inter-process and cross-mod communication |

## Known Codes (v1 Pre-Release)

The following codes are emitted by the v1 engine. Additional codes will be documented as the engine matures.

### BOOT

| Code | Severity | Description |
|:-----|:---------|:------------|
| `BOOT-W001` | Warning | Content directory does not exist |
| `BOOT-E001` | Error | JSON parse error in content file |

### MANIFEST

| Code | Severity | Description |
|:-----|:---------|:------------|
| `MANIFEST-E001` | Error | Failed to parse manifest file (invalid JSON or missing required fields) |
| `MANIFEST-E002` | Error | Mod requires a newer EFL version than installed |

### Content Registration

| Code | Severity | Description |
|:-----|:---------|:------------|
| `AREA-E001` | Error | Failed to parse area definition |
| `WARP-E001` | Error | Failed to parse warp definition |
| `RESOURCE-E001` | Error | Failed to parse resource definition |
| `NPC-E001` | Error | Failed to parse NPC definition |
| `QUEST-E001` | Error | Failed to parse quest definition |
| `TRIGGER-E001` | Error | Failed to register trigger |
| `CRAFT-E001` | Error | Failed to parse recipe definition |
| `DIALOGUE-E001` | Error | Failed to parse dialogue definition |
| `STORY-E001` | Error | Failed to parse event definition |

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
