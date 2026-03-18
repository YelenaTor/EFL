# EFL Project Skeleton Design

## Context

EFL (Expansion Framework Library) is a runtime expansion framework for Fields of Mistria. The architecture, module split, and public API are fully specified in planning documents. This spec defines the **project skeleton** — repository layout, language/framework choices, build targets, TUI design, diagnostic code system, and inter-process communication — so that implementation can begin.

## Decision: Split-Language Architecture

The project has two distinct runtime contexts with different requirements:

1. **Engine DLL** (C++ / CMake) — loaded by Aurie into GameMaker's process. Must use C++ to interface with Aurie/YYTK APIs. Produces a `.dll`.
2. **TUI Loader** (Rust / ratatui) — standalone process that displays boot progress, diagnostics, and live monitoring. Marathon (2025)-inspired display-only interface. Produces a single static binary.

These are independent build targets connected at runtime via a Windows named pipe.

**"Static binary" clarification**: The Rust TUI compiles to a single `.exe` with no Rust-specific shared libraries. It links against the MSVC C runtime dynamically (default Windows behavior). No .NET, no runtime installer required.

## Repository Layout

```
EFL/
├── engine/                        # C++ — Aurie/YYTK runtime DLL
│   ├── CMakeLists.txt
│   ├── vendor/                    # Git submodules
│   │   ├── Aurie/
│   │   └── YYToolkit/
│   ├── include/efl/
│   │   ├── efl.h                  # Master include, version macros
│   │   ├── core/                  # Layer A+C: bootstrap, manifest, services
│   │   │   ├── bootstrap.h
│   │   │   ├── manifest.h
│   │   │   ├── event_bus.h
│   │   │   ├── save_service.h
│   │   │   ├── trigger_service.h
│   │   │   ├── config_service.h
│   │   │   ├── log_service.h
│   │   │   ├── registry_service.h
│   │   │   ├── compatibility_service.h
│   │   │   └── diagnostics.h
│   │   ├── bridge/                # Layer B: PRIVATE (not shipped in SDK)
│   │   │   ├── hooks.h
│   │   │   ├── room_tracker.h
│   │   │   ├── routine_invoker.h
│   │   │   └── instance_walker.h
│   │   ├── registries/            # Layer D: public API interfaces
│   │   │   ├── area_registry.h
│   │   │   ├── warp_service.h
│   │   │   ├── resource_registry.h
│   │   │   ├── crafting_registry.h
│   │   │   ├── npc_registry.h
│   │   │   ├── quest_registry.h
│   │   │   ├── dialogue_service.h
│   │   │   ├── story_bridge.h
│   │   │   └── world_state_service.h
│   │   └── ipc/                   # Layer F: cross-mod IPC
│   │       └── channel_broker.h
│   ├── src/                       # Mirrors include/ layout
│   │   ├── core/
│   │   ├── bridge/
│   │   ├── registries/
│   │   └── ipc/
│   └── tests/
├── tui/                           # Rust — Marathon-styled TUI loader/monitor
│   ├── Cargo.toml
│   └── src/
│       ├── main.rs                # Entry, terminal setup, Ctrl+C handler
│       ├── app.rs                 # State machine: Boot → Diagnostics → Monitor
│       ├── phases/
│       │   ├── mod.rs
│       │   ├── boot.rs            # Phase 1: version checks, manifest discovery
│       │   ├── diagnostics.rs     # Phase 2: validation report with coded errors
│       │   └── monitor.rs         # Phase 3: live dashboard
│       ├── widgets/
│       │   ├── mod.rs
│       │   ├── panel.rs           # Marathon-style bordered panel with header
│       │   ├── status_bar.rs      # Colored stat bars (weapon-stat style)
│       │   ├── card_grid.rs       # Status cards grid (faction-screen style)
│       │   ├── diagnostic_list.rs # Scrollable coded error/warning/hazard list
│       │   └── phase_indicator.rs # Current phase header display
│       ├── diagnostics/
│       │   ├── mod.rs
│       │   ├── codes.rs           # All diagnostic codes by category
│       │   ├── collector.rs       # Collects diagnostics during boot/validation
│       │   └── severity.rs        # Error / Warning / Hazard enum
│       ├── theme.rs               # Marathon palette & styling constants
│       └── ipc.rs                 # Named pipe reader (\\.\pipe\efl-{pid})
├── schemas/                       # JSON Schema files (shared)
│   ├── efl-manifest.schema.json
│   ├── area.schema.json
│   ├── warp.schema.json
│   ├── resource.schema.json
│   ├── recipe.schema.json
│   ├── npc.schema.json
│   ├── quest.schema.json
│   ├── trigger.schema.json
│   ├── dialogue.schema.json
│   ├── event.schema.json
│   └── ipc-message.schema.json    # IPC envelope + all payload types
├── .temp/                         # GITIGNORED — scratch, resources, dev logs
│   ├── docs/                      # Planning docs (Base_Idea.txt, etc.)
│   └── logs/                      # Dev-time only: TUI debug output, local test runs
├── docs/
│   └── superpowers/specs/         # Design specs
├── .gitignore
├── CLAUDE.md
└── README.md
```

