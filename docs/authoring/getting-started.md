# Your First EFL Pack

This quickstart is intentionally simple: set up DevKit, build your first pack, stay on the EFL-vs-MOMI boundary, and know what to check when something fails.

## Setup (Prerequisites)

- Fields of Mistria with EFL installed (see [Installing Content Packs](../player/installing-content-packs.md))
- EFL DevKit installed and configured with:
  - Projects folder (your source pack workspaces)
  - Output folder (compiled artifacts)
- Optional: VS Code + this repo `schemas/` folder for JSON autocomplete

## First Pack (Fast Path)

1. Open DevKit.
2. Go to **Packs**.
3. Click **+ New** and create a pack with:
   - `modId` in namespaced format (`com.author.pack`)
   - `features` only for systems you use
4. Click **Pack → .efpack**.
5. Click **Validate** and clear all errors.
6. If upgrading an older pack, click **Migrate...**:
   - run **Analyze (dry-run)**
   - run **Apply Migration** only after reviewing changes
   - DevKit creates a full backup before writing

## Step 1: Create Your Pack Folder

Create a new folder in the EFL content directory:

```
<game>/mods/efl/my-first-pack/
```

This folder name doesn't matter to EFL — only the `modId` inside the manifest does.

## Step 2: Write Your Manifest

Create `manifest.efl` in your pack folder:

```json
{
    "schemaVersion": 2,
    "modId": "com.yourname.my-first-pack",
    "name": "My First Pack",
    "version": "0.1.0",
    "eflVersion": "1.0.0",
    "features": ["areas", "warps"],
    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked"
    }
}
```

### Key Fields Explained

- **schemaVersion**: Always `2` for the current manifest format.
- **modId**: A unique identifier in reverse-domain notation (e.g., `com.yourname.modname`). This must be globally unique across all EFL Packs.
- **name**: Human-readable display name for your pack.
- **version**: Your pack's version, following [semver](https://semver.org/).
- **eflVersion**: The minimum EFL version your pack requires.
- **features**: A string array listing which EFL subsystems your pack uses. Only declare what you need. In strict mode, accessing an undeclared subsystem is an error; in non-strict mode, it's a warning.
- **settings.strictMode**: When `true`, EFL rejects attempts to use undeclared features. Recommended for development.
- **settings.areaBackend**: Use `"hijacked"` — this reuses existing game rooms as the basis for custom areas. (`"native"` is planned but not yet available.)

## Step 3: Define an Area

Create `areas/secret_garden.json`:

```json
{
    "id": "secret_garden",
    "displayName": "Secret Garden",
    "backend": "hijacked",
    "hostRoom": "rm_water_seal_0",
    "music": "",
    "entryAnchor": "door_south",
    "unlockTrigger": ""
}
```

The `hostRoom` is the real game room that EFL hijacks and repopulates. In v1, all custom areas work this way — you're reusing the geometry of an existing room while replacing its contents.

An empty `unlockTrigger` means the area is always accessible. See [Triggers and Conditions](triggers-and-conditions.md) for gating access.

## Step 4: Define a Warp

Create `warps/town_to_garden.json`:

```json
{
    "id": "town_to_garden",
    "sourceArea": "town",
    "sourceAnchor": "garden_entrance",
    "targetArea": "secret_garden",
    "targetAnchor": "door_south",
    "requireTrigger": "",
    "failureText": ""
}
```

This creates a transition point from the town to your new area. The `sourceAnchor` and `targetAnchor` define where in each room the player enters/exits.

## Step 5: Test It

1. Drop your pack folder into `<game>/mods/efl/`
2. Launch the game
3. Check `<game>/EFL/logs/` for successful loading:
   ```
   [BOOT] Loaded manifest: com.yourname.my-first-pack v0.1.0
   [BOOT] Registered area: secret_garden
   [BOOT] Registered warp: town_to_garden
   ```

## What's Next

Your pack folder should now look like:

```
my-first-pack/
├── manifest.efl
├── areas/
│   └── secret_garden.json
└── warps/
    └── town_to_garden.json
```

From here you can:

- Add NPCs to your area — see [Content Types](content-types.md)
- Gate access with triggers — see [Triggers and Conditions](triggers-and-conditions.md)
- Study the example pack — see [Examples](examples.md)

## EFL vs MOMI (Cheat Sheet)

- **EFL owns runtime behavior**: triggers, quests, events, live registry state, runtime asset injection.
- **MOMI owns pre-launch delivery**: install, file placement, localization text delivery, fiddle data merge.
- If you see a `localisation/` folder inside an EFL pack workspace, move that text delivery to MOMI.

## Troubleshooting (Top Checks)

1. **Pack won’t build**
   - ensure `manifest.efl` exists and is valid JSON
   - ensure output directory is configured
2. **Validation errors**
   - run `recommended` profile first
   - fix `MANIFEST-E*` issues before warnings
3. **Runtime loads but behavior missing**
   - open DevKit **Diagnostics**
   - check Runtime Sequence and Hook health
4. **Upgraded old pack behaves strangely**
   - re-run **Migrate...** dry-run
   - confirm backup path was created before apply
5. **Compatibility uncertainty**
   - check loaded EFPacks, loaded MOMI mods, and relationship view in Diagnostics

## Current Limitations

- **Hijacked backend only**: Custom areas reuse existing game rooms. True custom room creation (`native` backend) is planned but not yet available — use `"areaBackend": "hijacked"`.
- **Dungeon vote injection is stubbed**: Resource nodes with `dungeonVotes` register the hook but won't appear on dungeon floors yet (pending an internal struct probe). See `RESOURCE-W001`.
- **Crafting station filtering**: All trigger-unlocked EFL recipes currently inject into every crafting menu regardless of the `station` field.
- **Resource item grants require `itemId`**: Yield table entries must include a numeric `itemId` (the FoM item index) for harvested items to actually be granted to the player. Without `itemId`, the harvest is logged and emitted via IPC but no item is given.
- **No GML script injection**: Content hooks use the `"callback"` mode only. `mode: "inject"` is reserved for a future release and emits `HOOK-W002` if used.
