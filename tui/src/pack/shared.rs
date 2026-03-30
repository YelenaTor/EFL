use std::{fs, io::Read, path::{Component, Path, PathBuf}};

use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use zip::ZipArchive;

pub fn sha256_hex(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("sha256:{:x}", hasher.finalize())
}

pub fn open_archive(file: &Path) -> Result<ZipArchive<fs::File>> {
    let f = fs::File::open(file).with_context(|| format!("opening {}", file.display()))?;
    ZipArchive::new(f).context("reading ZIP archive")
}

pub fn read_archive_entry_json(
    archive: &mut ZipArchive<fs::File>,
    name: &str,
) -> Result<Option<serde_json::Value>> {
    match archive.by_name(name) {
        Ok(mut entry) => {
            let mut buf = String::new();
            entry.read_to_string(&mut buf)?;
            Ok(Some(serde_json::from_str(&buf).with_context(|| format!("parsing {name}"))?))
        }
        Err(_) => Ok(None),
    }
}

/// Strip leading `/` and `..` components to prevent path traversal when
/// extracting untrusted ZIP archives.
pub fn safe_extract_path(out_dir: &Path, entry_name: &str) -> PathBuf {
    let sanitised = Path::new(entry_name)
        .components()
        .filter(|c| matches!(c, Component::Normal(_)))
        .collect::<PathBuf>();
    out_dir.join(sanitised)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_safe_extract_path_strips_traversal() {
        let base = Path::new("/out");
        assert_eq!(safe_extract_path(base, "../../etc/passwd"), Path::new("/out/etc/passwd"));
        assert_eq!(safe_extract_path(base, "/absolute/path"), Path::new("/out/absolute/path"));
        assert_eq!(safe_extract_path(base, "normal/file.txt"), Path::new("/out/normal/file.txt"));
    }
}
