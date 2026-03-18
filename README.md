<p align="center">
  <br />
  <strong><code>E F L</code></strong>
  <br />
  <em>Expansion Framework Library</em>
  <br />
  <sub>by yoru — runtime expansion framework for fields of mistria</sub>
  <br /><br />
  <a href="#"><img src="https://img.shields.io/badge/EFL-v0.2.0-B4A7D6?style=flat-square&labelColor=2D2D2D" alt="EFL Version" /></a>
  <a href="#"><img src="https://img.shields.io/badge/status-in%20development-D5A6BD?style=flat-square&labelColor=2D2D2D" alt="Status" /></a>
  <a href="#"><img src="https://img.shields.io/badge/platform-windows-A4C2F4?style=flat-square&labelColor=2D2D2D" alt="Platform" /></a>
  <a href="#"><img src="https://img.shields.io/badge/language-C%2B%2B%2020%20%7C%20Rust-B6D7A8?style=flat-square&labelColor=2D2D2D" alt="Languages" /></a>
</p>

---

<br />

> *a runtime expansion framework that sits above MOMI and provides a stable,*
> *declarative authoring surface for expansion-scale content mods.*

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

## what is efl

EFL is a **runtime expansion framework** for [Fields of Mistria](https://store.steampowered.com/app/2142790/Fields_of_Mistria/). It provides mod authors with a high-level, declarative API for building expansion-scale content — custom areas, NPCs, quests, crafting recipes, dialogue, and more — without touching raw game internals.

```
┌──────────────────────────────────────────────┐
│              content packs (.efl)             │
│         json manifests + asset folders        │
├──────────────────────────────────────────────┤
│                     efl                       │
│     bootstrap · services · registries         │
├──────────────────────────────────────────────┤
│            aurie + yytoolkit                  │
│          native hooks + gml bridge            │
├──────────────────────────────────────────────┤
│           fields of mistria (gm)              │
└──────────────────────────────────────────────┘
```

<br />

## architecture

Seven layers, each with a single responsibility:

| | layer | what it does |
|:-:|:------|:-------------|
| `A` | **bootstrap** | version checks, `.efl` manifest discovery, capability resolution |
| `B` | **engine bridge** | YYTK/Aurie hooks, room tracking, GML routine lookup |
| `C` | **core services** | save, events, triggers, config, logging, compatibility |
| `D` | **feature registries** | areas, warps, resources, crafting, NPCs, quests, dialogue, story |
| `E` | **content model** | declarative JSON definitions for all content types |
| `F` | **ipc** | cross-mod communication channels |
| `G` | **tooling** | schema validation, debug console, TUI monitor |

<br />

## project structure

```
efl/
├── engine/          c++ aurie/yytk dll (cmake, msvc)
│   ├── include/     public headers — layers a through f
│   ├── src/         implementation
│   ├── vendor/      aurie + yytk sdks (submodules)
│   └── tests/
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

> requires MSVC and vendored Aurie/YYTK SDKs (not yet bundled)

### tui

```bash
cd tui
cargo build --release
```

<br />

## content packs

Mods declare their content through `.efl` manifests and organized asset folders:

```
my-mod/
├── manifest.efl          mod metadata + capability declarations
├── areas/                custom area definitions
├── npcs/                 npc definitions + dialogue
├── quests/               quest chains + triggers
├── recipes/              crafting recipes
├── resources/            resource node definitions
├── warps/                warp point definitions
└── sprites/              sprite sheets + assets
```

All definitions are validated against the JSON schemas in `schemas/` at load time.

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
| 0 | bootstrap, manifest parser, logging | ![wip](https://img.shields.io/badge/-wip-E2CBFF?style=flat-square) |
| 1 | engine bridge (hooks, room tracker, routines) | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 2 | core services (events, saves, triggers) | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 3 | area / warp mvp | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 4 | resources mvp | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 5 | quests / unlocks mvp | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 6 | npc mvp (local npcs) | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 7 | crafting / story integration | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |
| 8 | world npcs, cross-area schedules, ipc | ![planned](https://img.shields.io/badge/-planned-D0D0D0?style=flat-square) |

<br />

---

<p align="center">
  <sub>efl — <em>by Yoru</em></sub>
</p>
