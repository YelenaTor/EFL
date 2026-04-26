use std::{
    collections::{HashMap, HashSet},
    env,
    fs,
    path::{Path, PathBuf},
};

use anyhow::Result;
use serde_json::Value;

const REQUIRED_FIELDS: &[&str] = &["schemaVersion", "modId", "name", "version", "eflVersion"];
const ALLOWED_TOP_LEVEL_FIELDS: &[&str] = &[
    "schemaVersion",
    "modId",
    "name",
    "version",
    "eflVersion",
    "author",
    "description",
    "features",
    "strict",
    "dependencies",
    "capabilities",
    "ipc",
    "scriptHooks",
    "settings",
];
const KNOWN_FEATURES: &[&str] = &[
    "areas",
    "warps",
    "npcs",
    "resources",
    "crafting",
    "quests",
    "dialogue",
    "story",
    "triggers",
    "assets",
    "ipc",
    "calendar",
];
const KNOWN_HANDLERS: &[&str] = &[
    "efl_resource_despawn",
    "efl_resource_spawn",
    "efl_trigger_fire",
    "efl_story_fire",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ValidationProfile {
    Recommended,
    Strict,
    Legacy,
}

impl ValidationProfile {
    pub fn id(self) -> &'static str {
        match self {
            Self::Recommended => "recommended",
            Self::Strict => "strict",
            Self::Legacy => "legacy",
        }
    }
}

/// A manifest validation issue.
#[derive(Debug, Clone)]
pub struct ValidationIssue {
    pub code: String,
    pub severity: String,
    pub message: String,
}

/// Snapshot of what the connected engine actually supports. Validators that
/// receive one prefer it over the static `KNOWN_*` whitelists; if absent
/// (e.g. headless `efl-pack` runs, no engine connected), the static lists
/// remain authoritative.
#[derive(Debug, Clone, Default)]
pub struct EngineCapabilities {
    pub efl_version: Option<String>,
    pub features: Vec<String>,
    pub handlers: Vec<String>,
    pub hook_kinds: Vec<String>,
    pub flags: std::collections::BTreeMap<String, bool>,
}

impl EngineCapabilities {
    pub fn is_meaningful(&self) -> bool {
        !self.features.is_empty() || !self.handlers.is_empty()
    }
}

/// Backwards-compatible entrypoint without a capability snapshot.
pub fn validate_manifest_with_profile(
    manifest_path: &Path,
    profile: ValidationProfile,
) -> Result<Vec<ValidationIssue>> {
    validate_manifest_with_capabilities(manifest_path, profile, None)
}

/// Validate a manifest against the chosen profile. When `capabilities` is
/// provided, it overrides the static handler/feature whitelists so warnings
/// reflect the actually-running engine instead of the DevKit build's
/// embedded snapshot.
pub fn validate_manifest_with_capabilities(
    manifest_path: &Path,
    profile: ValidationProfile,
    capabilities: Option<&EngineCapabilities>,
) -> Result<Vec<ValidationIssue>> {
    let mut issues = Vec::new();

    let bytes = match fs::read(manifest_path) {
        Ok(b) => b,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: format!("Cannot read manifest.efl: {e}"),
            });
            return Ok(issues);
        }
    };

    let json: Value = match serde_json::from_slice(&bytes) {
        Ok(v) => v,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: format!("Invalid JSON: {e}"),
            });
            return Ok(issues);
        }
    };

    let Some(root) = json.as_object() else {
        issues.push(ValidationIssue {
            code: "MANIFEST-E010".into(),
            severity: "error".into(),
            message: "Manifest root must be a JSON object".into(),
        });
        return Ok(issues);
    };

    for field in REQUIRED_FIELDS {
        match json.get(field) {
            None => issues.push(ValidationIssue {
                code: "MANIFEST-E011".into(),
                severity: "error".into(),
                message: format!("Missing required field \"{field}\""),
            }),
            Some(v)
                if v.is_null() || (v.is_string() && v.as_str().unwrap_or("").trim().is_empty()) =>
            {
                issues.push(ValidationIssue {
                    code: "MANIFEST-E011".into(),
                    severity: "error".into(),
                    message: format!("Required field \"{field}\" is empty"),
                });
            }
            _ => {}
        }
    }

    // schemaVersion must be an integer
    if let Some(sv) = json.get("schemaVersion") {
        if !sv.is_number() || sv.as_f64().map(|f| f.fract() != 0.0).unwrap_or(false) {
            issues.push(ValidationIssue {
                code: "MANIFEST-E012".into(),
                severity: "error".into(),
                message: "Field \"schemaVersion\" must be an integer".into(),
            });
        } else if sv.as_i64().unwrap_or_default() <= 0 {
            issues.push(ValidationIssue {
                code: "MANIFEST-E012".into(),
                severity: "error".into(),
                message: "Field \"schemaVersion\" must be a positive integer".into(),
            });
        }
    }

    // version should look like semver
    if let Some(v) = json.get("version").and_then(|v| v.as_str()) {
        if !looks_like_semver(v) {
            issues.push(ValidationIssue {
                code: "MANIFEST-W001".into(),
                severity: "warning".into(),
                message: format!(
                    "Field \"version\" \"{v}\" does not look like semver (expected x.y.z)"
                ),
            });
        }
    }

    if let Some(mod_id) = json.get("modId").and_then(|v| v.as_str()) {
        if !looks_like_namespaced_mod_id(mod_id) {
            issues.push(ValidationIssue {
                code: "MANIFEST-W010".into(),
                severity: "warning".into(),
                message: format!(
                    "Field \"modId\" \"{mod_id}\" should be namespaced (example: com.author.pack)"
                ),
            });
        }
    }

    if let Some(features) = json.get("features").and_then(|v| v.as_array()) {
        let live_features: Option<&[String]> = capabilities
            .and_then(|c| (!c.features.is_empty()).then_some(c.features.as_slice()));
        for feature in features {
            if let Some(feature_name) = feature.as_str() {
                let is_known = match live_features {
                    Some(live) => live.iter().any(|f| f == feature_name),
                    None => KNOWN_FEATURES.contains(&feature_name),
                };
                if !is_known {
                    let severity = compatibility_severity(profile);
                    let known_list: String = match live_features {
                        Some(live) => live.join(", "),
                        None => KNOWN_FEATURES.join(", "),
                    };
                    let suffix = if live_features.is_some() {
                        " (per connected engine snapshot)"
                    } else {
                        ""
                    };
                    issues.push(ValidationIssue {
                        code: "MANIFEST-W011".into(),
                        severity: severity.into(),
                        message: format!(
                            "Unknown feature \"{feature_name}\"{suffix}. Known features: {known_list}"
                        ),
                    });
                }
            } else {
                issues.push(ValidationIssue {
                    code: "MANIFEST-E013".into(),
                    severity: "error".into(),
                    message: "Field \"features\" must only contain string values".into(),
                });
            }
        }
    } else if json.get("features").is_some() {
        issues.push(ValidationIssue {
            code: "MANIFEST-E013".into(),
            severity: "error".into(),
            message: "Field \"features\" must be an array when present".into(),
        });
    }
    validate_ipc_contract(&json, profile, &mut issues);
    validate_script_hooks(&json, profile, capabilities, &mut issues);
    validate_trigger_reachability(manifest_path, profile, &mut issues)?;
    validate_cross_pack_graph(manifest_path, profile, &json, &mut issues)?;
    validate_content_graph(manifest_path, profile, &mut issues)?;

    for key in root.keys() {
        if !ALLOWED_TOP_LEVEL_FIELDS.contains(&key.as_str()) {
            let severity = compatibility_severity(profile);
            issues.push(ValidationIssue {
                code: "MANIFEST-W012".into(),
                severity: severity.into(),
                message: format!(
                    "Unknown top-level field \"{key}\". This may be ignored by current tooling."
                ),
            });
        }
    }

    let localization_path = manifest_path
        .parent()
        .map(|p| p.join("localisation"))
        .filter(|p| p.exists());
    if localization_path.is_some() {
        issues.push(ValidationIssue {
            code: "MANIFEST-W013".into(),
            severity: compatibility_severity(profile).into(),
            message: "Found a local \"localisation/\" folder. Localization text delivery belongs to MOMI pre-launch; keep only runtime keys in EFL content.".into(),
        });
    }

    Ok(issues)
}

