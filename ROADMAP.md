# ROADMAP

Official roadmap for EFL.

This file supersedes the older high-level roadmap summary in `README.md`.

## Current Position

EFL is currently in the `v2.0.0` era, with all V2 work complete and the project transitioning toward V2.5:

- All V2 runtime systems are implemented and tested: dungeon vote injection, `NativeRoomBackend`, WorldNpc schedules, resource lifecycle, StoryBridge cutscene and item-grant wiring, efpack loading, and hot reload.
- The DevKit has basic pack build, inspect, and validation surfaces but is not the full authoring environment yet. V2.5 promotes it to the official compiler and validator pipeline.

The roadmap is now split into four release bands:

- `V2` = runtime-complete expansion framework
- `V2.5` = full DevKit / compiler / validation environment
- `V3` = remaining Fields of Mistria content-system coverage
- `V4` = Creation Kit era

## V2 — Runtime-Complete Expansion Framework

Goal: ship EFL as a complete runtime expansion framework for Fields of Mistria, not just a promising foundation.

This means EFL can load and run expansion-scale content packs without major fallback behavior, placeholder diagnostics, or "pending native bridge" gaps in the core runtime.

### V2 targets

- Finish dungeon vote injection for resource spawning in the mines.
- Finish native room creation so `NativeRoomBackend` becomes real instead of delegating to hijacked rooms.
- Finish world NPC runtime support:
  - real FoM time hook
  - schedule-driven spawning and movement
  - area/anchor transitions during the day
  - hearts and gift flow tied into live runtime behavior
- Finish the resource runtime lifecycle:
  - dungeon vote placement
  - grid-native spawn path
  - node interaction hooks
  - despawn / harvest / respawn behavior
- Finish StoryBridge native integration:
  - real dialogue open path
  - real native story / cutscene bridge
  - real item-grant bridge
- Keep `.efpack` loading as a first-class runtime path and remove any remaining "proof of concept" rough edges around packaged packs.
- Keep hot reload as a development runtime feature for content iteration.
- Bring shipped diagnostics, docs, and test fixtures back in line with actual runtime behavior before calling V2 complete.

### V2 non-goals

- The full DevKit authoring environment
- The Creation Kit
- "Everything in FoM"

## V2.5 — Full DevKit

Goal: make the DevKit the official EFL authoring, compile, inspect, validate, and runtime-debug environment.

This release is about toolchain authority. EFL should not just run content packs; it should provide the proper workflow for building them.

### V2.5 targets

- Make the DevKit the official `.efpack` compiler pipeline:
  - source workspace in one location
  - compiled output in another
  - versioned artifacts
  - checksums / integrity metadata
- Expand pack tooling beyond basic packing:
  - inspect `.efpack`
  - edit and rebuild `.efpack`
  - verify artifact metadata
- Implement the real validator direction discussed in planning:
  - environment compatibility checks
  - manifest / schema / semantic checks
  - cross-pack reference validation
  - reachability checks across the assembled content graph
  - IPC schema compatibility checks
  - hook/runtime validation using the same runtime model EFL actually uses
- Make the DevKit runtime-aware:
  - live diagnostics from the engine pipe
  - loaded EFPacks view
  - loaded MOMI mods view
  - clear relationship display between packs and the MOMI mods they depend on
  - identifier color mapping shared across related entries
- Add MOMI-awareness as an official compatibility feature:
  - read active MOMI mods
  - expose them to validation
  - allow compatibility-aware pack authoring
- Introduce `.efdat` as the compatibility artifact layer between MOMI mods and EFL packs:
  - standalone distributable compatibility artifact
  - zero-content relationship shims
  - support for required / optional / conflicting relationships
  - support for third-party compatibility shims
  - compiled and checksummed the same way as `.efpack`
- Support DevKit-driven reload workflows:
  - rebuild and reload for rapid iteration
  - keep the diagnostics/runtime monitor integrated with the authoring flow
