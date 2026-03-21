# Your First EFL Pack

This guide walks you through creating a minimal EFL Pack from scratch. By the end, you'll have a custom area with a warp point that you can walk into in-game.

## What You Need

- A text editor (VS Code recommended — it supports JSON Schema autocomplete)
- The `schemas/` folder from this repository (for editor autocomplete)
- Fields of Mistria with EFL installed (see [Installing Content Packs](../player/installing-content-packs.md))

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
    "schemaVersion": 1,
    "modId": "com.yourname.my-first-pack",
    "name": "My First Pack",
    "version": "0.1.0",
    "eflVersion": "1.0.0-pre.1",
    "dependencies": {
        "required": [],
        "optional": []
    },
    "features": {
        "areas": true,
        "warps": true,
        "resources": false,
        "crafting": false,
        "npcs": false,
        "quests": false,
        "triggers": false,
        "dialogue": false,
        "story": false,
        "ipcPublish": false,
        "ipcConsume": false,
        "migrations": false
    },
    "settings": {
        "strictMode": true,
        "areaBackend": "hijacked",
        "saveScope": "EFL/com.yourname.my-first-pack"
    }
}
```

### Key Fields Explained

- **schemaVersion**: Always `1` for now. This tracks manifest format changes.
- **modId**: A unique identifier in reverse-domain notation (e.g., `com.yourname.modname`). This must be globally unique across all EFL Packs.
- **name**: Human-readable display name for your pack.
- **version**: Your pack's version, following [semver](https://semver.org/).
- **eflVersion**: The minimum EFL version your pack requires.
- **features**: Boolean flags declaring which EFL subsystems your pack uses. In strict mode, accessing an undeclared subsystem is an error. In non-strict mode, it's a warning.
- **settings.strictMode**: When `true`, EFL rejects attempts to use undeclared features. Recommended for development.
- **settings.areaBackend**: Use `"hijacked"` in v1. This reuses existing game rooms as the basis for custom areas.
- **settings.saveScope**: The namespace prefix for your pack's save data. Convention: `EFL/<modId>`.

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

## V1 Limitations

- **Hijacked backend only**: Custom areas reuse existing game rooms. True custom room creation (`native` backend) is planned for v2.
- **Local NPCs only**: NPCs can only exist within EFL areas. World NPCs (global schedules, hearts, gifts) are planned for v2.
- **No script injection**: You cannot define custom NPC behaviors or player abilities through scripting. This is planned for v2.
