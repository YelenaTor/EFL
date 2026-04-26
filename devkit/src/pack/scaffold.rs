use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::{bail, Result};

pub struct NewPackOptions {
    pub display_name: String,
    pub mod_id: String,
    pub author: String,
    pub version: String,
    pub description: String,
    pub efl_version: String,
    pub features: Vec<String>,
    pub uses_momi: bool,
}

/// Folders to create for each feature tag.
fn folders_for_feature(feature: &str) -> &'static [&'static str] {
    match feature {
        "areas" => &["areas"],
        "warps" => &["warps"],
        "resources" => &["resources"],
        "crafting" => &["recipes"],
        "npcs" => &["npcs", "world_npcs"],
        "quests" => &["quests"],
        "triggers" => &["triggers"],
        "dialogue" => &["dialogue"],
        "story" => &["events"],
        "assets" => &["assets/sprites", "assets/sounds"],
        "calendar" => &["calendar"],
        _ => &[],
    }
}

/// Scaffold a new content pack under `<projects_dir>/<mod_id>/`.
/// Returns the path to the created pack directory.
pub fn scaffold_pack(projects_dir: &Path, opts: &NewPackOptions) -> Result<PathBuf> {
    if opts.mod_id.is_empty() {
        bail!("modId cannot be empty");
    }
    if opts.display_name.is_empty() {
        bail!("Display name cannot be empty");
    }

    let pack_dir = projects_dir.join(&opts.mod_id);
    if pack_dir.exists() {
        bail!("A pack folder named '{}' already exists", opts.mod_id);
    }

    fs::create_dir_all(&pack_dir)?;

    // Create feature subdirectories.
    for feature in &opts.features {
        for folder in folders_for_feature(feature) {
            fs::create_dir_all(pack_dir.join(folder))?;
        }
    }

    // Build manifest.efl.
    let mut manifest = serde_json::json!({
        "schemaVersion": 2,
        "modId": opts.mod_id,
        "name": opts.display_name,
        "version": if opts.version.is_empty() { "1.1.0" } else { &opts.version },
        "eflVersion": opts.efl_version,
        "features": opts.features,
    });

    if opts.features.iter().any(|f| f == "areas") {
        manifest["settings"] = serde_json::json!({
            "areaBackend": "native"
        });
    }

    if !opts.author.is_empty() {
        manifest["author"] = serde_json::Value::String(opts.author.clone());
    }
    if !opts.description.is_empty() {
        manifest["description"] = serde_json::Value::String(opts.description.clone());
    }

    let manifest_bytes = serde_json::to_vec_pretty(&manifest)?;
    fs::write(pack_dir.join("manifest.efl"), manifest_bytes)?;

    if opts.uses_momi {
        let dat = serde_json::json!({
            "schemaVersion": 1,
            "datId": format!("{}.momi.compat", opts.mod_id),
            "name": format!("{} MOMI Compatibility", opts.display_name),
            "version": if opts.version.is_empty() { "1.1.0" } else { &opts.version },
            "eflVersion": opts.efl_version,
            "description": format!(
                "Compatibility shim for {} when used with MOMI-provided content.",
                opts.display_name
            ),
            "relationships": [
                {
                    "type": "requires",
                    "target": {
                        "kind": "efpack",
                        "id": opts.mod_id,
                        "versionRange": "^1.0"
                    }
                },
                {
                    "type": "optional",
                    "target": {
                        "kind": "momi",
                        "id": "example.momi.mod",
                        "versionRange": ">=1.1.0"
                    },
                    "reason": "Replace this placeholder with the MOMI mod your pack integrates with."
                }
            ]
        });
        let dat_name = format!("{}.MOMI.efdat", opts.mod_id);
        fs::write(pack_dir.join(dat_name), serde_json::to_vec_pretty(&dat)?)?;
    }

    Ok(pack_dir)
}

