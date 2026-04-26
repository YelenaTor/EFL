use std::{fs, path::PathBuf};

use anyhow::Result;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppSettings {
    pub projects_dir: Option<PathBuf>,
    pub output_dir: Option<PathBuf>,
    pub first_run_complete: bool,
    pub window_size: [f32; 2],
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            projects_dir: None,
            output_dir: None,
            first_run_complete: false,
            window_size: [1200.0, 800.0],
        }
    }
}

impl AppSettings {
    pub fn load() -> Self {
        let path = Self::settings_path();
        match fs::read(&path) {
            Ok(bytes) => serde_json::from_slice(&bytes).unwrap_or_default(),
            Err(_) => Self::default(),
        }
    }

    pub fn save(&self) -> Result<()> {
        let path = Self::settings_path();
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let json = serde_json::to_vec_pretty(self)?;
        fs::write(&path, json)?;
        Ok(())
    }

    fn settings_path() -> PathBuf {
        dirs::data_local_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join("EFLDevKit")
            .join("settings.json")
    }
}
