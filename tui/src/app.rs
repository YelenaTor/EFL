use ratatui::Frame;

use crate::diagnostics::collector::{Diagnostic, DiagnosticCollector};
use crate::ipc::PipeReader;
use crate::phases;

/// TUI phases: Boot -> Diagnostics -> Monitor
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

/// Root application state machine.
pub struct App {
    pub phase: Phase,
    pub collector: DiagnosticCollector,
    pub pipe_reader: Option<PipeReader>,

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

impl App {
    pub fn new(pipe_reader: Option<PipeReader>) -> Self {
        Self {
            phase: Phase::Boot,
            collector: DiagnosticCollector::new(),
            pipe_reader,
            boot_steps: Vec::new(),
            efl_version: String::from("0.2.0"),
            hooks: Vec::new(),
            event_log: Vec::new(),
            save_log: Vec::new(),
            mods: Vec::new(),
            uptime_ticks: 0,
        }
    }

    /// Render the current phase.
    pub fn render(&self, frame: &mut Frame) {
        match self.phase {
            Phase::Boot => phases::boot::render(frame, self),
            Phase::Diagnostics => phases::diagnostics::render(frame, self),
            Phase::Monitor => phases::monitor::render(frame, self),
        }
    }

    /// Called each tick — process incoming pipe messages and update state.
    pub fn tick(&mut self) {
        self.uptime_ticks += 1;

        // Drain all available messages
        if self.pipe_reader.is_some() {
            // Borrow-checker dance: take the reader out temporarily
            let reader = self.pipe_reader.as_ref().unwrap();
            let mut messages = Vec::new();
            while let Some(msg) = reader.try_recv() {
                messages.push(msg);
            }
            for msg in messages {
                self.handle_message(msg);
            }
        }
    }

    fn handle_message(&mut self, msg: crate::ipc::IpcMessage) {
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
                // Update existing step or add new
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
                if !self.hooks.iter().any(|h| h.name == name) {
                    self.hooks.push(HookEntry { name, fire_count: 0 });
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
                // Keep event log bounded
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
                let id = msg.payload.get("id")
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
            _ => {}
        }
    }

    /// Transition to a new phase.
    pub fn transition_to(&mut self, phase: Phase) {
        self.phase = phase;
    }
}
