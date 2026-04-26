# IPC Protocol

EFL uses two IPC mechanisms: a named pipe for engine-to-DevKit communication, and a channel broker for cross-mod messaging.

## Engine-to-DevKit Pipe

### Connection

The engine creates a Windows named pipe at startup:

```
\\.\pipe\efl-{pid}
```

Where `{pid}` is the game's process ID. The DevKit connects to this pipe and reads passively.

### Best-Effort Delivery

The pipe is best-effort. If the DevKit is not connected or fails to read, the engine continues normally. Messages are not queued or retried — the DevKit sees whatever it reads in real time.

### Format

Messages are sent as **JSON Lines** — one JSON object per line, separated by newlines (`\n`).

### Message Envelope

Every message follows this structure:

```json
{
    "type": "boot.status",
    "timestamp": "2025-01-15T10:30:00.000Z",
    "payload": { ... }
}
```

| Field | Type | Description |
|:------|:-----|:------------|
| `type` | string | Message type identifier |
| `timestamp` | string | ISO 8601 UTC timestamp |
| `payload` | object | Type-specific data |

Schema: `schemas/ipc-message.schema.json`

## Message Types

### boot.status

Emitted during the boot phase as each initialization step progresses.

```json
{
    "type": "boot.status",
    "timestamp": "...",
    "payload": {
        "step": "discover",
        "status": "pass",
        "detail": "3 manifest(s) found"
    }
}
```

- `step`: Name of the boot step (e.g., `"version_check"`, `"discover"`, `"validate"`, `"load_content"`)
- `status`: `"running"`, `"pass"`, `"fail"`, `"warn"`, `"skip"`
- `detail`: Optional human-readable detail string

### diagnostic

Emitted when a validation issue is detected.

```json
{
    "type": "diagnostic",
    "timestamp": "...",
    "payload": {
        "code": "MANIFEST-E001",
        "severity": "error",
        "category": "MANIFEST",
        "message": "Failed to parse manifest: broken-mod.efl"
    }
}
```

### hook.registered

Emitted when a YYTK hook is successfully registered.

```json
{
    "type": "hook.registered",
    "timestamp": "...",
    "payload": {
        "name": "gml_Object_obj_roomtransition_Create_0"
    }
}
```

### hook.fired

Emitted each time a registered hook is invoked by the game.

```json
{
    "type": "hook.fired",
    "timestamp": "...",
    "payload": {
        "name": "gml_Object_obj_roomtransition_Create_0"
    }
}
```

### event.published

Emitted when an event is published to the event bus.

```json
{
    "type": "event.published",
    "timestamp": "...",
    "payload": {
        "name": "room_transition"
    }
}
```

### save.operation

Emitted when a save read or write occurs.

```json
{
    "type": "save.operation",
    "timestamp": "...",
    "payload": {
        "operation": "save",
        "key": "EFL/com.author.mod/area/forgotten_greenhouse"
    }
}
```

- `operation`: `"save"` or `"load"`
- `key`: The full namespaced save key

### mod.status

Emitted when a mod's status changes.

```json
{
    "type": "mod.status",
    "timestamp": "...",
    "payload": {
        "modId": "com.author.mod",
        "name": "Forgotten Greenhouse Expansion",
        "version": "1.1.0",
        "status": "active"
    }
}
```

- `status`: `"active"`, `"error"`, `"disabled"`

### phase.transition

Emitted when the DevKit should switch between boot, diagnostics, and monitor views.

```json
{
    "type": "phase.transition",
    "timestamp": "...",
    "payload": {
        "phase": "diagnostics"
    }
}
```

- `phase`: `"boot"`, `"diagnostics"`, `"monitor"`

### command_pipe.ready

Emitted once the inbound command pipe is bound. Tells DevKit that explicit reload signaling is available for this engine session.

```json
{
    "type": "command_pipe.ready",
    "timestamp": "...",
    "payload": {
        "name": "\\\\.\\pipe\\efl-12345-cmd",
        "protocol": "json-lines",
        "version": 1
    }
}
```

