# Save Format

EFL uses a namespaced persistence system that isolates each pack's save data and organizes it by feature and content ID.

## Namespace Structure

All EFL save data follows this pattern:

```
EFL/<modId>/<feature>/<contentId>
```

- **EFL**: Top-level prefix, separating EFL data from the base game's saves
- **modId**: The pack's unique identifier from its manifest (e.g., `com.author.my-mod`)
- **feature**: The subsystem that owns the data (e.g., `flags`, `quest`, `npc`, `area`)
- **contentId**: The specific content item's identifier

### Examples

```
EFL/com.example.hello_adventurer/flags/met_guide
EFL/com.example.hello_adventurer/quest/greenhouse_restoration
EFL/com.example.hello_adventurer/npc/town_guide
EFL/com.example.hello_adventurer/area/forgotten_greenhouse
EFL/com.example.hello_adventurer/resource/wild_herb_respawn
```

## What Is Persisted

| Feature | Data Stored |
|:--------|:------------|
| Flags | Boolean flags set by events, quests, and dialogue |
| Quest progress | Current stage, completed objectives, turn-in state |
| NPC state | Interaction count, visibility state, current schedule position |
| Resource respawn | Respawn timers for harvested/broken resource nodes |
| Area state | Per-area flags and dynamic content state |

## saveScope Setting

The `saveScope` field in the manifest controls the namespace prefix:

```json
{
    "settings": {
        "saveScope": "EFL/com.author.my-mod"
    }
}
```

Convention: always use `EFL/<modId>`. Custom scopes are allowed but not recommended — they can cause collisions if two packs choose the same scope.

## Pack Isolation

Packs cannot read or write each other's save data. The save service enforces namespace boundaries:

- Write operations are scoped to the calling pack's `saveScope`
- Read operations only return data within the pack's own namespace
- Cross-pack data sharing must go through the IPC channel broker

This prevents accidental data corruption and ensures packs can be installed or removed independently without affecting each other's save state.

## Migration System

The `migrations` feature flag in the manifest enables save data migration:

```json
{
    "features": {
        "migrations": true
    }
}
```

When enabled, EFL checks for version changes in a pack's save data and runs migration logic to update the data format. This is important when a pack update changes the structure of persisted data (e.g., renaming a flag, restructuring quest stages).

Without migrations enabled, EFL loads save data as-is. If the format has changed, the pack is responsible for handling any incompatibilities.

## Save Timing

EFL save operations are synchronized with the game's save events — data is written when the player saves their game, not at arbitrary times. This ensures:

- Save data is consistent with the game state at the point of save
- No partial writes if the game crashes
- Compatibility with the game's save slot system