- Implement runtime asset injection as a first-class `.efpack` feature:
  - `.efpack` files may include an `assets/` bundle (sprites, sounds, etc.)
  - EFL's AssetService injects bundled assets at boot via YYTK, the same way mods like Deep Dungeon inject assets directly into the running GameMaker runtime
  - Packs become fully self-contained: content, behavior, and visuals ship together
  - DevKit compiler pipeline handles asset packing, stripping, and inject-ready formatting
  - Manifest declares asset IDs so the validator and other packs can reference them

### V2.5 non-goals

- The Creation Kit map/editor workflow

That work belongs to `V4`.

## V3 — Remaining Fields of Mistria Runtime Systems

Goal: expand EFL coverage beyond its current "areas / NPCs / quests / resources / dialogue / crafting / story" core and into the rest of the Fields of Mistria gameplay stack.

V3 is not "become MOMI."
EFL should continue to own runtime relationships, behavior, conditions, and live system interaction, while still referencing raw assets and base-game data by ID rather than trying to own prelaunch asset delivery.

### V3 targets

- Combat runtime support:
  - weapons
  - armor / equipment slots
  - rings / related combat equipment
  - monster interaction and combat-facing content hooks
- Infusions and effect systems:
  - weapon infusions
  - armor infusions
  - runtime-driven effect application
- Buff / status / consumable systems:
  - food and drink buffs
  - restorative / speed / mana / revival style effect handling
  - temporary runtime modifiers and stateful effects
- Magic and mana systems:
  - spell unlock relationships
  - mana-linked runtime behavior
  - spell-gated content and interactions
- Social progression systems:
  - friendship gates
  - romance / dating / marriage hooks where appropriate
  - social-state-driven content unlocks
- World progression systems:
  - town rank / renown integration
  - museum and donation-linked runtime content relationships
  - crown request / progression-linked unlock logic
- Calendar and world-event systems:
  - festivals
  - market / rotating world events
  - season / weather / day-cycle-sensitive content behavior
- Shop and economy-facing runtime systems:
  - rotating stock relationships
  - shop condition gates
  - runtime unlocks and compatibility overlays for shops
- Ranching / creature extensions:
  - animal-related runtime relationships
  - breeding-linked content hooks
  - pet / creature-facing extension points
- Remaining content families that require runtime truth to matter, rather than just static asset injection.

### V3 design rule

V3 must still obey the EFL line:

- if it requires the game to be running to be true, it can belong to EFL
- if it is just a static thing definition or prelaunch file delivery concern, it does not

## V4 — Creation Kit

Goal: the Creation Kit era.

No committed feature goals yet.

V4 is reserved for the game-aware visual content editing layer, once the runtime and DevKit/compiler pipeline are both mature enough to support it properly.

## Semver Status Checklists

Status below is based on the current repo state.

### Key

| Mark | Meaning |
|:-----|:--------|
| `X` | Done |
| `↔` | In progress |
| `▼` | Deferred |

### V2 Checklist

| Mark | Action |
|:-----|:-------|
| `X` | Finish dungeon vote injection for mine resource spawning. **Implemented:** hooks `gml_Script_load_dungeon` post-call; walks `self.biomes[N].votes.pool` at runtime and pushes `{object, votes}` entries via `array_push`. Biome indices confirmed from `__fiddle__.json`. Original probe target (`register_node@Anchor@Anchor`) was superseded — vote injection does not require struct layout knowledge. |
| `X` | Finish native room creation so `NativeRoomBackend` is real. **Implemented:** two-phase lifecycle via `room_add()`/`room_goto()` with deferred population on room-tracker callback. `room_delete()` on departure. `supportsNativeRooms()` returns true. |
| `X` | Finish world NPC runtime support with time, movement, and live gift/hearts behavior. Time hook resolved (`unified_time`). Schedule dispatch is Model B — EFL drives WorldNpc positions via teleport (despawn/respawn on schedule boundary crossing). Hearts and gift flow backed by SaveService. |
| `X` | Finish the resource runtime lifecycle: placement, interaction, despawn, harvest, respawn, and seasonal respawn. |
| `X` | Finish StoryBridge native integration. Cutscene eligibility wired (`check_cutscene_eligible`). Item grant wired (`give_item@Ari@Ari`). Quest start/advance wired. Dialogue open path is a design boundary — Mist/MOMI owns dialogue content; EFL owns when cutscenes play. |
| `X` | Harden `.efpack` loading as a fully first-class runtime path. |
| `X` | Keep hot reload available as a runtime development feature. |
| `X` | Bring diagnostics, docs, and test fixtures back in line with shipped runtime behavior. |

