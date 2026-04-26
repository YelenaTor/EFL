use std::{
    collections::HashMap,
    fs,
    io::{Read, Write},
    path::{Path, PathBuf},
};

use anyhow::{bail, Context, Result};
use chrono::Utc;
use zip::{write::SimpleFileOptions, ZipWriter};

use super::registry::{append_build, BuildHistoryEntry};
use super::shared::sha256_hex;

#[derive(Debug, Clone, Default, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct BuildConfigFile {
    #[serde(default)]
    pub output_dir: Option<PathBuf>,
    #[serde(default)]
    pub ci_mode: Option<bool>,
    #[serde(default)]
    pub manifest_path: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub enum BuildInput {
    ProjectFolder(PathBuf),
    ManifestEntrypoint(PathBuf),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum BuildKind {
    Efpack,
    Efdat,
}

#[derive(Debug, Clone, Default)]
pub struct BuildRequest {
    pub input: Option<BuildInput>,
    pub output_dir: Option<PathBuf>,
    pub config_file: Option<PathBuf>,
    pub ci_mode: bool,
}

impl BuildRequest {
    /// One-command defaults using an explicit project folder and output directory.
    pub fn one_command(project_folder: PathBuf, output_dir: PathBuf) -> Self {
        Self {
            input: Some(BuildInput::ProjectFolder(project_folder)),
            output_dir: Some(output_dir),
            config_file: None,
            ci_mode: false,
        }
    }

    /// CI mode contract sourced from environment variables.
    ///
    /// Supported variables:
    /// - EFL_PACK_PROJECT
    /// - EFL_PACK_MANIFEST
    /// - EFL_PACK_OUTPUT
    /// - EFL_PACK_CONFIG
    /// - EFL_PACK_CI (true/false/1/0/yes/no)
    pub fn from_ci_env() -> Result<Self> {
        let project = std::env::var_os("EFL_PACK_PROJECT").map(PathBuf::from);
        let manifest = std::env::var_os("EFL_PACK_MANIFEST").map(PathBuf::from);
        let output_dir = std::env::var_os("EFL_PACK_OUTPUT").map(PathBuf::from);
        let config_file = std::env::var_os("EFL_PACK_CONFIG").map(PathBuf::from);
        let ci_mode = std::env::var("EFL_PACK_CI")
            .ok()
            .map(|v| parse_bool(&v))
            .transpose()?
            .unwrap_or(true);

        let input = match (project, manifest) {
            (Some(_), Some(_)) => {
                bail!("CI input is ambiguous: set either EFL_PACK_PROJECT or EFL_PACK_MANIFEST, not both")
            }
            (Some(p), None) => Some(BuildInput::ProjectFolder(p)),
            (None, Some(m)) => Some(BuildInput::ManifestEntrypoint(m)),
            (None, None) => None,
        };

        Ok(Self {
            input,
            output_dir,
            config_file,
            ci_mode,
        })
    }
}

/// How many JSON files live in each content subdirectory.
#[derive(Debug, Clone, Default, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ContentInventory {
    pub areas: usize,
    pub warps: usize,
    pub npcs: usize,
    pub world_npcs: usize,
    pub resources: usize,
    pub recipes: usize,
    pub quests: usize,
    pub dialogue: usize,
    pub events: usize,
    pub calendar: usize,
    pub triggers: usize,
    pub assets_sprites: usize,
    pub assets_sounds: usize,
}

/// Result of a successful pack operation.
#[derive(Debug, Clone)]
pub struct PackResult {
    pub out_path: PathBuf,
    pub build_meta_path: PathBuf,
    pub mod_id: String,
    pub version: String,
    pub manifest_hash: String,
    /// zip-relative path → "sha256:<hex>"
    pub file_checksums: HashMap<String, String>,
    pub content_inventory: ContentInventory,
    /// Non-fatal asset validation messages (pack still succeeds).
    pub asset_issues: Vec<String>,
    /// Build-mode summary intended for UI and CI logs.
    pub build_summary: String,
}

