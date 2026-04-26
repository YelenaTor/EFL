use std::{fs, path::PathBuf};

use egui::{Align2, Frame, Margin, RichText, ScrollArea, SidePanel, Vec2};

use crate::pack::{
    analyze_pack, apply_migration, extract_archive, inspect_efdat, inspect_efpack, pack_dat_folder,
    pack_with_request, scaffold_pack, slugify, validate_dat_with_profile,
    validate_manifest_with_capabilities, BuildInput, BuildRequest, NewPackOptions,
    ValidationProfile,
    WatcherHandle,
};
use crate::pipe::{command_pipe_for_event_pipe, send_reload, ReloadCommand};
use crate::state::{AppState, NewPackDraft, NewPackWizardStep, PackFolder, PipeStatus, ALL_FEATURES};
use crate::theme;

/// Map the discovered event pipe (`PipeStatus::Connected(...)`) to its
/// sibling command pipe and emit a reload signal.
///
/// Returns `Ok(message)` if delivered, `Err(message)` if it could not be
/// delivered (no engine connected, unsupported pipe name, write failure).
pub fn try_send_reload(state: &AppState, reason: &str) -> Result<String, String> {
    let event_pipe = match &state.diag.pipe_status {
        PipeStatus::Connected(name) if name != "demo" => name.clone(),
        PipeStatus::Connected(_) => {
            return Err("Demo mode is active — no real engine to reload.".into());
        }
        _ => return Err("No engine connected on the IPC pipe.".into()),
    };
    let command_pipe = command_pipe_for_event_pipe(&event_pipe)
        .ok_or_else(|| format!("Cannot derive command pipe from {event_pipe}"))?;
    send_reload(&command_pipe, ReloadCommand { reason })
        .map(|_| format!("reload sent to {command_pipe} (reason={reason})"))
}

/// Extract `archive` into a sibling working directory and register it as the
/// active edit-in-place workspace. The workspace is also added to the pack
/// folders list so the existing Inspect / Validate / Pack action row applies
/// to it without further wiring.
fn begin_edit_in_place(state: &mut AppState, archive: &std::path::Path) -> Result<String, String> {
    let parent = archive
        .parent()
        .ok_or_else(|| "Archive has no parent directory".to_string())?;
    let stem = archive
        .file_stem()
        .and_then(|s| s.to_str())
        .ok_or_else(|| "Archive path has no readable name".to_string())?;
    let workspace = unique_edit_workspace(parent, stem);

    let result = extract_archive(archive, &workspace).map_err(|e| e.to_string())?;

    state.packs.edit_in_place_workspace = Some(workspace.clone());
    state.packs.edit_in_place_source = Some(archive.to_path_buf());
    let folder = PackFolder {
        path: workspace.clone(),
        name: workspace
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| "edit-workspace".into()),
        has_manifest: workspace.join("manifest.efl").exists()
            || workspace.join("manifest.efdat").exists(),
    };
    if !state
        .packs
        .pack_folders
        .iter()
        .any(|f| f.path == folder.path)
    {
        state.packs.pack_folders.push(folder);
    }
    state.packs.selected_pack = Some(workspace.clone());
    Ok(format!(
        "extracted {} files into {} (pack-meta {})",
        result.files_written,
        workspace.display(),
        if result.stripped_pack_meta {
            "regenerated on next pack"
        } else {
            "no original found"
        }
    ))
}

/// Pick a non-clobbering directory next to the source archive. We append a
/// numeric suffix when the obvious `<stem>-edit` folder already exists.
fn unique_edit_workspace(parent: &std::path::Path, stem: &str) -> PathBuf {
    let base_name = format!("{stem}-edit");
    let candidate = parent.join(&base_name);
    if !candidate.exists() {
        return candidate;
    }
    for n in 2..1000 {
        let candidate = parent.join(format!("{base_name}-{n}"));
        if !candidate.exists() {
            return candidate;
        }
    }
    parent.join(format!("{base_name}-overflow"))
}

