# Architecture

EFL is organized into seven layers, each with a single responsibility. Dependencies flow downward — higher layers depend on lower layers, never the reverse.

## Layer Overview

```
┌─────────────────────────────────────────────────┐
│  G  Tooling         schema validator, DevKit, docs │
├─────────────────────────────────────────────────┤
│  F  IPC             cross-mod channels          │
├─────────────────────────────────────────────────┤
│  E  Content Model   declarative JSON defs       │
├─────────────────────────────────────────────────┤
│  D  Registries      areas, warps, NPCs, etc.    │
├─────────────────────────────────────────────────┤
│  C  Core Services   save, events, triggers      │
├─────────────────────────────────────────────────┤
│  B  Engine Bridge   YYTK/Aurie hooks            │
├─────────────────────────────────────────────────┤
│  A  Bootstrap       init, manifests, crash gate  │
└─────────────────────────────────────────────────┘
```

### Layer A — Bootstrap

Entry point for EFL. Handles:
- Version compatibility checks (EFL version vs. pack requirements)
- `.efl` manifest discovery and parsing from the content directory
- Capability resolution (which subsystems to initialize based on declared features)
- Crash boundary — wraps initialization so a failing pack doesn't bring down the game
- DevKit pipe creation and lifecycle management

### Layer B — Engine Bridge

The **only layer that touches game internals** (YYTK, Aurie, GML). Everything above Layer B uses EFL's own abstractions. Contains:
- **Hook registry**: Registers and manages YYTK function hooks
- **Room tracker**: Tracks the current game room and room transitions
- **Named routine invoker**: Calls GML scripts by name through YYTK
- **Instance walker**: Iterates over GameMaker instances
- **Save namespace plumbing**: Bridges EFL save operations to the game's persistence layer
- **Area backend abstraction**: `HijackedRoomBackend` repopulates existing rooms; `NativeRoomBackend` creates runtime custom rooms when a pack opts into `settings.areaBackend = "native"`
- **StoryBridge**: Reuses the game's `StoryExecutor` and cutscene hooks

### Layer C — Core Services

Framework services used by all feature registries:
- **SaveService**: Namespaced persistence (`EFL/<modId>/<feature>/<contentId>`)
- **EventBus**: Publish/subscribe event system for internal communication
- **TriggerService**: Evaluates boolean trigger conditions at runtime
- **ConfigService**: Per-pack configuration management
- **LogService**: Structured logging with category tags
- **RegistryService**: Coordinates all feature registries
- **CompatibilityService**: Semver comparison for version checks
- **DiagnosticEmitter**: Structured diagnostic code emission

### Layer D — Feature Registries

Domain-specific registries that manage content:
- **AreaRegistry**: Custom area definitions and room hijacking
- **WarpService**: Warp point registration and transition handling
- **ResourceRegistry**: Resource node types, spawn rules, yield tables
- **CraftingRegistry**: Crafting recipes and station requirements
- **NpcRegistry**: NPC definitions, spawning, and lifecycle
- **QuestRegistry**: Quest chains, stages, objectives, and rewards
- **DialogueService**: Dialogue sets with conditional entry filtering
- **StoryBridge**: Story event registration and cutscene bridge
- **WorldStateService**: Global state tracking and cross-area data

### Layer E — Content Model

The declarative JSON format that pack authors write. Not code — pure data definitions validated against JSON schemas at load time. Content types: areas, warps, resources, recipes, NPCs, dialogue, quests, triggers, events.

### Layer F — IPC

Cross-mod communication:
- **PipeWriter**: Writes JSON Lines to the named pipe for DevKit/runtime diagnostics consumption
- **ChannelBroker**: Versioned pub/sub channels for mod-to-mod messaging, declared in manifests

### Layer G — Tooling

Developer-facing tools (partially external to the DLL):
- JSON Schema validation at load time
- DevKit app (separate Rust binary)
- Diagnostic code system

## Module List

| Module | Layers | Responsibility |
|:-------|:-------|:---------------|
| `EFL.Core` | A, C | Bootstrap, core services |
| `EFL.EngineBridge.FoM` | B | All YYTK/Aurie/GML interaction |
| `EFL.Areas` | D | Area registration, room backends |
| `EFL.Warps` | D | Warp points and transitions |
| `EFL.Resources` | D | Resource nodes and spawn tables |
| `EFL.Crafting` | D | Recipes and crafting integration |
| `EFL.NPC` | D | NPC registration and lifecycle |
| `EFL.Quests` | D | Quest chains and objectives |
| `EFL.Story` | D | Story events and cutscene bridge |
| `EFL.Triggers` | C, D | Trigger condition evaluation |
| `EFL.IPC` | F | Pipe writer and channel broker |

## Key Design Rule

Public EFL APIs are high-level and stable. All raw YYTK, Aurie, and GML details are private to Layer B (`EFL.EngineBridge.FoM`). Pack authors and other modules never interact with game internals directly.

## Initialization Flow

1. **Aurie** loads `EFL.dll` and calls `ModuleInitialize`
2. `EflBootstrap::initialize()` begins the boot sequence
3. Named pipe is created for DevKit communication
4. Version check runs (EFL version logged)
5. Content directory is scanned for `.efl` manifest files
6. Each manifest is parsed and validated against the running EFL version
7. For each valid manifest, content is loaded in dependency order: triggers first (so other systems can reference them), then areas, warps, resources, quests, NPCs, crafting, dialogue, events
8. Boot status is emitted to the DevKit pipe at each step
9. Engine enters runtime mode — hooks are active, registries are populated

## Manifest-Driven Initialization

EFL only initializes subsystems declared in a pack's `features` map. If a pack declares `"areas": true` but `"quests": false`, the quest registry is never invoked for that pack. In strict mode, attempting to use an undeclared feature is an error.

## Further Reading

- [Engine Bridge](engine-bridge.md) — Layer B details
- [IPC Protocol](ipc-protocol.md) — Pipe format and message types
- [Save Format](save-format.md) — Persistence namespace and isolation
