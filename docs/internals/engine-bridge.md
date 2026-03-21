# Engine Bridge

Layer B is the **only layer in EFL that directly interacts with game internals** — YYTK, Aurie, and GML. Everything above Layer B uses EFL's own abstractions, ensuring that changes to the game or modding tools only require updates in one place.

## Module

`EFL.EngineBridge.FoM` — the "FoM" suffix indicates this bridge is specific to Fields of Mistria. The architecture allows for game-specific bridges while keeping the rest of EFL game-agnostic.

## Hook Registry

The hook registry manages YYTK function hooks. It provides:

- Registration of hooks by GML script name
- Hook lifecycle management (register, fire, unregister)
- Fallback mode when hook targets aren't found (emits `HOOK-W003` warnings)

### Known Hook Targets

These GML functions have been identified through reverse engineering and are hooked by EFL v1:

| Hook Target | Purpose |
|:------------|:--------|
| `gml_Object_obj_roomtransition_Create_0` | Room transition detection |
| `gml_Script_initialize_on_room_start@Grid@Grid` | Grid initialization on room entry |
| `gml_Script_hoe_node` | Hoe tool interaction with nodes |
| `gml_Script_pick_node` | Pickaxe tool interaction with nodes |
| `gml_Script_water_node` | Watering can interaction with nodes |
| `par_NPC` init | NPC initialization and lifecycle |
| NPC FSM behavior | NPC state machine for movement/actions |
| NPC talk handlers | NPC dialogue interaction |
| NPC serialization | NPC save/load |
| `StoryExecutor` | Cutscene playback |
| Cutscene load/validate/start/skip | Cutscene lifecycle hooks |
| `assert_quest_active` | Quest state checking |
| `fulfill_quest` | Quest objective completion |
| `turn_in_quest` | Quest turn-in and reward |

## Room Tracker

Tracks the current game room and detects room transitions. Used by:

- Area registry to know when a hijacked room is entered
- Warp service to trigger transitions
- Event system to fire room-entry events

## Named Routine Invoker

Calls GML scripts by name through YYTK's runtime bridge. This allows EFL to invoke game functions without hardcoding numeric IDs that change between game versions.

## Instance Walker

Iterates over GameMaker instances in the current room. Used to:

- Find NPC instances for dialogue and interaction
- Locate resource nodes for spawn management
- Query game state for trigger evaluation

## Save Namespace Plumbing

Bridges EFL's namespaced save operations to the game's persistence layer. All save data flows through this bridge, ensuring proper serialization format and timing (saves happen during the game's save events, not at arbitrary times).

## Area Backend Abstraction

### HijackedRoomBackend (v1)

The v1 backend repopulates existing game rooms with custom content. When the player enters a hijacked room:

1. Room tracker detects the room transition
2. Area registry checks if the room is hijacked by an EFL area
3. The backend clears or modifies the room's default content
4. EFL-defined entities (NPCs, resources, etc.) are spawned
5. Entry anchor positions the player

This approach works within GameMaker's existing room system without requiring new room creation.

### NativeRoomBackend (Planned, v2)

The planned v2 backend will create entirely new rooms in the GameMaker runtime. This removes the dependency on existing rooms and allows for fully custom geometry.

## StoryBridge

Reuses the game's built-in `StoryExecutor` and cutscene system rather than implementing a second cutscene engine. EFL events with `"mode": "nativeBridge"` are translated into StoryExecutor commands.

This provides:

- Native-quality cutscene playback
- Compatibility with the game's existing cutscene format
- Camera control, character movement, and dialogue through the game's own systems