pub fn validate_dat_with_profile(
    manifest_path: &Path,
    profile: ValidationProfile,
) -> Result<Vec<ValidationIssue>> {
    let mut issues = Vec::new();
    let bytes = match fs::read(manifest_path) {
        Ok(b) => b,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "DAT-E001".into(),
                severity: "error".into(),
                message: format!("Cannot read manifest.efdat: {e}"),
            });
            return Ok(issues);
        }
    };
    let json: Value = match serde_json::from_slice(&bytes) {
        Ok(v) => v,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "DAT-E001".into(),
                severity: "error".into(),
                message: format!("Invalid JSON: {e}"),
            });
            return Ok(issues);
        }
    };
    let Some(root) = json.as_object() else {
        issues.push(ValidationIssue {
            code: "DAT-E010".into(),
            severity: "error".into(),
            message: "manifest.efdat root must be a JSON object".into(),
        });
        return Ok(issues);
    };

    for field in [
        "schemaVersion",
        "datId",
        "name",
        "version",
        "eflVersion",
        "relationships",
    ] {
        if !root.contains_key(field) {
            issues.push(ValidationIssue {
                code: "DAT-E011".into(),
                severity: "error".into(),
                message: format!("Missing required field \"{field}\""),
            });
        }
    }
    if json.get("schemaVersion").and_then(|v| v.as_i64()) != Some(1) {
        issues.push(ValidationIssue {
            code: "DAT-E012".into(),
            severity: "error".into(),
            message: "schemaVersion must be 1 for .efdat".into(),
        });
    }
    if let Some(dat_id) = json.get("datId").and_then(|v| v.as_str()) {
        if !looks_like_namespaced_mod_id(dat_id) {
            issues.push(ValidationIssue {
                code: "DAT-W010".into(),
                severity: "warning".into(),
                message: format!("datId \"{dat_id}\" should use reverse-domain style"),
            });
        }
    }
    let momi_inventory = scan_momi_inventory_for_dat(manifest_path);
    if momi_inventory.is_none() {
        issues.push(ValidationIssue {
            code: "DAT-W021".into(),
            severity: compatibility_severity(profile).into(),
            message: "No MOMI inventory source available (runtime snapshot or mods folder index). MOMI relationship enforcement skipped for this run.".into(),
        });
    }

    if let Some(rel) = json.get("relationships").and_then(|v| v.as_array()) {
        if rel.is_empty() {
            issues.push(ValidationIssue {
                code: "DAT-E013".into(),
                severity: "error".into(),
                message: "relationships must include at least one entry".into(),
            });
        }
        for (idx, entry) in rel.iter().enumerate() {
            let Some(obj) = entry.as_object() else {
                issues.push(ValidationIssue {
                    code: "DAT-E014".into(),
                    severity: "error".into(),
                    message: format!("relationships[{idx}] must be an object"),
                });
                continue;
            };
            let rel_type = obj.get("type").and_then(|v| v.as_str()).unwrap_or("");
            if !["requires", "optional", "conflicts"].contains(&rel_type) {
                issues.push(ValidationIssue {
                    code: "DAT-E015".into(),
                    severity: "error".into(),
                    message: format!(
                        "relationships[{idx}].type must be requires|optional|conflicts"
                    ),
                });
            }
            let target = obj.get("target").and_then(|v| v.as_object());
            if target.is_none() {
                issues.push(ValidationIssue {
                    code: "DAT-E016".into(),
                    severity: "error".into(),
                    message: format!("relationships[{idx}] is missing target object"),
                });
                continue;
            }
            let target = target.unwrap();
            let kind = target.get("kind").and_then(|v| v.as_str()).unwrap_or("");
            if !["efpack", "momi"].contains(&kind) {
                issues.push(ValidationIssue {
                    code: "DAT-E017".into(),
                    severity: "error".into(),
                    message: format!("relationships[{idx}].target.kind must be efpack|momi"),
                });
            }
            if target
                .get("id")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .is_empty()
            {
                issues.push(ValidationIssue {
                    code: "DAT-E018".into(),
                    severity: "error".into(),
                    message: format!("relationships[{idx}].target.id must be a non-empty string"),
                });
            }
            if rel_type != "conflicts"
                && target
                    .get("versionRange")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .is_empty()
            {
                issues.push(ValidationIssue {
                    code: "DAT-W011".into(),
                    severity: compatibility_severity(profile).into(),
                    message: format!(
                        "relationships[{idx}] should define target.versionRange for {rel_type}"
                    ),
                });
            }

            // Minimal MOMI enforcement slice (V2.5 closeout):
            // - requires momi target must be present in inventory
            // - conflicts momi target must not be present
            // - optional momi target remains advisory
            if kind == "momi" {
                if let Some(target_id) = target.get("id").and_then(|v| v.as_str()) {
                    if let Some(inventory) = momi_inventory.as_ref() {
                        let present = inventory.contains(target_id);
                        match rel_type {
                            "requires" if !present => issues.push(ValidationIssue {
                                code: "DAT-E020".into(),
                                severity: "error".into(),
                                message: format!(
                                    "relationships[{idx}] requires MOMI mod \"{target_id}\" but it is not active/present in the detected inventory"
                                ),
                            }),
                            "conflicts" if present => issues.push(ValidationIssue {
                                code: "DAT-E021".into(),
                                severity: "error".into(),
                                message: format!(
                                    "relationships[{idx}] conflicts with MOMI mod \"{target_id}\" but it is active/present in the detected inventory"
                                ),
                            }),
                            "optional" if !present => issues.push(ValidationIssue {
                                code: "DAT-W020".into(),
                                severity: compatibility_severity(profile).into(),
                                message: format!(
                                    "relationships[{idx}] optional MOMI mod \"{target_id}\" is not active/present (compatibility extension will stay disabled)"
                                ),
                            }),
                            _ => {}
                        }
                    }
                }
            }
        }
    }
    Ok(issues)
}

