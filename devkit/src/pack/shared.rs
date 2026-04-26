use std::{fs, io::Read, path::Path};

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
            Ok(Some(
                serde_json::from_str(&buf).with_context(|| format!("parsing {name}"))?,
            ))
        }
        Err(_) => Ok(None),
    }
}
