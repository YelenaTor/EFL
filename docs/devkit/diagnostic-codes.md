# Diagnostic Codes

EFL emits structured diagnostic codes during content validation and runtime. These codes appear in log files and in the DevKit diagnostics view.

## Stability Policy (V2.5 Beta)

- Diagnostic **IDs and categories are stable** during V2.5 beta.
- Diagnostic **message text and severity may be tuned** as validation improves.
- New codes may be added, but shipped IDs are not repurposed.

## Code Format

```
CATEGORY-SEVERITY###
```

- **CATEGORY**: The subsystem that emitted the diagnostic
- **SEVERITY**: Single letter indicating severity
- **###**: Three-digit numeric identifier

Example: `MANIFEST-E001` — an error from the manifest subsystem, code 001.

## Severity Levels

| Prefix | Severity | Meaning |
|:-------|:---------|:--------|
| `E` | Error | Fatal to the subsystem. The affected content will not load. |
| `W` | Warning | Degraded but functional. Content loads with reduced capability. |
| `H` | Hazard | Not currently broken, but likely to cause problems (e.g., potential conflicts with base game or unimplemented stubs). |

## Categories

| Category | Subsystem |
|:---------|:----------|
| `BOOT` | Bootstrap and initialization |
| `MANIFEST` | Manifest parsing and validation |
| `PACK` | `.efpack` archive loading |
| `HOOK` | Engine bridge hook registration |
| `AREA` | Area registration and room hijacking |
| `WARP` | Warp point registration |
| `RESOURCE` | Resource node registration |
| `CRAFT` | Crafting recipe registration |
| `NPC` | NPC registration and lifecycle |
| `QUEST` | Quest registration and stage validation |
| `TRIGGER` | Trigger condition registration |
| `STORY` | Story event and cutscene bridge |
| `DIALOGUE` | Dialogue tree registration |
| `SAVE` | Save namespace and persistence |
| `IPC` | Inter-process and cross-mod communication |
| `EDIT` | DevKit edit-in-place workspace operations |
| `RELOAD` | Engine command pipe / live reload signaling |

## Codes

### BOOT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `BOOT-W001` | Warning | Content directory does not exist | Create the `EFL/` content directory or verify `contentDir` in your manifest |
| `BOOT-E001` | Error | JSON parse error in a content file during load | Check JSON syntax in the reported file |
| `RELOAD-W001` | Warning | Hot-reload watcher failed to start, or a file change triggered a parse error or reload exception | Check file permissions; inspect the hint text for parse details |

### MANIFEST

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `MANIFEST-E001` | Error | Failed to parse manifest file (invalid JSON or missing required fields) | Check JSON syntax and required fields in your `.efl` manifest |
| `MANIFEST-E010` | Error | Manifest root is not a JSON object | Ensure the manifest root is a JSON object |
| `MANIFEST-E011` | Error | Missing or empty required manifest field | Fill required fields (`schemaVersion`, `modId`, `name`, `version`, `eflVersion`) |
| `MANIFEST-E012` | Error | `schemaVersion` is not a positive integer | Set `schemaVersion` to a positive integer |
| `MANIFEST-E013` | Error | `features` exists but is not a string array | Ensure `features` is an array of feature tag strings |
| `MANIFEST-E002` | Error | Mod's `eflVersion` field requires a newer EFL than is installed | Update EFL via MOMI, or use a mod version compatible with your EFL version |
| `MANIFEST-E003` | Error | A required dependency declared in `dependencies.required` is not loaded | Install the missing dependency via MOMI before loading this pack |
| `MANIFEST-W001` | Warning | An optional dependency declared in `dependencies.optional` is not loaded | Install the optional dependency to enable its related features |
| `MANIFEST-W010` | Warning | `modId` is not namespaced | Use a namespaced `modId` (for example `com.author.pack`) |
| `MANIFEST-W011` | Warning | Unknown feature tag was declared | Check for typos or remove unsupported feature tags. When DevKit is connected to a running engine, this is checked against the engine's `capabilities.snapshot` instead of the DevKit's static list. |
| `MANIFEST-W012` | Warning | Unknown top-level manifest field found | Remove or gate custom fields by toolchain version |
| `MANIFEST-W013` | Warning | Local `localisation/` folder detected in pack workspace | Move localization delivery to MOMI and keep only runtime keys in EFL content |
| `MANIFEST-W020` | Warning | Optional dependency missing in workspace graph checks | Install the optional dependency or accept reduced compatibility |
| `MANIFEST-W030` | Warning | Required dependency appears to be MOMI-scoped (`momi.*`) | Declare MOMI requirements through `.efdat` compatibility relationships |
| `MANIFEST-W031` | Warning | Optional dependency appears to be MOMI-scoped (`momi.*`) | Use `.efdat` optional MOMI relationship declarations |
| `MANIFEST-W032` | Warning | Conflict dependency appears to be MOMI-scoped (`momi.*`) | Represent MOMI conflicts in `.efdat` compatibility shims |
| `MANIFEST-E020` | Error | Duplicate `modId` detected in workspace manifests | Ensure each workspace pack uses a globally unique `modId` |
| `MANIFEST-E021` | Error | Pack declares itself as a required dependency | Remove self-referential dependency entries |
| `MANIFEST-E022` | Error | Declared conflicting dependency exists in workspace | Remove one side of the conflict or provide a compatibility shim |