// Dev artifacts to exclude from all archives.
const SKIP_NAMES: &[&str] = &["Thumbs.db", "desktop.ini"];

fn should_skip(file_name: &str) -> bool {
    if file_name.starts_with('.') {
        return true; // .DS_Store, .git*, etc.
    }
    if file_name.ends_with(".tmp") || file_name.ends_with(".bak") {
        return true;
    }
    SKIP_NAMES.contains(&file_name)
}

/// Pack a content directory into a versioned .efpack artifact.
///
/// Output goes to `<output_base_dir>/<modId>/<version>/<modId>-<version>.efpack`.
/// A `build-meta.json` sidecar is written alongside the artifact.
/// `<output_base_dir>/build-history.json` is updated with the new build entry.
#[allow(dead_code)]
pub fn pack_folder(content_dir: &Path, output_base_dir: &Path) -> Result<PackResult> {
    pack_with_request(&BuildRequest::one_command(
        content_dir.to_path_buf(),
        output_base_dir.to_path_buf(),
    ))
}

pub fn pack_with_request(request: &BuildRequest) -> Result<PackResult> {
    let mut merged = request.clone();
    if let Some(config_path) = &request.config_file {
        let config = read_build_config(config_path)?;
        merged = apply_config_overrides(merged, config);
    }

    let input = merged
        .input
        .clone()
        .ok_or_else(|| anyhow::anyhow!("BUILD-E001: no build input specified"))?;
    let output_base_dir = merged
        .output_dir
        .clone()
        .ok_or_else(|| anyhow::anyhow!("BUILD-E002: no output directory specified"))?;

    let (content_dir, entry_mode, kind) = resolve_input(input)?;
    let ci_mode = merged.ci_mode;

    let mut result = match kind {
        BuildKind::Efpack => pack_folder_impl(&content_dir, &output_base_dir)?,
        BuildKind::Efdat => pack_dat_impl(&content_dir, &output_base_dir)?,
    };
    result.build_summary = format!(
        "input={} | ciMode={} | output={}",
        entry_mode,
        ci_mode,
        output_base_dir.display()
    );
    Ok(result)
}

