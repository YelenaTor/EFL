use std::{
    fs,
    io::{self, Write},
    path::{Path, PathBuf},
};

use anyhow::{bail, Context, Result};
use chrono::Utc;
use zip::{write::SimpleFileOptions, ZipWriter};

use super::shared::sha256_hex;

/// Result of a successful pack operation.
#[derive(Debug, Clone)]
pub struct PackResult {
    pub out_path: PathBuf,
    pub mod_id: String,
    pub version: String,
    pub manifest_hash: String,
}

/// Pack a content directory into an .efpack file.
///
/// If `output` is None, the file is written to `<cwd>/<modId>-<version>.efpack`.
pub fn pack_folder(content_dir: &Path, output: Option<&Path>) -> Result<PackResult> {
    let manifest_path = content_dir.join("manifest.efl");
    if !manifest_path.exists() {
        bail!("MANIFEST-E001: manifest.efl not found in {}", content_dir.display());
    }

    let manifest_bytes = fs::read(&manifest_path)
        .with_context(|| format!("reading {}", manifest_path.display()))?;

    let manifest_json: serde_json::Value = serde_json::from_slice(&manifest_bytes)
        .context("parsing manifest.efl as JSON")?;

    let mod_id = manifest_json
        .get("modId")
        .and_then(|v| v.as_str())
        .ok_or_else(|| anyhow::anyhow!("MANIFEST-E002: manifest.efl missing required field \"modId\""))?
        .to_string();

    let version = manifest_json
        .get("version")
        .and_then(|v| v.as_str())
        .unwrap_or("0.0.0")
        .to_string();

    let manifest_hash = sha256_hex(&manifest_bytes);

    let pack_meta = serde_json::json!({
        "eflVersion": "2.0.0",
        "packedAt": Utc::now().to_rfc3339(),
        "manifestHash": manifest_hash,
        "packerVersion": "1.0.0",
    });
    let pack_meta_bytes = serde_json::to_vec_pretty(&pack_meta)?;

    let out_path: PathBuf = match output {
        Some(p) => p.to_owned(),
        None => PathBuf::from(format!("{mod_id}-{version}.efpack")),
    };

    let out_file = fs::File::create(&out_path)
        .with_context(|| format!("creating {}", out_path.display()))?;
    let mut zip = ZipWriter::new(out_file);
    let options = SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);

    add_directory_to_zip(&mut zip, content_dir, content_dir, options)?;

    zip.start_file("pack-meta.json", options)?;
    zip.write_all(&pack_meta_bytes)?;

    zip.finish()?;

    Ok(PackResult { out_path, mod_id, version, manifest_hash })
}

fn add_directory_to_zip(
    zip: &mut ZipWriter<fs::File>,
    base: &Path,
    dir: &Path,
    options: SimpleFileOptions,
) -> Result<()> {
    for entry in fs::read_dir(dir).with_context(|| format!("reading dir {}", dir.display()))? {
        let entry = entry?;
        let path = entry.path();
        let rel = path.strip_prefix(base).context("strip prefix")?;
        let zip_path = rel.to_string_lossy().replace('\\', "/");

        if path.is_dir() {
            zip.add_directory(&zip_path, options)?;
            add_directory_to_zip(zip, base, &path, options)?;
        } else {
            zip.start_file(&zip_path, options)?;
            let mut f = fs::File::open(&path)
                .with_context(|| format!("opening {}", path.display()))?;
            io::copy(&mut f, zip)?;
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write as _;
    use tempfile::TempDir;

    fn make_content_dir(dir: &Path) {
        let manifest = serde_json::json!({
            "schemaVersion": 1,
            "modId": "com.test.example",
            "name": "Test Mod",
            "version": "1.2.3",
            "eflVersion": "2.0.0",
            "features": {}
        });
        let mut f = fs::File::create(dir.join("manifest.efl")).unwrap();
        f.write_all(serde_json::to_string_pretty(&manifest).unwrap().as_bytes()).unwrap();

        fs::create_dir_all(dir.join("areas")).unwrap();
        let mut f2 = fs::File::create(dir.join("areas").join("test.json")).unwrap();
        f2.write_all(b"{\"id\":\"test\"}").unwrap();
    }

    #[test]
    fn test_pack_creates_efpack() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out = tmp.path().join("output.efpack");
        let result = pack_folder(&content_dir, Some(&out)).unwrap();
        assert!(result.out_path.exists());
        assert_eq!(result.mod_id, "com.test.example");
        assert_eq!(result.version, "1.2.3");
    }

    #[test]
    fn test_pack_missing_manifest() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("empty");
        fs::create_dir_all(&content_dir).unwrap();
        assert!(pack_folder(&content_dir, None).is_err());
    }

    #[test]
    fn test_roundtrip_produces_valid_archive() {
        use zip::ZipArchive;

        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out = tmp.path().join("output.efpack");
        let result = pack_folder(&content_dir, Some(&out)).unwrap();

        let f = fs::File::open(&result.out_path).unwrap();
        let mut archive = ZipArchive::new(f).unwrap();
        let names: Vec<String> = (0..archive.len())
            .map(|i| archive.by_index(i).unwrap().name().to_string())
            .collect();
        assert!(names.iter().any(|n| n == "manifest.efl"));
        assert!(names.iter().any(|n| n == "pack-meta.json"));
    }
}