### Probe Queue

| Priority | Probe Target | Status | Unblocks |
|:---------|:-------------|:-------|:---------|
| 1 | FoM time/clock | **Resolved** — `unified_time@Calendar@Calendar` returns `int64` total seconds; `season()` returns 0–3 | WorldNpc schedule teleport, seasonal respawn (both now implemented) |
| 2 | `APPLY_FIELDS_ONTO_STRUCT@GridPrototypes` struct | **Superseded for dungeon votes.** Dungeon vote injection solved via `load_dungeon` hook — struct layout not required. Probe still open for grid-native node spawn via `attempt_to_write_object_node` (RESOURCE-H003). Resources spawn via `objectName`/`instance_create_layer` interim path until probe completes. | Grid-native spawn (RESOURCE-H003) |
| 3 | NPC schedule dispatch | **Resolved (Model B)** — `on_new_day@Ari@Ari` fires argc=0; schedule movement is driven internally by FoM NPC objects with no injectable hook. EFL must drive WorldNpc positions itself. | WorldNpc movement design |
| 4 | FoM item grant script | **Resolved** — `give_item@Ari@Ari`, argc=2 form: `(itemId: int, qty: real)`. itemId = index into t2_input.json items array. | Item grant wired in StoryBridge and QuestRegistry |
| 5 | GML `room_add()` builtin | **Closed** — `room_add()` returns valid index; `room_set_width/height/persistent` all return status 0 (v7 probe 2026-04-06). `NativeRoomBackend` implemented. | `NativeRoomBackend` ✓ |

### V2.5 Checklist

