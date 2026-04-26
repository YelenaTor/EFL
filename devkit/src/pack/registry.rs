use std::{fs, path::Path};

use anyhow::Result;
use serde::{Deserialize, Serialize};

/// One entry in the output-dir build-history.json registry.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct BuildHistoryEntry {
    pub mod_id: String,
    pub version: String,
    pub built_at: String,
    /// Path relative to the output_base_dir, e.g. "com.yoru.mod/1.0.0/mod-1.0.0.efpack"
    pub artifact_path: String,
    pub manifest_hash: String,
}

const REGISTRY_FILE: &str = "build-history.json";

/// Read the build history from `<output_dir>/build-history.json`.
/// Returns an empty Vec if the file doesn't exist yet.
pub fn read_history(output_dir: &Path) -> Result<Vec<BuildHistoryEntry>> {
    let path = output_dir.join(REGISTRY_FILE);
    if !path.exists() {
        return Ok(Vec::new());
    }
    let bytes = fs::read(&path)?;
    Ok(serde_json::from_slice(&bytes)?)
}

/// Upsert an entry into `<output_dir>/build-history.json`.
/// If an entry with the same (mod_id, version) exists it is replaced;
/// otherwise the new entry is prepended. Result is always sorted newest-first.
pub fn append_build(output_dir: &Path, entry: &BuildHistoryEntry) -> Result<()> {
    let mut history = read_history(output_dir)?;

    // Remove any existing entry for the same (mod_id, version).
    history.retain(|e| !(e.mod_id == entry.mod_id && e.version == entry.version));

    // Prepend so newest appears first without a sort pass.
    history.insert(0, entry.clone());

    let path = output_dir.join(REGISTRY_FILE);
    let bytes = serde_json::to_vec_pretty(&history)?;
    fs::write(path, bytes)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn entry(mod_id: &str, version: &str, built_at: &str) -> BuildHistoryEntry {
        BuildHistoryEntry {
            mod_id: mod_id.into(),
            version: version.into(),
            built_at: built_at.into(),
            artifact_path: format!("{mod_id}/{version}/{mod_id}-{version}.efpack"),
            manifest_hash: "sha256:abc".into(),
        }
    }

    #[test]
    fn test_registry_empty_on_missing_file() {
        let tmp = TempDir::new().unwrap();
        let history = read_history(tmp.path()).unwrap();
        assert!(history.is_empty());
    }

    #[test]
    fn test_registry_append_two_entries() {
        let tmp = TempDir::new().unwrap();
        append_build(
            tmp.path(),
            &entry("com.a.mod", "1.0.0", "2026-04-20T10:00:00Z"),
        )
        .unwrap();
        append_build(
            tmp.path(),
            &entry("com.a.mod", "1.1.0", "2026-04-25T14:00:00Z"),
        )
        .unwrap();
        let history = read_history(tmp.path()).unwrap();
        assert_eq!(history.len(), 2);
        // Newest-first: 1.1.0 was appended last, so it's at index 0.
        assert_eq!(history[0].version, "1.1.0");
        assert_eq!(history[1].version, "1.0.0");
    }

    #[test]
    fn test_registry_upsert_same_version() {
        let tmp = TempDir::new().unwrap();
        let mut e = entry("com.a.mod", "1.0.0", "2026-04-20T10:00:00Z");
        append_build(tmp.path(), &e).unwrap();
        e.built_at = "2026-04-25T14:00:00Z".into();
        e.manifest_hash = "sha256:newHash".into();
        append_build(tmp.path(), &e).unwrap();
        let history = read_history(tmp.path()).unwrap();
        assert_eq!(
            history.len(),
            1,
            "same version should be upserted, not duplicated"
        );
        assert_eq!(history[0].manifest_hash, "sha256:newHash");
        assert_eq!(history[0].built_at, "2026-04-25T14:00:00Z");
    }

    #[test]
    fn test_registry_multiple_mods() {
        let tmp = TempDir::new().unwrap();
        append_build(
            tmp.path(),
            &entry("com.a.mod", "1.0.0", "2026-04-20T10:00:00Z"),
        )
        .unwrap();
        append_build(
            tmp.path(),
            &entry("com.b.mod", "1.0.0", "2026-04-21T10:00:00Z"),
        )
        .unwrap();
        let history = read_history(tmp.path()).unwrap();
        assert_eq!(history.len(), 2);
        assert_eq!(history[0].mod_id, "com.b.mod");
        assert_eq!(history[1].mod_id, "com.a.mod");
    }
}