## Engine DLL (C++)

### Build System
- CMake targeting MSVC (Windows DLL output for Aurie)
- Links against Aurie SDK and YYToolkit headers
- Output: `EFL.dll`

### Layer Mapping
| Directory | Layers | Visibility |
|-----------|--------|------------|
| `include/efl/core/` | A (Bootstrap) + C (Core Services) | PUBLIC SDK |
| `include/efl/bridge/` | B (Engine Bridge) | PRIVATE — never shipped |
| `include/efl/registries/` | D (Feature Registries) | PUBLIC SDK |
| `include/efl/ipc/` | F (IPC) | PUBLIC SDK |
| `src/` | All implementations | PRIVATE |

**Layer G (Tooling)**: Deferred to a future spec. The TUI's diagnostics phase covers schema validation and manifest linting for v1. Standalone tooling (doc generator, content pack compiler, debug console) will be specified separately once the core runtime is functional.

### Build Toolchain Requirements
- **CMake**: 3.20+ (for presets support)
- **MSVC**: Visual Studio 2022 (v143 toolset), Windows SDK 10.0.22621.0+
- **Aurie SDK**: Git submodule at `engine/vendor/Aurie/`
- **YYToolkit headers**: Git submodule at `engine/vendor/YYToolkit/`
- **nlohmann/json**: Vendored or fetched via CMake FetchContent (JSON parsing, IPC message serialization)
- **json-schema-validator** (pboettch/json-schema-validator): Wraps nlohmann/json for JSON Schema validation at runtime

### Key Design Rules
- `bridge/` is the only code that touches YYTK/Aurie/GML internals directly
- All public interfaces in `registries/` use stable types only (no engine-internal types leak)
- Content model (Layer E) is data — JSON files validated against `schemas/`, not compiled into the DLL
- Engine DLL validates content JSON against schemas at load time using json-schema-validator (wraps nlohmann/json)

## TUI Loader (Rust + ratatui)

### Behavior
- **Display-only**: no interactive input. Only Ctrl+C to stop.
- Three sequential phases, each with its own rendering layout:

#### Phase 1 — Boot
Shows EFL startup sequence: version checks, manifest discovery, subsystem initialization, hook registration. Items appear with status indicators as they complete (checkmark, warning icon, error icon).

#### Phase 2 — Diagnostics
Structured validation report. Marathon-style card grid showing per-subsystem results. Each card shows subsystem name, status (pass/warn/fail), and a count of diagnostics. Below the grid: scrollable list of all coded diagnostics with severity, code, and message.

#### Phase 3 — Monitor
Live dashboard while game runs. Panels for: active hooks, recent events, save operations, loaded mods and their status. Auto-updating via named pipe feed from engine DLL.

