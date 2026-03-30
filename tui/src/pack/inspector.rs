use std::{fs, path::Path};

use anyhow::Result;

use super::shared::read_archive_entry_json;

/// A single entry in an .efpack archive.
#[derive(Debug, Clone)]
pub struct ArchiveEntry {
    pub name: String,
    pub size: u64,
}

/// Result of inspecting an .efpack file.
#[derive(Debug, Clone)]
pub struct InspectResult {
    pub manifest: Option<serde_json::Value>,
    pub pack_meta: Option<serde_json::Value>,
    pub entries: Vec<ArchiveEntry>,
    pub total_bytes: u64,
}

/// Inspect an .efpack file and return its contents.
pub fn inspect_efpack(file: &Path) -> Result<InspectResult> {
    let f = fs::File::open(file)?;
    let mut archive = zip::ZipArchive::new(f)?;

    let manifest = read_archive_entry_json(&mut archive, "manifest.efl")?;
    let pack_meta = read_archive_entry_json(&mut archive, "pack-meta.json")?;

    let mut entries = Vec::new();
    let mut total_bytes = 0u64;

    for i in 0..archive.len() {
        let entry = archive.by_index(i)?;
        let size = entry.size();
        total_bytes += size;
        entries.push(ArchiveEntry {
            name: entry.name().to_string(),
            size,
        });
    }

    Ok(InspectResult { manifest, pack_meta, entries, total_bytes })
}