fn pack_folder_impl(content_dir: &Path, output_base_dir: &Path) -> Result<PackResult> {
    // ── 1. Read and parse manifest ──────────────────────────────────────────
    let manifest_path = content_dir.join("manifest.efl");
    if !manifest_path.exists() {
        bail!(
            "MANIFEST-E001: manifest.efl not found in {}",
            content_dir.display()
        );
    }

    let manifest_bytes =
        fs::read(&manifest_path).with_context(|| format!("reading {}", manifest_path.display()))?;

    let manifest_json: serde_json::Value =
        serde_json::from_slice(&manifest_bytes).context("parsing manifest.efl as JSON")?;

    let mod_id = manifest_json
        .get("modId")
        .and_then(|v| v.as_str())
        .ok_or_else(|| {
            anyhow::anyhow!("MANIFEST-E002: manifest.efl missing required field \"modId\"")
        })?
        .to_string();

    let version = manifest_json
        .get("version")
        .and_then(|v| v.as_str())
        .unwrap_or("0.0.0")
        .to_string();

    // ── 2. Compute versioned output path ─────────────────────────────────────
    let version_dir = output_base_dir.join(&mod_id).join(&version);
    fs::create_dir_all(&version_dir)
        .with_context(|| format!("creating version dir {}", version_dir.display()))?;

    let artifact_name = format!("{mod_id}-{version}.efpack");
    let out_path = version_dir.join(&artifact_name);

    // ── 3. Walk content directory, collect files (skip dev artifacts) ─────────
    let mut files: Vec<(PathBuf, String)> = Vec::new(); // (abs_path, zip_path)
    collect_files(content_dir, content_dir, &mut files)?;

    // ── 4. Asset validation ───────────────────────────────────────────────────
    let mut asset_issues = Vec::new();
    let features = manifest_json
        .get("features")
        .and_then(|v| v.as_array())
        .map(|arr| arr.iter().filter_map(|v| v.as_str()).collect::<Vec<_>>())
        .unwrap_or_default();

    if features.contains(&"assets") {
        validate_assets(&manifest_json, content_dir, &mut asset_issues);
    } else if content_dir.join("assets").exists() {
        asset_issues.push(
            "assets/ directory found but \"assets\" is not declared in manifest features".into(),
        );
    }

    // ── 5. Per-file checksums ─────────────────────────────────────────────────
    let manifest_hash = sha256_hex(&manifest_bytes);
    let mut file_checksums: HashMap<String, String> = HashMap::new();

    for (abs_path, zip_path) in &files {
        let bytes =
            fs::read(abs_path).with_context(|| format!("reading {}", abs_path.display()))?;
        file_checksums.insert(zip_path.clone(), sha256_hex(&bytes));
    }

    // ── 6. Content inventory ──────────────────────────────────────────────────
    let content_inventory = count_inventory(content_dir);

    // ── 7. Build pack-meta.json ───────────────────────────────────────────────
    let pack_meta = serde_json::json!({
        "eflVersion": "1.1.0",
        "packedAt": Utc::now().to_rfc3339(),
        "manifestHash": manifest_hash,
        "packerVersion": env!("CARGO_PKG_VERSION"),
        "fileChecksums": file_checksums,
        "contentInventory": content_inventory,
    });
    let pack_meta_bytes = serde_json::to_vec_pretty(&pack_meta)?;

    // ── 8. Build ZIP ──────────────────────────────────────────────────────────
    let out_file =
        fs::File::create(&out_path).with_context(|| format!("creating {}", out_path.display()))?;
    let mut zip = ZipWriter::new(out_file);
    let options = SimpleFileOptions::default().compression_method(zip::CompressionMethod::Deflated);

    for (abs_path, zip_path) in &files {
        let bytes =
            fs::read(abs_path).with_context(|| format!("reading {}", abs_path.display()))?;
        zip.start_file(zip_path, options)?;
        zip.write_all(&bytes)?;
    }

    zip.start_file("pack-meta.json", options)?;
    zip.write_all(&pack_meta_bytes)?;
    zip.finish()?;

    // ── 9. Write build-meta.json sidecar ──────────────────────────────────────
    let mut build_meta = pack_meta.clone();
    build_meta["artifactFile"] = serde_json::Value::String(artifact_name.clone());
    let build_meta_path = version_dir.join("build-meta.json");
    fs::write(&build_meta_path, serde_json::to_vec_pretty(&build_meta)?)
        .with_context(|| format!("writing {}", build_meta_path.display()))?;

    // ── 10. Update build-history.json ─────────────────────────────────────────
    let relative_artifact = format!("{}/{}/{}", mod_id, version, artifact_name);
    let registry_entry = BuildHistoryEntry {
        mod_id: mod_id.clone(),
        version: version.clone(),
        built_at: pack_meta["packedAt"].as_str().unwrap_or("").to_string(),
        artifact_path: relative_artifact,
        manifest_hash: manifest_hash.clone(),
    };
    append_build(output_base_dir, &registry_entry)
        .with_context(|| "updating build-history.json")?;

    Ok(PackResult {
        out_path,
        build_meta_path,
        mod_id,
        version,
        manifest_hash,
        file_checksums,
        content_inventory,
        asset_issues,
        build_summary: "input=project-folder | ciMode=false".into(),
    })
}

pub fn pack_dat_folder(content_dir: &Path, output_base_dir: &Path) -> Result<PackResult> {
    pack_dat_impl(content_dir, output_base_dir)
}

