pub mod builder;
pub mod inspector;
pub mod migration;
pub mod registry;
pub mod scaffold;
mod shared;
pub mod validator;
pub mod watcher;

pub use builder::{pack_dat_folder, pack_with_request, BuildInput, BuildRequest, PackResult};
pub use inspector::{
    extract_archive, inspect_efdat, inspect_efpack, patch_workspace_entry,
    read_archive_entry_bytes, ExtractResult, InspectResult,
};
pub use migration::{analyze_pack, apply_migration, MigrationApplyResult, MigrationReport};
pub use scaffold::{scaffold_pack, slugify, NewPackOptions};
pub use validator::{
    validate_dat_with_profile, validate_manifest_with_capabilities, validate_manifest_with_profile,
    EngineCapabilities, ValidationIssue, ValidationProfile,
};
pub use watcher::WatcherHandle;
