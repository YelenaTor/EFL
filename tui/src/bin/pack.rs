use std::{
    fs,
    io::{self, Read, Write},
    path::{Component, Path, PathBuf},
};

use anyhow::{bail, Context, Result};
use chrono::Utc;
use clap::{Parser, Subcommand};
use sha2::{Digest, Sha256};
use zip::{write::SimpleFileOptions, ZipArchive, ZipWriter};

// TODO: schema validation via jsonschema crate

#[derive(Parser)]
#[command(name = "efl-pack", about = "Pack and inspect EFL content packs (.efpack)")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Pack a content directory into an .efpack file
    Pack {
        /// Content directory containing manifest.efl
        content_dir: PathBuf,
        /// Output file path (default: <modId>-<version>.efpack)
        #[arg(short, long)]
        output: Option<PathBuf>,
    },
    /// Unpack an .efpack file into a directory
    Unpack {
        /// Input .efpack file
        file: PathBuf,
        /// Output directory (default: ./<modId>/)
        #[arg(short, long)]
        output: Option<PathBuf>,
    },
    /// Inspect an .efpack file
    Inspect {
        /// Input .efpack file
        file: PathBuf,
    },
}

fn main() {
    let cli = Cli::parse();
    if let Err(e) = run(cli) {
        eprintln!("error: {e:#}");
        std::process::exit(1);
    }
}

fn run(cli: Cli) -> Result<()> {
    match cli.command {
        Commands::Pack { content_dir, output } => cmd_pack(&content_dir, output.as_deref()),
        Commands::Unpack { file, output } => cmd_unpack(&file, output.as_deref()),
        Commands::Inspect { file } => cmd_inspect(&file),
    }
}

fn sha256_hex(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("sha256:{:x}", hasher.finalize())
}

fn open_archive(file: &Path) -> Result<ZipArchive<fs::File>> {
    let f = fs::File::open(file).with_context(|| format!("opening {}", file.display()))?;
    ZipArchive::new(f).context("reading ZIP archive")
}

fn read_archive_entry_json(
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
fn safe_extract_path(out_dir: &Path, entry_name: &str) -> PathBuf {
    let sanitised = Path::new(entry_name)
        .components()
        .filter(|c| matches!(c, Component::Normal(_)))
        .collect::<PathBuf>();
    out_dir.join(sanitised)
}

fn cmd_pack(content_dir: &Path, output: Option<&Path>) -> Result<()> {
    let manifest_path = content_dir.join("manifest.efl");
    if !manifest_path.exists() {
        eprintln!("MANIFEST-E001: manifest.efl not found in {}", content_dir.display());
        bail!("missing manifest.efl");
    }

    let manifest_bytes = fs::read(&manifest_path)
        .with_context(|| format!("reading {}", manifest_path.display()))?;

    let manifest_json: serde_json::Value = serde_json::from_slice(&manifest_bytes)
        .context("parsing manifest.efl as JSON")?;

    let mod_id = manifest_json
        .get("modId")
        .and_then(|v| v.as_str())
        .ok_or_else(|| {
            eprintln!("MANIFEST-E002: manifest.efl missing required field \"modId\"");
            anyhow::anyhow!("missing modId in manifest.efl")
        })?;

    let version = manifest_json
        .get("version")
        .and_then(|v| v.as_str())
        .unwrap_or("0.0.0");

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

    println!("packed: {}", out_path.display());
    println!("  mod: {mod_id} v{version}");
    println!("  manifest hash: {manifest_hash}");
    Ok(())
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

fn cmd_unpack(file: &Path, output: Option<&Path>) -> Result<()> {
    let mut archive = open_archive(file)?;

    let expected_hash: Option<String> = read_archive_entry_json(&mut archive, "pack-meta.json")?
        .and_then(|v| v.get("manifestHash").and_then(|h| h.as_str()).map(str::to_owned));

    let out_dir: PathBuf = match output {
        Some(p) => p.to_owned(),
        None => {
            let mod_id = read_archive_entry_json(&mut archive, "manifest.efl")?
                .and_then(|v| v.get("modId").and_then(|m| m.as_str()).map(str::to_owned))
                .unwrap_or_else(|| "unpacked".into());
            PathBuf::from(mod_id)
        }
    };

    fs::create_dir_all(&out_dir)
        .with_context(|| format!("creating {}", out_dir.display()))?;

    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)?;
        let out_path = safe_extract_path(&out_dir, entry.name());

        if entry.is_dir() {
            fs::create_dir_all(&out_path)?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent)?;
            }
            let mut out_file = fs::File::create(&out_path)
                .with_context(|| format!("creating {}", out_path.display()))?;
            io::copy(&mut entry, &mut out_file)?;
        }
    }

    println!("unpacked to: {}", out_dir.display());

    let extracted_manifest = out_dir.join("manifest.efl");
    if extracted_manifest.exists() {
        if let Some(expected) = expected_hash {
            let bytes = fs::read(&extracted_manifest)?;
            let actual = sha256_hex(&bytes);
            if actual == expected {
                println!("verification: OK ({actual})");
            } else {
                eprintln!("PACK-E001: hash mismatch");
                eprintln!("  expected: {expected}");
                eprintln!("  actual:   {actual}");
            }
        }
    }

    Ok(())
}

fn cmd_inspect(file: &Path) -> Result<()> {
    let mut archive = open_archive(file)?;

    if let Some(v) = read_archive_entry_json(&mut archive, "manifest.efl")? {
        println!("=== manifest.efl ===");
        println!("{}", serde_json::to_string_pretty(&v)?);
    }

    if let Some(v) = read_archive_entry_json(&mut archive, "pack-meta.json")? {
        println!("\n=== pack-meta.json ===");
        println!("{}", serde_json::to_string_pretty(&v)?);
    }

    println!("\n=== archive contents ===");
    println!("{:<10}  {}", "SIZE", "PATH");
    println!("{}", "-".repeat(50));
    let mut total: u64 = 0;
    for i in 0..archive.len() {
        let entry = archive.by_index(i)?;
        let size = entry.size();
        total += size;
        println!("{:<10}  {}", size, entry.name());
    }
    println!("{}", "-".repeat(50));
    println!("{:<10}  ({} files)", total, archive.len());

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
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
        f.write_all(serde_json::to_string_pretty(&manifest).unwrap().as_bytes())
            .unwrap();

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
        cmd_pack(&content_dir, Some(&out)).unwrap();
        assert!(out.exists());
    }

    #[test]
    fn test_pack_missing_manifest() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("empty");
        fs::create_dir_all(&content_dir).unwrap();
        assert!(cmd_pack(&content_dir, None).is_err());
    }

    #[test]
    fn test_roundtrip_hash_verification() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out = tmp.path().join("output.efpack");
        cmd_pack(&content_dir, Some(&out)).unwrap();

        let unpack_dir = tmp.path().join("unpacked");
        cmd_unpack(&out, Some(&unpack_dir)).unwrap();
        assert!(unpack_dir.join("manifest.efl").exists());
        assert!(unpack_dir.join("areas").join("test.json").exists());
    }

    #[test]
    fn test_safe_extract_path_strips_traversal() {
        let base = Path::new("/out");
        assert_eq!(safe_extract_path(base, "../../etc/passwd"), Path::new("/out/etc/passwd"));
        assert_eq!(safe_extract_path(base, "/absolute/path"), Path::new("/out/absolute/path"));
        assert_eq!(safe_extract_path(base, "normal/file.txt"), Path::new("/out/normal/file.txt"));
    }
}
