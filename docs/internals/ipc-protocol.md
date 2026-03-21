# IPC Protocol

EFL uses two IPC mechanisms: a named pipe for engine-to-TUI communication, and a channel broker for cross-mod messaging.

## Engine-to-TUI Pipe

### Connection

The engine creates a Windows named pipe at startup:

```
\\.\pipe\efl-{pid}
```

Where `{pid}` is the game's process ID. The TUI connects to this pipe and reads passively.

### Best-Effort Delivery

The pipe is best-effort. If the TUI is not connected or fails to read, the engine continues normally. Messages are not queued or retried — the TUI sees whatever it reads in real time.

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
        "key": "EFL/com.author.mod/area/crystal_cave"
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
        "id": "com.author.mod",
        "name": "Crystal Caves",
        "version": "1.0.0",
        "status": "active"
    }
}
```

- `status`: `"active"`, `"error"`, `"disabled"`

### phase.transition

Emitted when the TUI should switch display phases.

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
