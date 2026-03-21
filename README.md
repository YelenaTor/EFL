<p align="center">
  <br />
  <strong><code>E F L</code></strong>
  <br />
  <em>Expansion Framework Library</em>
  <br />
  <sub>by Yoru — runtime expansion framework for fields of mistria</sub>
  <br /><br />
  <a href="#"><img src="https://img.shields.io/badge/EFL-v1.0.0--pre.1-B4A7D6?style=flat-square&labelColor=2D2D2D" alt="EFL Version" /></a>
  <a href="#"><img src="https://img.shields.io/badge/status-v1.0.0--pre.1%20prerelease-B6D7A8?style=flat-square&labelColor=2D2D2D" alt="Status" /></a>
  <a href="#"><img src="https://img.shields.io/badge/platform-windows-A4C2F4?style=flat-square&labelColor=2D2D2D" alt="Platform" /></a>
  <a href="#"><img src="https://img.shields.io/badge/language-C%2B%2B%2020%20%7C%20Rust-B6D7A8?style=flat-square&labelColor=2D2D2D" alt="Languages" /></a>
</p>

---

<br />

> *a runtime expansion framework that sits above MOMI and provides a stable,*
> *declarative authoring surface for expansion-scale content mods.*

<br />

## what is efl

EFL is a **runtime expansion framework** for [Fields of Mistria](https://store.steampowered.com/app/2142790/Fields_of_Mistria/). It provides mod authors with a high-level, declarative API for building expansion-scale content — custom areas, NPCs, quests, crafting recipes, dialogue, and more — without touching raw game internals.

```
┌──────────────────────────────────────────────┐
│              content packs (.efl)            │
│         json manifests + asset folders       │
├──────────────────────────────────────────────┤
│                     efl                      │
│     bootstrap · services · registries        │
├──────────────────────────────────────────────┤
│            aurie + yytoolkit                 │
│          native hooks + gml bridge           │
├──────────────────────────────────────────────┤
│           fields of mistria (gm)             │
└──────────────────────────────────────────────┘
```

<br />

## what you can build

EFL v1 supports fully declarative content packs — no C++ or GML required:

| feature | what it does |
|:--------|:-------------|
| **areas** | hijack existing game rooms and repopulate them as new locations with custom music, spawn tables, and entry anchors |
| **warps** | define transition points between areas, gated by unlock triggers with customizable failure text |
| **resources** | register forageables, breakables, and harvestables with yield tables, respawn policies, seasonal availability, and tool requirements |
| **quests** | multi-stage quest chains with typed objectives (collect, talk to), stage completion actions, and item rewards |
| **npcs** | local NPCs that spawn in EFL areas with dialogue sets, portraits, spawn anchors, and trigger-gated visibility |
| **dialogue** | conditional dialogue entries filtered by game state — show different lines based on flags and quest progress |
| **crafting** | recipes at specific stations with ingredient lists, unlocked by completing quests or meeting trigger conditions |
| **story events** | bridge into the game's native cutscene system — define event sequences triggered by game state |
| **triggers** | unified boolean condition system (`allOf`, `anyOf`, `flagSet`, `questComplete`) that gates every feature |
| **cross-mod ipc** | versioned pub/sub channels for mods to communicate typed messages with compatibility warnings |

All content is defined through JSON files validated against schemas at load time. The engine emits structured diagnostic codes for any issues found during validation.

<br />

## dependencies

### runtime

| dependency | required | bundled | compatible |
|:-----------|:---------|:--------|:-----------|
| ![MOMI](https://img.shields.io/badge/MOMI-latest-FFD6E0?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![Aurie](https://img.shields.io/badge/Aurie-latest-E2CBFF?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![YYToolkit](https://img.shields.io/badge/YYToolkit-latest-A4C2F4?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

### engine dll — c++ dependencies

| package | version | compatible |
|:--------|:--------|:-----------|
| ![CMake](https://img.shields.io/badge/CMake-≥3.20-FFD6E0?style=flat-square&labelColor=2D2D2D) | `3.20+` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-v3.11.3-E2CBFF?style=flat-square&labelColor=2D2D2D) | `3.11.3` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![json-schema-validator](https://img.shields.io/badge/json--schema--validator-v2.3.0-A4C2F4?style=flat-square&labelColor=2D2D2D) | `2.3.0` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-D5A6BD?style=flat-square&labelColor=2D2D2D) | `C++20` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

### tui — rust dependencies

| crate | version | compatible |
|:------|:--------|:-----------|
| ![ratatui](https://img.shields.io/badge/ratatui-0.29.0-FFD6E0?style=flat-square&labelColor=2D2D2D) | `0.29.0` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![crossterm](https://img.shields.io/badge/crossterm-0.28.1-E2CBFF?style=flat-square&labelColor=2D2D2D) | `0.28.1` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![serde](https://img.shields.io/badge/serde-1.0.228-A4C2F4?style=flat-square&labelColor=2D2D2D) | `1.0.228` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![serde_json](https://img.shields.io/badge/serde__json-1.0.149-D5A6BD?style=flat-square&labelColor=2D2D2D) | `1.0.149` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![jsonschema](https://img.shields.io/badge/jsonschema-0.26.2-B6D7A8?style=flat-square&labelColor=2D2D2D) | `0.26.2` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![windows](https://img.shields.io/badge/windows-0.58.0-B4A7D6?style=flat-square&labelColor=2D2D2D) | `0.58.0` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

<br />

## architecture

Seven layers, each with a single responsibility:

| | layer | what it does |
|:-:|:------|:-------------|
| `A` | **bootstrap** | version checks, `.efl` manifest discovery, capability resolution, crash boundary |
| `B` | **engine bridge** | YYTK/Aurie hooks, room tracking, GML routine lookup, instance walking |
| `C` | **core services** | save, events, triggers, config, logging, compatibility |
| `D` | **feature registries** | areas, warps, resources, crafting, NPCs, quests, dialogue, story |
| `E` | **content model** | declarative JSON definitions for all content types |
| `F` | **ipc** | cross-mod communication channels (versioned, declared in manifest) |
| `G` | **tooling** | schema validation, debug console, TUI monitor |

<br />

## project structure

```
efl/
├── engine/          c++ aurie/yytk dll (cmake, msvc)
│   ├── include/     public headers — layers a through f
│   │   └── efl/
│   │       ├── core/         bootstrap, services, manifest, diagnostics
│   │       ├── bridge/       hook registry, room tracker, routine invoker
│   │       ├── registries/   areas, warps, resources, npcs, quests, crafting
│   │       └── ipc/          pipe writer, channel broker
│   ├── src/         implementation (mirrors include/)
│   ├── vendor/      aurie + yytk sdks
│   └── tests/       google test suite (83 tests)
├── tui/             rust tui loader and monitor (cargo)
│   └── src/
│       ├── phases/      boot · diagnostics · monitor
│       ├── widgets/     panel · cards · bars · lists
│       ├── diagnostics/ coded diagnostic system
│       ├── ipc.rs       named pipe reader
│       └── demo.rs      mock data for visual testing
├── schemas/         json schema files (draft 2020-12)
└── docs/
```

<br />

## content packs

Mods declare their content through `.efl` manifests and organized asset folders:

```
my-mod/
├── manifest.efl          mod metadata + capability declarations
├── areas/                custom area definitions
├── warps/                warp point definitions
├── resources/            resource node definitions
├── quests/               quest chains + objectives
├── triggers/             unlock conditions (flags, quest completion)
├── npcs/                 npc definitions
├── dialogue/             dialogue sets with conditional entries
├── recipes/              crafting recipes
├── events/               story/cutscene event definitions
└── sprites/              sprite sheets + assets
```

All definitions are validated against the JSON schemas in `schemas/` at load time. Invalid content emits structured diagnostic codes without crashing the game.

<br />

## tui

Display-only terminal interface. Three phases:

**`BOOT`** — animated startup sequence as the engine initializes subsystems

**`DIAGNOSTICS`** — structured validation report with per-subsystem status cards and coded errors (`MANIFEST-E001`, `HOOK-W003`, etc.)

**`MONITOR`** — live dashboard showing active hooks, recent events, save operations, and loaded mods

### demo mode

Run the TUI with mock data to preview all three phases without the game running:

```bash
cd tui && cargo run -- --demo
```

Press `Ctrl+C` to exit at any time.

### connecting to the engine

The engine DLL writes JSON Lines to a named pipe (`\\.\pipe\efl-{pid}`). The TUI connects and reads passively:

```bash
cd tui && cargo run -- --pipe \\.\pipe\efl-12345
```

<br />

## building

### engine dll

```bash
cd engine
cmake -B build
cmake --build build
```

### tui

```bash
cd tui
cargo build --release
```

### running tests

```bash
cd engine && ctest --test-dir build --output-on-failure
```

<br />

## diagnostic codes

Structured error reporting with category-severity codes:

| severity | prefix | meaning |
|:---------|:-------|:--------|
| ![error](https://img.shields.io/badge/E-error-FFB3BA?style=flat-square&labelColor=2D2D2D) | `E` | fatal to subsystem — prevents operation |
| ![warning](https://img.shields.io/badge/W-warning-FFFFBA?style=flat-square&labelColor=2D2D2D) | `W` | degraded but functional |
| ![hazard](https://img.shields.io/badge/H-hazard-BAE1FF?style=flat-square&labelColor=2D2D2D) | `H` | potential future problem |

Categories: `BOOT` · `MANIFEST` · `HOOK` · `AREA` · `WARP` · `RESOURCE` · `NPC` · `QUEST` · `TRIGGER` · `STORY` · `SAVE` · `IPC`

<br />

## roadmap

| phase | milestone | status |
|:-----:|:----------|:-------|
| 0 | bootstrap, manifest parser, logging | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 1 | engine bridge (hooks, room tracker, routines) | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 2 | core services (events, saves, triggers) | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 3 | area / warp mvp | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 4 | resources mvp | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 5 | quests / unlocks mvp | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 6 | npc mvp (local npcs) | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 7 | crafting / story integration | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |
| 8 | world state, cross-mod ipc | ![done](https://img.shields.io/badge/-done-B6D7A8?style=flat-square) |

### v2 (planned)

| feature | status |
|:--------|:-------|
| script injection (custom NPC behaviors, player abilities) | ![planned](https://img.shields.io/badge/-planned-E2CBFF?style=flat-square) |
| world NPCs (global schedules, hearts, gifts) | ![planned](https://img.shields.io/badge/-planned-E2CBFF?style=flat-square) |
| native room creation | ![planned](https://img.shields.io/badge/-planned-E2CBFF?style=flat-square) |
| hot-reload for development | ![planned](https://img.shields.io/badge/-planned-E2CBFF?style=flat-square) |

<br />

---

<p align="center">
  <sub>efl — <em>by Yoru</em></sub>
</p>