fn pack_dat_impl(content_dir: &Path, output_base_dir: &Path) -> Result<PackResult> {
    let manifest_path = content_dir.join("manifest.efdat");
    if !manifest_path.exists() {
        bail!(
            "DAT-E001: manifest.efdat not found in {}",
            content_dir.display()
        );
    }

    let manifest_bytes =
        fs::read(&manifest_path).with_context(|| format!("reading {}", manifest_path.display()))?;
    let manifest_json: serde_json::Value =
        serde_json::from_slice(&manifest_bytes).context("parsing manifest.efdat as JSON")?;

    let dat_id = manifest_json
        .get("datId")
        .and_then(|v| v.as_str())
        .ok_or_else(|| {
            anyhow::anyhow!("DAT-E002: manifest.efdat missing required field \"datId\"")
        })?
        .to_string();
    let version = manifest_json
        .get("version")
        .and_then(|v| v.as_str())
        .unwrap_or("0.0.0")
        .to_string();

    let version_dir = output_base_dir.join(&dat_id).join(&version);
    fs::create_dir_all(&version_dir)
        .with_context(|| format!("creating version dir {}", version_dir.display()))?;
    let artifact_name = format!("{dat_id}-{version}.efdat");
    let out_path = version_dir.join(&artifact_name);

    let mut files: Vec<(PathBuf, String)> = Vec::new();
    collect_files(content_dir, content_dir, &mut files)?;
    // efdat artifacts should only carry manifest + optional metadata docs.
    files.retain(|(_, zip_path)| {
        zip_path.eq_ignore_ascii_case("manifest.efdat")
            || zip_path.eq_ignore_ascii_case("README.md")
            || zip_path.eq_ignore_ascii_case("NOTES.md")
    });
    if files
        .iter()
        .all(|(_, p)| !p.eq_ignore_ascii_case("manifest.efdat"))
    {
        files.push((manifest_path.clone(), "manifest.efdat".into()));
    }

    let manifest_hash = sha256_hex(&manifest_bytes);
    let mut file_checksums: HashMap<String, String> = HashMap::new();
    for (abs_path, zip_path) in &files {
        let bytes =
            fs::read(abs_path).with_context(|| format!("reading {}", abs_path.display()))?;
        file_checksums.insert(zip_path.clone(), sha256_hex(&bytes));
    }

    let relationship_count = manifest_json
        .get("relationships")
        .and_then(|v| v.as_array())
        .map(|a| a.len())
        .unwrap_or(0);
    let pack_meta = serde_json::json!({
        "eflVersion": "1.1.0",
        "packedAt": Utc::now().to_rfc3339(),
        "manifestHash": manifest_hash,
        "packerVersion": env!("CARGO_PKG_VERSION"),
        "fileChecksums": file_checksums,
        "artifactType": "efdat",
        "relationshipCount": relationship_count,
    });
    let pack_meta_bytes = serde_json::to_vec_pretty(&pack_meta)?;

    let out_file =
        fs::File::create(&out_path).with_context(|| format!("creating {}", out_path.display()))?;
    let mut zip = ZipWriter::new(out_file);
    let options = SimpleFileOptions::default().compression_method(zip::CompressionMethod::Deflated);
    for (abs_path, zip_path) in &files {
        let bytes =
            fs::read(abs_path).with_context(|| format!("reading {}", abs_path.display()))?;
        zip.start_file(zip_path, options)?;
        zip.write_all(&bytes)?;
    }
    zip.start_file("pack-meta.json", options)?;
    zip.write_all(&pack_meta_bytes)?;
    zip.finish()?;

    let mut build_meta = pack_meta.clone();
    build_meta["artifactFile"] = serde_json::Value::String(artifact_name.clone());
    let build_meta_path = version_dir.join("build-meta.json");
    fs::write(&build_meta_path, serde_json::to_vec_pretty(&build_meta)?)
        .with_context(|| format!("writing {}", build_meta_path.display()))?;

    let relative_artifact = format!("{}/{}/{}", dat_id, version, artifact_name);
    let registry_entry = BuildHistoryEntry {
        mod_id: dat_id.clone(),
        version: version.clone(),
        built_at: pack_meta["packedAt"].as_str().unwrap_or("").to_string(),
        artifact_path: relative_artifact,
        manifest_hash: manifest_hash.clone(),
    };
    append_build(output_base_dir, &registry_entry)
        .with_context(|| "updating build-history.json")?;

    Ok(PackResult {
        out_path,
        build_meta_path,
        mod_id: dat_id,
        version,
        manifest_hash,
        file_checksums,
        content_inventory: ContentInventory::default(),
        asset_issues: Vec::new(),
        build_summary: format!(
            "input=project-folder | ciMode=false | artifact=efdat | relationships={relationship_count}"
        ),
    })
}