fn scan_momi_inventory_for_dat(manifest_path: &Path) -> Option<HashSet<String>> {
    let pack_root = manifest_path.parent()?;
    let projects_root = pack_root.parent()?;
    let mut roots: Vec<PathBuf> = Vec::new();

    if let Ok(path) = env::var("MOMI_MODS_DIR") {
        let p = PathBuf::from(path);
        if p.exists() {
            roots.push(p);
        }
    }

    let candidates = [
        projects_root.join("mods"),
        projects_root.join("MOMI").join("mods"),
        projects_root
            .parent()
            .map(|p| p.join("mods"))
            .unwrap_or_else(|| projects_root.join("mods")),
    ];
    for c in candidates {
        if c.exists() && !roots.iter().any(|r| r == &c) {
            roots.push(c);
        }
    }

    if roots.is_empty() {
        return None;
    }

    let mut ids = HashSet::new();
    for root in roots {
        let Ok(entries) = fs::read_dir(root) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_dir() {
                continue;
            }
            let mut id = path
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or_default()
                .to_string();
            let manifest = path.join("manifest.json");
            if manifest.exists() {
                if let Ok(bytes) = fs::read(&manifest) {
                    if let Ok(json) = serde_json::from_slice::<Value>(&bytes) {
                        if let Some(v) = json
                            .get("modId")
                            .or_else(|| json.get("id"))
                            .and_then(|v| v.as_str())
                        {
                            id = v.to_string();
                        }
                    }
                }
            }
            if !id.trim().is_empty() {
                ids.insert(id);
            }
        }
    }

    Some(ids)
}

fn validate_ipc_contract(
    json: &Value,
    profile: ValidationProfile,
    issues: &mut Vec<ValidationIssue>,
) {
    let Some(mod_id) = json.get("modId").and_then(|v| v.as_str()) else {
        return;
    };
    let Some(ipc) = json.get("ipc").and_then(|v| v.as_object()) else {
        return;
    };
    let publish = ipc
        .get("publish")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();
    let consume = ipc
        .get("consume")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();

    let mut publish_ids = HashSet::new();
    for channel in publish {
        let Some(channel_id) = channel.as_str() else {
            issues.push(ValidationIssue {
                code: "IPC-E010".into(),
                severity: "error".into(),
                message: "ipc.publish must contain only string channel IDs".into(),
            });
            continue;
        };
        if !channel_id.starts_with(&format!("{mod_id}:")) {
            issues.push(ValidationIssue {
                code: "IPC-E011".into(),
                severity: profile_strictness(profile, "error", "warning").into(),
                message: format!(
                    "publish channel \"{channel_id}\" is not owned by this pack (expected prefix \"{mod_id}:\")"
                ),
            });
        }
        if !publish_ids.insert(channel_id.to_string()) {
            issues.push(ValidationIssue {
                code: "IPC-W010".into(),
                severity: "warning".into(),
                message: format!("duplicate publish channel \"{channel_id}\""),
            });
        }
    }

    let mut consume_ids = HashSet::new();
    for channel in consume {
        let Some(channel_id) = channel.as_str() else {
            issues.push(ValidationIssue {
                code: "IPC-E010".into(),
                severity: "error".into(),
                message: "ipc.consume must contain only string channel IDs".into(),
            });
            continue;
        };
        if !consume_ids.insert(channel_id.to_string()) {
            issues.push(ValidationIssue {
                code: "IPC-W011".into(),
                severity: "warning".into(),
                message: format!("duplicate consume channel \"{channel_id}\""),
            });
        }
        if publish_ids.contains(channel_id) {
            issues.push(ValidationIssue {
                code: "IPC-W012".into(),
                severity: "warning".into(),
                message: format!(
                    "channel \"{channel_id}\" appears in both publish and consume; verify this is intentional"
                ),
            });
        }
    }
}

fn validate_script_hooks(
    json: &Value,
    profile: ValidationProfile,
    capabilities: Option<&EngineCapabilities>,
    issues: &mut Vec<ValidationIssue>,
) {
    let Some(hooks) = json.get("scriptHooks").and_then(|v| v.as_array()) else {
        return;
    };
    let live_handlers: Option<&[String]> = capabilities
        .and_then(|c| (!c.handlers.is_empty()).then_some(c.handlers.as_slice()));
    for hook in hooks {
        let Some(obj) = hook.as_object() else {
            issues.push(ValidationIssue {
                code: "HOOK-E010".into(),
                severity: "error".into(),
                message: "scriptHooks entries must be objects".into(),
            });
            continue;
        };
        let target = obj.get("target").and_then(|v| v.as_str()).unwrap_or("");
        let handler = obj.get("handler").and_then(|v| v.as_str()).unwrap_or("");
        let mode = obj
            .get("mode")
            .and_then(|v| v.as_str())
            .unwrap_or("callback");

        if !target.starts_with("gml_") {
            issues.push(ValidationIssue {
                code: "HOOK-W010".into(),
                severity: profile_strictness(profile, "error", "warning").into(),
                message: format!(
                    "scriptHooks target \"{target}\" does not look like a GameMaker script symbol (expected gml_*)"
                ),
            });
        }
        let handler_known = match live_handlers {
            Some(live) => live.iter().any(|h| h == handler),
            None => KNOWN_HANDLERS.contains(&handler),
        };
        if !handler_known {
            let suffix = if live_handlers.is_some() {
                " (per connected engine snapshot)"
            } else {
                ""
            };
            issues.push(ValidationIssue {
                code: "HOOK-W004".into(),
                severity: profile_strictness(profile, "error", "warning").into(),
                message: format!(
                    "scriptHooks handler \"{handler}\" is not known by current runtime hook model{suffix}"
                ),
            });
        }
        if mode == "inject" {
            issues.push(ValidationIssue {
                code: "HOOK-W002".into(),
                severity: compatibility_severity(profile).into(),
                message:
                    "scriptHooks mode \"inject\" is reserved/future; use callback handlers for now"
                        .into(),
            });
        }
    }
}

fn validate_trigger_reachability(
    manifest_path: &Path,
    profile: ValidationProfile,
    issues: &mut Vec<ValidationIssue>,
) -> Result<()> {
    let Some(pack_root) = manifest_path.parent() else {
        return Ok(());
    };
    let triggers = load_ids(pack_root.join("triggers"), "id")?;
    if triggers.is_empty() {
        return Ok(());
    }

    let mut referenced = HashSet::new();
    collect_trigger_refs(pack_root.join("areas"), "unlockTrigger", &mut referenced)?;
    collect_trigger_refs(pack_root.join("warps"), "requireTrigger", &mut referenced)?;
    collect_trigger_refs(pack_root.join("quests"), "requireTrigger", &mut referenced)?;
    collect_trigger_refs(pack_root.join("events"), "trigger", &mut referenced)?;
    collect_trigger_refs(
        pack_root.join("dialogue"),
        "requireTrigger",
        &mut referenced,
    )?;

    for reference in &referenced {
        if !triggers.contains(reference) {
            issues.push(ValidationIssue {
                code: "TRIGGER-E010".into(),
                severity: "error".into(),
                message: format!("trigger reference \"{reference}\" is not defined in triggers/"),
            });
        }
    }

    for trigger in triggers {
        if !referenced.contains(&trigger) {
            issues.push(ValidationIssue {
                code: "TRIGGER-W010".into(),
                severity: compatibility_severity(profile).into(),
                message: format!("trigger \"{trigger}\" is never referenced by pack content"),
            });
        }
    }
    Ok(())
}

