use std::{fs, path::Path};

use anyhow::Result;

/// A manifest validation issue.
#[derive(Debug, Clone)]
pub struct ValidationIssue {
    pub code: String,
    pub severity: String,
    pub message: String,
}

/// Validate a manifest.efl file against required field rules.
/// Returns an empty Vec if the manifest is valid.
pub fn validate_manifest(manifest_path: &Path) -> Result<Vec<ValidationIssue>> {
    let mut issues = Vec::new();

    let bytes = match fs::read(manifest_path) {
        Ok(b) => b,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: format!("Cannot read manifest.efl: {e}"),
            });
            return Ok(issues);
        }
    };

    let json: serde_json::Value = match serde_json::from_slice(&bytes) {
        Ok(v) => v,
        Err(e) => {
            issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: format!("Invalid JSON: {e}"),
            });
            return Ok(issues);
        }
    };

    // Required string fields
    let required = ["schemaVersion", "modId", "name", "version", "eflVersion"];
    for field in &required {
        match json.get(field) {
            None => issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: format!("Missing required field \"{field}\""),
            }),
            Some(v) if v.is_null() || (v.is_string() && v.as_str().unwrap_or("").is_empty()) => {
                issues.push(ValidationIssue {
                    code: "MANIFEST-E001".into(),
                    severity: "error".into(),
                    message: format!("Required field \"{field}\" is empty"),
                });
            }
            _ => {}
        }
    }

    // schemaVersion must be an integer
    if let Some(sv) = json.get("schemaVersion") {
        if !sv.is_number() || sv.as_f64().map(|f| f.fract() != 0.0).unwrap_or(false) {
            issues.push(ValidationIssue {
                code: "MANIFEST-E001".into(),
                severity: "error".into(),
                message: "Field \"schemaVersion\" must be an integer".into(),
            });
        }
    }

    // version should look like semver (contains at least one dot)
    if let Some(v) = json.get("version").and_then(|v| v.as_str()) {
        if !v.contains('.') {
            issues.push(ValidationIssue {
                code: "MANIFEST-W001".into(),
                severity: "warning".into(),
                message: format!("Field \"version\" \"{v}\" does not look like semver (expected x.y.z)"),
            });
        }
    }

    Ok(issues)
}