fn resolve_input(input: BuildInput) -> Result<(PathBuf, &'static str, BuildKind)> {
    match input {
        BuildInput::ProjectFolder(path) => {
            if path.join("manifest.efdat").exists() {
                Ok((path, "project-folder", BuildKind::Efdat))
            } else {
                Ok((path, "project-folder", BuildKind::Efpack))
            }
        }
        BuildInput::ManifestEntrypoint(path) => {
            let Some(name) = path.file_name().and_then(|n| n.to_str()) else {
                bail!(
                    "BUILD-E003: manifest entrypoint must point to manifest.efl or manifest.efdat, got {}",
                    path.display()
                );
            };
            let kind = if name.eq_ignore_ascii_case("manifest.efl") {
                BuildKind::Efpack
            } else if name.eq_ignore_ascii_case("manifest.efdat") {
                BuildKind::Efdat
            } else {
                bail!(
                    "BUILD-E003: manifest entrypoint must point to manifest.efl or manifest.efdat, got {}",
                    path.display()
                );
            };
            let parent = path.parent().ok_or_else(|| {
                anyhow::anyhow!("BUILD-E004: manifest path has no parent directory")
            })?;
            Ok((parent.to_path_buf(), "manifest-entrypoint", kind))
        }
    }
}

fn read_build_config(path: &Path) -> Result<BuildConfigFile> {
    let bytes =
        fs::read(path).with_context(|| format!("reading build config {}", path.display()))?;
    serde_json::from_slice::<BuildConfigFile>(&bytes)
        .with_context(|| format!("parsing build config {}", path.display()))
}

fn apply_config_overrides(mut request: BuildRequest, config: BuildConfigFile) -> BuildRequest {
    if request.output_dir.is_none() {
        request.output_dir = config.output_dir;
    }
    if !request.ci_mode {
        request.ci_mode = config.ci_mode.unwrap_or(false);
    }
    if request.input.is_none() {
        request.input = config.manifest_path.map(BuildInput::ManifestEntrypoint);
    }
    request
}

fn parse_bool(value: &str) -> Result<bool> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        _ => bail!("invalid boolean value \"{value}\""),
    }
}

/// Recursively collect non-skipped files under `dir`, relative to `base`.
fn collect_files(base: &Path, dir: &Path, out: &mut Vec<(PathBuf, String)>) -> Result<()> {
    for entry in fs::read_dir(dir).with_context(|| format!("reading dir {}", dir.display()))? {
        let entry = entry?;
        let path = entry.path();
        let file_name = path
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();

        if should_skip(&file_name) {
            continue;
        }

        if path.is_dir() {
            collect_files(base, &path, out)?;
        } else {
            let rel = path.strip_prefix(base).context("strip prefix")?;
            let zip_path = rel.to_string_lossy().replace('\\', "/");
            out.push((path, zip_path));
        }
    }
    Ok(())
}