/// Slugify a display name into a mod-id-safe lowercase token.
/// "Crystal Caverns Expansion" → "crystal-caverns-expansion"
pub fn slugify(name: &str) -> String {
    name.trim()
        .to_lowercase()
        .chars()
        .map(|c| {
            if c.is_alphanumeric() || c == '-' || c == '.' {
                c
            } else {
                '-'
            }
        })
        .collect::<String>()
        .split('-')
        .filter(|s| !s.is_empty())
        .collect::<Vec<_>>()
        .join("-")
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn opts(mod_id: &str, features: &[&str]) -> NewPackOptions {
        NewPackOptions {
            display_name: "Test Pack".into(),
            mod_id: mod_id.into(),
            author: "Yoru".into(),
            version: "1.1.0".into(),
            description: "".into(),
            efl_version: "1.1.0".into(),
            features: features.iter().map(|s| s.to_string()).collect(),
            uses_momi: false,
        }
    }

    #[test]
    fn test_scaffold_creates_directory_and_manifest() {
        let tmp = TempDir::new().unwrap();
        let pack_dir = scaffold_pack(tmp.path(), &opts("com.test.mod", &[])).unwrap();
        assert!(pack_dir.exists());
        assert!(pack_dir.join("manifest.efl").exists());
    }

    #[test]
    fn test_scaffold_manifest_fields() {
        let tmp = TempDir::new().unwrap();
        scaffold_pack(tmp.path(), &opts("com.test.mod", &[])).unwrap();
        let bytes = std::fs::read(tmp.path().join("com.test.mod").join("manifest.efl")).unwrap();
        let v: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(v["modId"].as_str().unwrap(), "com.test.mod");
        assert_eq!(v["name"].as_str().unwrap(), "Test Pack");
        assert_eq!(v["author"].as_str().unwrap(), "Yoru");
    }

    #[test]
    fn test_scaffold_feature_folders() {
        let tmp = TempDir::new().unwrap();
        let pack_dir = scaffold_pack(
            tmp.path(),
            &opts("com.test.mod", &["areas", "npcs", "assets"]),
        )
        .unwrap();
        assert!(pack_dir.join("areas").is_dir());
        assert!(pack_dir.join("npcs").is_dir());
        assert!(pack_dir.join("world_npcs").is_dir());
        assert!(pack_dir.join("assets").join("sprites").is_dir());
        assert!(pack_dir.join("assets").join("sounds").is_dir());
    }

    #[test]
    fn test_scaffold_rejects_existing_dir() {
        let tmp = TempDir::new().unwrap();
        std::fs::create_dir_all(tmp.path().join("com.test.mod")).unwrap();
        assert!(scaffold_pack(tmp.path(), &opts("com.test.mod", &[])).is_err());
    }

    #[test]
    fn test_scaffold_areas_sets_native_backend() {
        let tmp = TempDir::new().unwrap();
        scaffold_pack(tmp.path(), &opts("com.test.mod", &["areas"])).unwrap();
        let bytes = std::fs::read(tmp.path().join("com.test.mod").join("manifest.efl")).unwrap();
        let v: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(v["settings"]["areaBackend"].as_str().unwrap(), "native");
    }

    #[test]
    fn test_scaffold_writes_momi_efdat_when_enabled() {
        let tmp = TempDir::new().unwrap();
        let mut o = opts("com.test.mod", &[]);
        o.uses_momi = true;
        scaffold_pack(tmp.path(), &o).unwrap();
        assert!(tmp
            .path()
            .join("com.test.mod")
            .join("com.test.mod.MOMI.efdat")
            .exists());
    }

    #[test]
    fn test_slugify() {
        assert_eq!(
            slugify("Crystal Caverns Expansion"),
            "crystal-caverns-expansion"
        );
        assert_eq!(slugify("  My  Pack! "), "my-pack");
        assert_eq!(slugify("com.yoru.mod"), "com.yoru.mod");
    }

    #[test]
    fn test_slugify_collapses_dashes() {
        // slugify should not produce consecutive dashes
        let s = slugify("My  Pack!!");
        assert!(!s.contains("--"), "Got: {s}");
    }
}
