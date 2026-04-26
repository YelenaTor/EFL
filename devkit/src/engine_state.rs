use crate::diagnostics::collector::{Diagnostic, DiagnosticCollector};
use crate::pack::EngineCapabilities;
use crate::pipe::IpcMessage;
use std::collections::BTreeMap;
use std::collections::HashMap;
use std::path::PathBuf;

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
    pub kind: String, // "yyc_script" | "script" | "frame" | "detour"
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
    pub source: ModSource,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModSource {
    Efpack,
    Momi,
    Unknown,
}

#[derive(Debug, Clone)]
pub struct RelationshipEntry {
    pub pack_id: String,
    pub momi_mod_id: String,
    pub relation: String,
    pub status: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MomiPresence {
    Active,
    PresentInactive,
    Missing,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum MomiBadge {
    Needed,
    Compatible,
    Conflict,
}

#[derive(Debug, Clone)]
pub struct MomiEdge {
    pub pack_id: String,
    pub relation: String,
    pub status: String,
}

#[derive(Debug, Clone)]
pub struct MomiScannedMod {
    pub id: String,
    pub name: Option<String>,
    pub version: Option<String>,
    pub path: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct MomiProjectedRelation {
    pub pack_id: String,
    pub momi_mod_id: String,
    /// Canonical values: `requires`, `optional`, `conflicts`.
    pub relation: String,
    /// Source artifact label (for popout graph context), e.g. datId.
    pub source: String,
}

#[derive(Debug, Clone)]
pub struct MomiViewRow {
    pub id: String,
    pub name: String,
    pub version: String,
    pub presence: MomiPresence,
    pub path: Option<PathBuf>,
    pub badges: Vec<MomiBadge>,
    pub edges: Vec<MomiEdge>,
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
    pub relationships: Vec<RelationshipEntry>,
    pub capability_events: Vec<String>,
    pub trigger_events: Vec<String>,
    pub registry_events: Vec<String>,

    /// Latest `capabilities.snapshot` from the connected engine. The validator
    /// prefers this over the static handler/feature whitelists when present,
    /// so DevKit warnings reflect the live runtime instead of a stale
    /// build-time snapshot.
    pub capabilities: Option<EngineCapabilities>,
}

impl EngineState {
    pub fn new() -> Self {
        Self {
            phase: Phase::Boot,
            collector: DiagnosticCollector::new(),
            boot_steps: Vec::new(),
            efl_version: String::from("1.0.0"),
            hooks: Vec::new(),
            event_log: Vec::new(),
            save_log: Vec::new(),
            mods: Vec::new(),
            relationships: Vec::new(),
            capability_events: Vec::new(),
            trigger_events: Vec::new(),
            registry_events: Vec::new(),
            capabilities: None,
        }
    }

    pub fn handle_message(&mut self, msg: IpcMessage) {
        match msg.msg_type.as_str() {
            "boot.status" => {
                let label = msg
                    .payload
                    .get("step")
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
                if self
                    .boot_steps
                    .last()
                    .map(|s| s.label.to_ascii_lowercase().contains("capability"))
                    .unwrap_or(false)
                {
                    self.capability_events
                        .push(self.boot_steps.last().unwrap().label.clone());
                    if self.capability_events.len() > 30 {
                        self.capability_events.remove(0);
                    }
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
                let name = msg
                    .payload
                    .get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let kind = msg
                    .payload
                    .get("kind")
                    .and_then(|v| v.as_str())
                    .unwrap_or("script")
                    .to_string();
                if !self.hooks.iter().any(|h| h.name == name) {
                    self.hooks.push(HookEntry {
                        name,
                        kind,
                        fire_count: 0,
                    });
                }
            }
            "hook.fired" => {
                let name = msg
                    .payload
                    .get("name")
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
                let name = msg
                    .payload
                    .get("name")
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
                let lower = name.to_ascii_lowercase();
                if lower.contains("trigger") {
                    self.trigger_events.push(name.to_string());
                    if self.trigger_events.len() > 40 {
                        self.trigger_events.remove(0);
                    }
                }
                if lower.starts_with("area.")
                    || lower.starts_with("warp.")
                    || lower.starts_with("npc.")
                    || lower.starts_with("quest.")
                    || lower.starts_with("resource.")
                {
                    self.registry_events.push(name.to_string());
                    if self.registry_events.len() > 40 {
                        self.registry_events.remove(0);
                    }
                }
            }
            "item.granted" => {
                let item_id = msg
                    .payload
                    .get("itemId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let quantity = msg
                    .payload
                    .get("quantity")
                    .and_then(|v| v.as_u64())
                    .unwrap_or(1);
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "ITEM".to_string(),
                    detail: format!("{item_id} x{quantity}"),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            "save.operation" => {
                let op = msg
                    .payload
                    .get("operation")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown");
                let key = msg
                    .payload
                    .get("key")
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
                let id = msg
                    .payload
                    .get("modId")
                    .or_else(|| msg.payload.get("id")) // fallback for older engine builds
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let name = msg
                    .payload
                    .get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or(&id)
                    .to_string();
                let version = msg
                    .payload
                    .get("version")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?")
                    .to_string();
                let status = msg
                    .payload
                    .get("status")
                    .and_then(|v| v.as_str())
                    .map(normalize_status)
                    .unwrap_or("unknown")
                    .to_string();
                let source = parse_mod_source(msg.payload.get("source").and_then(|v| v.as_str()))
                    .unwrap_or(ModSource::Efpack);
                if let Some(m) = self.mods.iter_mut().find(|m| m.id == id) {
                    m.status = status;
                    m.source = source;
                } else {
                    self.mods.push(ModEntry {
                        id,
                        name,
                        version,
                        status,
                        source,
                    });
                }
            }
            "momi.mod.status" => {
                let id = msg
                    .payload
                    .get("modId")
                    .or_else(|| msg.payload.get("id"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown")
                    .to_string();
                let name = msg
                    .payload
                    .get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or(&id)
                    .to_string();
                let version = msg
                    .payload
                    .get("version")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?")
                    .to_string();
                let status = msg
                    .payload
                    .get("status")
                    .and_then(|v| v.as_str())
                    .map(normalize_status)
                    .unwrap_or("loaded")
                    .to_string();
                if let Some(m) = self.mods.iter_mut().find(|m| m.id == id) {
                    m.status = status;
                    m.source = ModSource::Momi;
                } else {
                    self.mods.push(ModEntry {
                        id,
                        name,
                        version,
                        status,
                        source: ModSource::Momi,
                    });
                }
            }
            "compat.relationship" | "pack.relationship" => {
                let pack_id = msg
                    .payload
                    .get("packId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?")
                    .to_string();
                let momi_mod_id = msg
                    .payload
                    .get("momiModId")
                    .or_else(|| msg.payload.get("modId"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("?")
                    .to_string();
                let relation = msg
                    .payload
                    .get("relationship")
                    .or_else(|| msg.payload.get("kind"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("required")
                    .to_string();
                let status = msg
                    .payload
                    .get("status")
                    .and_then(|v| v.as_str())
                    .map(normalize_relationship_status)
                    .unwrap_or("unknown")
                    .to_string();
                if let Some(existing) = self.relationships.iter_mut().find(|r| {
                    r.pack_id == pack_id && r.momi_mod_id == momi_mod_id && r.relation == relation
                }) {
                    existing.status = status;
                } else {
                    self.relationships.push(RelationshipEntry {
                        pack_id,
                        momi_mod_id,
                        relation,
                        status,
                    });
                }
            }
            "capability.resolved" => {
                if let Some(capability) = msg.payload.get("name").and_then(|v| v.as_str()) {
                    self.capability_events.push(capability.to_string());
                    if self.capability_events.len() > 30 {
                        self.capability_events.remove(0);
                    }
                }
            }
            "capabilities.snapshot" => {
                let mut snapshot = EngineCapabilities::default();
                if let Some(v) = msg.payload.get("eflVersion").and_then(|v| v.as_str()) {
                    snapshot.efl_version = Some(v.to_string());
                }
                if let Some(arr) = msg.payload.get("features").and_then(|v| v.as_array()) {
                    snapshot.features = arr
                        .iter()
                        .filter_map(|v| v.as_str().map(|s| s.to_string()))
                        .collect();
                }
                if let Some(arr) = msg.payload.get("handlers").and_then(|v| v.as_array()) {
                    snapshot.handlers = arr
                        .iter()
                        .filter_map(|v| v.as_str().map(|s| s.to_string()))
                        .collect();
                }
                if let Some(arr) = msg.payload.get("hookKinds").and_then(|v| v.as_array()) {
                    snapshot.hook_kinds = arr
                        .iter()
                        .filter_map(|v| v.as_str().map(|s| s.to_string()))
                        .collect();
                }
                if let Some(map) = msg.payload.get("flags").and_then(|v| v.as_object()) {
                    let mut flags = BTreeMap::new();
                    for (k, v) in map {
                        if let Some(b) = v.as_bool() {
                            flags.insert(k.clone(), b);
                        }
                    }
                    snapshot.flags = flags;
                }
                self.capabilities = Some(snapshot);
            }
            "efl.version" => {
                if let Some(v) = msg.payload.get("version").and_then(|v| v.as_str()) {
                    self.efl_version = v.to_string();
                }
            }
            "story.fired" => {
                let event_id = msg
                    .payload
                    .get("eventId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let status = msg
                    .payload
                    .get("status")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "STORY".to_string(),
                    detail: format!("{event_id} [{status}]"),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            "quest.updated" => {
                let quest_id = msg
                    .payload
                    .get("questId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let action = msg
                    .payload
                    .get("action")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "QUEST".to_string(),
                    detail: format!("{quest_id} {action}"),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            "dialogue.open" => {
                let npc = msg
                    .payload
                    .get("npcId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                let line = msg
                    .payload
                    .get("lineId")
                    .and_then(|v| v.as_str())
                    .unwrap_or("?");
                self.event_log.push(EventLogEntry {
                    timestamp: msg.timestamp.clone(),
                    event_type: "DIALOGUE".to_string(),
                    detail: format!("{npc}: {line}"),
                });
                if self.event_log.len() > 100 {
                    self.event_log.remove(0);
                }
            }
            _ => {}
        }
    }

    pub fn transition_to(&mut self, phase: Phase) {
        self.phase = phase;
    }

    /// Build a merged MOMI view from:
    /// 1) runtime-reported MOMI mods, 2) filesystem index, 3) relationship intent.
    pub fn build_momi_view(
        &self,
        scanned: &[MomiScannedMod],
        projected: &[MomiProjectedRelation],
    ) -> Vec<MomiViewRow> {
        let mut by_id: HashMap<String, MomiViewRow> = HashMap::new();

        for m in self.mods.iter().filter(|m| m.source == ModSource::Momi) {
            by_id.insert(
                m.id.clone(),
                MomiViewRow {
                    id: m.id.clone(),
                    name: m.name.clone(),
                    version: m.version.clone(),
                    presence: MomiPresence::Active,
                    path: None,
                    badges: Vec::new(),
                    edges: Vec::new(),
                },
            );
        }

        for s in scanned {
            if s.id.trim().is_empty() {
                continue;
            }
            match by_id.get_mut(&s.id) {
                Some(row) => {
                    if row.name == row.id {
                        if let Some(name) = &s.name {
                            row.name = name.clone();
                        }
                    }
                    if row.version == "?" {
                        if let Some(version) = &s.version {
                            row.version = version.clone();
                        }
                    }
                    if row.path.is_none() {
                        row.path = s.path.clone();
                    }
                }
                None => {
                    by_id.insert(
                        s.id.clone(),
                        MomiViewRow {
                            id: s.id.clone(),
                            name: s.name.clone().unwrap_or_else(|| s.id.clone()),
                            version: s.version.clone().unwrap_or_else(|| "?".to_string()),
                            presence: MomiPresence::PresentInactive,
                            path: s.path.clone(),
                            badges: Vec::new(),
                            edges: Vec::new(),
                        },
                    );
                }
            }
        }

        for rel in &self.relationships {
            let entry = by_id
                .entry(rel.momi_mod_id.clone())
                .or_insert_with(|| MomiViewRow {
                    id: rel.momi_mod_id.clone(),
                    name: rel.momi_mod_id.clone(),
                    version: "?".to_string(),
                    presence: MomiPresence::Missing,
                    path: None,
                    badges: Vec::new(),
                    edges: Vec::new(),
                });
            push_momi_badge(&mut entry.badges, relation_to_badge(&rel.relation));
            if !entry
                .edges
                .iter()
                .any(|e| e.pack_id == rel.pack_id && e.relation == rel.relation)
            {
                entry.edges.push(MomiEdge {
                    pack_id: rel.pack_id.clone(),
                    relation: rel.relation.clone(),
                    status: rel.status.clone(),
                });
            }
        }

        for rel in projected {
            let entry = by_id
                .entry(rel.momi_mod_id.clone())
                .or_insert_with(|| MomiViewRow {
                    id: rel.momi_mod_id.clone(),
                    name: rel.momi_mod_id.clone(),
                    version: "?".to_string(),
                    presence: MomiPresence::Missing,
                    path: None,
                    badges: Vec::new(),
                    edges: Vec::new(),
                });
            push_momi_badge(&mut entry.badges, relation_to_badge(&rel.relation));
            if !entry
                .edges
                .iter()
                .any(|e| e.pack_id == rel.pack_id && e.relation == rel.relation)
            {
                entry.edges.push(MomiEdge {
                    pack_id: rel.pack_id.clone(),
                    relation: rel.relation.clone(),
                    status: rel.source.clone(),
                });
            }
        }

        let mut rows: Vec<MomiViewRow> = by_id.into_values().collect();
        for row in &mut rows {
            row.badges.sort();
            row.badges.dedup();
            row.edges.sort_by(|a, b| {
                a.pack_id
                    .cmp(&b.pack_id)
                    .then_with(|| a.relation.cmp(&b.relation))
            });
        }
        rows.sort_by(|a, b| a.id.cmp(&b.id));
        rows
    }
}

fn relation_to_badge(relation: &str) -> MomiBadge {
    let normalized = relation.to_ascii_lowercase();
    if normalized.contains("conflict") {
        MomiBadge::Conflict
    } else if normalized.contains("optional") || normalized.contains("compatible") {
        MomiBadge::Compatible
    } else {
        MomiBadge::Needed
    }
}

fn push_momi_badge(badges: &mut Vec<MomiBadge>, badge: MomiBadge) {
    if !badges.contains(&badge) {
        badges.push(badge);
    }
}

fn parse_mod_source(source: Option<&str>) -> Option<ModSource> {
    match source? {
        "efpack" => Some(ModSource::Efpack),
        "momi" => Some(ModSource::Momi),
        _ => Some(ModSource::Unknown),
    }
}

fn normalize_status(status: &str) -> &'static str {
    match status {
        "ok" | "present" | "ready" | "loaded" => "loaded",
        "warn" | "warning" | "degraded" => "warning",
        "missing" | "error" | "failed" => "error",
        _ => "unknown",
    }
}

fn normalize_relationship_status(status: &str) -> &'static str {
    match status {
        "ok" | "loaded" | "present" | "satisfied" => "ok",
        "warning" | "optional-missing" | "degraded" => "warning",
        "missing" | "error" | "conflict" | "blocked" => "error",
        _ => "unknown",
    }
}