/// Count .json files in each named content subdirectory.
fn count_inventory(content_dir: &Path) -> ContentInventory {
    let count = |subdir: &str| -> usize {
        let dir = content_dir.join(subdir);
        if !dir.is_dir() {
            return 0;
        }
        fs::read_dir(&dir)
            .map(|entries| {
                entries
                    .filter_map(|e| e.ok())
                    .filter(|e| e.path().extension().map(|x| x == "json").unwrap_or(false))
                    .count()
            })
            .unwrap_or(0)
    };

    ContentInventory {
        areas: count("areas"),
        warps: count("warps"),
        npcs: count("npcs"),
        world_npcs: count("world_npcs"),
        resources: count("resources"),
        recipes: count("recipes"),
        quests: count("quests"),
        dialogue: count("dialogue"),
        events: count("events"),
        calendar: count("calendar"),
        triggers: count("triggers"),
        assets_sprites: count_files_in(content_dir, "assets/sprites"),
        assets_sounds: count_files_in(content_dir, "assets/sounds"),
    }
}

fn count_files_in(base: &Path, subdir: &str) -> usize {
    let dir = base.join(subdir);
    if !dir.is_dir() {
        return 0;
    }
    fs::read_dir(&dir)
        .map(|entries| {
            entries
                .filter_map(|e| e.ok())
                .filter(|e| e.path().is_file())
                .count()
        })
        .unwrap_or(0)
}

/// Validate asset declarations in the manifest against actual files on disk.
fn validate_assets(manifest: &serde_json::Value, content_dir: &Path, issues: &mut Vec<String>) {
    let assets = match manifest.get("assets") {
        Some(a) => a,
        None => return,
    };

    if let Some(sprites) = assets.get("sprites").and_then(|v| v.as_array()) {
        for sprite in sprites {
            let id = match sprite.as_str() {
                Some(s) => s,
                None => continue,
            };
            let path = content_dir
                .join("assets")
                .join("sprites")
                .join(format!("{id}.png"));
            if !path.exists() {
                issues.push(format!(
                    "sprite '{id}': file not found at assets/sprites/{id}.png"
                ));
                continue;
            }
            if !has_magic(&path, &[0x89, 0x50, 0x4E, 0x47]) {
                issues.push(format!(
                    "sprite '{id}': assets/sprites/{id}.png does not appear to be a valid PNG"
                ));
            }
        }
    }

    if let Some(sounds) = assets.get("sounds").and_then(|v| v.as_array()) {
        for sound in sounds {
            let id = match sound.as_str() {
                Some(s) => s,
                None => continue,
            };
            // Accept .ogg or .wav
            let ogg_path = content_dir
                .join("assets")
                .join("sounds")
                .join(format!("{id}.ogg"));
            let wav_path = content_dir
                .join("assets")
                .join("sounds")
                .join(format!("{id}.wav"));

            if ogg_path.exists() {
                if !has_magic(&ogg_path, &[0x4F, 0x67, 0x67, 0x53]) {
                    issues.push(format!(
                        "sound '{id}': assets/sounds/{id}.ogg does not appear to be a valid OGG file"
                    ));
                }
            } else if wav_path.exists() {
                if !has_magic(&wav_path, &[0x52, 0x49, 0x46, 0x46]) {
                    issues.push(format!(
                        "sound '{id}': assets/sounds/{id}.wav does not appear to be a valid WAV file"
                    ));
                }
            } else {
                issues.push(format!(
                    "sound '{id}': no file found at assets/sounds/{id}.ogg or .wav"
                ));
            }
        }
    }
}

