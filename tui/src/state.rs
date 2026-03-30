use std::path::PathBuf;

use crate::engine_state::EngineState;
use crate::pack::{InspectResult, PackResult, ValidationIssue, WatcherHandle};
use crate::pipe::PipeReader;
use crate::settings::AppSettings;

#[derive(Debug, Clone, PartialEq)]
pub enum Tab {
    Packs,
    Diagnostics,
    CreationKit,
}

pub enum PipeStatus {
    Searching,
    Connected(String),
    Disconnected,
    NoEngine,
}

pub struct PackFolder {
    pub path: PathBuf,
    pub name: String,
    pub has_manifest: bool,
}

pub struct PacksState {
    pub pack_folders: Vec<PackFolder>,
    pub selected_pack: Option<PathBuf>,
    pub watcher: Option<WatcherHandle>,
    pub watch_active: bool,
    pub last_pack_result: Option<Result<PackResult, String>>,
    pub last_inspect_result: Option<InspectResult>,
    pub last_validation: Vec<ValidationIssue>,
    pub first_run_modal_open: bool,
    pub projects_dir_draft: String,
    pub output_dir_draft: String,
}

impl PacksState {
    fn new(settings: &AppSettings) -> Self {
        let projects_dir_draft = settings
            .projects_dir
            .as_ref()
            .map(|p| p.to_string_lossy().into_owned())
            .unwrap_or_default();
        let output_dir_draft = settings
            .output_dir
            .as_ref()
            .map(|p| p.to_string_lossy().into_owned())
            .unwrap_or_default();

        Self {
            pack_folders: Vec::new(),
            selected_pack: None,
            watcher: None,
            watch_active: false,
            last_pack_result: None,
            last_inspect_result: None,
            last_validation: Vec::new(),
            first_run_modal_open: !settings.first_run_complete,
            projects_dir_draft,
            output_dir_draft,
        }
    }
}

pub struct DiagnosticsState {
    pub pipe_reader: Option<PipeReader>,
    pub pipe_status: PipeStatus,
}

impl DiagnosticsState {
    fn new() -> Self {
        Self {
            pipe_reader: None,
            pipe_status: PipeStatus::Searching,
        }
    }
}

pub struct AppState {
    pub settings: AppSettings,
    pub active_tab: Tab,
    pub ck_visible: bool,
    pub engine: EngineState,
    pub packs: PacksState,
    pub diag: DiagnosticsState,
}

impl AppState {
    pub fn new(settings: AppSettings) -> Self {
        let packs = PacksState::new(&settings);
        Self {
            packs,
            active_tab: Tab::Packs,
            ck_visible: false,
            engine: EngineState::new(),
            diag: DiagnosticsState::new(),
            settings,
        }
    }
}