/// Cross-family reference and ID-uniqueness validation across the assembled
/// content graph for a single pack.
///
/// Checks:
/// - ID uniqueness within each family (areas, warps, npcs, world_npcs,
///   resources, recipes, quests, dialogue, events).
/// - References resolve to defined IDs:
///   - warps `sourceArea` / `targetArea` -> areas
///   - LocalNpc `areaId` -> areas
///   - WorldNpc `defaultAreaId` and `schedule[*].areaId` -> areas
///   - dialogue `npc` -> npcs
///   - resource `spawnRules.areas` and `spawnRules.anchors` keys -> areas
///   - area `entryEvent` / `exitEvent` -> events
///   - cutscene event `onFire.startQuest` / `onFire.advanceQuest` -> quests
///
/// Severity rules:
/// - duplicate / missing IDs within a family are always errors,
/// - LocalNpc `areaId` is pack-internal so unresolved values are errors,
/// - other cross-family references that may target base-game content
///   (warps, world NPC areas, dialogue NPC, resource spawn areas, area
///   entry/exit events, story events -> quests) are surfaced as hazards
///   (`-H0xx`), warnings under recommended/legacy and errors under strict.
fn validate_content_graph(
    manifest_path: &Path,
    profile: ValidationProfile,
    issues: &mut Vec<ValidationIssue>,
) -> Result<()> {
    let Some(pack_root) = manifest_path.parent() else {
        return Ok(());
    };

    let area_objects = load_json_objects(pack_root.join("areas"))?;
    let warp_objects = load_json_objects(pack_root.join("warps"))?;
    let npc_objects = load_json_objects(pack_root.join("npcs"))?;
    let world_npc_objects = load_json_objects(pack_root.join("world_npcs"))?;
    let resource_objects = load_json_objects(pack_root.join("resources"))?;
    let recipe_objects = load_json_objects(pack_root.join("recipes"))?;
    let quest_objects = load_json_objects(pack_root.join("quests"))?;
    let dialogue_objects = load_json_objects(pack_root.join("dialogue"))?;
    let event_objects = load_json_objects(pack_root.join("events"))?;
    let calendar_objects = load_json_objects(pack_root.join("calendar"))?;

    let area_ids = check_id_uniqueness(&area_objects, "id", "AREA", "area", issues);
    let _warp_ids = check_id_uniqueness(&warp_objects, "id", "WARP", "warp", issues);
    let npc_ids = check_id_uniqueness(&npc_objects, "id", "NPC", "npc", issues);
    let world_npc_ids =
        check_id_uniqueness(&world_npc_objects, "id", "NPC", "worldNpc", issues);
    let _resource_ids =
        check_id_uniqueness(&resource_objects, "id", "RESOURCE", "resource", issues);
    let _recipe_ids = check_id_uniqueness(&recipe_objects, "id", "CRAFT", "recipe", issues);
    let quest_ids = check_id_uniqueness(&quest_objects, "id", "QUEST", "quest", issues);
    let _dialogue_ids =
        check_id_uniqueness(&dialogue_objects, "id", "DIALOGUE", "dialogue", issues);
    let event_ids = check_id_uniqueness(&event_objects, "id", "STORY", "event", issues);
    let _calendar_ids =
        check_id_uniqueness(&calendar_objects, "id", "CALENDAR", "calendar event", issues);

    // npc + world_npc share a logical namespace because dialogue can target
    // either flavor without distinguishing them.
    let mut all_npc_ids = npc_ids.clone();
    all_npc_ids.extend(world_npc_ids.iter().cloned());

    // External-reference severity. Cross-references can legitimately point at
    // base-game content (e.g. a warp's `sourceArea = "town"`), so we surface
    // unresolved targets as hazards under recommended/legacy and escalate to
    // hard errors only under strict.
    let ext_sev = profile_strictness(profile, "error", "warning");

    // ── Warps -> areas ───────────────────────────────────────────────────────
    for warp in &warp_objects {
        let id = warp.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        if let Some(source) = warp.get("sourceArea").and_then(|v| v.as_str()) {
            if !source.is_empty() && !area_ids.contains(source) {
                issues.push(ValidationIssue {
                    code: "WARP-H010".into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "warp \"{id}\" sourceArea \"{source}\" is not defined in this pack \
                         (ok if it targets a base-game area, otherwise check the id)"
                    ),
                });
            }
        }
        if let Some(target) = warp.get("targetArea").and_then(|v| v.as_str()) {
            if !target.is_empty() && !area_ids.contains(target) {
                issues.push(ValidationIssue {
                    code: "WARP-H011".into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "warp \"{id}\" targetArea \"{target}\" is not defined in this pack \
                         (ok if it targets a base-game area, otherwise check the id)"
                    ),
                });
            }
        }
    }

    // ── LocalNpc -> areas ────────────────────────────────────────────────────
    // LocalNpcs only live in pack-defined areas, so unresolved areaId is a
    // pack-internal error.
    for npc in &npc_objects {
        let id = npc.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        if let Some(area) = npc.get("areaId").and_then(|v| v.as_str()) {
            if !area.is_empty() && !area_ids.contains(area) {
                issues.push(ValidationIssue {
                    code: "NPC-E020".into(),
                    severity: "error".into(),
                    message: format!(
                        "localNpc \"{id}\" areaId \"{area}\" is not a defined pack area"
                    ),
                });
            }
        }
    }

    // ── WorldNpc -> areas (default + schedule) ───────────────────────────────
    // WorldNpcs may also walk between base-game rooms, so unresolved area refs
    // are hazards.
    for npc in &world_npc_objects {
        let id = npc.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        if let Some(area) = npc.get("defaultAreaId").and_then(|v| v.as_str()) {
            if !area.is_empty() && !area_ids.contains(area) {
                issues.push(ValidationIssue {
                    code: "NPC-H021".into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "worldNpc \"{id}\" defaultAreaId \"{area}\" is not defined in this pack"
                    ),
                });
            }
        }
        if let Some(schedule) = npc.get("schedule").and_then(|v| v.as_array()) {
            for (idx, entry) in schedule.iter().enumerate() {
                let area = entry.get("areaId").and_then(|v| v.as_str()).unwrap_or("");
                if area.is_empty() {
                    continue;
                }
                if !area_ids.contains(area) {
                    issues.push(ValidationIssue {
                        code: "NPC-H022".into(),
                        severity: ext_sev.into(),
                        message: format!(
                            "worldNpc \"{id}\" schedule[{idx}] areaId \"{area}\" is not defined in this pack"
                        ),
                    });
                }
            }
        }
    }

    // ── Dialogue -> npc ──────────────────────────────────────────────────────
    // Dialogue can attach to base-game NPCs (e.g. `ari`), so unresolved npc
    // refs are hazards.
    for dialogue in &dialogue_objects {
        let id = dialogue.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        if let Some(npc) = dialogue.get("npc").and_then(|v| v.as_str()) {
            if !npc.is_empty() && !all_npc_ids.contains(npc) {
                issues.push(ValidationIssue {
                    code: "DIALOGUE-H010".into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "dialogue \"{id}\" references npc \"{npc}\" which is not defined in this pack \
                         (ok if attaching to a base-game npc, otherwise check the id)"
                    ),
                });
            }
        }
        if let Some(entries) = dialogue.get("entries").and_then(|v| v.as_array()) {
            let mut seen = HashSet::new();
            for entry in entries {
                let entry_id = entry.get("id").and_then(|v| v.as_str()).unwrap_or("");
                if entry_id.is_empty() {
                    continue;
                }
                if !seen.insert(entry_id.to_string()) {
                    issues.push(ValidationIssue {
                        code: "DIALOGUE-E011".into(),
                        severity: "error".into(),
                        message: format!(
                            "dialogue \"{id}\" entry id \"{entry_id}\" appears more than once"
                        ),
                    });
                }
            }
        }
    }

    // ── Resources -> areas (spawnRules) ──────────────────────────────────────
    // Resources can be configured to spawn in base-game rooms; treat unknown
    // refs as hazards.
    for resource in &resource_objects {
        let id = resource.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        let Some(spawn_rules) = resource.get("spawnRules") else {
            continue;
        };
        if let Some(areas) = spawn_rules.get("areas").and_then(|v| v.as_array()) {
            for area_value in areas {
                let Some(area) = area_value.as_str() else {
                    continue;
                };
                if !area_ids.contains(area) {
                    issues.push(ValidationIssue {
                        code: "RESOURCE-H010".into(),
                        severity: ext_sev.into(),
                        message: format!(
                            "resource \"{id}\" spawnRules.areas references area \"{area}\" \
                             which is not defined in this pack"
                        ),
                    });
                }
            }
        }
        if let Some(anchors) = spawn_rules.get("anchors").and_then(|v| v.as_object()) {
            for area in anchors.keys() {
                if !area_ids.contains(area) {
                    issues.push(ValidationIssue {
                        code: "RESOURCE-H011".into(),
                        severity: ext_sev.into(),
                        message: format!(
                            "resource \"{id}\" spawnRules.anchors references area \"{area}\" \
                             which is not defined in this pack"
                        ),
                    });
                }
            }
        }
    }

    // ── Areas -> events ──────────────────────────────────────────────────────
    // entry/exitEvent typically point at pack cutscenes, but base-game events
    // are also valid targets, so these stay as hazards.
    for area in &area_objects {
        let id = area.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        for (field, code) in [("entryEvent", "AREA-H020"), ("exitEvent", "AREA-H021")] {
            let Some(target) = area.get(field).and_then(|v| v.as_str()) else {
                continue;
            };
            if target.is_empty() {
                continue;
            }
            if !event_ids.contains(target) {
                issues.push(ValidationIssue {
                    code: code.into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "area \"{id}\" {field} \"{target}\" is not defined in this pack"
                    ),
                });
            }
        }
    }

    // ── Events.onFire -> quests ──────────────────────────────────────────────
    // Events may legitimately advance base-game quests, so unresolved quest
    // refs are hazards.
    for event in &event_objects {
        let id = event.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        let Some(on_fire) = event.get("onFire").and_then(|v| v.as_object()) else {
            continue;
        };
        for (field, code) in [
            ("startQuest", "STORY-H020"),
            ("advanceQuest", "STORY-H021"),
        ] {
            let Some(target) = on_fire.get(field).and_then(|v| v.as_str()) else {
                continue;
            };
            if target.is_empty() {
                continue;
            }
            if !quest_ids.contains(target) {
                issues.push(ValidationIssue {
                    code: code.into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "event \"{id}\" onFire.{field} \"{target}\" is not defined in this pack"
                    ),
                });
            }
        }
    }

    // ── Quest stage uniqueness ───────────────────────────────────────────────
    for quest in &quest_objects {
        let id = quest.get("id").and_then(|v| v.as_str()).unwrap_or("?");
        let Some(stages) = quest.get("stages").and_then(|v| v.as_array()) else {
            continue;
        };
        let mut stage_ids = HashSet::new();
        for stage in stages {
            let stage_id = stage.get("id").and_then(|v| v.as_str()).unwrap_or("");
            if stage_id.is_empty() {
                issues.push(ValidationIssue {
                    code: "QUEST-E010".into(),
                    severity: "error".into(),
                    message: format!("quest \"{id}\" has a stage missing required \"id\""),
                });
                continue;
            }
            if !stage_ids.insert(stage_id.to_string()) {
                issues.push(ValidationIssue {
                    code: "QUEST-E011".into(),
                    severity: "error".into(),
                    message: format!(
                        "quest \"{id}\" has duplicate stage id \"{stage_id}\""
                    ),
                });
            }
        }
        if stages.is_empty() {
            issues.push(ValidationIssue {
                code: "QUEST-W010".into(),
                severity: profile_strictness(profile, "error", "warning").into(),
                message: format!("quest \"{id}\" has no stages defined"),
            });
        }
    }

    // ── Calendar events (V3 pilot) ───────────────────────────────────────────
    // - season must be a recognised string ("spring"/"summer"/"fall"/"winter"/
    //   "any") or 0..=3 if given as an integer.
    // - dayOfSeason must be in 1..=28 when present.
    // - onActivate may legitimately point at a base-game story event, so
    //   unresolved targets are surfaced as hazards.
    const KNOWN_SEASONS: &[&str] = &["spring", "summer", "fall", "winter", "any"];

    for event in &calendar_objects {
        let id = event.get("id").and_then(|v| v.as_str()).unwrap_or("?");

        if let Some(season) = event.get("season") {
            let bad = if let Some(s) = season.as_str() {
                !KNOWN_SEASONS.contains(&s)
            } else if let Some(n) = season.as_i64() {
                !(0..=3).contains(&n)
            } else {
                true
            };
            if bad {
                issues.push(ValidationIssue {
                    code: "CALENDAR-E010".into(),
                    severity: "error".into(),
                    message: format!(
                        "calendar event \"{id}\" season is invalid (expected one of \
                         spring/summer/fall/winter/any or 0..=3)"
                    ),
                });
            }
        }

        if let Some(day) = event.get("dayOfSeason").and_then(|v| v.as_i64()) {
            if !(1..=28).contains(&day) {
                issues.push(ValidationIssue {
                    code: "CALENDAR-E011".into(),
                    severity: "error".into(),
                    message: format!(
                        "calendar event \"{id}\" dayOfSeason {day} is out of range (1..=28)"
                    ),
                });
            }
        }

        if let Some(lifecycle) = event.get("lifecycle").and_then(|v| v.as_str()) {
            if lifecycle != "daily" && lifecycle != "once" {
                issues.push(ValidationIssue {
                    code: "CALENDAR-E012".into(),
                    severity: "error".into(),
                    message: format!(
                        "calendar event \"{id}\" lifecycle \"{lifecycle}\" must be \"daily\" or \"once\""
                    ),
                });
            }
        }

        if let Some(target) = event.get("onActivate").and_then(|v| v.as_str()) {
            if !target.is_empty() && !event_ids.contains(target) {
                issues.push(ValidationIssue {
                    code: "CALENDAR-H020".into(),
                    severity: ext_sev.into(),
                    message: format!(
                        "calendar event \"{id}\" onActivate \"{target}\" is not a story event \
                         defined in this pack (ok if it targets a base-game cutscene)"
                    ),
                });
            }
        }
    }

    Ok(())
}