### PACK

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `PACK-E001` | Error | Failed to extract `.efpack` archive | Ensure the file is a valid efpack archive containing a `manifest.efl`; re-pack with `efl-pack` |
| `PACK-E002` | Error | `.efpack` extracted successfully but `manifest.efl` inside failed to parse | Check JSON syntax and required fields in the manifest inside the archive |

### HOOK

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `HOOK-W002` | Warning | Manifest script hook uses `mode: "inject"` which is not yet supported | Remove `mode: "inject"` or use `mode: "callback"`; GML injection is planned for a future release |
| `HOOK-W003` | Warning | A core EFL engine hook failed to register (room_transition, grid_init, frame_update, or tool-node hooks) | This usually means the target script name changed in a FoM update; report to EFL maintainers |
| `HOOK-W004` | Warning | Manifest script hook references an unknown handler name | Check `handler` value against the list of EFL built-in handlers (`efl_resource_despawn`, etc.). When DevKit is connected to a running engine, this is checked against the engine's `capabilities.snapshot` handler list instead of the DevKit's static list. |
| `HOOK-W010` | Warning | Script hook target does not match expected runtime symbol style (`gml_*`) | Verify target symbol against known GameMaker runtime script names |

### AREA

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `AREA-H001` | Hazard | `NativeRoomBackend` activated but native rooms are not yet implemented; falling back to `HijackedRoomBackend` | Set `areaBackend` to `"hijacked"` in manifest settings to suppress this warning |
| `AREA-E001` | Error | Failed to parse an area definition file | Check JSON syntax and required fields in the reported `.json` file |
| `AREA-E030` | Error | An area definition is missing a required `id` | Add a non-empty `id` to the area JSON file |
| `AREA-E031` | Error | Two area definitions share the same `id` | Ensure each area has a unique `id` within the pack |
| `AREA-H020` | Hazard (warning under recommended, error under strict) | `entryEvent` references an event that is not defined in this pack | Confirm the event id, or accept the hazard if it is a base-game cutscene |
| `AREA-H021` | Hazard (warning under recommended, error under strict) | `exitEvent` references an event that is not defined in this pack | Confirm the event id, or accept the hazard if it is a base-game cutscene |

### WARP

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `WARP-W001` | Warning | A warp transition was suppressed because its `requireTrigger` condition is not met | This is expected during gameplay; fulfil the trigger condition to allow the warp |
| `WARP-E001` | Error | Failed to parse a warp definition file | Check JSON syntax and required fields |
| `WARP-E030` | Error | A warp definition is missing a required `id` | Add a non-empty `id` to the warp JSON file |
| `WARP-E031` | Error | Two warp definitions share the same `id` | Ensure each warp id is unique within the pack |
| `WARP-H010` | Hazard (warning under recommended, error under strict) | `sourceArea` is not an area defined in this pack | Confirm the area id; ok if the warp targets a base-game area |
| `WARP-H011` | Hazard (warning under recommended, error under strict) | `targetArea` is not an area defined in this pack | Confirm the area id; ok if the warp targets a base-game area |

