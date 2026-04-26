// Library surface for the efl-devkit crate.
//
// This exists so additional binaries (e.g. the headless `efl-pack` CLI in
// src/bin/efl-pack.rs) can reuse the GUI's pack/validate pipeline without
// duplicating logic. The GUI binary in src/main.rs links against this lib too
// so there is exactly one source of truth.

pub mod app;
pub mod demo;
pub mod diagnostics;
pub mod engine_state;
pub mod pack;
pub mod pipe;
pub mod settings;
pub mod state;
pub mod tabs;
pub mod theme;