/// Walks a list of JSON objects, collects ID values from `key`, and emits
/// `<CATEGORY>-E030` (missing id) / `<CATEGORY>-E031` (duplicate id) issues.
/// The 030/031 numbering keeps these graph diagnostics clear of the
/// runtime-side parse-error codes (typically `<CATEGORY>-E001`).
/// Returns the resolved set of IDs for downstream cross-reference checks.
fn check_id_uniqueness(
    objects: &[Value],
    key: &str,
    category: &str,
    label: &str,
    issues: &mut Vec<ValidationIssue>,
) -> HashSet<String> {
    let mut seen: HashSet<String> = HashSet::new();
    let mut duplicates: HashSet<String> = HashSet::new();

    for obj in objects {
        let id = obj.get(key).and_then(|v| v.as_str()).unwrap_or("");
        if id.is_empty() {
            issues.push(ValidationIssue {
                code: format!("{category}-E030"),
                severity: "error".into(),
                message: format!("{label} entry is missing required \"{key}\""),
            });
            continue;
        }
        if !seen.insert(id.to_string()) {
            duplicates.insert(id.to_string());
        }
    }

    for duplicate in &duplicates {
        issues.push(ValidationIssue {
            code: format!("{category}-E031"),
            severity: "error".into(),
            message: format!("duplicate {label} id \"{duplicate}\""),
        });
    }

    seen
}