### RESOURCE

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `RESOURCE-H001` | Hazard | A manifest-declared `efl_resource_despawn` script hook fired, but custom-target dispatch is not yet wired | Standard resource interactions via hoe, pick, and water node hooks are active. Manifest-declared custom-target dispatch is a V2.5 feature; remove the `scriptHooks` entry to suppress this diagnostic. |
| `RESOURCE-H002` | Hazard | Dungeon vote injection fired but the `biomes` array was not found on the `load_dungeon` self struct | EFL could not locate `self.biomes` or `self.dungeon.biomes` at runtime. Check the engine log for the self property dump to find the correct path; this may indicate a FoM update changed the struct layout. |
| `RESOURCE-H003` | Hazard | Grid-native node spawn via `attempt_to_write_object_node` is deferred pending the same struct probe | Use `"objectName"` in your resource JSON as an interim spawn path. Grid-native spawn will replace this once the probe is complete. |
| `RESOURCE-W001` | Warning | Failed to register the dungeon vote injection hook (`create_node_prototypes`) | EFL resource nodes with `dungeonVotes` will not appear in FoM dungeon floors |
| `RESOURCE-W002` | Warning | Failed to register the `new_day` hook | EFL resources will not respawn on day change; resource respawn is disabled for this session |
| `RESOURCE-W003` | Warning | A resource definition has no `objectName` field — interim spawn skipped | Add `"objectName": "obj_<fom_object_name>"` to the resource JSON. This is the interim spawn path until grid-native spawn is wired. |
| `RESOURCE-E001` | Error | Failed to parse a resource definition file | Check JSON syntax and required fields |
| `RESOURCE-E030` | Error | A resource definition is missing a required `id` | Add a non-empty `id` to the resource JSON file |
| `RESOURCE-E031` | Error | Two resource definitions share the same `id` | Ensure each resource id is unique within the pack |
| `RESOURCE-H010` | Hazard (warning under recommended, error under strict) | `spawnRules.areas` references an area not defined in this pack | Confirm the area id; ok if it is a base-game room |
| `RESOURCE-H011` | Hazard (warning under recommended, error under strict) | `spawnRules.anchors` key is not an area defined in this pack | Confirm the area id; ok if it is a base-game room |

### CRAFT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `CRAFT-W001` | Warning | Failed to register the crafting menu hook (`spawn_crafting_menu`) | EFL recipes will not be injected into crafting stations; this may indicate a FoM update changed the script name |
| `CRAFT-E001` | Error | Failed to parse a recipe definition file | Check JSON syntax and required fields |
| `CRAFT-E030` | Error | A recipe definition is missing a required `id` | Add a non-empty `id` to the recipe JSON file |
| `CRAFT-E031` | Error | Two recipe definitions share the same `id` | Ensure each recipe id is unique within the pack |

### NPC

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `NPC-E001` | Error | Failed to parse a local or world NPC definition file | Check JSON syntax and required fields |
| `NPC-E030` | Error | An NPC definition is missing a required `id` | Add a non-empty `id` to the npc JSON file |
| `NPC-E031` | Error | Two NPC definitions share the same `id` (across `npcs/` and `world_npcs/` namespaces) | Ensure each NPC id is unique |
| `NPC-E020` | Error | LocalNpc `areaId` does not resolve to a pack-defined area | LocalNpcs only live in pack areas; fix the `areaId` |
| `NPC-H021` | Hazard (warning under recommended, error under strict) | WorldNpc `defaultAreaId` is not defined in this pack | Confirm the id, or accept the hazard if it is a base-game room |
| `NPC-H022` | Hazard (warning under recommended, error under strict) | WorldNpc `schedule[*].areaId` is not defined in this pack | Confirm the id, or accept the hazard if it is a base-game room |

