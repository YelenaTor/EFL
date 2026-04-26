<p align="center">
  <br />
  <strong><code>E F L</code></strong>
  <br />
  <em>Expansion Framework Library</em>
  <br />
  <sub>by Yoru — runtime expansion framework for Fields of Mistria</sub>
  <br /><br />
  <a href="#"><img src="https://img.shields.io/badge/EFL-v1.1.0-B4A7D6?style=flat-square&labelColor=2D2D2D" alt="EFL Version" /></a>
  <a href="#"><img src="https://img.shields.io/badge/status-stable%201.x-B6D7A8?style=flat-square&labelColor=2D2D2D" alt="Status" /></a>
  <a href="#"><img src="https://img.shields.io/badge/platform-windows-A4C2F4?style=flat-square&labelColor=2D2D2D" alt="Platform" /></a>
  <a href="#"><img src="https://img.shields.io/badge/language-C%2B%2B%2020%20%7C%20Rust-B6D7A8?style=flat-square&labelColor=2D2D2D" alt="Languages" /></a>
</p>

---

<br />

> *A runtime expansion framework that sits above MOMI and provides a stable,*
> *declarative authoring surface for expansion-scale content mods.*

<br />

## What Is EFL

EFL is a **runtime expansion framework** for [Fields of Mistria](https://store.steampowered.com/app/2142790/Fields_of_Mistria/). It provides mod authors with a high-level, declarative API for building expansion-scale content — custom areas, NPCs, quests, crafting recipes, dialogue, and more — without touching raw game internals.

```
┌──────────────────────────────────────────────┐
│      content packs (.efpack / loose)         │
│       manifests · json · runtime data        │
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

## What You Can Build

EFL currently supports declarative runtime content packs — no C++ or GML required for the core content surface:

| Feature | What It Does |
|:--------|:-------------|
| **areas** | build new locations using `hijacked` (reuse host room) or `native` (runtime-created room) backends |
| **warps** | define transition points between areas, gated by unlock triggers with customizable failure text |
| **resources** | register forageables, breakables, and harvestables with yield tables, respawn policies, seasonal availability, and tool requirements |
| **quests** | multi-stage quest chains with typed objectives (collect, talk to), stage completion actions, and item rewards |
| **npcs** | local NPCs that spawn in EFL areas with dialogue sets, portraits, spawn anchors, and trigger-gated visibility |
| **dialogue** | conditional dialogue entries filtered by game state — show different lines based on flags and quest progress |
| **crafting** | recipes at specific stations with ingredient lists, unlocked by completing quests or meeting trigger conditions |
| **story events** | bridge into the game's native cutscene system — define event sequences triggered by game state |
| **triggers** | unified boolean condition system (`allOf`, `anyOf`, `flagSet`, `questComplete`) that gates every feature |
| **cross-mod IPC** | versioned pub/sub channels for mods to communicate typed messages with compatibility warnings |

All content is defined through JSON files validated against schemas at load time. The engine emits structured diagnostic codes for any issues found during validation.

<br />

## Dependencies

### Runtime

| Dependency | Required | Bundled | Compatible |
|:-----------|:---------|:--------|:-----------|
| ![MOMI](https://img.shields.io/badge/MOMI-latest-FFD6E0?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![Aurie](https://img.shields.io/badge/Aurie-latest-E2CBFF?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![YYToolkit](https://img.shields.io/badge/YYToolkit-latest-A4C2F4?style=flat-square&labelColor=2D2D2D) | yes | no | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

### Engine DLL - C++ Dependencies

| Package | Version | Compatible |
|:--------|:--------|:-----------|
| ![CMake](https://img.shields.io/badge/CMake-≥3.20-FFD6E0?style=flat-square&labelColor=2D2D2D) | `3.20+` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-v3.11.3-E2CBFF?style=flat-square&labelColor=2D2D2D) | `3.11.3` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![json-schema-validator](https://img.shields.io/badge/json--schema--validator-v2.3.0-A4C2F4?style=flat-square&labelColor=2D2D2D) | `2.3.0` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-D5A6BD?style=flat-square&labelColor=2D2D2D) | `C++20` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

### DevKit - Rust Dependencies

| Crate | Version | Compatible |
|:------|:--------|:-----------|
| ![egui](https://img.shields.io/badge/egui-0.29-FFD6E0?style=flat-square&labelColor=2D2D2D) | `0.29` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![eframe](https://img.shields.io/badge/eframe-0.29-E2CBFF?style=flat-square&labelColor=2D2D2D) | `0.29` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![notify](https://img.shields.io/badge/notify-6-A4C2F4?style=flat-square&labelColor=2D2D2D) | `6` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![serde](https://img.shields.io/badge/serde-1.x-D5A6BD?style=flat-square&labelColor=2D2D2D) | `1.x` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![zip](https://img.shields.io/badge/zip-2-B4A7D6?style=flat-square&labelColor=2D2D2D) | `2` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |
| ![windows](https://img.shields.io/badge/windows-0.58.0-B6D7A8?style=flat-square&labelColor=2D2D2D) | `0.58.0` | ![yes](https://img.shields.io/badge/-✓-B6D7A8?style=flat-square) |

<br />

## Architecture

Seven layers, each with a single responsibility:

| | Layer | What It Does |
|:-:|:------|:-------------|
| `A` | **bootstrap** | version checks, `.efl` manifest discovery, capability resolution, crash boundary |
| `B` | **engine bridge** | YYTK/Aurie hooks, room tracking, GML routine lookup, instance walking |
| `C` | **core services** | save, events, triggers, config, logging, compatibility |
| `D` | **feature registries** | areas, warps, resources, crafting, NPCs, quests, dialogue, story |
| `E` | **content model** | declarative JSON definitions for all content types |
| `F` | **IPC** | cross-mod communication channels (versioned, declared in manifest) |
| `G` | **tooling** | schema validation, DevKit, runtime monitor |

<br />

## Project Structure

```
EFL/
├── engine/          C++ Aurie/YYTK DLL (CMake, MSVC)
│   ├── include/     public headers — layers a through f
│   │   └── efl/
│   │       ├── core/         bootstrap, services, manifest, diagnostics
│   │       ├── bridge/       hook registry, room tracker, routine invoker
│   │       ├── registries/   areas, warps, resources, npcs, quests, crafting
│   │       └── ipc/          pipe writer, channel broker
│   ├── src/         implementation (mirrors include/)
│   ├── vendor/      aurie + yytk sdks
│   └── tests/       GoogleTest suite
├── devkit/          Rust egui DevKit desktop app (Cargo)
│   └── src/
│       ├── tabs/        packs · diagnostics · Creation Kit
│       ├── pack/        packer · inspector · validator · watcher
│       ├── pipe/        named pipe reader / discovery
│       └── app.rs       main DevKit shell
├── schemas/         JSON schema files (draft 2020-12)
└── docs/
```

<br />

## Content Packs

Mods can be worked on as loose folders during development and compiled into `.efpack` artifacts for distribution and runtime loading.

Loose development layout:

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
└── assets/               runtime-injected assets (e.g. sprites/, sounds/)
```

At runtime, EFL can also load packaged `.efpack` content by extracting the archive and reading the embedded `manifest.efl`.

All definitions are validated against the JSON schemas in `schemas/` at load time. Invalid content emits structured diagnostic codes without crashing the game.

<br />

## DevKit

The DevKit grew out of the old TUI monitor and is now the current desktop authoring tool.

Current DevKit capabilities:

- guided **New Pack Wizard** (author -> mod name -> modId -> features -> MOMI -> review)
- pack loose content into `.efpack`
- inspect `.efpack`
- validate manifests and compatibility artifacts (`.efdat`)
- watch projects and auto-repack
- display live runtime diagnostics from the engine pipe
- scaffold `{mod-id}.MOMI.efdat` starter files when MOMI integration is selected

The runtime monitor path still exists through the named pipe (`\\.\pipe\efl-{pid}`), but it now lives inside the desktop DevKit rather than as the project's primary end-user UI.

### Current Direction

The full DevKit / compiler / validator direction is tracked in `ROADMAP.md` (including V2.5 closeout and post-V2.5 work).

<br />

## Building

### Engine DLL

```bash
cd engine
cmake -B build
cmake --build build
```

### DevKit

```bash
cd devkit
cargo build --release
```

### Running Tests

```bash
cd engine && ctest --test-dir build --output-on-failure
cd devkit && cargo test
```

<br />

## Diagnostic Codes

Structured error reporting with category-severity codes:

| Severity | Prefix | Meaning |
|:---------|:-------|:--------|
| ![error](https://img.shields.io/badge/E-error-FFB3BA?style=flat-square&labelColor=2D2D2D) | `E` | fatal to subsystem — prevents operation |
| ![warning](https://img.shields.io/badge/W-warning-FFFFBA?style=flat-square&labelColor=2D2D2D) | `W` | degraded but functional |
| ![hazard](https://img.shields.io/badge/H-hazard-BAE1FF?style=flat-square&labelColor=2D2D2D) | `H` | potential future problem |

Categories: `BOOT` · `MANIFEST` · `HOOK` · `AREA` · `WARP` · `RESOURCE` · `NPC` · `QUEST` · `TRIGGER` · `STORY` · `SAVE` · `IPC`

<br />

## Roadmap

The full roadmap now lives in [`ROADMAP.md`](ROADMAP.md).

It tracks:

- `V2` runtime completion
- `V2.5` full DevKit
- `V3` further Fields of Mistria runtime systems
- `V4` Creation Kit era

<br />

---

<p align="center">
  <sub>EFL — <em>by Yoru</em></sub>
</p>