| Mark | Action |
|:-----|:-------|
| `X` | Make the DevKit the official `.efpack` compiler pipeline. **Implemented:** versioned artifact output (`<output_dir>/<modId>/<version>/`), `build-meta.json` sidecar, `build-history.json` registry with upsert-by-version, per-file checksums, content inventory, `packerVersion` from Cargo.toml, asset declaration validation (PNG/OGG/WAV magic), dev-artifact stripping (`.DS_Store`, `Thumbs.db`, etc.). Headless `efl-pack` CLI shares the GUI build path; CI smoke workflow drives the example pack through it on every push. |
| `X` | Expand pack tooling beyond basic packing into inspect / edit / rebuild flows. **Implemented:** inspect, rebuild, watch clear-diff summaries, advanced input modes (project/manifest/config/CI env), migration wizard, and edit-in-place workflow (`Edit & Repack...` extracts an archive into a sibling workspace, repacks via the standard build path, regenerates `pack-meta.json`). |
| `X` | Implement the full runtime-aware validator direction. **Implemented:** profile-based validator (`recommended` default, `strict`, `legacy`) covers structural checks, workspace cross-pack dependency/conflict checks, trigger reachability checks, IPC contract checks, script-hook model checks, and content-graph parity (ID uniqueness + cross-references for areas, warps, NPCs, quests, dialogue, story events, resources, recipes) with stable diagnostic IDs/categories. Cross-references that may legitimately target base-game content surface as `-H0xx` hazards (warning under `recommended`, error under `strict`). Engine emits a `capabilities.snapshot` IPC event on boot complete (re-broadcast via the inbound `caps` command); the DevKit caches it on `EngineState` and feeds it into `validate_manifest_with_capabilities`, so handler/feature whitelists adapt to the connected engine instead of the DevKit's embedded defaults. |
| `X` | Make the DevKit runtime-aware with diagnostics, loaded packs, loaded MOMI mods, and relationship views. **Implemented:** runtime sequence telemetry, loaded EFPacks panel, loaded MOMI mods panel, pack⇄MOMI relationship view, and deterministic identifier color mapping. |
| `X` | Add MOMI-awareness as a formal compatibility feature. **Implemented:** DevKit MOMI monitor now merges runtime MOMI status with a silent mods-folder index to classify `Active` / `PresentInactive` / `Missing`, overlays relationship intent badges (`Needed` / `Compatible` / `Conflict`) from `.efdat`, provides a row-click relationship popout tree, and surfaces relationship-risk counters. Validator now enforces minimal MOMI rules for `.efdat`: `requires` MOMI targets must be present (`DAT-E020`), active/present conflicts are blocked (`DAT-E021`), optional MOMI targets stay advisory (`DAT-W020`), and missing inventory source is explicitly reported (`DAT-W021`). |
| `X` | Introduce `.efdat` as the compatibility artifact layer between MOMI mods and EFL packs. **Implemented:** `.efdat` schema in `schemas/efl-dat.schema.json`, DevKit build path (`Pack -> .efdat`), inspect support, validation flow for `manifest.efdat`, and app-level artifact import/open handling for `.efpack` + `.efdat`. |
| `X` | Support DevKit-driven rebuild / reload workflows for fast iteration. **Implemented:** rebuild/watch flows plus an inbound command pipe (`\\.\pipe\efl-<pid>-cmd`). DevKit ships a manual `Reload engine` button and an `Auto-reload after build` toggle; engine emits `command_pipe.ready`, `reload.requested`, and `reload.complete` events. See `docs/internals/ipc-protocol.md`. |
| `X` | Ship curated example workspaces. **Implemented:** `examples/hello_adventurer` (`.efpack`) covers areas, warps, NPCs, triggers, dialogue, story, and the V3 calendar pilot. `examples/hello_compat` (`.efdat`) demonstrates `requires` / `optional` / `conflicts` relationships across `.efpack` and MOMI targets. CI smoke (`.github/workflows/smoke.yml`) and the release workflow build both via `efl-pack` on every push and bundle them into the released DevKit zip. Walkthroughs at `docs/authoring/examples.md`. |
| `X` | Polish the Diagnostics tab for triage. **Implemented:** severity chip toggles (errors / warnings / hazards), live category dropdown sourced from the active diagnostic stream, "Reset filters" affordance, and per-entry quick-jump (`Open` / `Reveal`) when a diagnostic carries a resolvable `source.file`. Stats badges keep showing the unfiltered totals so filters never hide how many issues exist. See `docs/devkit/devkit-guide.md`. |

### V3 Checklist

| Mark | Action |
|:-----|:-------|
| `▼` | Add combat runtime support for weapons, armor, rings, and combat hooks. |
| `▼` | Add infusions and effect-system runtime support. |
| `▼` | Add buff / status / consumable runtime systems. |
| `▼` | Add magic and mana-linked runtime systems. |
| `▼` | Add social progression systems such as friendship, romance, and marriage-facing hooks. |
| `▼` | Add world progression systems such as town rank, museum, donation, and crown-request-linked content logic. |
| `↔` | Add calendar and world-event systems such as festivals, markets, seasons, weather, and day-cycle-sensitive behavior. **V3 pilot landed:** minimal `CalendarRegistry` (`engine/src/registries/calendar_registry.cpp`) loads `calendar/*.json` declarations under the `calendar` feature tag, fires from EFL's `new_day` hook, evaluates an optional trigger condition, and dispatches an `onActivate` story event through `StoryBridge`. Schema at `schemas/event-calendar.schema.json`. DevKit recognises the feature, scaffolds the folder, counts events in build summaries, and validates IDs / season / `dayOfSeason` / `lifecycle` / `onActivate` cross-refs (codes `CALENDAR-E001`/`E010..E012`/`E030..E031`/`H020`). Festivals, markets, weather, and per-pack persistence are still future work. |
| `▼` | Add shop and economy-facing runtime systems. |
| `▼` | Add ranching / creature runtime extensions. |
| `▼` | Add any remaining FoM content families that require runtime truth rather than static asset injection. |

### V4 Checklist

| Mark | Action |
|:-----|:-------|
| `▼` | Hold V4 for the Creation Kit era; no committed feature goals yet. |
