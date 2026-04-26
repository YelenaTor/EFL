# Installing EFL Packs

## What Are EFL Packs?

EFL Packs are content mods for Fields of Mistria built on the Expansion Framework Library. They add new areas, NPCs, quests, crafting recipes, dialogue, and more to the game — all through declarative JSON files, no programming required. Each pack is a folder containing a `.efl` manifest and organized content directories.

## Prerequisites

Before installing any EFL Pack, you need:

1. **Fields of Mistria** — the base game, installed and launched at least once
2. **MOMI** (Mods of Mistria Installer) — the community mod manager
3. **Aurie** — native module loader, installed via MOMI
4. **YYToolkit** — GameMaker runtime hook system, installed via MOMI

## Installing EFL

1. Download `EFL.dll` from the [latest release](https://github.com/YoruAkio/EFL/releases)
2. Place it in your game's Aurie modules directory:
   ```
   <game>/mods/aurie/EFL.dll
   ```
3. Launch the game once to verify EFL initializes — check the log file at `<game>/EFL/logs/`

## Installing an EFL Pack

1. Download the EFL Pack (usually a `.zip` file)
2. Extract it into the EFL content directory:
   ```
   <game>/mods/efl/<pack-folder>/
   ```
3. The pack folder should contain a `manifest.efl` file at its root:
   ```
   <game>/mods/efl/my-cool-mod/
   ├── manifest.efl
   ├── areas/
   ├── npcs/
   └── ...
   ```
4. Launch the game — EFL discovers and loads the pack automatically

## Verifying Installation

Check the EFL log file at `<game>/EFL/logs/` after launching. A successful load looks like:

```
[BOOT] EFL v2.0.0 initializing
[BOOT] Loaded manifest: com.author.my-mod v1.0.0
[BOOT] Bootstrap complete — 1 manifest(s) loaded
```

## Troubleshooting

- **EFL log file doesn't exist** — EFL.dll is not loading. Verify it's in `mods/aurie/` and that Aurie and YYToolkit are installed correctly.
- **"Content directory not found"** — The `mods/efl/` directory doesn't exist. Create it manually.
- **"Failed to parse manifest"** — The pack's `manifest.efl` has a JSON syntax error. Open it in a text editor and check for missing commas or brackets.
- **"requires EFL v..."** — The pack needs a newer version of EFL than you have installed. Update EFL.dll.
- **Game crashes on startup** — Remove the pack folder and report the issue to the pack author. EFL is designed to emit diagnostics rather than crash, but edge cases exist in pre-release.