### Marathon Visual Style
Derived from Marathon (2025) UI reference screenshots:
- Dark background (~#1a1a1a) with neon accent colors
- Bordered rectangular panels with thin border lines
- Color-coded status: green = active/pass, amber = warning, magenta/red = error/hostile, cyan = info
- Bold all-caps section headers
- Horizontal colored stat bars with numeric values
- Small pill/badge labels for categories and severity
- Top bar for system status, footer for version/branding
- Clean sans-serif for headers, monospaced for data

### Build Toolchain Requirements
- **Rust edition**: 2021
- **Target**: `x86_64-pc-windows-msvc`
- **Build**: `cargo build --release` from `tui/`

### Key Dependencies (Cargo.toml)
- `ratatui` — TUI framework
- `crossterm` — terminal backend
- `serde` / `serde_json` — JSON parsing (manifests, schemas, pipe messages)
- `windows` (windows-rs) — named pipe reading via dedicated thread (lighter than tokio for single-pipe I/O)
- `jsonschema` — JSON Schema validation during diagnostics phase (using JSON Schema draft 2020-12)

### TUI Launch Model
The engine DLL spawns the TUI as a child process during bootstrap (Phase 0). Sequence:
1. Engine DLL starts, creates named pipe `\\.\pipe\efl-{pid}` (PID-suffixed to avoid collisions)
2. Engine spawns `efl-tui.exe` with the pipe name as a CLI argument
3. TUI connects to the pipe and begins reading
4. If TUI fails to launch or connect, engine continues without it (logs to `.efl.log` only)
5. If TUI disconnects mid-session, engine silently drops pipe messages and continues running

## Diagnostic Code System

### Format
```
CATEGORY-S###
```
Where `CATEGORY` is the subsystem, `S` is severity, `###` is a zero-padded numeric ID.

### Severities
| Code | Name | Meaning |
|------|------|---------|
| E | Error | Fatal to subsystem, prevents operation |
| W | Warning | Degraded but functional, should be fixed |
| H | Hazard | Potential future problem, proactive alert |

### Categories
| Prefix | Subsystem |
|--------|-----------|
| `BOOT` | Bootstrap, version checks, init/shutdown |
| `MANIFEST` | Manifest parsing, capability resolution |
| `HOOK` | Hook registration, trampolines, detours |
| `AREA` | Area backends, host rooms, anchors |
| `WARP` | Warp targets, transitions, trigger eval |
| `RESOURCE` | Spawn tables, node registration |
| `NPC` | NPC definitions, spawn, schedules |
| `QUEST` | Quest lifecycle, stage progression |
| `TRIGGER` | Condition evaluation, circular deps |
| `STORY` | StoryExecutor bridge, cutscenes |
| `SAVE` | Namespace errors, migrations, corruption |
| `IPC` | Channel errors, version mismatches |

### Example Diagnostics
```
MANIFEST-E001: Failed to parse .efl manifest — invalid JSON at line 23
MANIFEST-W003: Manifest declares 'areas' feature but no area JSON files found
HOOK-E002: Failed to register hook for gml_Object_obj_roomtransition_Create_0
BOOT-H001: YYTK version 0.4.2 is below recommended minimum 0.5.0
AREA-W001: Area "moonhollow.main" references host room "rm_unused_shell_01" which was not found
```

## Inter-Process Communication

### Named Pipe: `\\.\pipe\efl-{pid}`

The engine DLL creates the pipe (PID-suffixed). The TUI connects and reads. The TUI is a passive consumer — it never writes to the pipe.

**Message format**: JSON Lines (one JSON object per line, newline-delimited).

### Message Envelope

Every message uses this common envelope:
```json
{
  "type": "boot.status",
  "timestamp": "2026-03-17T14:22:01.003Z",
  "payload": { ... }
}
```

`type` determines the payload shape. `timestamp` is ISO 8601 UTC. All severity strings on the wire are **lowercase** (`"error"`, `"warning"`, `"hazard"`).

### Message Types and Payloads

| Type | Purpose |
|------|---------|
| `boot.status` | Bootstrap step completed/failed |
| `diagnostic` | Coded diagnostic emitted |
| `hook.registered` | Hook successfully registered |
| `hook.fired` | Hook callback executed |
| `event.published` | EventBus event fired |
| `save.operation` | Save/load operation |
| `mod.status` | Mod loaded/enabled/errored |
| `phase.transition` | Signals TUI to switch phase |

**`boot.status` payload**:
```json
{
  "step": "manifest_discovery",
  "status": "ok",
  "detail": "Found 3 .efl manifests"
}
```
`status` is one of: `"ok"`, `"warning"`, `"error"`.

**`diagnostic` payload** (same as Diagnostic Message Structure above):
```json
{
  "code": "MANIFEST-W003",
  "severity": "warning",
  "category": "MANIFEST",
  "message": "Manifest declares 'areas' feature but no area JSON files found",
  "suggestion": "Add area definition files to areas/ or remove 'areas' from features",
  "source": { "file": "moonhollow.efl", "field": "features.areas" }
}
```

**`hook.registered` payload**:
```json
{
  "hookId": "roomtransition_create",
  "target": "gml_Object_obj_roomtransition_Create_0",
  "status": "ok"
}
```

**`hook.fired` payload**:
```json
{
  "hookId": "roomtransition_create",
  "context": { "fromRoom": "rm_farm", "toRoom": "rm_town" }
}
```

**`event.published` payload**:
```json
{
  "eventName": "EflWarpCompleted",
  "source": "com.yourname.moonhollow",
  "data": { "warpId": "farm_to_moonhollow_gate" }
}
```

**`save.operation` payload**:
```json
{
  "operation": "save",
  "namespace": "EFL/com.yourname.moonhollow/areas/moonhollow.main",
  "status": "ok"
}
```
`operation` is one of: `"save"`, `"load"`, `"migrate"`.

**`mod.status` payload**:
```json
{
  "modId": "com.yourname.moonhollow",
  "name": "Moonhollow Expansion",
  "status": "loaded",
  "subsystems": ["areas", "warps", "npcs", "quests"]
}
```
`status` is one of: `"discovered"`, `"loaded"`, `"enabled"`, `"error"`.

**`phase.transition` payload**:
```json
{
  "phase": "diagnostics"
}
```
`phase` is one of: `"boot"`, `"diagnostics"`, `"monitor"`. The engine drives phase transitions — the TUI switches its rendering mode when it receives this message.

### Pipe Lifecycle and Error Handling

- **Creation**: Engine creates the pipe during bootstrap, before any diagnostics are emitted.
- **Buffer**: 64KB pipe buffer. Messages exceeding this during burst are dropped (engine logs a warning to `.efl.log`).
- **No TUI**: If TUI is not running or fails to connect within 2 seconds, engine proceeds without it. All output goes to `.efl.log` only.
- **TUI disconnect**: Engine detects broken pipe on next write, disables pipe output, continues running. No retry.
- **Multi-instance**: PID-suffixed pipe name (`\\.\pipe\efl-{pid}`) avoids collisions between game instances.

## .gitignore Key Entries

```
# Scratch / resources / logs
.temp/

# C++ build artifacts
engine/build/
engine/out/
engine/vendor/*/build/
*.obj
*.dll
*.lib
*.exp
*.pdb
*.ilk

# Rust build artifacts
tui/target/

# IDE
.vs/
.vscode/
*.user
CMakeUserPresets.json
```

## Schemas

All schemas use **JSON Schema draft 2020-12**. Both the engine DLL (via nlohmann/json) and the Rust TUI (via `jsonschema` crate) consume them. The engine validates content JSON at load time; the TUI validates during the diagnostics phase for richer error reporting.

## What This Spec Does NOT Cover

- Implementation details of any specific registry or service (those are in the existing planning docs)
- Content pack authoring format (already specified in GPT Stuff.txt)
- Specific hook implementations or GML routine targets
- Game-specific reverse engineering details
- Layer G standalone tooling (deferred — TUI diagnostics covers v1 validation needs)

## Known Limitations

- Named pipe supports one TUI client per engine instance. A future spec may add multi-client support if needed (e.g., headless log consumer alongside visual TUI).
- Layer G tooling (standalone doc generator, content pack compiler) is deferred to post-Phase 2.