### QUEST

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `QUEST-E001` | Error | Failed to parse a quest definition file | Check JSON syntax and required fields |
| `QUEST-E030` | Error | A quest definition is missing a required `id` | Add a non-empty `id` to the quest JSON file |
| `QUEST-E031` | Error | Two quest definitions share the same `id` | Ensure each quest id is unique within the pack |
| `QUEST-E010` | Error | A quest stage is missing a required `id` | Add an `id` to every entry in `stages[]` |
| `QUEST-E011` | Error | A quest has duplicate stage ids | Use unique stage ids within the same quest |
| `QUEST-W010` | Warning (error under strict) | A quest has no stages defined | Add at least one stage to the quest |

### TRIGGER

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `TRIGGER-E001` | Error | Failed to register a trigger condition (invalid condition type or circular dependency) | Check trigger syntax and ensure no self-referential or mutually circular conditions |
| `TRIGGER-E010` | Error | Content references a trigger ID that is not defined in `triggers/` | Define the missing trigger or remove the reference |
| `TRIGGER-W010` | Warning | Trigger is defined but never referenced by content graph checks | Remove dead trigger definitions or reference them from content |

### IPC

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `IPC-E010` | Error | `ipc.publish`/`ipc.consume` contains non-string channel entries | Use string channel IDs only |
| `IPC-E011` | Error/Warning | Publish channel ownership mismatch (`modId:` prefix) | Prefix published channels with your pack `modId` |
| `IPC-W010` | Warning | Duplicate channel in `ipc.publish` | Remove duplicate publish channel declarations |
| `IPC-W011` | Warning | Duplicate channel in `ipc.consume` | Remove duplicate consume channel declarations |
| `IPC-W012` | Warning | Same channel appears in both publish and consume | Confirm intentional loopback usage or split channels |

### DAT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `DAT-E001` | Error | Failed to parse/read `manifest.efdat` | Check file existence, permissions, and JSON syntax |
| `DAT-E010` | Error | `manifest.efdat` root is not an object | Ensure the top-level JSON value is an object |
| `DAT-E011` | Error | Missing required `.efdat` field | Add missing required fields (`schemaVersion`, `datId`, `name`, `version`, `eflVersion`, `relationships`) |
| `DAT-E012` | Error | Invalid `.efdat` schemaVersion | Use schemaVersion `1` for `.efdat` |
| `DAT-E013` | Error | `relationships` is empty | Provide at least one compatibility relationship entry |
| `DAT-E014` | Error | Relationship entry is not an object | Ensure each relationship item is a JSON object |
| `DAT-E015` | Error | Invalid relationship type | Use `requires`, `optional`, or `conflicts` |
| `DAT-E016` | Error | Missing relationship target object | Add `target` with `kind`/`id` |
| `DAT-E017` | Error | Invalid relationship target kind | Use target kind `efpack` or `momi` |
| `DAT-E018` | Error | Missing/empty relationship target id | Provide a valid target id |
| `DAT-E020` | Error | `.efdat` `requires` relationship targets a MOMI mod that is not active/present in detected inventory | Install/enable the required MOMI mod, or adjust the compatibility requirements |
| `DAT-E021` | Error | `.efdat` `conflicts` relationship targets a MOMI mod that is active/present in detected inventory | Disable/remove the conflicting MOMI mod, or remove the conflict declaration |
| `DAT-W010` | Warning | `datId` naming does not match reverse-domain guidance | Use reverse-domain `datId` naming |
| `DAT-W011` | Warning | Relationship missing versionRange for non-conflict entry | Add a `versionRange` for `requires`/`optional` entries |
| `DAT-W020` | Warning/Hazard (error under strict) | `.efdat` `optional` MOMI relationship target not active/present in detected inventory | Install/enable the optional MOMI mod if you want the compatibility extension active |
| `DAT-W021` | Warning/Hazard (error under strict) | MOMI inventory source unavailable for this validation run (no runtime snapshot and no mods-folder index found) | Provide a MOMI runtime connection or configure/discover a mods folder so MOMI enforcement can run |