fn validate_cross_pack_graph(
    manifest_path: &Path,
    profile: ValidationProfile,
    json: &Value,
    issues: &mut Vec<ValidationIssue>,
) -> Result<()> {
    let Some(pack_root) = manifest_path.parent() else {
        return Ok(());
    };
    let Some(projects_root) = pack_root.parent() else {
        return Ok(());
    };
    let Some(my_mod_id) = json.get("modId").and_then(|v| v.as_str()) else {
        return Ok(());
    };

    let available = load_workspace_manifests(projects_root)?;
    let duplicates = duplicate_workspace_mod_ids(projects_root)?;
    if duplicates.contains(my_mod_id) {
        issues.push(ValidationIssue {
            code: "MANIFEST-E020".into(),
            severity: "error".into(),
            message: format!("duplicate modId \"{my_mod_id}\" exists in this workspace"),
        });
    }

    let deps = json.get("dependencies").and_then(|v| v.as_object());
    if deps.is_none() {
        return Ok(());
    }
    let deps = deps.unwrap();

    for required in dependency_ids(deps.get("required")) {
        if required.starts_with("momi.") {
            issues.push(ValidationIssue {
                code: "MANIFEST-W030".into(),
                severity: compatibility_severity(profile).into(),
                message: format!(
                    "required dependency \"{required}\" looks like a MOMI mod ID; prefer declaring MOMI relationships via .efdat compatibility artifacts"
                ),
            });
        }
        if !available.contains_key(&required) {
            issues.push(ValidationIssue {
                code: "MANIFEST-E003".into(),
                severity: "error".into(),
                message: format!(
                    "required dependency \"{required}\" was not found among workspace packs"
                ),
            });
        }
        if required == my_mod_id {
            issues.push(ValidationIssue {
                code: "MANIFEST-E021".into(),
                severity: "error".into(),
                message: "pack cannot require itself".into(),
            });
        }
    }
    for optional in dependency_ids(deps.get("optional")) {
        if optional.starts_with("momi.") {
            issues.push(ValidationIssue {
                code: "MANIFEST-W031".into(),
                severity: compatibility_severity(profile).into(),
                message: format!(
                    "optional dependency \"{optional}\" looks like a MOMI mod ID; prefer .efdat optional relationships for MOMI compatibility"
                ),
            });
        }
        if !available.contains_key(&optional) {
            issues.push(ValidationIssue {
                code: "MANIFEST-W020".into(),
                severity: compatibility_severity(profile).into(),
                message: format!(
                    "optional dependency \"{optional}\" not found in workspace (feature may degrade)"
                ),
            });
        }
    }
    for conflict in dependency_ids(deps.get("conflicts")) {
        if conflict.starts_with("momi.") {
            issues.push(ValidationIssue {
                code: "MANIFEST-W032".into(),
                severity: compatibility_severity(profile).into(),
                message: format!(
                    "conflict dependency \"{conflict}\" looks like MOMI scope; represent MOMI conflicts in .efdat relationship shims"
                ),
            });
        }
        if available.contains_key(&conflict) {
            issues.push(ValidationIssue {
                code: "MANIFEST-E022".into(),
                severity: profile_strictness(profile, "error", "warning").into(),
                message: format!("conflicting dependency \"{conflict}\" is present in workspace"),
            });
        }
    }
    Ok(())
}

fn dependency_ids(value: Option<&Value>) -> Vec<String> {
    let Some(arr) = value.and_then(|v| v.as_array()) else {
        return Vec::new();
    };
    arr.iter()
        .filter_map(|entry| {
            if let Some(id) = entry.get("modId").and_then(|v| v.as_str()) {
                Some(id.to_string())
            } else {
                entry.as_str().map(|s| s.to_string())
            }
        })
        .collect()
}

fn load_workspace_manifests(root: &Path) -> Result<HashMap<String, PathBuf>> {
    let mut output = HashMap::new();
    let Ok(entries) = fs::read_dir(root) else {
        return Ok(output);
    };
    for entry in entries.flatten() {
        let path = entry.path();
        let manifest = path.join("manifest.efl");
        if !manifest.exists() {
            continue;
        }
        let bytes = match fs::read(&manifest) {
            Ok(v) => v,
            Err(_) => continue,
        };
        let json: Value = match serde_json::from_slice(&bytes) {
            Ok(v) => v,
            Err(_) => continue,
        };
        if let Some(id) = json.get("modId").and_then(|v| v.as_str()) {
            output.insert(id.to_string(), manifest);
        }
    }
    Ok(output)
}

fn duplicate_workspace_mod_ids(root: &Path) -> Result<HashSet<String>> {
    let mut seen = HashSet::new();
    let mut duplicates = HashSet::new();
    let Ok(entries) = fs::read_dir(root) else {
        return Ok(duplicates);
    };
    for entry in entries.flatten() {
        let manifest = entry.path().join("manifest.efl");
        if !manifest.exists() {
            continue;
        }
        let Ok(bytes) = fs::read(&manifest) else {
            continue;
        };
        let Ok(json) = serde_json::from_slice::<Value>(&bytes) else {
            continue;
        };
        let Some(id) = json.get("modId").and_then(|v| v.as_str()) else {
            continue;
        };
        if !seen.insert(id.to_string()) {
            duplicates.insert(id.to_string());
        }
    }
    Ok(duplicates)
}

fn load_ids(dir: PathBuf, key: &str) -> Result<HashSet<String>> {
    let mut ids = HashSet::new();
    for json in load_json_objects(dir)? {
        if let Some(id) = json.get(key).and_then(|v| v.as_str()) {
            ids.insert(id.to_string());
        }
    }
    Ok(ids)
}

fn collect_trigger_refs(dir: PathBuf, key: &str, out: &mut HashSet<String>) -> Result<()> {
    for json in load_json_objects(dir)? {
        if let Some(v) = json.get(key).and_then(|v| v.as_str()) {
            if !v.trim().is_empty() {
                out.insert(v.to_string());
            }
        }
    }
    Ok(())
}

fn load_json_objects(dir: PathBuf) -> Result<Vec<Value>> {
    let mut out = Vec::new();
    if !dir.exists() || !dir.is_dir() {
        return Ok(out);
    }
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) != Some("json") {
            continue;
        }
        let bytes = match fs::read(&path) {
            Ok(v) => v,
            Err(_) => continue,
        };
        if let Ok(value) = serde_json::from_slice::<Value>(&bytes) {
            out.push(value);
        }
    }
    Ok(out)
}

