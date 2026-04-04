use crate::diagnostics::collector::{Diagnostic, DiagnosticCollector};
use crate::pipe::{IpcMessage, PipeReader};

/// Engine phases as reported by the engine via IPC.
#[derive(Debug, Clone, PartialEq)]
pub enum Phase {
    Boot,
    Diagnostics,
    Monitor,
}

/// Status of a single boot step.
#[derive(Debug, Clone)]
pub struct BootStep {
    pub label: String,
    pub status: BootStepStatus,
}

#[derive(Debug, Clone, PartialEq)]
pub enum BootStepStatus {
    Ok,
    Warning,
    Error,
    InProgress,
}

/// A registered hook entry for the monitor.
#[derive(Debug, Clone)]
pub struct HookEntry {
    pub name: String,
    pub kind: String,   // "yyc_script" | "script" | "frame" | "detour"
    pub fire_count: u32,
}

/// An event log entry for the monitor.
#[derive(Debug, Clone)]
pub struct EventLogEntry {
    #[allow(dead_code)]
    pub timestamp: String,
    pub event_type: String,
    pub detail: String,
}

/// A loaded mod entry for the monitor.
#[derive(Debug, Clone)]
pub struct ModEntry {
    pub id: String,
    pub name: String,
    pub version: String,
    pub status: String,
}

/// IPC-driven engine state — boot steps, hooks, events, mods, diagnostics.
pub struct EngineState {
    pub phase: Phase,
    pub collector: DiagnosticCollector,

    // Boot state
    pub boot_steps: Vec<BootStep>,
    pub efl_version: String,

    // Monitor state
    pub hooks: Vec<HookEntry>,
    pub event_log: Vec<EventLogEntry>,
    pub save_log: Vec<EventLogEntry>,
    pub mods: Vec<ModEntry>,
    pub uptime_ticks: u64,
}

impl EngineState {
    pub fn new() -> Self {
        Self {
            phase: Phase::Boot,
            collector: DiagnosticCollector::new(),
            boot_steps: Vec::new(),
            efl_version: String::from("0.2.0"),
            hooks: Vec::new(),
            event_log: Vec::new(),
            save_log: Vec::new(),
            mods: Vec::new(),
            uptime_ticks: 0,
        }
    }

    /// Called each frame — drain pipe messages and update state.
    pub fn tick(&mut self, pipe_reader: &Option<PipeReader>) {
        self.uptime_ticks += 1;

        if let Some(reader) = pipe_reader {
            let mut messages = Vec::new();
            while let Some(msg) = reader.try_recv() {
                messages.push(msg);
                if messages.len() >= 100 { break; } // cap per frame
            }
            for msg in messages {
                self.handle_message(msg);
            }
        }
    }

    pub fn handle_message(&mut self, msg: IpcMessage) {
        match msg.msg_type.as_str() {
            "boot.status" => {
                let label = msg.payload.get("step")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let status = match msg.payload.get("status").and_then(|v| v.as_str()) {
                    Some("ok") => BootStepStatus::Ok,
                    Some("warning") => BootStepStatus::Warning,
                    Some("error") => BootStepStatus::Error,
                    _ => BootStepStatus::InProgress,
                };
                if let Some(step) = self.boot_steps.iter_mut().find(|s| s.label == label) {
                    step.status = status;
                } else {
                    self.boot_steps.push(BootStep { label, status });
                }
            }
            "diagnostic" => {
                if let Ok(diag) = serde_json::from_value::<Diagnostic>(msg.payload) {
                    self.collector.add(diag);
                }
            }
            "phase.transition" => {
                if let Some(phase_str) = msg.payload.get("phase").and_then(|v| v.as_str()) {
                    match phase_str {
                        "boot" => self.transition_to(Phase::Boot),
                        "diagnostics" => self.transition_to(Phase::Diagnostics),
                        "monitor" => self.transition_to(Phase::Monitor),
                        _ => {}
                    }
                }
            }
            "hook.registered" => {
                let name = msg.payload.get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let kind = msg.payload.get("kind")
                    .and_then(|v| v.as_str())
                    .unwrap_or("script")
                    .to_string();
                if !self.hooks.iter().any(|h| h.name == name) {
                    self.hooks.push(HookEntry { name, kind, fire_count: 0 });
                }
            }
            "hook.fired" => {
                let name = msg.payload.get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown");
                if let Some(hook) = self.hooks.iter_mut().find(|h| h.name == name) {
                    hook.fire_count += 1;
                }
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "HOOK".to_string(),
                    detail: name.to_string(),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            "event.published" => {
                let name = msg.payload.get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "EVENT".to_string(),
                    detail: name.to_string(),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            "save.operation" => {
                let op = msg.payload.get("operation")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown");
                let key = msg.payload.get("key")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                self.save_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: op.to_uppercase(),
                    detail: key.to_string(),
                });
                if self.save_log.len() > 50 {
                    self.save_log.remove(0);
                }
            }
            "mod.status" => {
                // Engine emits "modId" (matches bootstrap.cpp pipe_->write("mod.status",...))
                let id = msg.payload.get("modId")
                    .or_else(|| msg.payload.get("id"))  // fallback for older engine builds
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let name = msg.payload.get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or(&id)
                    .to_string();
                let version = msg.payload.get("version")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?")
                    .to_string();
                let status = msg.payload.get("status")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                if let Some(m) = self.mods.iter_mut().find(|m| m.id == id) {
                    m.status = status;
                } else {
                    self.mods.push(ModEntry { id, name, version, status });
                }
            }
            "efl.version" => {
                if let Some(v) = msg.payload.get("version").and_then(|v| v.as_str()) {
                    self.efl_version = v.to_string();
                }
            }
            "story.fired" => {
                let event_id = msg.payload.get("eventId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let status = msg.payload.get("status")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "STORY".to_string(),
                    detail: format!("{event_id} [{status}]"),
                });
                if self.event_log.len() > 100 { self.event_log.remove(0); }
            }
            "quest.updated" => {
                let quest_id = msg.payload.get("questId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let action = msg.payload.get("action")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "QUEST".to_string(),
                    detail: format!("{quest_id} {action}"),
                });
                if self.event_log.len() > 100 { self.event_log.remove(0); }
            }
            "dialogue.open" => {
                let npc = msg.payload.get("npcId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let line = msg.payload.get("lineId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "DIALOGUE".to_string(),
                    detail: format!("{npc}: {line}"),
                });
                if self.event_log.len() > 100 { self.event_log.remove(0); }
            }
            _ => {}
        }
    }

    pub fn transition_to(&mut self, phase: Phase) {
        self.phase = phase;
    }
}