### EDIT

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `EDIT-E001` | Error | DevKit refused to extract into a non-empty target folder | Pick a clean destination or remove the folder before retrying |
| `EDIT-E002` | Error | Archive entry has an empty path | Re-pack the source artifact with a non-broken builder |
| `EDIT-E003` | Error | Archive entry escapes the workspace via `..` | Reject the archive; do not unpack untrusted artifacts |
| `EDIT-E004` | Error | Archive entry contains an absolute path or stream marker | Reject the archive; rebuild it with relative paths |

### RELOAD

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `RELOAD-W002` | Warning | Engine could not bind the inbound command pipe | Confirm no other DevKit/engine instance owns the pipe; engine falls back to file-watcher reload |

### DIALOGUE

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `DIALOGUE-E001` | Error | Failed to parse a dialogue definition file | Check JSON syntax and required fields |
| `DIALOGUE-E030` | Error | A dialogue definition is missing a required `id` | Add a non-empty `id` to the dialogue JSON file |
| `DIALOGUE-E031` | Error | Two dialogue definitions share the same `id` | Ensure each dialogue id is unique within the pack |
| `DIALOGUE-H010` | Hazard (warning under recommended, error under strict) | Dialogue `npc` does not resolve to a pack-defined NPC (LocalNpc or WorldNpc) | Confirm the id; ok if attaching to a base-game NPC like `ari` |
| `DIALOGUE-E011` | Error | A dialogue file has duplicate `entries[].id` values | Use unique ids within a dialogue file |

### STORY

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `STORY-E001` | Error | Failed to parse a cutscene definition file in `events/` | Check JSON syntax and required fields (`id`, `trigger`, `once`, `onFire`) |
| `STORY-E030` | Error | A cutscene event definition is missing a required `id` | Add a non-empty `id` to the event JSON file |
| `STORY-E031` | Error | Two event definitions share the same `id` | Ensure each event id is unique within the pack |
| `STORY-W001` | Warning | Failed to register the `load_cutscenes` observation hook | EFL cutscene keys will not be reported to the DevKit diagnostics view; eligibility gating is unaffected |
| `STORY-E002` | Error | Failed to register the `check_cutscene_eligible` intercept hook | EFL-gated cutscenes will never become eligible; FoM cutscenes without EFL triggers are unaffected |
| `STORY-H020` | Hazard (warning under recommended, error under strict) | `onFire.startQuest` is not a quest defined in this pack | Confirm the id; ok if it advances a base-game quest |
| `STORY-H021` | Hazard (warning under recommended, error under strict) | `onFire.advanceQuest` is not a quest defined in this pack | Confirm the id; ok if it advances a base-game quest |

### CALENDAR

V3 pilot — calendar / world-event registry. Files live under `calendar/` and require the `calendar` feature in the manifest.

| Code | Severity | Trigger | What to do |
|:-----|:---------|:--------|:-----------|
| `CALENDAR-E001` | Error | Failed to parse a calendar event file in `calendar/` | Check JSON syntax and the required `id` field |
| `CALENDAR-E010` | Error | `season` is not one of `spring`, `summer`, `fall`, `winter`, `any`, or 0..=3 | Use a recognised season string |
| `CALENDAR-E011` | Error | `dayOfSeason` is outside the 1..=28 range | Use 1..=28 or omit to match every day |
| `CALENDAR-E012` | Error | `lifecycle` is not `daily` or `once` | Use one of the supported lifecycle values |
| `CALENDAR-E030` | Error | A calendar event is missing a required `id` | Add a non-empty `id` to the event JSON file |
| `CALENDAR-E031` | Error | Two calendar events share the same `id` | Ensure each calendar event id is unique within the pack |
| `CALENDAR-H020` | Hazard (warning under recommended, error under strict) | `onActivate` does not resolve to a story event defined in this pack | Confirm the id; ok if it triggers a base-game cutscene |

## Reading Diagnostics

When a diagnostic is emitted, the log entry includes:

1. The diagnostic code (e.g., `MANIFEST-E001`)
2. A human-readable description of the problem
3. A suggested fix or next step

Example log output:

```
[MANIFEST-E001] Failed to parse manifest: my-mod.efl
  → Check JSON syntax and required fields
```

In the DevKit, diagnostics appear as color-coded entries grouped by category during the diagnostics view.
