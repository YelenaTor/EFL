use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::{Context, Result};
use chrono::Utc;

#[derive(Debug, Clone)]
pub struct MigrationChange {
    pub file: String,
    pub description: String,
}

#[derive(Debug, Clone)]
pub struct MigrationReport {
    pub pack_path: PathBuf,
    pub changes: Vec<MigrationChange>,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct MigrationApplyResult {
    pub backup_path: PathBuf,
    pub report: MigrationReport,
}

pub fn analyze_pack(pack_path: &Path) -> Result<MigrationReport> {
    let manifest_path = pack_path.join("manifest.efl");
    let mut changes = Vec::new();
    let mut warnings = Vec::new();

    if !manifest_path.exists() {
        warnings.push("manifest.efl not found; migration cannot proceed".into());
        return Ok(MigrationReport {
            pack_path: pack_path.to_path_buf(),
            changes,
            warnings,
        });
    }

    let bytes =
        fs::read(&manifest_path).with_context(|| format!("reading {}", manifest_path.display()))?;
    let json: serde_json::Value = serde_json::from_slice(&bytes)
        .with_context(|| format!("parsing {}", manifest_path.display()))?;

    let Some(root) = json.as_object() else {
        warnings.push("manifest root is not a JSON object".into());
        return Ok(MigrationReport {
            pack_path: pack_path.to_path_buf(),
            changes,
            warnings,
        });
    };

    if root
        .get("schemaVersion")
        .and_then(|v| v.as_i64())
        .unwrap_or(0)
        < 2
    {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Set schemaVersion to 2".into(),
        });
    }
    if !root.contains_key("eflVersion") {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Add missing eflVersion (default 1.0.0)".into(),
        });
    }
    if root.get("features").is_some_and(|v| v.is_object()) {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Convert legacy features object to string array".into(),
        });
    }
    if root.get("dependencies").is_some() && dependencies_need_migration(&json) {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Convert legacy dependency string arrays to object arrays".into(),
        });
    }
    if root.contains_key("saveScope") {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Remove deprecated saveScope (derived from modId)".into(),
        });
    }
    if root
        .get("settings")
        .and_then(|v| v.as_object())
        .is_some_and(|s| s.contains_key("strict"))
    {
        changes.push(MigrationChange {
            file: "manifest.efl".into(),
            description: "Rename settings.strict to settings.strictMode".into(),
        });
    }

    if pack_path.join("localisation").exists() {
        warnings.push(
            "Found localisation/ folder. Localization text delivery belongs to MOMI pre-launch."
                .into(),
        );
    }

    Ok(MigrationReport {
        pack_path: pack_path.to_path_buf(),
        changes,
        warnings,
    })
}

pub fn apply_migration(pack_path: &Path) -> Result<MigrationApplyResult> {
    let report = analyze_pack(pack_path)?;
    if report.changes.is_empty() {
        return Ok(MigrationApplyResult {
            backup_path: PathBuf::new(),
            report,
        });
    }

    let backup_path = create_backup(pack_path)?;
    migrate_manifest(pack_path)?;

    let applied_report = analyze_pack(pack_path)?;
    Ok(MigrationApplyResult {
        backup_path,
        report: MigrationReport {
            pack_path: pack_path.to_path_buf(),
            changes: report.changes,
            warnings: applied_report.warnings,
        },
    })
}

fn migrate_manifest(pack_path: &Path) -> Result<()> {
    let manifest_path = pack_path.join("manifest.efl");
    let bytes =
        fs::read(&manifest_path).with_context(|| format!("reading {}", manifest_path.display()))?;
    let mut json: serde_json::Value = serde_json::from_slice(&bytes)
        .with_context(|| format!("parsing {}", manifest_path.display()))?;

    let Some(root) = json.as_object_mut() else {
        return Ok(());
    };

    if root
        .get("schemaVersion")
        .and_then(|v| v.as_i64())
        .unwrap_or(0)
        < 2
    {
        root.insert("schemaVersion".into(), serde_json::json!(2));
    }
    if !root.contains_key("eflVersion") {
        root.insert("eflVersion".into(), serde_json::json!("1.0.0"));
    }
    if let Some(features) = root.get("features").cloned() {
        if let Some(obj) = features.as_object() {
            let converted: Vec<String> = obj
                .iter()
                .filter(|(_, enabled)| enabled.as_bool().unwrap_or(false))
                .map(|(name, _)| name.to_string())
                .collect();
            root.insert("features".into(), serde_json::json!(converted));
        }
    }

    migrate_dependencies(root);

    if let Some(settings) = root.get_mut("settings").and_then(|v| v.as_object_mut()) {
        if let Some(strict) = settings.remove("strict") {
            settings.insert("strictMode".into(), strict);
        }
    }

    root.remove("saveScope");

    fs::write(&manifest_path, serde_json::to_vec_pretty(&json)?)
        .with_context(|| format!("writing {}", manifest_path.display()))?;
    Ok(())
}

fn migrate_dependencies(root: &mut serde_json::Map<String, serde_json::Value>) {
    let Some(deps) = root.get_mut("dependencies").and_then(|v| v.as_object_mut()) else {
        return;
    };

    for key in ["required", "optional"] {
        if let Some(arr) = deps.get_mut(key).and_then(|v| v.as_array_mut()) {
            if arr.iter().all(|v| v.is_string()) {
                let converted: Vec<serde_json::Value> = arr
                    .iter()
                    .filter_map(|v| v.as_str())
                    .map(|mod_id| serde_json::json!({"modId": mod_id, "versionRange": "*"}))
                    .collect();
                *arr = converted;
            }
        }
    }

    if let Some(arr) = deps.get_mut("conflicts").and_then(|v| v.as_array_mut()) {
        if arr.iter().all(|v| v.is_string()) {
            let converted: Vec<serde_json::Value> = arr
                .iter()
                .filter_map(|v| v.as_str())
                .map(|mod_id| serde_json::json!({"modId": mod_id, "reason": "Migrated from legacy conflict entry"}))
                .collect();
            *arr = converted;
        }
    }
}

fn dependencies_need_migration(json: &serde_json::Value) -> bool {
    let Some(deps) = json.get("dependencies").and_then(|v| v.as_object()) else {
        return false;
    };
    for key in ["required", "optional", "conflicts"] {
        if deps
            .get(key)
            .and_then(|v| v.as_array())
            .is_some_and(|arr| !arr.is_empty() && arr.iter().all(|v| v.is_string()))
        {
            return true;
        }
    }
    false
}

fn create_backup(pack_path: &Path) -> Result<PathBuf> {
    let parent = pack_path
        .parent()
        .ok_or_else(|| anyhow::anyhow!("cannot backup pack root without parent directory"))?;
    let name = pack_path
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("pack");
    let timestamp = Utc::now().format("%Y%m%d-%H%M%S");
    let backup_path = parent.join(format!("{name}.backup-{timestamp}"));
    copy_dir_recursive(pack_path, &backup_path)?;
    Ok(backup_path)
}

fn copy_dir_recursive(source: &Path, target: &Path) -> Result<()> {
    fs::create_dir_all(target).with_context(|| format!("creating {}", target.display()))?;
    for entry in fs::read_dir(source).with_context(|| format!("reading {}", source.display()))? {
        let entry = entry?;
        let path = entry.path();
        let destination = target.join(entry.file_name());
        if path.is_dir() {
            copy_dir_recursive(&path, &destination)?;
        } else {
            fs::copy(&path, &destination).with_context(|| {
                format!("copying {} to {}", path.display(), destination.display())
            })?;
        }
    }
    Ok(())
}
