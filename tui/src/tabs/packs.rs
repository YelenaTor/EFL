use std::{fs, path::PathBuf};

use egui::{Align2, RichText, ScrollArea, SidePanel, Vec2};

use crate::pack::{inspect_efpack, pack_folder, validate_manifest, WatcherHandle};
use crate::state::{AppState, PackFolder};
use crate::theme;

pub fn show(ui: &mut egui::Ui, state: &mut AppState, ctx: &egui::Context) {
    // First-run modal — show it and disable the rest of the UI while open
    if state.packs.first_run_modal_open {
        show_first_run_modal(ctx, state);
    }

    // ── Left panel — pack list ───────────────────────────────────────────────
    SidePanel::left("pack_list")
        .resizable(true)
        .min_width(180.0)
        .max_width(320.0)
        .default_width(220.0)
        .show_inside(ui, |ui| {
            ui.add_space(6.0);
            ui.label(
                RichText::new("Content Packs")
                    .color(theme::TEXT_SECONDARY)
                    .size(11.0),
            );
            ui.separator();

            if state.settings.projects_dir.is_none() {
                ui.add_space(12.0);
                ui.label(
                    RichText::new("No projects folder set.")
                        .color(theme::TEXT_MUTED)
                        .size(12.0),
                );
                if ui.button("Configure...").clicked() {
                    state.packs.first_run_modal_open = true;
                }
                return;
            }

            ScrollArea::vertical().show(ui, |ui| {
                let selected = state.packs.selected_pack.clone();
                for folder in &state.packs.pack_folders {
                    let is_selected = selected.as_deref() == Some(&folder.path);

                    ui.horizontal(|ui| {
                        if !folder.has_manifest {
                            ui.label(
                                RichText::new("!")
                                    .color(theme::AMBER)
                                    .strong()
                                    .size(12.0),
                            );
                        }
                        let label = RichText::new(&folder.name).size(13.0);
                        if ui.selectable_label(is_selected, label).clicked() {
                            state.packs.selected_pack = Some(folder.path.clone());
                            // Clear previous results when switching packs
                            state.packs.last_pack_result = None;
                            state.packs.last_inspect_result = None;
                            state.packs.last_validation.clear();
                        }
                    });
                }
            });

            ui.with_layout(egui::Layout::bottom_up(egui::Align::LEFT), |ui| {
                if ui.small_button("Refresh").clicked() {
                    scan_project_folders(state);
                }
            });
        });

    // ── Right panel — actions ────────────────────────────────────────────────
    let selected = state.packs.selected_pack.clone();

    match selected {
        None => {
            ui.add_space(60.0);
            ui.vertical_centered(|ui| {
                ui.label(
                    RichText::new("Select a content pack")
                        .color(theme::TEXT_MUTED)
                        .size(14.0),
                );
            });
        }
        Some(ref pack_path) => {
            show_pack_actions(ui, state, pack_path);
        }
    }
}

fn show_pack_actions(ui: &mut egui::Ui, state: &mut AppState, pack_path: &PathBuf) {
    let pack_name = pack_path
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| pack_path.to_string_lossy().into_owned());

    ui.add_space(6.0);
    ui.label(
        RichText::new(&pack_name)
            .color(theme::TEXT_PRIMARY)
            .size(16.0)
            .strong(),
    );
    ui.label(
        RichText::new(pack_path.to_string_lossy().as_ref())
            .color(theme::TEXT_MUTED)
            .size(10.0),
    );
    ui.separator();

    // ── Actions ──────────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        if ui.button("Pack →  .efpack").clicked() {
            let out_dir = state.settings.output_dir.clone();
            let out_path = out_dir.map(|d| {
                let name = pack_path
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_else(|| "pack".into());
                d.join(format!("{name}.efpack"))
            });
            state.packs.last_pack_result = Some(
                pack_folder(pack_path, out_path.as_deref())
                    .map_err(|e| e.to_string()),
            );
        }

        if ui.button("Inspect...").clicked() {
            if let Some(file) = rfd::FileDialog::new()
                .add_filter("EFL Pack", &["efpack"])
                .pick_file()
            {
                state.packs.last_inspect_result =
                    inspect_efpack(&file).ok();
            }
        }

        if ui.button("Validate").clicked() {
            let manifest = pack_path.join("manifest.efl");
            state.packs.last_validation =
                validate_manifest(&manifest).unwrap_or_default();
        }

        // Watch toggle
        let watch_label = if state.packs.watch_active {
            RichText::new("⏹ Stop Watch").color(theme::ACCENT)
        } else {
            RichText::new("▶ Watch + Repack").color(theme::TEXT_PRIMARY)
        };
        if ui.button(watch_label).clicked() {
            if state.packs.watch_active {
                state.packs.watcher = None;
                state.packs.watch_active = false;
            } else {
                match WatcherHandle::watch(pack_path) {
                    Ok(handle) => {
                        state.packs.watcher = Some(handle);
                        state.packs.watch_active = true;
                    }
                    Err(e) => {
                        state.packs.last_pack_result =
                            Some(Err(format!("Watcher error: {e}")));
                    }
                }
            }
        }
    });

    ui.add_space(8.0);
    ui.separator();

    // ── Results ───────────────────────────────────────────────────────────────
    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            // Pack result
            if let Some(ref result) = state.packs.last_pack_result {
                match result {
                    Ok(r) => {
                        ui.label(
                            RichText::new("Pack result")
                                .color(theme::TEXT_SECONDARY)
                                .size(11.0),
                        );
                        ui.label(
                            RichText::new(format!("  {}", r.out_path.display()))
                                .color(theme::GREEN)
                                .monospace()
                                .size(12.0),
                        );
                        ui.label(
                            RichText::new(format!(
                                "  {} v{}  |  {}",
                                r.mod_id, r.version, r.manifest_hash
                            ))
                            .color(theme::TEXT_MUTED)
                            .size(11.0),
                        );
                    }
                    Err(e) => {
                        ui.label(
                            RichText::new(format!("Pack error: {e}"))
                                .color(theme::SEV_ERROR)
                                .size(12.0),
                        );
                    }
                }
                ui.add_space(6.0);
            }

            // Validation issues
            if !state.packs.last_validation.is_empty() {
                ui.label(
                    RichText::new("Validation")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                for issue in &state.packs.last_validation {
                    let color = match issue.severity.as_str() {
                        "error" => theme::SEV_ERROR,
                        "warning" => theme::SEV_WARNING,
                        _ => theme::SEV_HAZARD,
                    };
                    ui.label(
                        RichText::new(format!(
                            "  [{}] {}",
                            issue.code, issue.message
                        ))
                        .color(color)
                        .size(12.0),
                    );
                }
                if state.packs.last_validation.is_empty() {
                    ui.label(
                        RichText::new("  ✓ No issues found")
                            .color(theme::GREEN)
                            .size(12.0),
                    );
                }
                ui.add_space(6.0);
            }

            // Inspect result
            if let Some(ref inspect) = state.packs.last_inspect_result {
                ui.label(
                    RichText::new("Archive contents")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                if let Some(ref manifest) = inspect.manifest {
                    if let Some(mod_id) = manifest.get("modId").and_then(|v| v.as_str()) {
                        ui.label(
                            RichText::new(format!("  modId: {mod_id}"))
                                .color(theme::TEXT_PRIMARY)
                                .size(12.0),
                        );
                    }
                }
                for entry in &inspect.entries {
                    ui.label(
                        RichText::new(format!("  {:>8}  {}", entry.size, entry.name))
                            .color(theme::TEXT_MUTED)
                            .monospace()
                            .size(11.0),
                    );
                }
                ui.label(
                    RichText::new(format!(
                        "  Total: {} bytes ({} files)",
                        inspect.total_bytes,
                        inspect.entries.len()
                    ))
                    .color(theme::TEXT_SECONDARY)
                    .size(11.0),
                );
            }
        });
}

