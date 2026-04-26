use std::{
    fs,
    io::{copy, Read, Write},
    path::{Path, PathBuf},
};

use anyhow::{bail, Context, Result};

use super::shared::{open_archive, read_archive_entry_json};

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
    inspect_archive(file)
}

/// Inspect an .efdat file and return its contents.
pub fn inspect_efdat(file: &Path) -> Result<InspectResult> {
    inspect_archive(file)
}

fn inspect_archive(file: &Path) -> Result<InspectResult> {
    let mut archive = open_archive(file)?;

    let manifest = read_archive_entry_json(&mut archive, "manifest.efl")?
        .or(read_archive_entry_json(&mut archive, "manifest.efdat")?);
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

    Ok(InspectResult {
        manifest,
        pack_meta,
        entries,
        total_bytes,
    })
}

/// Result of extracting an .efpack/.efdat archive to a working directory.
#[derive(Debug, Clone)]
pub struct ExtractResult {
    /// Folder the archive was extracted into.
    pub workspace: PathBuf,
    /// Number of entries written.
    pub files_written: usize,
    /// Whether `pack-meta.json` was found in the archive (and skipped).
    pub stripped_pack_meta: bool,
}

/// Extract an .efpack or .efdat archive into `dest_dir`.
///
/// `pack-meta.json` is intentionally not extracted: it is regenerated on every
/// repack so that checksums and timestamps stay authoritative. The destination
/// directory is created if missing; non-empty existing destinations are
/// rejected so we never silently merge into someone else's working folder.
pub fn extract_archive(file: &Path, dest_dir: &Path) -> Result<ExtractResult> {
    if dest_dir.exists() {
        let any_entry = fs::read_dir(dest_dir)
            .with_context(|| format!("reading {}", dest_dir.display()))?
            .next()
            .is_some();
        if any_entry {
            bail!(
                "EDIT-E001: refusing to extract into non-empty folder {}",
                dest_dir.display()
            );
        }
    } else {
        fs::create_dir_all(dest_dir)
            .with_context(|| format!("creating {}", dest_dir.display()))?;
    }

    let mut archive = open_archive(file)?;
    let mut files_written = 0usize;
    let mut stripped_pack_meta = false;

    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)?;
        let entry_name = entry.name().replace('\\', "/");
        if entry.is_dir() {
            continue;
        }
        if entry_name.eq_ignore_ascii_case("pack-meta.json") {
            stripped_pack_meta = true;
            continue;
        }

        let target = sanitized_target(dest_dir, &entry_name)?;
        if let Some(parent) = target.parent() {
            fs::create_dir_all(parent)
                .with_context(|| format!("creating {}", parent.display()))?;
        }

        let mut out = fs::File::create(&target)
            .with_context(|| format!("creating {}", target.display()))?;
        copy(&mut entry, &mut out)
            .with_context(|| format!("writing {}", target.display()))?;
        out.flush().ok();
        files_written += 1;
    }

    Ok(ExtractResult {
        workspace: dest_dir.to_path_buf(),
        files_written,
        stripped_pack_meta,
    })
}

/// Patch a single entry inside an extracted workspace.
///
/// `relative_path` is normalized to forward slashes, validated against
/// directory traversal, and written as `bytes`. Used by the DevKit edit-in-place
/// flow to overlay quick patches onto an extracted archive before repacking.
pub fn patch_workspace_entry(
    workspace: &Path,
    relative_path: &str,
    bytes: &[u8],
) -> Result<PathBuf> {
    let target = sanitized_target(workspace, relative_path)?;
    if let Some(parent) = target.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("creating {}", parent.display()))?;
    }
    let mut out = fs::File::create(&target)
        .with_context(|| format!("creating {}", target.display()))?;
    out.write_all(bytes)
        .with_context(|| format!("writing {}", target.display()))?;
    out.flush().ok();
    Ok(target)
}

/// Read the bytes of a single entry from an .efpack/.efdat archive.
///
/// Used by edit-in-place flows that want to view a specific file without
/// extracting the whole archive.
pub fn read_archive_entry_bytes(file: &Path, name: &str) -> Result<Option<Vec<u8>>> {
    let mut archive = open_archive(file)?;
    let result = match archive.by_name(name) {
        Ok(mut entry) => {
            let mut buf = Vec::with_capacity(entry.size() as usize);
            entry.read_to_end(&mut buf)?;
            Ok(Some(buf))
        }
        Err(_) => Ok(None),
    };
    result
}

fn sanitized_target(base: &Path, relative: &str) -> Result<PathBuf> {
    let normalized = relative.replace('\\', "/");
    if normalized.is_empty() {
        bail!("EDIT-E002: empty entry path");
    }

    let mut target = base.to_path_buf();
    for part in normalized.split('/') {
        match part {
            "" | "." => continue,
            ".." => bail!(
                "EDIT-E003: archive entry escapes workspace via '..': {}",
                relative
            ),
            other => {
                if other.contains(':') {
                    bail!(
                        "EDIT-E004: archive entry has a drive letter or stream marker: {}",
                        relative
                    );
                }
                target.push(other);
            }
        }
    }

    Ok(target)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::TempDir;
    use zip::{write::SimpleFileOptions, ZipWriter};

    fn write_test_archive(path: &Path) {
        let f = fs::File::create(path).unwrap();
        let mut zip = ZipWriter::new(f);
        let opts = SimpleFileOptions::default();
        zip.start_file("manifest.efl", opts).unwrap();
        zip.write_all(br#"{"modId":"com.test.edit","version":"0.1.0"}"#)
            .unwrap();
        zip.start_file("areas/test.json", opts).unwrap();
        zip.write_all(br#"{"id":"a"}"#).unwrap();
        zip.start_file("pack-meta.json", opts).unwrap();
        zip.write_all(br#"{"placeholder":true}"#).unwrap();
        zip.finish().unwrap();
    }

    #[test]
    fn extract_skips_pack_meta_and_writes_entries() {
        let tmp = TempDir::new().unwrap();
        let archive = tmp.path().join("test.efpack");
        write_test_archive(&archive);

        let dest = tmp.path().join("workspace");
        let result = extract_archive(&archive, &dest).unwrap();
        assert_eq!(result.files_written, 2);
        assert!(result.stripped_pack_meta);
        assert!(dest.join("manifest.efl").exists());
        assert!(dest.join("areas/test.json").exists());
        assert!(!dest.join("pack-meta.json").exists());
    }

    #[test]
    fn extract_rejects_nonempty_destination() {
        let tmp = TempDir::new().unwrap();
        let archive = tmp.path().join("test.efpack");
        write_test_archive(&archive);

        let dest = tmp.path().join("workspace");
        fs::create_dir_all(&dest).unwrap();
        fs::write(dest.join("hello.txt"), b"hi").unwrap();

        let err = extract_archive(&archive, &dest).unwrap_err();
        assert!(err.to_string().contains("EDIT-E001"));
    }

    #[test]
    fn sanitized_target_blocks_traversal() {
        let tmp = TempDir::new().unwrap();
        let result = sanitized_target(tmp.path(), "../escape.txt");
        assert!(result.is_err());
    }

    #[test]
    fn patch_workspace_entry_writes_bytes() {
        let tmp = TempDir::new().unwrap();
        let workspace = tmp.path();
        let path = patch_workspace_entry(workspace, "areas/new.json", b"{\"id\":\"x\"}").unwrap();
        assert_eq!(fs::read(path).unwrap(), b"{\"id\":\"x\"}");
    }
}