fn profile_strictness(
    profile: ValidationProfile,
    strict: &'static str,
    default: &'static str,
) -> &'static str {
    match profile {
        ValidationProfile::Strict => strict,
        ValidationProfile::Recommended | ValidationProfile::Legacy => default,
    }
}

fn compatibility_severity(profile: ValidationProfile) -> &'static str {
    match profile {
        ValidationProfile::Strict => "error",
        ValidationProfile::Recommended | ValidationProfile::Legacy => "warning",
    }
}

fn looks_like_semver(version: &str) -> bool {
    let mut parts = version.split('.');
    let (Some(major), Some(minor), Some(patch)) = (parts.next(), parts.next(), parts.next()) else {
        return false;
    };
    if parts.next().is_some() {
        return false;
    }
    is_digits(major) && is_digits(minor) && is_digits(patch)
}

fn looks_like_namespaced_mod_id(mod_id: &str) -> bool {
    // Mirrors the regex in schemas/efl-manifest.schema.json:
    //   ^[a-z0-9]+(?:\.[a-z0-9][a-z0-9-]*)*$
    // and additionally requires at least three segments so we still warn on
    // bare names like "mypack" or "author.mypack".
    let parts: Vec<&str> = mod_id.split('.').collect();
    if parts.len() < 3 {
        return false;
    }
    parts.iter().enumerate().all(|(idx, part)| {
        if part.is_empty() {
            return false;
        }
        let mut chars = part.chars();
        let first = chars.next().unwrap();
        let first_ok = first.is_ascii_lowercase() || first.is_ascii_digit();
        if !first_ok {
            return false;
        }
        let allow_hyphen = idx > 0;
        chars.all(|ch| {
            ch.is_ascii_lowercase()
                || ch.is_ascii_digit()
                || ch == '_'
                || (allow_hyphen && ch == '-')
        })
    })
}

