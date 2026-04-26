use std::path::PathBuf;

use crate::engine_state::EngineState;
use crate::pack::{
    InspectResult, MigrationApplyResult, MigrationReport, PackResult, ValidationIssue,
    ValidationProfile, WatcherHandle,
};
use crate::pipe::PipeReader;
use crate::settings::AppSettings;

/// All EFL content features, in display order.
pub const ALL_FEATURES: &[(&str, &str)] = &[
    ("areas", "Areas"),
    ("warps", "Warps"),
    ("npcs", "NPCs"),
    ("resources", "Resources"),
    ("crafting", "Crafting"),
    ("quests", "Quests"),
    ("dialogue", "Dialogue"),
    ("story", "Story / Cutscenes"),
    ("triggers", "Triggers"),
    ("assets", "Assets (sprites & sounds)"),
    ("ipc", "IPC channels"),
];

/// Live editing state for the New Pack wizard.
pub struct NewPackDraft {
    pub display_name: String,
    pub mod_id: String,
    pub mod_id_user_edited: bool, // stops auto-mirroring display_name → mod_id
    pub author: String,
    pub version: String,
    pub description: String,
    pub selected_features: Vec<bool>, // parallel to ALL_FEATURES
    pub error: Option<String>,
}

impl Default for NewPackDraft {
    fn default() -> Self {
        Self {
            display_name: String::new(),
            mod_id: String::new(),
            mod_id_user_edited: false,
            author: String::new(),
            version: "1.0.0".into(),
            description: String::new(),
            selected_features: vec![false; ALL_FEATURES.len()],
            error: None,
        }
    }
}

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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MomiFilter {
    All,
    Active,
    Inactive,
    Conflicts,
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
    pub last_watch_summary: Option<String>,
    pub build_use_manifest_entry: bool,
    pub build_config_path: String,
    pub build_ci_mode: bool,
    pub build_use_ci_env: bool,
    pub build_help_modal_open: bool,
    pub last_pack_result: Option<Result<PackResult, String>>,
    pub last_inspect_result: Option<InspectResult>,
    pub last_validation: Vec<ValidationIssue>,
    pub validation_profile: ValidationProfile,
    /// Auto-send a `reload` command to the running engine after every
    /// successful manual or watch-triggered build.
    pub auto_reload_on_build: bool,
    /// Most recent reload signal status, surfaced under the build result.
    pub last_reload_status: Option<Result<String, String>>,
    pub migration_modal_open: bool,
    pub migration_report: Option<MigrationReport>,
    pub migration_apply_result: Option<MigrationApplyResult>,
    pub migration_error: Option<String>,
    /// Folder where the most recent .efpack/.efdat was extracted for
    /// edit-in-place. Cleared on a successful repack.
    pub edit_in_place_workspace: Option<PathBuf>,
    /// Original archive that the current edit-in-place workspace came from.
    pub edit_in_place_source: Option<PathBuf>,
    /// Status string shown under the action row after an extract or repack.
    pub edit_in_place_status: Option<Result<String, String>>,
    pub first_run_modal_open: bool,
    pub projects_dir_draft: String,
    pub output_dir_draft: String,
    pub new_pack_modal_open: bool,
    pub new_pack_draft: NewPackDraft,
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
            last_watch_summary: None,
            build_use_manifest_entry: false,
            build_config_path: String::new(),
            build_ci_mode: false,
            build_use_ci_env: false,
            build_help_modal_open: false,
            last_pack_result: None,
            last_inspect_result: None,
            last_validation: Vec::new(),
            validation_profile: ValidationProfile::Recommended,
            auto_reload_on_build: true,
            last_reload_status: None,
            migration_modal_open: false,
            migration_report: None,
            migration_apply_result: None,
            migration_error: None,
            edit_in_place_workspace: None,
            edit_in_place_source: None,
            edit_in_place_status: None,
            first_run_modal_open: !settings.first_run_complete,
            projects_dir_draft,
            output_dir_draft,
            new_pack_modal_open: false,
            new_pack_draft: NewPackDraft::default(),
        }
    }
}

pub struct DiagnosticsState {
    pub pipe_reader: Option<PipeReader>,
    pub pipe_status: PipeStatus,
    /// Filter: when true, entries of this severity are visible in the log.
    /// Default: all severities visible.
    pub show_errors: bool,
    pub show_warnings: bool,
    pub show_hazards: bool,
    /// Optional category filter. When `Some`, only entries with this exact
    /// category are visible. When `None`, all categories pass through.
    /// The category set is derived dynamically from the live diagnostic log.
    pub category_filter: Option<String>,
    /// MOMI monitor filter chip selection.
    pub momi_filter: MomiFilter,
    /// Selected MOMI mod id for the popout graph/details panel.
    pub selected_momi_mod: Option<String>,
    /// Whether the MOMI popout window is open.
    pub momi_popout_open: bool,
}

impl DiagnosticsState {
    fn new() -> Self {
        Self {
            pipe_reader: None,
            pipe_status: PipeStatus::Searching,
            show_errors: true,
            show_warnings: true,
            show_hazards: true,
            category_filter: None,
            momi_filter: MomiFilter::All,
            selected_momi_mod: None,
            momi_popout_open: false,
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
