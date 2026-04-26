# DevKit Guide

EFL DevKit is the desktop GUI for pack authoring, archive inspection, validation, and live runtime diagnostics.

## What It Does

The DevKit combines content-pack tooling with a passive runtime monitor:

- **Packs** — scan project folders, validate manifests, build `.efpack` and `.efdat`, inspect artifacts, run migration wizard, and watch packs for auto-repack
- **Diagnostics** — show boot progress, structured diagnostics, runtime-sequence telemetry, hooks, events, save activity, loaded EFPacks, loaded MOMI mods, and compatibility relationships
- **Creation Kit** — reserved in the UI shell for future editing workflows

The runtime side is read-only. The engine writes JSON Lines to a named pipe, and the DevKit consumes that feed without mutating game state.

## Installation

The release zip includes `efl-devkit.exe` in the `EFL DevKit` folder. Launch it directly from there, or move it anywhere convenient on your system.

### Building from Source

```bash
cd devkit
cargo build --release
```

The binary is output to `devkit/target/release/efl-devkit.exe`.

## Launching

Start the application normally:

```bash
efl-devkit
```

If Fields of Mistria is already running with EFL loaded, the DevKit will automatically discover the latest active EFL named pipe:

```text
\\.\pipe\efl-{pid}
```

If no engine is running, the Diagnostics tab stays idle and the pack tooling continues to work.

### Demo Mode

Preview the boot, diagnostics, and monitor surfaces with mock data:

```bash
efl-devkit --demo
```

## Runtime Views

### Boot

Shows engine-reported boot progress as initialization steps complete.

### Diagnostics

Displays structured issues emitted by the engine during validation and runtime.

The filter row above the log lets you triage at speed:

- **Severity chips** — toggle errors, warnings, and hazards on or off independently. The stats badges above keep showing the full counts so the filtered view never hides how many issues actually exist.
- **Category dropdown** — narrow the log to a single category (`MANIFEST`, `HOOK`, `AREA`, etc.). The list is built from whatever the engine has already reported, so categories appear and disappear as the run progresses.
- **Reset filters** — restore all severities and clear the category in one click.

Each diagnostic that carries a `source.file` exposes two quick-jump buttons:

- **Open** launches the file in your default app (handy for JSON files in your pack).
- **Reveal** highlights the file in your OS file manager.

Quick-jump resolves relative paths against the currently selected pack on the Packs tab; absolute paths are used as-is. If neither resolves to a real path on disk, the buttons stay hidden so they never lie.

See [Diagnostic Codes](diagnostic-codes.md) for the code format and categories.

### Monitor

Shows live runtime state from the engine pipe:

- **Runtime Sequence** — ordered telemetry view:
  - Boot -> Capability -> Hook health -> Registry -> Trigger -> Pack state
- **Active hooks** — registered and firing hooks
- **Recent events** — event, story, quest, and dialogue activity
- **Save operations** — namespaced save reads/writes
- **Loaded EFPacks** — current status and versions
- **MOMI Inventory** — merged runtime + filesystem view with state filters:
  - `Active` = reported active by runtime
  - `PresentInactive` = discovered in mods folder index but not active
  - `Missing` = referenced by compatibility relationships but not discovered
- **Relationship chips** — per-MOMI-mod badges:
  - `NEEDED` from `requires`
  - `COMPAT` from `optional`
  - `CONFLICT` from `conflicts`
- **MOMI popout graph** — click a MOMI mod row to open a tree-style relationship popout showing incoming EFPack/`.efdat` edges and derived risk summary.
- **Relationship risk summary** — hard conflict, missing-needed, and soft-missing-compatible counters. These are relationship-based checks, not runtime load-order execution traces.

## Migration Wizard

In the Packs tab, use **Migrate...** for pre-V2.5 pack upgrades:

1. **Analyze (dry-run)** to preview changes
2. **Apply Migration** to rewrite legacy manifest structures
3. DevKit always creates a full backup directory before applying changes

The wizard currently migrates common legacy manifest patterns:
- legacy `features` object -> string array
- legacy dependency string arrays -> object arrays
- `settings.strict` -> `settings.strictMode`
- deprecated `saveScope` removal

## Edit-in-place Workflow

If you only have a built `.efpack` or `.efdat` and want to patch it without going back to the source workspace:

1. Click **Edit & Repack...** in the action row.
2. Pick the artifact. DevKit creates a sibling `<artifact-stem>-edit/` folder, extracts every entry except `pack-meta.json`, and registers it as a regular pack folder.
3. Edit any file in the extracted workspace using your normal editor.
4. Hit **Pack -> .efpack** (or **Pack -> .efdat** if it was a `.efdat`) to rebuild. Checksums, `build-meta.json`, and `pack-meta.json` are regenerated.
5. Use **Discard workspace** when you're done. DevKit forgets the workspace; the extracted folder stays on disk for you to clean up or keep as a side-by-side diff.

The extract is sandboxed: archive entries that try to escape the workspace via `..` or absolute paths are rejected with `EDIT-E003`. Re-extracting into a non-empty folder also fails with `EDIT-E001` to avoid silently merging into an existing workspace.

## `.efdat` Compatibility Artifacts

DevKit supports `.efdat` as a first-class compatibility artifact flow:

- Build using **Pack -> .efdat**
- Inspect `.efdat` archives from **Inspect...**
- Validate `manifest.efdat` using the same Validate action

`.efdat` artifacts are intended for compatibility relationships (EFPack/MOMI requires/optional/conflicts), not content payloads.

## Exiting

Close the window normally. This does not affect the running game or the EFL engine.
