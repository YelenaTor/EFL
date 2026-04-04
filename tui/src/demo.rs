use std::sync::mpsc;
use std::thread;
use std::time::Duration;

use crate::pipe::IpcMessage;

/// Spawn a background thread that feeds mock IPC messages through the channel,
/// simulating: boot sequence -> diagnostics -> monitor with live activity.
pub fn start_demo() -> mpsc::Receiver<IpcMessage> {
    let (tx, rx) = mpsc::channel();

    thread::spawn(move || {
        let send = |tx: &mpsc::Sender<IpcMessage>, msg: IpcMessage| -> bool {
            tx.send(msg).is_ok()
        };

        // === BOOT PHASE ===
        let boot_steps = [
            ("Version check", "ok"),
            ("Aurie connection", "ok"),
            ("YYTK bridge init", "ok"),
            ("Manifest discovery", "ok"),
            ("Parsing example-mod.efl", "ok"),
            ("Capability resolution", "ok"),
            ("Hook registry init", "warning"),
            ("Save namespace setup", "ok"),
            ("Event bus init", "ok"),
            ("Subsystem startup", "ok"),
        ];

        for (step, status) in &boot_steps {
            if !send(&tx, msg("boot.status", serde_json::json!({
                "step": step,
                "status": "in_progress"
            }))) { return; }
            thread::sleep(Duration::from_millis(300));

            if !send(&tx, msg("boot.status", serde_json::json!({
                "step": step,
                "status": status
            }))) { return; }
            thread::sleep(Duration::from_millis(150));
        }

        thread::sleep(Duration::from_millis(500));

        // Transition to diagnostics
        if !send(&tx, msg("phase.transition", serde_json::json!({ "phase": "diagnostics" }))) { return; }

        // === DIAGNOSTICS PHASE ===
        let diagnostics = [
            ("MANIFEST-E001", "error", "MANIFEST", "Missing required field 'version' in example-mod.efl"),
            ("HOOK-W003", "warning", "HOOK", "Hook target gml_Script_hoe_node may have changed in game update 0.14"),
            ("AREA-H002", "hazard", "AREA", "Area 'crystal_cave' references room r_mine_04 which may conflict with base game"),
            ("MANIFEST-W001", "warning", "MANIFEST", "Optional field 'author_url' is empty"),
            ("NPC-E002", "error", "NPC", "NPC 'flora_spirit' references undefined dialogue tree 'flora_intro'"),
            ("SAVE-W001", "warning", "SAVE", "Save namespace 'EFL/example-mod' exceeds recommended key count (>50)"),
            ("TRIGGER-H001", "hazard", "TRIGGER", "Trigger 'unlock_cave' uses allOf with 5+ conditions — consider splitting"),
            ("BOOT-W001", "warning", "BOOT", "Hook registry fallback mode active — some targets may be unavailable"),
            ("RESOURCE-E001", "error", "RESOURCE", "Resource node 'mythril_ore' sprite sheet not found at sprites/mythril.png"),
            ("QUEST-W002", "warning", "QUEST", "Quest 'find_crystals' has no fail condition defined"),
        ];

        for (code, severity, category, message) in &diagnostics {
            if !send(&tx, msg("diagnostic", serde_json::json!({
                "code": code,
                "severity": severity,
                "category": category,
                "message": message,
            }))) { return; }
            thread::sleep(Duration::from_millis(100));
        }

        thread::sleep(Duration::from_secs(3));

        // Transition to monitor
        if !send(&tx, msg("phase.transition", serde_json::json!({ "phase": "monitor" }))) { return; }

        // === MONITOR PHASE ===
        // Emit engine version
        if !send(&tx, msg("efl.version", serde_json::json!({ "version": "2.0.0" }))) { return; }

        // Register hooks with realistic kinds
        let hooks: &[(&str, &str)] = &[
            ("room_transition",                          "yyc_script"),
            ("grid_init",                                "yyc_script"),
            ("frame_update",                             "frame"),
            ("efl_crafting_inject",                      "yyc_script"),
            ("efl_dungeon_vote_inject",                  "yyc_script"),
            ("pick_node",                                "yyc_script"),
            ("hoe_node",                                 "yyc_script"),
            ("warp_gate",                                "yyc_script"),
        ];
        for (name, kind) in hooks {
            if !send(&tx, msg("hook.registered", serde_json::json!({
                "name": name, "target": name, "kind": kind
            }))) { return; }
            thread::sleep(Duration::from_millis(150));
        }

        // Register mods (use modId to match engine output)
        if !send(&tx, msg("mod.status", serde_json::json!({
            "modId": "dev.yoru.crystal-caves",
            "name": "Crystal Caves",
            "version": "0.3.1",
            "status": "loaded"
        }))) { return; }

        if !send(&tx, msg("mod.status", serde_json::json!({
            "modId": "dev.yoru.flora-expansion",
            "name": "Flora's Garden",
            "version": "1.0.0",
            "status": "loaded"
        }))) { return; }

        if !send(&tx, msg("mod.status", serde_json::json!({
            "modId": "dev.yoru.broken-mod",
            "name": "Broken Mod",
            "version": "0.1.0",
            "status": "error"
        }))) { return; }

        // Simulate ongoing activity
        for cycle in 0..60 {
            thread::sleep(Duration::from_millis(800));

            // Fire frame_update hook every cycle
            if !send(&tx, msg("hook.fired", serde_json::json!({
                "name": "frame_update", "kind": "frame",
                "count": 1, "frameNumber": cycle * 60
            }))) { return; }

            // Fire script hooks periodically
            if cycle % 5 == 0 {
                if !send(&tx, msg("hook.fired", serde_json::json!({
                    "name": "room_transition", "target": "gml_Object_obj_roomtransition_Create_0"
                }))) { return; }
            }

            // Story event every ~8 cycles
            if cycle % 8 == 0 {
                if !send(&tx, msg("story.fired", serde_json::json!({
                    "eventId": "crystal_cave_reveal",
                    "commandCount": 2,
                    "status": "ok"
                }))) { return; }
            }

            // Quest updates
            if cycle == 12 {
                if !send(&tx, msg("quest.updated", serde_json::json!({
                    "questId": "find_crystals", "action": "start"
                }))) { return; }
            }
            if cycle == 30 {
                if !send(&tx, msg("quest.updated", serde_json::json!({
                    "questId": "find_crystals", "action": "advance",
                    "prevStage": "gather", "currentStage": "deliver"
                }))) { return; }
            }

            // Dialogue open events
            if cycle % 15 == 5 {
                if !send(&tx, msg("dialogue.open", serde_json::json!({
                    "eventId": "flora_intro",
                    "npcId": "flora_spirit",
                    "lineId": "intro_01"
                }))) { return; }
            }

            // EFL EventBus events
            if cycle % 4 == 0 {
                if !send(&tx, msg("event.published", serde_json::json!({
                    "name": "area.activated", "subscribers": 2
                }))) { return; }
            }

            // Save operations
            if cycle % 7 == 0 {
                let op = if cycle % 14 == 0 { "set" } else { "remove" };
                if !send(&tx, msg("save.operation", serde_json::json!({
                    "operation": op,
                    "key": format!("EFL/dev.yoru.crystal-caves/area/cave_visited_{}", cycle)
                }))) { return; }
            }
        }
    });

    rx
}

fn msg(msg_type: &str, payload: serde_json::Value) -> IpcMessage {
    IpcMessage {
        msg_type: msg_type.to_string(),
        timestamp: chrono_now(),
        payload,
    }
}

fn chrono_now() -> String {
    // Simple timestamp without pulling in chrono crate
    let d = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    format!("T+{:.1}s", d.as_secs_f64() % 10000.0)
}