fn is_digits(value: &str) -> bool {
    !value.is_empty() && value.chars().all(|ch| ch.is_ascii_digit())
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use tempfile::TempDir;

    fn write_pack(tmp: &TempDir) -> PathBuf {
        let pack = tmp.path().join("pack");
        fs::create_dir_all(&pack).unwrap();
        let manifest = json!({
            "schemaVersion": 2,
            "modId": "com.test.graph",
            "name": "Graph Test",
            "version": "1.0.0",
            "eflVersion": "1.0.0",
            "features": ["areas", "warps", "npcs", "dialogue", "story", "quests", "resources"],
        });
        fs::write(
            pack.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        pack
    }

    fn write_json(dir: &Path, name: &str, value: serde_json::Value) {
        fs::create_dir_all(dir).unwrap();
        fs::write(
            dir.join(name),
            serde_json::to_string_pretty(&value).unwrap(),
        )
        .unwrap();
    }

    fn issue_codes(issues: &[ValidationIssue]) -> Vec<&str> {
        issues.iter().map(|i| i.code.as_str()).collect()
    }

    fn write_dat_pack(tmp: &TempDir, relationships: serde_json::Value) -> PathBuf {
        let pack = tmp.path().join("compat-pack");
        fs::create_dir_all(&pack).unwrap();
        let manifest = json!({
            "schemaVersion": 1,
            "datId": "com.test.compat.bridge",
            "name": "Compat Bridge",
            "version": "1.0.0",
            "eflVersion": "1.0.0",
            "relationships": relationships,
        });
        fs::write(
            pack.join("manifest.efdat"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        pack
    }

    #[test]
    fn warp_targeting_undefined_area_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("areas"),
            "valley.json",
            json!({"id": "valley", "displayName": "Valley", "backend": "hijacked", "hostRoom": "room_x"}),
        );
        write_json(
            &pack.join("warps"),
            "out.json",
            json!({
                "id": "out",
                "sourceArea": "valley",
                "targetArea": "missing",
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"WARP-H011"));
    }

    #[test]
    fn duplicate_npc_id_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("areas"),
            "a.json",
            json!({"id": "valley", "displayName": "V", "backend": "hijacked", "hostRoom": "r"}),
        );
        write_json(
            &pack.join("npcs"),
            "alice.json",
            json!({"id": "alice", "displayName": "A", "areaId": "valley", "spawnAnchor": "1,1"}),
        );
        write_json(
            &pack.join("npcs"),
            "alice2.json",
            json!({"id": "alice", "displayName": "A2", "areaId": "valley", "spawnAnchor": "2,2"}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"NPC-E031"));
    }

    #[test]
    fn dialogue_targeting_unknown_npc_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("dialogue"),
            "set.json",
            json!({
                "id": "set",
                "npc": "ghost",
                "entries": [{"id": "intro", "text": "hi"}]
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"DIALOGUE-H010"));
    }

    #[test]
    fn area_entry_event_must_resolve() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("areas"),
            "v.json",
            json!({
                "id": "valley",
                "displayName": "V",
                "backend": "hijacked",
                "hostRoom": "r",
                "entryEvent": "cs_intro"
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"AREA-H020"));
    }

    #[test]
    fn cutscene_starting_unknown_quest_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("events"),
            "cs.json",
            json!({
                "id": "cs_intro",
                "trigger": "",
                "once": true,
                "onFire": {"startQuest": "missing"}
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"STORY-H020"));
    }

    #[test]
    fn quest_with_duplicate_stage_id_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("quests"),
            "q.json",
            json!({
                "id": "q1",
                "title": "Q",
                "stages": [
                    {"id": "s1"},
                    {"id": "s1"}
                ]
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"QUEST-E011"));
    }

    #[test]
    fn resource_with_unknown_area_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("areas"),
            "v.json",
            json!({"id": "valley", "displayName": "V", "backend": "hijacked", "hostRoom": "r"}),
        );
        write_json(
            &pack.join("resources"),
            "r.json",
            json!({
                "id": "ore",
                "kind": "breakable_node",
                "spawnRules": {"areas": ["valley", "phantom"]}
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"RESOURCE-H010"));
    }

    #[test]
    fn fully_consistent_pack_passes() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack(&tmp);
        write_json(
            &pack.join("areas"),
            "v.json",
            json!({"id": "valley", "displayName": "V", "backend": "hijacked", "hostRoom": "r"}),
        );
        write_json(
            &pack.join("warps"),
            "w.json",
            json!({"id": "w1", "sourceArea": "valley", "targetArea": "valley"}),
        );
        write_json(
            &pack.join("npcs"),
            "alice.json",
            json!({"id": "alice", "displayName": "A", "areaId": "valley", "spawnAnchor": "1,1"}),
        );
        write_json(
            &pack.join("dialogue"),
            "set.json",
            json!({"id": "set", "npc": "alice", "entries": [{"id": "intro", "text": "hi"}]}),
        );
        write_json(
            &pack.join("quests"),
            "q.json",
            json!({"id": "q1", "title": "Q", "stages": [{"id": "s1"}]}),
        );
        write_json(
            &pack.join("events"),
            "cs.json",
            json!({"id": "cs1", "trigger": "", "once": true, "onFire": {"startQuest": "q1"}}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        // Filter out warnings unrelated to graph (e.g. modId namespacing, missing assets) and
        // confirm there are no graph errors.
        let graph_errors: Vec<&str> = issues
            .iter()
            .filter(|i| i.severity == "error")
            .map(|i| i.code.as_str())
            .collect();
        assert!(
            graph_errors.is_empty(),
            "expected no graph errors, got: {graph_errors:?}"
        );
    }

    fn write_pack_with_hooks(tmp: &TempDir) -> PathBuf {
        let pack = tmp.path().join("hookpack");
        fs::create_dir_all(&pack).unwrap();
        let manifest = json!({
            "schemaVersion": 2,
            "modId": "com.test.hooks",
            "name": "Hook Test",
            "version": "1.0.0",
            "eflVersion": "1.0.0",
            "features": ["resources", "experimental_widgets"],
            "scriptHooks": [
                {"target": "gml_Script_demo", "handler": "efl_resource_despawn", "mode": "callback"},
                {"target": "gml_Script_other", "handler": "efl_unknown_handler", "mode": "callback"},
            ],
        });
        fs::write(
            pack.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        pack
    }

    #[test]
    fn live_capabilities_widen_handler_whitelist() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack_with_hooks(&tmp);

        let caps = EngineCapabilities {
            efl_version: Some("99.0.0".into()),
            features: vec!["resources".into(), "experimental_widgets".into()],
            handlers: vec![
                "efl_resource_despawn".into(),
                "efl_unknown_handler".into(),
            ],
            hook_kinds: vec!["yyc_script".into()],
            flags: Default::default(),
        };
        let issues = validate_manifest_with_capabilities(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
            Some(&caps),
        )
        .unwrap();
        let codes = issue_codes(&issues);
        assert!(
            !codes.contains(&"HOOK-W004"),
            "live snapshot should accept handler `efl_unknown_handler`, got: {codes:?}"
        );
        assert!(
            !codes.contains(&"MANIFEST-W011"),
            "live snapshot should accept feature `experimental_widgets`, got: {codes:?}"
        );
    }

    #[test]
    fn missing_capabilities_falls_back_to_static_lists() {
        let tmp = TempDir::new().unwrap();
        let pack = write_pack_with_hooks(&tmp);
        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let codes = issue_codes(&issues);
        assert!(codes.contains(&"HOOK-W004"));
        assert!(codes.contains(&"MANIFEST-W011"));
    }

    fn write_calendar_pack(tmp: &TempDir) -> PathBuf {
        let pack = tmp.path().join("calpack");
        fs::create_dir_all(&pack).unwrap();
        let manifest = json!({
            "schemaVersion": 2,
            "modId": "com.test.calendar",
            "name": "Calendar Test",
            "version": "1.0.0",
            "eflVersion": "1.0.0",
            "features": ["calendar", "story"],
        });
        fs::write(
            pack.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        pack
    }

    #[test]
    fn calendar_unknown_season_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("calendar"),
            "bad.json",
            json!({"id": "bad", "season": "monsoon"}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"CALENDAR-E010"));
    }

    #[test]
    fn calendar_out_of_range_day_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("calendar"),
            "weird.json",
            json!({"id": "weird", "season": "spring", "dayOfSeason": 99}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"CALENDAR-E011"));
    }

    #[test]
    fn calendar_invalid_lifecycle_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("calendar"),
            "weekly.json",
            json!({"id": "weekly", "lifecycle": "weekly"}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"CALENDAR-E012"));
    }

    #[test]
    fn calendar_unknown_on_activate_is_hazard() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("calendar"),
            "fest.json",
            json!({
                "id": "summer_kickoff",
                "season": "summer",
                "dayOfSeason": 1,
                "onActivate": "ghost_event",
                "lifecycle": "once",
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let h = issues.iter().find(|i| i.code == "CALENDAR-H020");
        assert!(h.is_some(), "expected CALENDAR-H020 hazard");
        assert_eq!(h.unwrap().severity, "warning");

        let strict = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Strict,
        )
        .unwrap();
        let strict_h = strict.iter().find(|i| i.code == "CALENDAR-H020");
        assert_eq!(strict_h.unwrap().severity, "error");
    }

    #[test]
    fn calendar_duplicate_id_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("calendar"),
            "a.json",
            json!({"id": "festival", "season": "spring"}),
        );
        write_json(
            &pack.join("calendar"),
            "b.json",
            json!({"id": "festival", "season": "summer"}),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        assert!(issue_codes(&issues).contains(&"CALENDAR-E031"));
    }

    #[test]
    fn calendar_well_formed_event_passes() {
        let tmp = TempDir::new().unwrap();
        let pack = write_calendar_pack(&tmp);
        write_json(
            &pack.join("events"),
            "intro.json",
            json!({"id": "summer_intro", "trigger": ""}),
        );
        write_json(
            &pack.join("calendar"),
            "fest.json",
            json!({
                "id": "summer_kickoff",
                "season": "summer",
                "dayOfSeason": 1,
                "onActivate": "summer_intro",
                "lifecycle": "once",
            }),
        );

        let issues = validate_manifest_with_profile(
            &pack.join("manifest.efl"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let calendar_errors: Vec<&str> = issues
            .iter()
            .filter(|i| i.severity == "error" && i.code.starts_with("CALENDAR"))
            .map(|i| i.code.as_str())
            .collect();
        assert!(
            calendar_errors.is_empty(),
            "expected no calendar errors, got: {calendar_errors:?}"
        );
    }

    #[test]
    fn dat_requires_momi_missing_is_error_when_inventory_available() {
        let tmp = TempDir::new().unwrap();
        let pack = write_dat_pack(
            &tmp,
            json!([
                {
                    "type": "requires",
                    "target": { "kind": "momi", "id": "com.example.required-mod", "versionRange": ">=1.0.0" }
                }
            ]),
        );
        fs::create_dir_all(tmp.path().join("mods")).unwrap();

        let issues = validate_dat_with_profile(
            &pack.join("manifest.efdat"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let codes = issue_codes(&issues);
        assert!(codes.contains(&"DAT-E020"), "got codes: {codes:?}");
    }

    #[test]
    fn dat_conflicts_momi_present_is_error() {
        let tmp = TempDir::new().unwrap();
        let pack = write_dat_pack(
            &tmp,
            json!([
                {
                    "type": "conflicts",
                    "target": { "kind": "momi", "id": "com.example.bad-mod" }
                }
            ]),
        );
        let mods_dir = tmp.path().join("mods").join("com.example.bad-mod");
        fs::create_dir_all(&mods_dir).unwrap();

        let issues = validate_dat_with_profile(
            &pack.join("manifest.efdat"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let codes = issue_codes(&issues);
        assert!(codes.contains(&"DAT-E021"), "got codes: {codes:?}");
    }

    #[test]
    fn dat_without_inventory_source_emits_warning() {
        let tmp = TempDir::new().unwrap();
        let pack = write_dat_pack(
            &tmp,
            json!([
                {
                    "type": "requires",
                    "target": { "kind": "momi", "id": "com.example.any-mod", "versionRange": ">=1.0.0" }
                }
            ]),
        );

        let issues = validate_dat_with_profile(
            &pack.join("manifest.efdat"),
            ValidationProfile::Recommended,
        )
        .unwrap();
        let codes = issue_codes(&issues);
        assert!(codes.contains(&"DAT-W021"), "got codes: {codes:?}");
    }
}