fn show_first_run_modal(ctx: &egui::Context, state: &mut AppState) {
    egui::Window::new("First-time Setup")
        .anchor(Align2::CENTER_CENTER, Vec2::ZERO)
        .collapsible(false)
        .resizable(false)
        .min_width(420.0)
        .show(ctx, |ui| {
            ui.add_space(4.0);
            ui.label(
                RichText::new("Welcome to EFL DevKit")
                    .size(18.0)
                    .color(theme::TEXT_PRIMARY)
                    .strong(),
            );
            ui.add_space(2.0);
            ui.label(
                RichText::new("Set up your workspace folders to get started.")
                    .color(theme::TEXT_SECONDARY)
                    .size(12.0),
            );
            ui.add_space(12.0);

            // Projects folder
            ui.label(
                RichText::new("Projects folder")
                    .color(theme::TEXT_SECONDARY)
                    .size(12.0),
            );
            ui.label(
                RichText::new("Where your content pack folders live.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.horizontal(|ui| {
                ui.text_edit_singleline(&mut state.packs.projects_dir_draft);
                if ui.button("Browse...").clicked() {
                    if let Some(path) = rfd::FileDialog::new().pick_folder() {
                        state.packs.projects_dir_draft =
                            path.to_string_lossy().into_owned();
                    }
                }
            });

            ui.add_space(8.0);

            // Output folder
            ui.label(
                RichText::new("Output folder")
                    .color(theme::TEXT_SECONDARY)
                    .size(12.0),
            );
            ui.label(
                RichText::new("Where packed .efpack files will be written.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.horizontal(|ui| {
                ui.text_edit_singleline(&mut state.packs.output_dir_draft);
                if ui.button("Browse...").clicked() {
                    if let Some(path) = rfd::FileDialog::new().pick_folder() {
                        state.packs.output_dir_draft =
                            path.to_string_lossy().into_owned();
                    }
                }
            });

            ui.add_space(16.0);

            let can_save = !state.packs.projects_dir_draft.is_empty()
                && !state.packs.output_dir_draft.is_empty();

            ui.add_enabled_ui(can_save, |ui| {
                if ui.button("Save & Continue").clicked() {
                    state.settings.projects_dir =
                        Some(PathBuf::from(&state.packs.projects_dir_draft));
                    state.settings.output_dir =
                        Some(PathBuf::from(&state.packs.output_dir_draft));
                    state.settings.first_run_complete = true;
                    let _ = state.settings.save();
                    state.packs.first_run_modal_open = false;
                    scan_project_folders(state);
                }
            });
        });
}

/// Scan the projects directory and populate pack_folders.
pub fn scan_project_folders(state: &mut AppState) {
    state.packs.pack_folders.clear();

    let Some(ref projects_dir) = state.settings.projects_dir.clone() else {
        return;
    };

    let Ok(entries) = fs::read_dir(projects_dir) else {
        return;
    };

    for entry in entries.flatten() {
        let path = entry.path();
        if !path.is_dir() {
            continue;
        }
        let name = path
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();
        let has_manifest = path.join("manifest.efl").exists();
        state.packs.pack_folders.push(PackFolder { path, name, has_manifest });
    }

    // Sort alphabetically
    state.packs.pack_folders.sort_by(|a, b| a.name.cmp(&b.name));
}