/// Check that `path` starts with the given magic bytes.
fn has_magic(path: &Path, magic: &[u8]) -> bool {
    let Ok(mut f) = fs::File::open(path) else {
        return false;
    };
    let mut buf = vec![0u8; magic.len()];
    matches!(f.read_exact(&mut buf), Ok(())) && buf == magic
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn make_manifest(dir: &Path, mod_id: &str, version: &str, features: &[&str]) {
        let manifest = serde_json::json!({
            "schemaVersion": 2,
            "modId": mod_id,
            "name": "Test Mod",
            "version": version,
            "eflVersion": "1.1.0",
            "features": features,
        });
        fs::write(
            dir.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
    }

    fn make_content_dir(dir: &Path) {
        make_manifest(dir, "com.test.example", "1.2.3", &[]);
        fs::create_dir_all(dir.join("areas")).unwrap();
        fs::write(dir.join("areas").join("test.json"), b"{\"id\":\"test\"}").unwrap();
    }

    fn run_pack(content_dir: &Path) -> (PackResult, TempDir) {
        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(content_dir, out_tmp.path()).unwrap();
        (result, out_tmp)
    }

    #[test]
    fn test_pack_creates_efpack() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let (result, _out_tmp) = run_pack(&content_dir);
        assert!(result.out_path.exists());
        assert_eq!(result.mod_id, "com.test.example");
        assert_eq!(result.version, "1.2.3");
    }

    #[test]
    fn test_version_subdir_output() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(&content_dir, out_tmp.path()).unwrap();

        let expected = out_tmp
            .path()
            .join("com.test.example")
            .join("1.2.3")
            .join("com.test.example-1.2.3.efpack");
        assert_eq!(result.out_path, expected);
        assert!(result.out_path.exists());
    }

    #[test]
    fn test_build_meta_sidecar() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(&content_dir, out_tmp.path()).unwrap();

        assert!(
            result.build_meta_path.exists(),
            "build-meta.json sidecar must exist"
        );
        let meta: serde_json::Value =
            serde_json::from_slice(&fs::read(&result.build_meta_path).unwrap()).unwrap();
        assert_eq!(
            meta["artifactFile"].as_str().unwrap(),
            "com.test.example-1.2.3.efpack"
        );
        assert!(meta.get("manifestHash").is_some());
        assert!(meta.get("fileChecksums").is_some());
    }

    #[test]
    fn test_per_file_checksums() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let (result, _out_tmp) = run_pack(&content_dir);
        assert!(result.file_checksums.contains_key("manifest.efl"));
        assert!(result.file_checksums.contains_key("areas/test.json"));
        for v in result.file_checksums.values() {
            assert!(
                v.starts_with("sha256:"),
                "checksums must be prefixed with sha256:"
            );
        }
    }

    #[test]
    fn test_content_inventory() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_manifest(&content_dir, "com.test.inv", "1.1.0", &[]);

        fs::create_dir_all(content_dir.join("areas")).unwrap();
        fs::write(content_dir.join("areas").join("a.json"), b"{}").unwrap();
        fs::write(content_dir.join("areas").join("b.json"), b"{}").unwrap();
        fs::create_dir_all(content_dir.join("npcs")).unwrap();
        fs::write(content_dir.join("npcs").join("npc1.json"), b"{}").unwrap();

        let (result, _out_tmp) = run_pack(&content_dir);
        assert_eq!(result.content_inventory.areas, 2);
        assert_eq!(result.content_inventory.npcs, 1);
        assert_eq!(result.content_inventory.quests, 0);
    }

    #[test]
    fn test_dev_artifact_stripping() {
        use zip::ZipArchive;

        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        // Add dev artifacts
        fs::write(content_dir.join(".DS_Store"), b"junk").unwrap();
        fs::write(content_dir.join("Thumbs.db"), b"junk").unwrap();
        fs::write(content_dir.join("areas").join("temp.tmp"), b"junk").unwrap();

        let (result, _out_tmp) = run_pack(&content_dir);

        let f = fs::File::open(&result.out_path).unwrap();
        let mut archive = ZipArchive::new(f).unwrap();
        let names: Vec<String> = (0..archive.len())
            .map(|i| archive.by_index(i).unwrap().name().to_string())
            .collect();
        assert!(!names.iter().any(|n| n.contains(".DS_Store")));
        assert!(!names.iter().any(|n| n.contains("Thumbs.db")));
        assert!(!names.iter().any(|n| n.ends_with(".tmp")));
    }

    #[test]
    fn test_pack_missing_manifest() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("empty");
        fs::create_dir_all(&content_dir).unwrap();
        let out_tmp = TempDir::new().unwrap();
        assert!(pack_folder(&content_dir, out_tmp.path()).is_err());
    }

    #[test]
    fn test_roundtrip_produces_valid_archive() {
        use zip::ZipArchive;

        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let (result, _out_tmp) = run_pack(&content_dir);

        let f = fs::File::open(&result.out_path).unwrap();
        let mut archive = ZipArchive::new(f).unwrap();
        let names: Vec<String> = (0..archive.len())
            .map(|i| archive.by_index(i).unwrap().name().to_string())
            .collect();
        assert!(names.iter().any(|n| n == "manifest.efl"));
        assert!(names.iter().any(|n| n == "pack-meta.json"));
    }

    #[test]
    fn test_registry_updated_after_pack() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();
        make_content_dir(&content_dir);

        let out_tmp = TempDir::new().unwrap();
        pack_folder(&content_dir, out_tmp.path()).unwrap();

        let history = super::super::registry::read_history(out_tmp.path()).unwrap();
        assert_eq!(history.len(), 1);
        assert_eq!(history[0].mod_id, "com.test.example");
        assert_eq!(history[0].version, "1.2.3");
    }

    #[test]
    fn test_asset_validation_missing_file() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(&content_dir).unwrap();

        let manifest = serde_json::json!({
            "schemaVersion": 2, "modId": "com.test.assets", "name": "A",
            "version": "1.1.0", "eflVersion": "1.1.0",
            "features": ["assets"],
            "assets": { "sprites": ["spr_MyNpc"], "sounds": [] }
        });
        fs::write(
            content_dir.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        // No assets/sprites/spr_MyNpc.png created.

        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(&content_dir, out_tmp.path()).unwrap();
        assert!(
            !result.asset_issues.is_empty(),
            "should flag missing sprite file"
        );
        assert!(result.asset_issues[0].contains("spr_MyNpc"));
    }

    #[test]
    fn test_asset_validation_bad_magic() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(content_dir.join("assets").join("sprites")).unwrap();

        let manifest = serde_json::json!({
            "schemaVersion": 2, "modId": "com.test.assets", "name": "A",
            "version": "1.1.0", "eflVersion": "1.1.0",
            "features": ["assets"],
            "assets": { "sprites": ["spr_Bad"], "sounds": [] }
        });
        fs::write(
            content_dir.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        // Write a file with wrong magic bytes.
        fs::write(
            content_dir
                .join("assets")
                .join("sprites")
                .join("spr_Bad.png"),
            b"NOT A PNG",
        )
        .unwrap();

        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(&content_dir, out_tmp.path()).unwrap();
        assert!(
            !result.asset_issues.is_empty(),
            "should flag invalid PNG magic"
        );
        assert!(result.asset_issues[0].contains("spr_Bad"));
    }

    #[test]
    fn test_asset_validation_clean() {
        let tmp = TempDir::new().unwrap();
        let content_dir = tmp.path().join("mod");
        fs::create_dir_all(content_dir.join("assets").join("sprites")).unwrap();

        let manifest = serde_json::json!({
            "schemaVersion": 2, "modId": "com.test.assets", "name": "A",
            "version": "1.1.0", "eflVersion": "1.1.0",
            "features": ["assets"],
            "assets": { "sprites": ["spr_Good"], "sounds": [] }
        });
        fs::write(
            content_dir.join("manifest.efl"),
            serde_json::to_string_pretty(&manifest).unwrap(),
        )
        .unwrap();
        // Write a valid PNG magic header.
        let mut png_bytes = vec![0x89u8, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
        png_bytes.extend_from_slice(b"fake png body");
        fs::write(
            content_dir
                .join("assets")
                .join("sprites")
                .join("spr_Good.png"),
            &png_bytes,
        )
        .unwrap();

        let out_tmp = TempDir::new().unwrap();
        let result = pack_folder(&content_dir, out_tmp.path()).unwrap();
        assert!(
            result.asset_issues.is_empty(),
            "valid PNG should produce no issues: {:?}",
            result.asset_issues
        );
    }
}