pub fn show(ui: &mut egui::Ui, state: &mut AppState, ctx: &egui::Context) {
    // First-run modal — show it and disable the rest of the UI while open
    if state.packs.first_run_modal_open {
        show_first_run_modal(ctx, state);
    }

    // New Pack wizard modal
    if state.packs.new_pack_modal_open {
        show_new_pack_modal(ctx, state);
    }
    if state.packs.migration_modal_open {
        if let Some(ref pack_path) = state.packs.selected_pack.clone() {
            show_migration_modal(ctx, state, pack_path);
        } else {
            state.packs.migration_modal_open = false;
        }
    }
    if state.packs.build_help_modal_open {
        show_advanced_build_help_modal(ctx, state);
    }

    // ── Left panel — pack list ───────────────────────────────────────────────
    SidePanel::left("pack_list")
        .resizable(true)
        .min_width(180.0)
        .max_width(320.0)
        .default_width(220.0)
        .show_inside(ui, |ui| {
            ui.add_space(6.0);
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new("Content Packs")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let new_btn = ui.small_button("+ New");
                    new_btn.clone().on_hover_text(
                        "Start a new pack with a ready-to-edit manifest and folder setup.",
                    );
                    if new_btn.clicked() && state.settings.projects_dir.is_some() {
                        state.packs.new_pack_modal_open = true;
                        state.packs.new_pack_draft = NewPackDraft::default();
                    }
                });
            });
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
                            ui.label(RichText::new("!").color(theme::AMBER).strong().size(12.0));
                        }
                        let label = RichText::new(&folder.name).size(13.0);
                        if ui.selectable_label(is_selected, label).clicked() {
                            state.packs.selected_pack = Some(folder.path.clone());
                            // Clear previous results when switching packs
                            state.packs.last_pack_result = None;
                            state.packs.last_inspect_result = None;
                            state.packs.last_validation.clear();
                            state.packs.last_watch_summary = None;
                        }
                    });
                }
            });

            ui.with_layout(egui::Layout::bottom_up(egui::Align::LEFT), |ui| {
                let refresh_btn = ui.small_button("Refresh");
                refresh_btn
                    .clone()
                    .on_hover_text("Rescan your projects folder to pick up new or moved packs.");
                if refresh_btn.clicked() {
                    scan_project_folders(state);
                }
            });
        });

    // ── Right panel — actions ────────────────────────────────────────────────
    let selected = state.packs.selected_pack.clone();
    Frame::none()
        .inner_margin(Margin::symmetric(12.0, 0.0))
        .show(ui, |ui| match selected {
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
        });
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
        ui.label(
            RichText::new("Validation profile")
                .color(theme::TEXT_SECONDARY)
                .size(11.0),
        );
        egui::ComboBox::from_id_salt("validation_profile")
            .selected_text(state.packs.validation_profile.id())
            .show_ui(ui, |ui| {
                ui.selectable_value(
                    &mut state.packs.validation_profile,
                    ValidationProfile::Recommended,
                    "recommended",
                );
                ui.selectable_value(
                    &mut state.packs.validation_profile,
                    ValidationProfile::Strict,
                    "strict",
                );
                ui.selectable_value(
                    &mut state.packs.validation_profile,
                    ValidationProfile::Legacy,
                    "legacy",
                );
            });
    });
    ui.add_space(4.0);
    egui::CollapsingHeader::new("Advanced build options")
        .default_open(false)
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new("Need help?").color(theme::TEXT_MUTED).size(10.0),
                );
                if ui.small_button("(i)").clicked() {
                    state.packs.build_help_modal_open = true;
                }
            });
            ui.horizontal(|ui| {
                ui.checkbox(
                    &mut state.packs.build_use_manifest_entry,
                    "Use manifest entrypoint mode",
                );
                ui.checkbox(&mut state.packs.build_ci_mode, "CI mode");
                ui.checkbox(&mut state.packs.build_use_ci_env, "Load from CI env");
            });
            ui.horizontal(|ui| {
                ui.checkbox(
                    &mut state.packs.auto_reload_on_build,
                    "Auto-reload engine after build",
                )
                .on_hover_text(
                    "After a successful Pack or Watch repack, ping the connected engine \
                     so it re-reads the content directory immediately.",
                );
            });
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new("Config file")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                ui.text_edit_singleline(&mut state.packs.build_config_path);
                if ui.small_button("Browse...").clicked() {
                    if let Some(file) = rfd::FileDialog::new()
                        .add_filter("JSON", &["json"])
                        .pick_file()
                    {
                        state.packs.build_config_path = file.to_string_lossy().into_owned();
                    }
                }
            });
            ui.label(
                RichText::new(
                    "Input contract: project folder, manifest entrypoint, config override, one-command defaults, CI env mode.",
                )
                .color(theme::TEXT_MUTED)
                .size(10.0),
            );
        });
    ui.add_space(4.0);

    ui.horizontal(|ui| {
        let pack_efpack_btn = ui.button("Pack →  .efpack");
        pack_efpack_btn
            .clone()
            .on_hover_text("Build this workspace into a versioned .efpack you can ship or test.");
        if pack_efpack_btn.clicked() {
            let outcome = build_pack_request(state, pack_path)
                .and_then(|request| pack_with_request(&request).map_err(|e| e.to_string()));
            let succeeded = outcome.is_ok();
            state.packs.last_pack_result = Some(outcome);
            if succeeded && state.packs.auto_reload_on_build {
                state.packs.last_reload_status = Some(try_send_reload(state, "devkit-pack"));
            }
        }
        let pack_efdat_btn = ui.button("Pack →  .efdat");
        pack_efdat_btn.clone().on_hover_text(
            "Build a .efdat compatibility shim (relationships only, no content payload).",
        );
        if pack_efdat_btn.clicked() {
            state.packs.last_pack_result =
                Some(if let Some(ref out_dir) = state.settings.output_dir {
                    pack_dat_folder(pack_path, out_dir).map_err(|e| e.to_string())
                } else {
                    Err("No output directory configured — open Settings to set one.".into())
                });
        }

        let inspect_btn = ui.button("Inspect...");
        inspect_btn
            .clone()
            .on_hover_text("Peek inside an .efpack or .efdat archive without rebuilding anything.");
        if inspect_btn.clicked() {
            if let Some(file) = rfd::FileDialog::new()
                .add_filter("EFL Artifacts", &["efpack", "efdat"])
                .pick_file()
            {
                state.packs.last_inspect_result = match file
                    .extension()
                    .and_then(|e| e.to_str())
                    .unwrap_or_default()
                    .to_ascii_lowercase()
                    .as_str()
                {
                    "efdat" => inspect_efdat(&file).ok(),
                    _ => inspect_efpack(&file).ok(),
                };
            }
        }

        let edit_btn = ui.button("Edit & Repack...");
        edit_btn.clone().on_hover_text(
            "Open an existing .efpack/.efdat in an editable workspace. Tweak files in your editor, \
             then come back and Repack to rebuild with refreshed checksums.",
        );
        if edit_btn.clicked() {
            if let Some(file) = rfd::FileDialog::new()
                .add_filter("EFL Artifacts", &["efpack", "efdat"])
                .pick_file()
            {
                match begin_edit_in_place(state, &file) {
                    Ok(message) => {
                        state.packs.edit_in_place_status = Some(Ok(message));
                    }
                    Err(err) => {
                        state.packs.edit_in_place_status = Some(Err(err));
                    }
                }
            }
        }

        let validate_btn = ui.button("Validate");
        let validate_hover = if state.engine.capabilities.is_some() {
            "Run checks on this pack. Engine capabilities snapshot is live, so handler/feature \
             checks reflect the running engine instead of the DevKit's built-in defaults."
        } else {
            "Run checks on this pack and get actionable issues before runtime."
        };
        validate_btn.clone().on_hover_text(validate_hover);
        if validate_btn.clicked() {
            let manifest = pack_path.join("manifest.efl");
            let dat_manifest = pack_path.join("manifest.efdat");
            state.packs.last_validation = if manifest.exists() {
                validate_manifest_with_capabilities(
                    &manifest,
                    state.packs.validation_profile,
                    state.engine.capabilities.as_ref(),
                )
                .unwrap_or_default()
            } else if dat_manifest.exists() {
                validate_dat_with_profile(&dat_manifest, state.packs.validation_profile)
                    .unwrap_or_default()
            } else {
                vec![crate::pack::ValidationIssue {
                    code: "MANIFEST-E001".into(),
                    severity: "error".into(),
                    message: "No manifest.efl or manifest.efdat found in selected folder".into(),
                }]
            };
        }
        let migrate_btn = ui.button("Migrate...");
        migrate_btn
            .clone()
            .on_hover_text("Upgrade older manifests safely. Apply mode makes a backup first.");
        if migrate_btn.clicked() {
            state.packs.migration_modal_open = true;
            state.packs.migration_error = None;
            state.packs.migration_apply_result = None;
            state.packs.migration_report = None;
        }

        // Watch toggle
        let watch_label = if state.packs.watch_active {
            RichText::new("⏹ Stop Watch").color(theme::ACCENT)
        } else {
            RichText::new("▶ Watch + Repack").color(theme::TEXT_PRIMARY)
        };
        let watch_btn = ui.button(watch_label);
        watch_btn.clone().on_hover_text(
            "Keep an eye on file changes and auto-rebuild with a simple change summary.",
        );
        if watch_btn.clicked() {
            if state.packs.watch_active {
                state.packs.watcher = None;
                state.packs.watch_active = false;
                state.packs.last_watch_summary = None;
            } else {
                match WatcherHandle::watch(pack_path) {
                    Ok(handle) => {
                        state.packs.watcher = Some(handle);
                        state.packs.watch_active = true;
                    }
                    Err(e) => {
                        state.packs.last_pack_result = Some(Err(format!("Watcher error: {e}")));
                    }
                }
            }
        }

        let reload_btn = ui.button("⟳ Reload engine");
        reload_btn.clone().on_hover_text(
            "Tell the connected engine to re-scan the content directory now. Requires a running EFL.",
        );
        if reload_btn.clicked() {
            state.packs.last_reload_status = Some(try_send_reload(state, "devkit-manual"));
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
                        let inv = &r.content_inventory;
                        let inv_parts: Vec<String> = [
                            ("areas", inv.areas),
                            ("npcs", inv.npcs + inv.world_npcs),
                            ("resources", inv.resources),
                            ("quests", inv.quests),
                            ("dialogue", inv.dialogue),
                            ("events", inv.events),
                            ("calendar", inv.calendar),
                        ]
                        .iter()
                        .filter(|(_, n)| *n > 0)
                        .map(|(label, n)| format!("{n} {label}"))
                        .collect();
                        let inv_str = if inv_parts.is_empty() {
                            "0 content files".into()
                        } else {
                            inv_parts.join("  ·  ")
                        };
                        ui.label(
                            RichText::new(format!("  {} v{}  |  {}", r.mod_id, r.version, inv_str))
                                .color(theme::TEXT_MUTED)
                                .size(11.0),
                        );
                        ui.label(
                            RichText::new(format!(
                                "  manifest {}  |  {} checksums",
                                &r.manifest_hash.chars().take(20).collect::<String>(),
                                r.file_checksums.len()
                            ))
                            .color(theme::TEXT_MUTED)
                            .size(10.0),
                        );
                        ui.label(
                            RichText::new(format!("  build meta {}", r.build_meta_path.display()))
                                .color(theme::TEXT_MUTED)
                                .size(10.0),
                        );
                        if !r.asset_issues.is_empty() {
                            for issue in &r.asset_issues {
                                ui.label(
                                    RichText::new(format!("  ⚠ {issue}"))
                                        .color(theme::SEV_WARNING)
                                        .size(11.0),
                                );
                            }
                        }
                        ui.label(
                            RichText::new(format!("  {}", r.build_summary))
                                .color(theme::TEXT_MUTED)
                                .size(10.0),
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

            if let Some(ref watch_summary) = state.packs.last_watch_summary {
                ui.label(
                    RichText::new("Watch")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                ui.label(
                    RichText::new(format!("  {watch_summary}"))
                        .color(theme::TEXT_MUTED)
                        .size(11.0),
                );
                ui.add_space(6.0);
            }

            if let Some(ref reload) = state.packs.last_reload_status {
                ui.label(
                    RichText::new("Reload signal")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                match reload {
                    Ok(msg) => {
                        ui.label(
                            RichText::new(format!("  ✓ {msg}"))
                                .color(theme::GREEN)
                                .size(11.0),
                        );
                    }
                    Err(err) => {
                        ui.label(
                            RichText::new(format!("  ✗ {err}"))
                                .color(theme::SEV_WARNING)
                                .size(11.0),
                        );
                    }
                }
                ui.add_space(6.0);
            }

            // Edit-in-place workspace panel
            if let Some(ref workspace) = state.packs.edit_in_place_workspace.clone() {
                ui.label(
                    RichText::new("Edit workspace")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                ui.label(
                    RichText::new(format!("  workspace: {}", workspace.display()))
                        .color(theme::TEXT_MUTED)
                        .monospace()
                        .size(11.0),
                );
                if let Some(ref source) = state.packs.edit_in_place_source {
                    ui.label(
                        RichText::new(format!("  source:    {}", source.display()))
                            .color(theme::TEXT_MUTED)
                            .monospace()
                            .size(11.0),
                    );
                }
                if let Some(ref status) = state.packs.edit_in_place_status {
                    match status {
                        Ok(msg) => {
                            ui.label(
                                RichText::new(format!("  ✓ {msg}"))
                                    .color(theme::GREEN)
                                    .size(11.0),
                            );
                        }
                        Err(err) => {
                            ui.label(
                                RichText::new(format!("  ✗ {err}"))
                                    .color(theme::SEV_WARNING)
                                    .size(11.0),
                            );
                        }
                    }
                }
                ui.horizontal(|ui| {
                    let open_btn = ui.button("Open in file manager");
                    open_btn
                        .clone()
                        .on_hover_text("Open the extracted workspace folder in your OS file manager.");
                    if open_btn.clicked() {
                        if let Err(e) = open_in_file_manager(workspace) {
                            state.packs.edit_in_place_status = Some(Err(e));
                        }
                    }
                    let discard_btn = ui.button("Discard workspace");
                    discard_btn.clone().on_hover_text(
                        "Forget this edit workspace. The extracted folder is left on disk so you \
                         can clean it up later.",
                    );
                    if discard_btn.clicked() {
                        state
                            .packs
                            .pack_folders
                            .retain(|f| f.path != *workspace);
                        if state.packs.selected_pack.as_ref() == Some(workspace) {
                            state.packs.selected_pack = None;
                        }
                        state.packs.edit_in_place_workspace = None;
                        state.packs.edit_in_place_source = None;
                        state.packs.edit_in_place_status =
                            Some(Ok("workspace cleared (folder still on disk)".into()));
                    }
                });
                ui.add_space(6.0);
            } else if let Some(ref status) = state.packs.edit_in_place_status {
                ui.label(
                    RichText::new("Edit workspace")
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
                match status {
                    Ok(msg) => {
                        ui.label(
                            RichText::new(format!("  ✓ {msg}"))
                                .color(theme::GREEN)
                                .size(11.0),
                        );
                    }
                    Err(err) => {
                        ui.label(
                            RichText::new(format!("  ✗ {err}"))
                                .color(theme::SEV_WARNING)
                                .size(11.0),
                        );
                    }
                }
                ui.add_space(6.0);
            }

            // Validation issues
            if !state.packs.last_validation.is_empty() {
                let live_caps_suffix = if state.engine.capabilities.is_some() {
                    " · live engine snapshot"
                } else {
                    ""
                };
                ui.label(
                    RichText::new(format!(
                        "Validation ({}){live_caps_suffix}",
                        state.packs.validation_profile.id()
                    ))
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
                        RichText::new(format!("  [{}] {}", issue.code, issue.message))
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
                if let Some(ref pack_meta) = inspect.pack_meta {
                    let efl_version = pack_meta
                        .get("eflVersion")
                        .and_then(|v| v.as_str())
                        .unwrap_or("?");
                    let packed_at = pack_meta
                        .get("packedAt")
                        .and_then(|v| v.as_str())
                        .unwrap_or("?");
                    let packer_version = pack_meta
                        .get("packerVersion")
                        .and_then(|v| v.as_str())
                        .unwrap_or("?");
                    ui.label(
                        RichText::new(format!(
                            "  pack-meta: efl {}  |  packed {}  |  packer {}",
                            efl_version, packed_at, packer_version
                        ))
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                    );
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

fn build_pack_request(state: &AppState, pack_path: &PathBuf) -> Result<BuildRequest, String> {
    if state.packs.build_use_ci_env {
        let mut request = BuildRequest::from_ci_env().map_err(|e| e.to_string())?;
        if request.output_dir.is_none() {
            request.output_dir = state.settings.output_dir.clone();
        }
        if request.input.is_none() {
            request.input = Some(BuildInput::ProjectFolder(pack_path.clone()));
        }
        if request.config_file.is_none() && !state.packs.build_config_path.trim().is_empty() {
            request.config_file = Some(PathBuf::from(state.packs.build_config_path.trim()));
        }
        return Ok(request);
    }

    let Some(output_dir) = state.settings.output_dir.clone() else {
        return Err("No output directory configured — open Settings to set one.".into());
    };
    let input = if state.packs.build_use_manifest_entry {
        BuildInput::ManifestEntrypoint(pack_path.join("manifest.efl"))
    } else {
        BuildInput::ProjectFolder(pack_path.clone())
    };

    Ok(BuildRequest {
        input: Some(input),
        output_dir: Some(output_dir),
        config_file: if state.packs.build_config_path.trim().is_empty() {
            None
        } else {
            Some(PathBuf::from(state.packs.build_config_path.trim()))
        },
        ci_mode: state.packs.build_ci_mode,
    })
}

fn show_migration_modal(ctx: &egui::Context, state: &mut AppState, pack_path: &PathBuf) {
    let mut open = state.packs.migration_modal_open;
    egui::Window::new("Migration Wizard")
        .open(&mut open)
        .collapsible(false)
        .resizable(true)
        .min_width(560.0)
        .min_height(420.0)
        .anchor(Align2::CENTER_CENTER, Vec2::ZERO)
        .show(ctx, |ui| {
            ui.label(
                RichText::new("Analyze and migrate pre-V2.5 manifests")
                    .color(theme::TEXT_PRIMARY)
                    .size(14.0)
                    .strong(),
            );
            ui.label(
                RichText::new(pack_path.to_string_lossy())
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.add_space(6.0);
            ui.label(
                RichText::new("Migration apply always creates a full backup before writing.")
                    .color(theme::AMBER)
                    .size(11.0),
            );
            ui.add_space(8.0);

            ui.horizontal(|ui| {
                if ui.button("Analyze (dry-run)").clicked() {
                    match analyze_pack(pack_path) {
                        Ok(report) => {
                            state.packs.migration_report = Some(report);
                            state.packs.migration_apply_result = None;
                            state.packs.migration_error = None;
                        }
                        Err(e) => {
                            state.packs.migration_error = Some(e.to_string());
                        }
                    }
                }
                if ui.button("Apply Migration").clicked() {
                    match apply_migration(pack_path) {
                        Ok(result) => {
                            state.packs.migration_apply_result = Some(result.clone());
                            state.packs.migration_report = Some(result.report);
                            state.packs.migration_error = None;
                        }
                        Err(e) => {
                            state.packs.migration_error = Some(e.to_string());
                        }
                    }
                }
                if ui.button("Close").clicked() {
                    state.packs.migration_modal_open = false;
                }
            });

            ui.separator();
            ScrollArea::vertical().show(ui, |ui| {
                if let Some(ref report) = state.packs.migration_report {
                    ui.label(
                        RichText::new(format!("Pack: {}", report.pack_path.display()))
                            .color(theme::TEXT_MUTED)
                            .size(10.0),
                    );
                    ui.label(
                        RichText::new(format!("Planned changes: {}", report.changes.len()))
                            .color(theme::TEXT_SECONDARY)
                            .size(11.0),
                    );
                    if report.changes.is_empty() {
                        ui.label(
                            RichText::new("  No migration changes required.")
                                .color(theme::GREEN)
                                .size(11.0),
                        );
                    } else {
                        for change in &report.changes {
                            ui.label(
                                RichText::new(format!(
                                    "  • [{}] {}",
                                    change.file, change.description
                                ))
                                .color(theme::TEXT_PRIMARY)
                                .size(11.0),
                            );
                        }
                    }
                    if !report.warnings.is_empty() {
                        ui.add_space(6.0);
                        ui.label(
                            RichText::new("Warnings")
                                .color(theme::SEV_WARNING)
                                .size(11.0),
                        );
                        for warning in &report.warnings {
                            ui.label(
                                RichText::new(format!("  ⚠ {warning}"))
                                    .color(theme::SEV_WARNING)
                                    .size(11.0),
                            );
                        }
                    }
                } else {
                    ui.label(
                        RichText::new("Run Analyze to generate a migration plan.")
                            .color(theme::TEXT_MUTED)
                            .size(11.0),
                    );
                }

                if let Some(ref applied) = state.packs.migration_apply_result {
                    ui.add_space(8.0);
                    ui.label(
                        RichText::new("Apply result")
                            .color(theme::TEXT_SECONDARY)
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(format!("  Backup: {}", applied.backup_path.display()))
                            .color(theme::GREEN)
                            .size(11.0),
                    );
                }

                if let Some(ref err) = state.packs.migration_error {
                    ui.add_space(8.0);
                    ui.label(
                        RichText::new(format!("Error: {err}"))
                            .color(theme::SEV_ERROR)
                            .size(11.0),
                    );
                }
            });
        });
    if !open {
        state.packs.migration_modal_open = false;
    }
}

fn show_advanced_build_help_modal(ctx: &egui::Context, state: &mut AppState) {
    let mut open = state.packs.build_help_modal_open;
    egui::Window::new("Advanced Build Options Help")
        .open(&mut open)
        .collapsible(false)
        .resizable(false)
        .min_width(520.0)
        .anchor(Align2::CENTER_CENTER, Vec2::ZERO)
        .show(ctx, |ui| {
            ui.label(
                RichText::new("Use defaults unless you have a specific workflow need.")
                    .color(theme::TEXT_SECONDARY)
                    .size(12.0),
            );
            ui.add_space(6.0);

            ui.label(
                RichText::new("Use manifest entrypoint mode")
                    .color(theme::TEXT_PRIMARY)
                    .size(12.0)
                    .strong(),
            );
            ui.label(
                RichText::new("Build starts from `manifest.efl` instead of inferring from selected folder.")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
            ui.label(
                RichText::new("Use when: your workspace layout is non-standard or you want strict manifest-driven entry.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.add_space(6.0);

            ui.label(
                RichText::new("CI mode")
                    .color(theme::TEXT_PRIMARY)
                    .size(12.0)
                    .strong(),
            );
            ui.label(
                RichText::new("Marks build as automation-oriented (less interactive assumptions).")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
            ui.label(
                RichText::new("Use when: running repeatable pipeline/build server jobs.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.add_space(6.0);

            ui.label(
                RichText::new("Load from CI env")
                    .color(theme::TEXT_PRIMARY)
                    .size(12.0)
                    .strong(),
            );
            ui.label(
                RichText::new("Reads build input/output/config from `EFL_PACK_*` environment variables.")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
            ui.label(
                RichText::new("Use when: you need the same build config to work in CI and local scripting.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.add_space(6.0);

            ui.label(
                RichText::new("Config file")
                    .color(theme::TEXT_PRIMARY)
                    .size(12.0)
                    .strong(),
            );
            ui.label(
                RichText::new("Optional JSON file to override output path, CI mode, or manifest entrypoint.")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
            ui.label(
                RichText::new("Use when: your team wants shared, versioned build settings.")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );

            ui.add_space(10.0);
            ui.label(
                RichText::new("Tip: If unsure, leave all advanced toggles off and use `Pack -> .efpack`.")
                    .color(theme::GREEN)
                    .size(11.0),
            );
        });
    if !open {
        state.packs.build_help_modal_open = false;
    }
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
                        state.packs.projects_dir_draft = path.to_string_lossy().into_owned();
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
                        state.packs.output_dir_draft = path.to_string_lossy().into_owned();
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
                    state.settings.output_dir = Some(PathBuf::from(&state.packs.output_dir_draft));
                    state.settings.first_run_complete = true;
                    let _ = state.settings.save();
                    state.packs.first_run_modal_open = false;
                    scan_project_folders(state);
                }
            });
        });
}

/// Scan the projects directory and populate pack_folders.
/// Open the given path in the host OS file manager. Best-effort: errors are
/// surfaced back to the caller so the UI can show them inline.
fn open_in_file_manager(path: &std::path::Path) -> Result<(), String> {
    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("explorer")
            .arg(path)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to launch explorer: {e}"))
    }
    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(path)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to launch open: {e}"))
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        std::process::Command::new("xdg-open")
            .arg(path)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to launch xdg-open: {e}"))
    }
}

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
        state.packs.pack_folders.push(PackFolder {
            path,
            name,
            has_manifest,
        });
    }

    // Sort alphabetically
    state.packs.pack_folders.sort_by(|a, b| a.name.cmp(&b.name));
}

fn sanitize_domain_root(input: &str) -> String {
    input
        .split('.')
        .map(slugify)
        .filter(|s| !s.is_empty())
        .collect::<Vec<_>>()
        .join(".")
}

fn default_mod_id_from(author: &str, mod_name: &str, domain_override: &str) -> String {
    let author_slug = slugify(author);
    let mod_slug = slugify(mod_name);
    if mod_slug.is_empty() {
        return String::new();
    }
    let root = sanitize_domain_root(domain_override);
    if !root.is_empty() {
        return format!("com.{root}.{mod_slug}");
    }
    if author_slug.is_empty() {
        return format!("com.author.{mod_slug}");
    }
    format!("com.{author_slug}.{mod_slug}")
}

fn step_index(step: NewPackWizardStep) -> usize {
    match step {
        NewPackWizardStep::Author => 1,
        NewPackWizardStep::ModName => 2,
        NewPackWizardStep::ModId => 3,
        NewPackWizardStep::Features => 4,
        NewPackWizardStep::Momi => 5,
        NewPackWizardStep::Review => 6,
    }
}

fn next_step(step: NewPackWizardStep) -> NewPackWizardStep {
    match step {
        NewPackWizardStep::Author => NewPackWizardStep::ModName,
        NewPackWizardStep::ModName => NewPackWizardStep::ModId,
        NewPackWizardStep::ModId => NewPackWizardStep::Features,
        NewPackWizardStep::Features => NewPackWizardStep::Momi,
        NewPackWizardStep::Momi => NewPackWizardStep::Review,
        NewPackWizardStep::Review => NewPackWizardStep::Review,
    }
}

fn prev_step(step: NewPackWizardStep) -> NewPackWizardStep {
    match step {
        NewPackWizardStep::Author => NewPackWizardStep::Author,
        NewPackWizardStep::ModName => NewPackWizardStep::Author,
        NewPackWizardStep::ModId => NewPackWizardStep::ModName,
        NewPackWizardStep::Features => NewPackWizardStep::ModId,
        NewPackWizardStep::Momi => NewPackWizardStep::Features,
        NewPackWizardStep::Review => NewPackWizardStep::Momi,
    }
}

fn can_advance(draft: &NewPackDraft) -> Result<(), String> {
    match draft.step {
        NewPackWizardStep::Author => {
            if draft.author.trim().is_empty() {
                Err("Author name is required.".into())
            } else {
                Ok(())
            }
        }
        NewPackWizardStep::ModName => {
            if draft.mod_name.trim().is_empty() {
                Err("Mod name is required.".into())
            } else {
                Ok(())
            }
        }
        NewPackWizardStep::ModId => {
            if draft.mod_id.trim().is_empty() {
                Err("modId is required.".into())
            } else {
                Ok(())
            }
        }
        _ => Ok(()),
    }
}

fn show_new_pack_modal(ctx: &egui::Context, state: &mut AppState) {
    let efl_version = state.engine.efl_version.clone();
    let mut open = state.packs.new_pack_modal_open;
    egui::Window::new("New Content Pack Wizard")
        .open(&mut open)
        .collapsible(false)
        .resizable(false)
        .fixed_size([520.0, 560.0])
        .anchor(Align2::CENTER_CENTER, Vec2::ZERO)
        .show(ctx, |ui| {
            let draft = &mut state.packs.new_pack_draft;
            let selected_features: Vec<String> = ALL_FEATURES
                .iter()
                .enumerate()
                .filter(|(i, _)| draft.selected_features[*i])
                .map(|(_, (tag, _))| tag.to_string())
                .collect();
            ui.label(
                RichText::new(format!("Step {} of 6", step_index(draft.step)))
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
            ui.add_space(6.0);
            match draft.step {
                NewPackWizardStep::Author => {
                    ui.label(RichText::new("Who is the pack author?").size(15.0).strong());
                    ui.add_space(8.0);
                    let changed = ui
                        .add(
                            egui::TextEdit::singleline(&mut draft.author)
                                .desired_width(360.0)
                                .hint_text("Author name"),
                        )
                        .changed();
                    if changed {
                        draft.author_slug = slugify(&draft.author);
                        if !draft.mod_id_user_edited {
                            draft.mod_id = default_mod_id_from(
                                &draft.author,
                                &draft.mod_name,
                                &draft.domain_override,
                            );
                        }
                    }
                }
                NewPackWizardStep::ModName => {
                    ui.label(RichText::new("What should this pack be called?").size(15.0).strong());
                    ui.add_space(8.0);
                    let changed = ui
                        .add(
                            egui::TextEdit::singleline(&mut draft.mod_name)
                                .desired_width(360.0)
                                .hint_text("Pack name"),
                        )
                        .changed();
                    if changed {
                        draft.mod_slug = slugify(&draft.mod_name);
                        if draft.display_name.trim().is_empty() {
                            draft.display_name = draft.mod_name.clone();
                        }
                        if !draft.mod_id_user_edited {
                            draft.mod_id = default_mod_id_from(
                                &draft.author,
                                &draft.mod_name,
                                &draft.domain_override,
                            );
                        }
                    }
                    ui.add_space(8.0);
                    ui.label(
                        RichText::new("Display name (shown in logs and DevKit)")
                            .color(theme::TEXT_SECONDARY)
                            .size(12.0),
                    );
                    ui.add(
                        egui::TextEdit::singleline(&mut draft.display_name)
                            .desired_width(360.0)
                            .hint_text("Display name"),
                    );
                }
                NewPackWizardStep::ModId => {
                    ui.label(RichText::new("Confirm your modId").size(15.0).strong());
                    ui.add_space(6.0);
                    ui.label(
                        RichText::new(
                            "Default is generated from author + mod name. You can edit it directly.",
                        )
                        .color(theme::TEXT_MUTED)
                        .size(12.0),
                    );
                    ui.add_space(8.0);
                    ui.label(RichText::new("Advanced root override (optional)").size(12.0));
                    let override_changed = ui
                        .add(
                            egui::TextEdit::singleline(&mut draft.domain_override)
                                .desired_width(360.0)
                                .hint_text("yourname.dev"),
                        )
                        .changed();
                    if override_changed && !draft.mod_id_user_edited {
                        draft.mod_id =
                            default_mod_id_from(&draft.author, &draft.mod_name, &draft.domain_override);
                    }
                    ui.add_space(8.0);
                    let id_resp = ui.add(
                        egui::TextEdit::singleline(&mut draft.mod_id)
                            .desired_width(360.0)
                            .hint_text("com.author.pack"),
                    );
                    if id_resp.changed() {
                        draft.mod_id_user_edited = true;
                    }
                    if ui.small_button("Recompute Default").clicked() {
                        draft.mod_id =
                            default_mod_id_from(&draft.author, &draft.mod_name, &draft.domain_override);
                        draft.mod_id_user_edited = false;
                    }
                }
                NewPackWizardStep::Features => {
                    ui.label(RichText::new("Pick features to initialize").size(15.0).strong());
                    ui.add_space(8.0);
                    egui::Grid::new("features_grid")
                        .num_columns(2)
                        .spacing([24.0, 4.0])
                        .show(ui, |ui| {
                            for (i, (_, label)) in ALL_FEATURES.iter().enumerate() {
                                let checked = &mut draft.selected_features[i];
                                ui.checkbox(checked, RichText::new(*label).size(12.0));
                                if i % 2 == 1 {
                                    ui.end_row();
                                }
                            }
                            if ALL_FEATURES.len() % 2 == 1 {
                                ui.end_row();
                            }
                        });
                    ui.add_space(8.0);
                    ui.label(
                        RichText::new("If you add content families later that are not initialized here, update manifest feature scope and folders manually.")
                            .color(theme::TEXT_MUTED)
                            .size(11.0),
                    );
                }
                NewPackWizardStep::Momi => {
                    ui.label(RichText::new("MOMI integration").size(15.0).strong());
                    ui.add_space(8.0);
                    ui.checkbox(
                        &mut draft.uses_momi,
                        "Uses MOMI content (resources/sprites/other mod data)",
                    );
                    ui.add_space(6.0);
                    ui.label(
                        RichText::new(
                            "If enabled, wizard generates a companion {mod-id}.MOMI.efdat starter artifact.",
                        )
                        .color(theme::TEXT_MUTED)
                        .size(11.0),
                    );
                }
                NewPackWizardStep::Review => {
                    ui.label(RichText::new("Review and create").size(15.0).strong());
                    ui.add_space(8.0);
                    egui::Grid::new("review_grid")
                        .num_columns(2)
                        .spacing([8.0, 6.0])
                        .show(ui, |ui| {
                            ui.label("Author");
                            ui.label(draft.author.trim());
                            ui.end_row();
                            ui.label("Name");
                            ui.label(draft.mod_name.trim());
                            ui.end_row();
                            ui.label("Display");
                            ui.label(draft.display_name.trim());
                            ui.end_row();
                            ui.label("modId");
                            ui.label(draft.mod_id.trim());
                            ui.end_row();
                            ui.label("Version");
                            ui.label(draft.version.trim());
                            ui.end_row();
                            ui.label("EFL");
                            ui.label(&efl_version);
                            ui.end_row();
                            ui.label("Uses MOMI");
                            ui.label(if draft.uses_momi { "Yes" } else { "No" });
                            ui.end_row();
                        });
                    ui.add_space(6.0);
                    if selected_features.is_empty() {
                        ui.label(RichText::new("Features: none").color(theme::TEXT_MUTED));
                    } else {
                        ui.label(format!("Features: {}", selected_features.join(", ")));
                    }
                }
            }

            let pending_opts = NewPackOptions {
                display_name: if draft.display_name.trim().is_empty() {
                    draft.mod_name.trim().to_string()
                } else {
                    draft.display_name.trim().to_string()
                },
                mod_id: draft.mod_id.trim().to_string(),
                author: draft.author.trim().to_string(),
                version: if draft.version.trim().is_empty() {
                    "1.1.0".to_string()
                } else {
                    draft.version.trim().to_string()
                },
                description: draft.description.trim().to_string(),
                efl_version: efl_version.clone(),
                features: selected_features,
                uses_momi: draft.uses_momi,
            };
            let can_create =
                !pending_opts.display_name.is_empty() && !pending_opts.mod_id.is_empty();
            let is_review = draft.step == NewPackWizardStep::Review;
            let is_first = draft.step == NewPackWizardStep::Author;
            let error_msg = draft.error.clone();
            let current_step = draft.step;
            let _ = draft;

            ui.add_space(12.0);
            ui.separator();
            if let Some(ref err) = error_msg {
                ui.label(RichText::new(err).color(theme::SEV_ERROR).size(12.0));
                ui.add_space(4.0);
            }
            ui.horizontal(|ui| {
                if ui.add_enabled(!is_first, egui::Button::new("Back")).clicked() {
                    state.packs.new_pack_draft.step = prev_step(current_step);
                    state.packs.new_pack_draft.error = None;
                }
                if !is_review {
                    if ui.button("Next").clicked() {
                        match can_advance(&state.packs.new_pack_draft) {
                            Ok(_) => {
                                state.packs.new_pack_draft.step = next_step(current_step);
                                state.packs.new_pack_draft.error = None;
                            }
                            Err(e) => state.packs.new_pack_draft.error = Some(e),
                        }
                    }
                } else if ui
                    .add_enabled(can_create, egui::Button::new("Create Pack"))
                    .clicked()
                {
                    let projects_dir = state.settings.projects_dir.clone().unwrap();
                    match scaffold_pack(&projects_dir, &pending_opts) {
                        Ok(_) => {
                            state.packs.new_pack_modal_open = false;
                            state.packs.new_pack_draft = NewPackDraft::default();
                            scan_project_folders(state);
                        }
                        Err(e) => {
                            state.packs.new_pack_draft.error = Some(e.to_string());
                        }
                    }
                }
                if ui.button("Cancel").clicked() {
                    state.packs.new_pack_modal_open = false;
                    state.packs.new_pack_draft = NewPackDraft::default();
                }
            });
        });

    if !open {
        state.packs.new_pack_modal_open = false;
        state.packs.new_pack_draft = NewPackDraft::default();
    }
}