### reload.requested / reload.complete

Emitted before and after a reload triggered by a DevKit `reload` command (see "Inbound Command Pipe" below).

```json
{
    "type": "reload.complete",
    "timestamp": "...",
    "payload": {
        "path": "C:\\game\\mods\\efl",
        "reason": "devkit-pack",
        "files": 14,
        "failures": 0,
        "status": "ok"
    }
}
```

`reason` is whatever the DevKit passed in the command payload (e.g. `devkit-pack`, `devkit-watch`, `devkit-manual`).

### pong

Echoes a `ping` command back over the event pipe. Used as a liveness probe for the inbound channel.

### capabilities.snapshot

Broadcast once after `phase.transition: boot complete`, and re-broadcast on demand when the DevKit sends a `caps` command. Tells the DevKit which scriptHook handlers, feature tags, and runtime capability flags this engine build actually supports.

```json
{
    "type": "capabilities.snapshot",
    "timestamp": "...",
    "payload": {
        "eflVersion": "1.1.0",
        "handlers": ["efl_resource_despawn"],
        "features": [
            "areas", "warps", "npcs", "resources", "crafting", "quests",
            "dialogue", "story", "triggers", "assets", "ipc"
        ],
        "hookKinds": ["yyc_script", "frame", "detour"],
        "flags": {
            "dungeonVoteInjection": false,
            "worldNpcSchedules": false,
            "nativeRoomBackend": false,
            "assetInjection": false,
            "scriptHookCallbacks": true,
            "scriptHookInject": false
        }
    }
}
```

The DevKit caches the most recent snapshot on `EngineState` and feeds it to `validate_manifest_with_capabilities`. When a snapshot is present:

- `MANIFEST-W011` (unknown feature) is checked against `payload.features` instead of the DevKit's static list.
- `HOOK-W004` (unknown handler) is checked against `payload.handlers` instead of the DevKit's static list.

If no snapshot has arrived (e.g. headless `efl-pack` runs, no engine connected), the DevKit falls back to its build-time defaults.

## Inbound Command Pipe (DevKit -> Engine)

In addition to the outbound event pipe, the engine binds a sibling inbound pipe at:

```
\\.\pipe\efl-{pid}-cmd
```

This pipe accepts JSON Lines, one command envelope per line:

```json
{"type":"reload","payload":{"reason":"devkit-pack"}}
```

| Command | Payload | Effect |
|:--------|:--------|:-------|
| `reload` | `{ "reason": "..." }` | Re-walks the EFL content directory, dispatches every `.json` through the existing per-file reload path. Emits `reload.requested` and `reload.complete` on the event pipe. |
| `ping`   | any | Engine emits a matching `pong` envelope. Used to confirm the command pipe is alive end-to-end. |
| `caps` (alias `capabilities`) | none | Engine re-broadcasts the latest `capabilities.snapshot` event. Use this when the DevKit attaches mid-session and missed the boot-time emission. |

The DevKit closes the connection after each command; the engine then resets the pipe so the next command starts a fresh stream. Unknown command types are ignored (and the engine emits `command.unknown` for visibility).

If the engine cannot bind the command pipe (e.g. a stale handle held by another process), it emits diagnostic `RELOAD-W002` and falls back to file-watcher-only reload. The DevKit's "Reload engine" button reports the failure inline.

## Cross-Mod IPC (ChannelBroker)

The ChannelBroker provides versioned pub/sub channels for mod-to-mod communication.

### Channel Declaration

Channels are declared in the manifest via the `ipcPublish` and `ipcConsume` feature flags. A pack that publishes must declare `"ipcPublish": true`; a pack that subscribes must declare `"ipcConsume": true`.

### Message Format

Cross-mod messages are typed JSON objects sent through named channels. The ChannelBroker handles:

- Channel creation and ownership
- Version compatibility checking between publishers and consumers
- Message delivery to all subscribers
- Compatibility warnings when message schemas change between pack versions
