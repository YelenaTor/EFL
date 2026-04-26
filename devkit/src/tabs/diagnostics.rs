use std::fs;
use std::path::{Path, PathBuf};

use egui::{CollapsingHeader, Color32, RichText, ScrollArea};

use crate::diagnostics::codes::DiagnosticCode;
use crate::engine_state::{
    BootStepStatus, EngineState, ModSource, MomiBadge, MomiEdge, MomiPresence, MomiProjectedRelation,
    MomiScannedMod, MomiViewRow, Phase,
};
use crate::state::{DiagnosticsState, MomiFilter, PipeStatus};
use crate::theme;

pub fn show(
    ctx: &egui::Context,
    ui: &mut egui::Ui,
    diag: &mut DiagnosticsState,
    engine: &mut EngineState,
    selected_pack: Option<&Path>,
    projects_root: Option<&Path>,
) {
    // ── Status bar ──────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        let (status_color, status_text) = match &diag.pipe_status {
            PipeStatus::Connected(name) => (theme::GREEN, format!("Connected: {name}")),
            PipeStatus::Searching => (theme::AMBER, "Searching for engine...".into()),
            PipeStatus::Disconnected => (theme::SEV_ERROR, "Disconnected".into()),
            PipeStatus::NoEngine => (theme::SEV_ERROR, "No engine found".into()),
        };

        ui.label(
            RichText::new(format!("  ●  {status_text}  "))
                .size(12.0)
                .color(status_color),
        );

        if matches!(
            diag.pipe_status,
            PipeStatus::NoEngine | PipeStatus::Disconnected
        ) {
            if ui.small_button("Retry").clicked() {
                diag.pipe_reader = None;
                diag.pipe_status = PipeStatus::Searching;
            }
        }
        if ui.small_button("Clear").clicked() {
            engine.collector.clear();
        }

        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
            ui.label(
                RichText::new(format!("EFL v{}", engine.efl_version))
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        });
    });

    ui.separator();

    // ── Stats row ────────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        badge(
            ui,
            &format!("{} errors", engine.collector.error_count()),
            theme::SEV_ERROR,
        );
        ui.add_space(6.0);
        badge(
            ui,
            &format!("{} warnings", engine.collector.warning_count()),
            theme::SEV_WARNING,
        );
        ui.add_space(6.0);
        badge(
            ui,
            &format!("{} hazards", engine.collector.hazard_count()),
            theme::SEV_HAZARD,
        );
        ui.add_space(10.0);
        badge(
            ui,
            &format!(
                "{} manifest",
                engine.collector.count_by_category("MANIFEST")
            ),
            theme::TEXT_SECONDARY,
        );
        ui.add_space(6.0);
        badge(
            ui,
            &format!("{} hook", engine.collector.count_by_category("HOOK")),
            theme::TEXT_SECONDARY,
        );
    });

    ui.add_space(4.0);
    ui.separator();

    // ── Monitor panels (visible once engine reaches Monitor phase) ───────────
    if engine.phase == Phase::Monitor {
        let scanned = scan_momi_inventory(projects_root);
        let projected = project_efdat_relationships(projects_root);
        let momi_rows = engine.build_momi_view(&scanned, &projected);
        show_monitor_panels(ui, diag, engine, &momi_rows);
        show_momi_popout(ctx, diag, &momi_rows);
        ui.separator();
    } else if !engine.boot_steps.is_empty() {
        // Show boot progress even before monitor phase
        show_boot_steps(ui, engine, true);
        ui.separator();
    }

    // ── Diagnostic log ───────────────────────────────────────────────────────
    let entries = engine.collector.all();

    if entries.is_empty() {
        ui.add_space(20.0);
        ui.vertical_centered(|ui| {
            ui.label(
                RichText::new("No diagnostics — engine is clean")
                    .color(theme::TEXT_MUTED)
                    .size(13.0),
            );
        });
        return;
    }

    // ── Filter chips ─────────────────────────────────────────────────────────
    // Severity toggles plus a category dropdown. Filters apply to the
    // diagnostic log only; the stats badges above keep showing the full
    // counts so authors don't lose sight of how many issues actually exist.
    let categories = engine.collector.distinct_categories();
    show_filter_chips(ui, diag, &categories);
    ui.add_space(4.0);

    let total = entries.len();
    let visible_entries: Vec<&crate::diagnostics::collector::Diagnostic> = entries
        .iter()
        .filter(|e| entry_passes_filters(e, diag))
        .collect();

    if visible_entries.is_empty() {
        ui.add_space(20.0);
        ui.vertical_centered(|ui| {
            ui.label(
                RichText::new(format!(
                    "No diagnostics match the current filters ({total} hidden)"
                ))
                .color(theme::TEXT_MUTED)
                .size(13.0),
            );
        });
        return;
    }

    if visible_entries.len() != total {
        ui.label(
            RichText::new(format!(
                "Showing {} of {} diagnostics",
                visible_entries.len(),
                total
            ))
            .color(theme::TEXT_MUTED)
            .size(10.0),
        );
        ui.add_space(2.0);
    }

    ScrollArea::vertical()
        .stick_to_bottom(true)
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            for entry in visible_entries {
                let parsed = DiagnosticCode::parse(&entry.code);
                ui.horizontal(|ui| {
                    let (sev_color, sev_label) =
                        match parsed.as_ref().map(|p| p.severity.wire_name()) {
                            Some("error") => (theme::SEV_ERROR, "E"),
                            Some("warning") => (theme::SEV_WARNING, "W"),
                            Some("hazard") => (theme::SEV_HAZARD, "H"),
                            _ => match entry.severity.as_str() {
                                "error" => (theme::SEV_ERROR, "E"),
                                "warning" => (theme::SEV_WARNING, "W"),
                                "hazard" => (theme::SEV_HAZARD, "H"),
                                _ => (theme::TEXT_MUTED, "?"),
                            },
                        };
                    ui.label(
                        RichText::new(format!("[{sev_label}]"))
                            .color(sev_color)
                            .monospace()
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(&entry.code)
                            .color(theme::TEXT_SECONDARY)
                            .monospace()
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(&entry.message)
                            .color(theme::TEXT_PRIMARY)
                            .size(12.0),
                    );
                });
                if let Some(ref suggestion) = entry.suggestion {
                    ui.label(
                        RichText::new(format!("      → {suggestion}"))
                            .color(theme::TEXT_MUTED)
                            .size(10.0),
                    );
                }
                if let Some(ref source) = entry.source {
                    let file = source.file.as_deref().unwrap_or("unknown");
                    let field = source.field.as_deref().unwrap_or("-");
                    ui.horizontal(|ui| {
                        ui.label(
                            RichText::new(format!("      source: {file} [{field}]"))
                                .color(theme::TEXT_MUTED)
                                .size(10.0),
                        );
                        if let Some(file_str) = source.file.as_deref() {
                            if let Some(target) = resolve_source_target(file_str, selected_pack) {
                                if ui
                                    .small_button("Open")
                                    .on_hover_text(format!(
                                        "Open '{}' in your default app",
                                        target.display()
                                    ))
                                    .clicked()
                                {
                                    if let Err(err) = open_target(&target) {
                                        eprintln!("diagnostics quick-jump failed: {err}");
                                    }
                                }
                                if let Some(parent) = target.parent() {
                                    if ui
                                        .small_button("Reveal")
                                        .on_hover_text(format!(
                                            "Show '{}' in your file manager",
                                            target.display()
                                        ))
                                        .clicked()
                                    {
                                        if let Err(err) = reveal_in_file_manager(&target, parent) {
                                            eprintln!(
                                                "diagnostics quick-jump reveal failed: {err}"
                                            );
                                        }
                                    }
                                }
                            }
                        }
                    });
                }
                ui.label(
                    RichText::new(format!("      category: {}", entry.category))
                        .color(theme::TEXT_MUTED)
                        .size(10.0),
                );
                if let Some(parsed) = parsed {
                    let normalized = parsed.to_string();
                    if normalized != entry.code {
                        ui.label(
                            RichText::new(format!("      normalized code: {normalized}"))
                                .color(theme::TEXT_MUTED)
                                .size(10.0),
                        );
                    }
                }
            }
        });
}

/// Renders the severity + category filter chip row above the diagnostic log.
fn show_filter_chips(
    ui: &mut egui::Ui,
    diag: &mut DiagnosticsState,
    categories: &[String],
) {
    ui.horizontal(|ui| {
        ui.label(
            RichText::new("Filter:")
                .color(theme::TEXT_MUTED)
                .size(11.0),
        );

        severity_chip(
            ui,
            "Errors",
            theme::SEV_ERROR,
            &mut diag.show_errors,
        );
        severity_chip(
            ui,
            "Warnings",
            theme::SEV_WARNING,
            &mut diag.show_warnings,
        );
        severity_chip(
            ui,
            "Hazards",
            theme::SEV_HAZARD,
            &mut diag.show_hazards,
        );

        ui.add_space(8.0);
        ui.label(
            RichText::new("Category:")
                .color(theme::TEXT_MUTED)
                .size(11.0),
        );

        let current_label = diag
            .category_filter
            .clone()
            .unwrap_or_else(|| "All".to_string());
        egui::ComboBox::from_id_salt("diag_category_filter")
            .selected_text(current_label)
            .show_ui(ui, |ui| {
                let any_value: Option<String> = None;
                ui.selectable_value(&mut diag.category_filter, any_value, "All");
                for cat in categories {
                    ui.selectable_value(
                        &mut diag.category_filter,
                        Some(cat.clone()),
                        cat,
                    );
                }
            });

        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
            if ui
                .small_button("Reset filters")
                .on_hover_text("Show every severity and clear the category filter")
                .clicked()
            {
                diag.show_errors = true;
                diag.show_warnings = true;
                diag.show_hazards = true;
                diag.category_filter = None;
            }
        });
    });
}

/// Toggleable colored chip used in the filter row.
fn severity_chip(ui: &mut egui::Ui, label: &str, color: Color32, on: &mut bool) {
    let active_color = if *on { color } else { theme::TEXT_MUTED };
    let dot = if *on { "●" } else { "○" };
    let response = ui
        .small_button(
            RichText::new(format!("{dot} {label}"))
                .color(active_color)
                .size(11.0),
        )
        .on_hover_text(if *on {
            format!("Hide {} entries", label.to_lowercase())
        } else {
            format!("Show {} entries", label.to_lowercase())
        });
    if response.clicked() {
        *on = !*on;
    }
}

fn entry_passes_filters(
    entry: &crate::diagnostics::collector::Diagnostic,
    diag: &DiagnosticsState,
) -> bool {
    let sev = entry.severity.as_str();
    match sev {
        "error" if !diag.show_errors => return false,
        "warning" if !diag.show_warnings => return false,
        "hazard" if !diag.show_hazards => return false,
        _ => {}
    }
    if let Some(ref cat) = diag.category_filter {
        if &entry.category != cat {
            return false;
        }
    }
    true
}

/// Try to map a diagnostic's reported `source.file` to a real path on disk.
/// Returns `None` when nothing usable exists, in which case quick-jump is hidden.
fn resolve_source_target(file: &str, pack_root: Option<&Path>) -> Option<PathBuf> {
    if file.is_empty() || file == "unknown" {
        return None;
    }

    let candidate = PathBuf::from(file);
    if candidate.is_absolute() && candidate.exists() {
        return Some(candidate);
    }

    if let Some(root) = pack_root {
        let joined = root.join(&candidate);
        if joined.exists() {
            return Some(joined);
        }
    }
    None
}

fn open_target(path: &Path) -> Result<(), String> {
    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("cmd")
            .args(["/C", "start", "", path.to_string_lossy().as_ref()])
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to open {}: {e}", path.display()))
    }
    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(path)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to open {}: {e}", path.display()))
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        std::process::Command::new("xdg-open")
            .arg(path)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to open {}: {e}", path.display()))
    }
}

fn reveal_in_file_manager(path: &Path, _fallback_dir: &Path) -> Result<(), String> {
    #[cfg(target_os = "windows")]
    {
        let arg = format!("/select,{}", path.to_string_lossy());
        std::process::Command::new("explorer")
            .arg(arg)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to reveal {}: {e}", path.display()))
    }
    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .args(["-R", path.to_string_lossy().as_ref()])
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to reveal {}: {e}", path.display()))
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        // xdg-open lacks a "select" verb, so fall back to opening the parent directory.
        std::process::Command::new("xdg-open")
            .arg(_fallback_dir)
            .spawn()
            .map(|_| ())
            .map_err(|e| format!("failed to reveal {}: {e}", path.display()))
    }
}

fn show_monitor_panels(
    ui: &mut egui::Ui,
    diag: &mut DiagnosticsState,
    engine: &EngineState,
    momi_rows: &[MomiViewRow],
) {
    show_runtime_sequence(ui, engine);

    // Boot Steps (collapsed by default in monitor phase — init succeeded)
    show_boot_steps(ui, engine, false);

    // Loaded EFPacks
    let efpacks: Vec<_> = engine
        .mods
        .iter()
        .filter(|m| m.source == ModSource::Efpack || m.source == ModSource::Unknown)
        .collect();
    let pack_header = format!("LOADED EFPACKS  ({})", efpacks.len());
    CollapsingHeader::new(
        RichText::new(pack_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        if efpacks.is_empty() {
            ui.label(
                RichText::new("No EFPacks reported")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            for m in efpacks {
                ui.horizontal(|ui| {
                    let status_color = match m.status.as_str() {
                        "loaded" => theme::GREEN,
                        "error" => theme::SEV_ERROR,
                        _ => theme::AMBER,
                    };
                    let id_color = id_color(&m.id);
                    ui.label(RichText::new("  ●").color(status_color).size(12.0));
                    ui.label(RichText::new(&m.id).color(id_color).size(12.0));
                    ui.label(
                        RichText::new(format!("v{}  {}", m.version, m.name))
                            .color(theme::TEXT_SECONDARY)
                            .size(11.0),
                    );
                });
            }
        }
    });

    ui.add_space(2.0);

    // MOMI inventory (runtime + filesystem + relationship projection)
    let momi_header = format!("MOMI INVENTORY  ({})", momi_rows.len());
    CollapsingHeader::new(
        RichText::new(momi_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        show_momi_filter_row(ui, diag, momi_rows);
        ui.add_space(4.0);

        let visible: Vec<&MomiViewRow> = momi_rows
            .iter()
            .filter(|row| momi_row_visible(row, diag.momi_filter))
            .collect();

        if visible.is_empty() {
            ui.label(
                RichText::new("No MOMI mods match the current filter")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            for row in visible {
                ui.horizontal(|ui| {
                    let (status_color, status_text, icon) = match row.presence {
                        MomiPresence::Active => (theme::GREEN, "active", "●"),
                        MomiPresence::PresentInactive => (theme::TEXT_MUTED, "present inactive", "◌"),
                        MomiPresence::Missing => (theme::SEV_WARNING, "missing", "▲"),
                    };
                    ui.label(RichText::new(format!("  {icon}")).color(status_color).size(12.0));

                    let selected = diag
                        .selected_momi_mod
                        .as_ref()
                        .map(|m| m == &row.id)
                        .unwrap_or(false);
                    if ui
                        .selectable_label(
                            selected,
                            RichText::new(&row.id).color(id_color(&row.id)).size(12.0),
                        )
                        .on_hover_text("Open dependency view")
                        .clicked()
                    {
                        diag.selected_momi_mod = Some(row.id.clone());
                        diag.momi_popout_open = true;
                    }

                    ui.label(
                        RichText::new(format!("v{}  {}", row.version, row.name))
                            .color(theme::TEXT_SECONDARY)
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(format!("[{status_text}]"))
                            .color(status_color)
                            .size(10.0),
                    );

                    for badge in &row.badges {
                        let (label, color) = badge_label_color(*badge);
                        ui.label(
                            RichText::new(format!(" {label} "))
                                .color(color)
                                .size(10.0),
                        );
                    }

                    if row.path.is_some() {
                        ui.label(
                            RichText::new(" [indexed]")
                                .color(theme::TEXT_MUTED)
                                .size(10.0),
                        );
                    }
                });
            }
        }
    });

    ui.add_space(2.0);

    // Relationship risk summary (presence + relationship-driven).
    let rel_header = "RELATIONSHIP RISK SUMMARY";
    CollapsingHeader::new(
        RichText::new(rel_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        let hard_conflicts = momi_rows
            .iter()
            .filter(|m| {
                m.presence == MomiPresence::Active && m.badges.contains(&MomiBadge::Conflict)
            })
            .count();
        let missing_needed = momi_rows
            .iter()
            .filter(|m| {
                m.presence != MomiPresence::Active && m.badges.contains(&MomiBadge::Needed)
            })
            .count();
        let soft_missing = momi_rows
            .iter()
            .filter(|m| {
                m.presence != MomiPresence::Active && m.badges.contains(&MomiBadge::Compatible)
            })
            .count();

        if momi_rows.is_empty() {
            ui.label(
                RichText::new("No MOMI relationship data available")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            ui.label(
                RichText::new("Relationship-based checks (not runtime load-order):")
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new(format!("hard conflicts: {hard_conflicts}"))
                        .color(theme::SEV_ERROR)
                        .size(11.0),
                );
                ui.add_space(8.0);
                ui.label(
                    RichText::new(format!("missing needed: {missing_needed}"))
                        .color(theme::SEV_WARNING)
                        .size(11.0),
                );
                ui.add_space(8.0);
                ui.label(
                    RichText::new(format!("soft missing compatible: {soft_missing}"))
                        .color(theme::TEXT_MUTED)
                        .size(11.0),
                );
            });
        }
    });

    ui.add_space(2.0);

    // Active Hooks
    let hook_header = format!("HOOKS  ({})", engine.hooks.len());
    CollapsingHeader::new(
        RichText::new(hook_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        if engine.hooks.is_empty() {
            ui.label(
                RichText::new("No hooks registered")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            // Sort by fire count descending so active hooks rise to the top
            let mut sorted: Vec<_> = engine.hooks.iter().collect();
            sorted.sort_by(|a, b| b.fire_count.cmp(&a.fire_count));

            for hook in sorted {
                ui.horizontal(|ui| {
                    let kind_color = kind_color(&hook.kind);
                    ui.label(
                        RichText::new(format!("  {:>5}× ", hook.fire_count))
                            .color(if hook.fire_count > 0 {
                                theme::GREEN
                            } else {
                                theme::TEXT_MUTED
                            })
                            .monospace()
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(&hook.name)
                            .color(theme::TEXT_PRIMARY)
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(format!("[{}]", hook.kind))
                            .color(kind_color)
                            .size(10.0),
                    );
                });
            }
        }
    });

    ui.add_space(2.0);

    // Event Log
    let event_header = format!("EVENT LOG  (last {})", engine.event_log.len().min(20));
    CollapsingHeader::new(
        RichText::new(event_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        if engine.event_log.is_empty() {
            ui.label(
                RichText::new("No events yet")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            let recent: Vec<_> = engine.event_log.iter().rev().take(20).collect();
            ScrollArea::vertical()
                .max_height(120.0)
                .id_salt("event_log_scroll")
                .show(ui, |ui| {
                    for entry in recent {
                        ui.horizontal(|ui| {
                            let type_color = event_type_color(&entry.event_type);
                            ui.label(
                                RichText::new(format!("[{}]", entry.event_type))
                                    .color(type_color)
                                    .monospace()
                                    .size(11.0),
                            );
                            ui.label(
                                RichText::new(&entry.detail)
                                    .color(theme::TEXT_SECONDARY)
                                    .size(11.0),
                            );
                        });
                    }
                });
        }
    });

    ui.add_space(2.0);

    // Save Activity
    let save_header = format!("SAVE ACTIVITY  (last {})", engine.save_log.len().min(10));
    CollapsingHeader::new(
        RichText::new(save_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(false)
    .show(ui, |ui| {
        if engine.save_log.is_empty() {
            ui.label(
                RichText::new("No save operations")
                    .color(theme::TEXT_MUTED)
                    .size(11.0),
            );
        } else {
            let recent: Vec<_> = engine.save_log.iter().rev().take(10).collect();
            for entry in recent {
                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new(format!("[{}]", entry.event_type))
                            .color(theme::AMBER)
                            .monospace()
                            .size(11.0),
                    );
                    ui.label(
                        RichText::new(&entry.detail)
                            .color(theme::TEXT_SECONDARY)
                            .size(11.0),
                    );
                });
            }
        }
    });
}

fn show_runtime_sequence(ui: &mut egui::Ui, engine: &EngineState) {
    let hooks_ok = engine.hooks.iter().filter(|h| h.fire_count > 0).count();
    let phase_label = match engine.phase {
        Phase::Boot => "boot",
        Phase::Diagnostics => "diagnostics",
        Phase::Monitor => "monitor",
    };
    let steps = [
        ("Boot", format!("{} step(s)", engine.boot_steps.len())),
        (
            "Capability",
            format!("{} signal(s)", engine.capability_events.len()),
        ),
        (
            "Hook health",
            format!("{hooks_ok}/{} active", engine.hooks.len()),
        ),
        (
            "Registry",
            format!("{} activity event(s)", engine.registry_events.len()),
        ),
        (
            "Trigger",
            format!("{} event(s)", engine.trigger_events.len()),
        ),
        (
            "Pack state",
            format!("{} loaded mod entry(s)", engine.mods.len()),
        ),
    ];

    CollapsingHeader::new(
        RichText::new(format!("RUNTIME SEQUENCE  (phase: {phase_label})"))
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(true)
    .show(ui, |ui| {
        for (index, (name, detail)) in steps.iter().enumerate() {
            ui.horizontal(|ui| {
                let color = if index <= runtime_progress_index(engine) {
                    theme::GREEN
                } else {
                    theme::TEXT_MUTED
                };
                ui.label(
                    RichText::new(format!("  {}.", index + 1))
                        .color(theme::TEXT_MUTED)
                        .monospace()
                        .size(10.0),
                );
                ui.label(RichText::new(*name).color(color).size(11.0).strong());
                ui.label(
                    RichText::new(detail)
                        .color(theme::TEXT_SECONDARY)
                        .size(10.0),
                );
            });
        }
    });
    ui.add_space(2.0);
}

fn show_momi_filter_row(ui: &mut egui::Ui, diag: &mut DiagnosticsState, rows: &[MomiViewRow]) {
    let active = rows
        .iter()
        .filter(|m| m.presence == MomiPresence::Active)
        .count();
    let inactive = rows
        .iter()
        .filter(|m| m.presence == MomiPresence::PresentInactive)
        .count();
    let conflicts = rows
        .iter()
        .filter(|m| m.badges.contains(&MomiBadge::Conflict))
        .count();
    ui.horizontal(|ui| {
        ui.label(
            RichText::new("Filter:")
                .color(theme::TEXT_MUTED)
                .size(10.0),
        );
        momi_filter_chip(ui, "All", MomiFilter::All, &mut diag.momi_filter, rows.len());
        momi_filter_chip(ui, "Active", MomiFilter::Active, &mut diag.momi_filter, active);
        momi_filter_chip(
            ui,
            "Inactive",
            MomiFilter::Inactive,
            &mut diag.momi_filter,
            inactive,
        );
        momi_filter_chip(
            ui,
            "Conflicts",
            MomiFilter::Conflicts,
            &mut diag.momi_filter,
            conflicts,
        );
    });
}

fn momi_filter_chip(
    ui: &mut egui::Ui,
    label: &str,
    value: MomiFilter,
    current: &mut MomiFilter,
    count: usize,
) {
    let selected = *current == value;
    let text = if selected {
        RichText::new(format!("● {label} ({count})"))
            .color(theme::ACCENT)
            .size(10.0)
    } else {
        RichText::new(format!("○ {label} ({count})"))
            .color(theme::TEXT_MUTED)
            .size(10.0)
    };
    if ui.small_button(text).clicked() {
        *current = value;
    }
}

fn momi_row_visible(row: &MomiViewRow, filter: MomiFilter) -> bool {
    match filter {
        MomiFilter::All => true,
        MomiFilter::Active => row.presence == MomiPresence::Active,
        MomiFilter::Inactive => row.presence == MomiPresence::PresentInactive,
        MomiFilter::Conflicts => row.badges.contains(&MomiBadge::Conflict),
    }
}

fn badge_label_color(badge: MomiBadge) -> (&'static str, Color32) {
    match badge {
        MomiBadge::Needed => ("NEEDED", theme::SEV_WARNING),
        MomiBadge::Compatible => ("COMPAT", theme::GREEN),
        MomiBadge::Conflict => ("CONFLICT", theme::SEV_ERROR),
    }
}

fn show_momi_popout(ctx: &egui::Context, diag: &mut DiagnosticsState, rows: &[MomiViewRow]) {
    if !diag.momi_popout_open {
        return;
    }
    let Some(selected_id) = diag.selected_momi_mod.clone() else {
        return;
    };
    let Some(row) = rows.iter().find(|r| r.id == selected_id) else {
        return;
    };

    let mut open = diag.momi_popout_open;
    egui::Window::new(format!("MOMI Relationship Graph — {}", row.id))
        .open(&mut open)
        .resizable(true)
        .default_width(680.0)
        .show(ctx, |ui| {
            ui.label(
                RichText::new(format!("mod: {}", row.id))
                    .color(id_color(&row.id))
                    .size(12.0),
            );
            ui.label(
                RichText::new(format!("state: {:?}", row.presence))
                    .color(theme::TEXT_MUTED)
                    .size(10.0),
            );
            if let Some(path) = &row.path {
                ui.label(
                    RichText::new(format!("path: {}", path.display()))
                        .color(theme::TEXT_MUTED)
                        .size(10.0),
                );
            }
            ui.add_space(6.0);
            ui.label(
                RichText::new("Tree:")
                    .color(theme::TEXT_PRIMARY)
                    .size(11.0)
                    .strong(),
            );
            if row.edges.is_empty() {
                ui.label(
                    RichText::new("No incoming EFPack/.efdat relationship edges")
                        .color(theme::TEXT_MUTED)
                        .size(10.0),
                );
            } else {
                for edge in &row.edges {
                    render_tree_edge(ui, row, edge);
                }
            }
        });
    diag.momi_popout_open = open;
}

fn render_tree_edge(ui: &mut egui::Ui, row: &MomiViewRow, edge: &MomiEdge) {
    let status_color = if edge.relation.to_ascii_lowercase().contains("conflict") {
        theme::SEV_ERROR
    } else if edge.relation.to_ascii_lowercase().contains("optional") {
        theme::GREEN
    } else {
        theme::SEV_WARNING
    };
    ui.horizontal(|ui| {
        ui.label(RichText::new("  ").size(10.0));
        ui.label(
            RichText::new(&edge.pack_id)
                .color(id_color(&edge.pack_id))
                .size(10.0),
        );
        ui.label(
            RichText::new(format!(" --{}--> ", edge.relation))
                .color(status_color)
                .size(10.0),
        );
        ui.label(RichText::new(&row.id).color(id_color(&row.id)).size(10.0));
        ui.label(
            RichText::new(format!("[{}]", edge.status))
                .color(theme::TEXT_MUTED)
                .size(9.0),
        );
    });
}

fn scan_momi_inventory(projects_root: Option<&Path>) -> Vec<MomiScannedMod> {
    let mut roots = Vec::new();
    if let Ok(path) = std::env::var("MOMI_MODS_DIR") {
        let p = PathBuf::from(path);
        if p.exists() {
            roots.push(p);
        }
    }
    if let Some(projects) = projects_root {
        let candidates = [
            projects.join("mods"),
            projects.join("MOMI").join("mods"),
            projects
                .parent()
                .map(|p| p.join("mods"))
                .unwrap_or_else(|| projects.join("mods")),
        ];
        for c in candidates {
            if c.exists() && !roots.iter().any(|r| r == &c) {
                roots.push(c);
            }
        }
    }

    let mut out = Vec::new();
    for root in roots {
        let Ok(entries) = fs::read_dir(&root) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_dir() {
                continue;
            }
            let mut id = path
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or_default()
                .to_string();
            let mut name = None;
            let mut version = None;

            let manifest = path.join("manifest.json");
            if manifest.exists() {
                if let Ok(bytes) = fs::read(&manifest) {
                    if let Ok(json) = serde_json::from_slice::<serde_json::Value>(&bytes) {
                        if let Some(v) = json
                            .get("modId")
                            .or_else(|| json.get("id"))
                            .and_then(|v| v.as_str())
                        {
                            id = v.to_string();
                        }
                        name = json
                            .get("name")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string());
                        version = json
                            .get("version")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string());
                    }
                }
            }
            if id.is_empty() {
                continue;
            }
            out.push(MomiScannedMod {
                id,
                name,
                version,
                path: Some(path),
            });
        }
    }
    out.sort_by(|a, b| a.id.cmp(&b.id));
    out
}

fn project_efdat_relationships(projects_root: Option<&Path>) -> Vec<MomiProjectedRelation> {
    let Some(root) = projects_root else {
        return Vec::new();
    };
    let Ok(entries) = fs::read_dir(root) else {
        return Vec::new();
    };
    let mut out = Vec::new();
    for entry in entries.flatten() {
        let pack_path = entry.path();
        let manifest = pack_path.join("manifest.efdat");
        if !manifest.exists() {
            continue;
        }
        let Ok(bytes) = fs::read(&manifest) else {
            continue;
        };
        let Ok(json) = serde_json::from_slice::<serde_json::Value>(&bytes) else {
            continue;
        };
        let source = json
            .get("datId")
            .and_then(|v| v.as_str())
            .unwrap_or("manifest.efdat")
            .to_string();
        let relationships = json
            .get("relationships")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();
        for rel in relationships {
            let rel_type = rel
                .get("type")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let target = rel.get("target").and_then(|v| v.as_object());
            let Some(target) = target else {
                continue;
            };
            if target.get("kind").and_then(|v| v.as_str()) != Some("momi") {
                continue;
            }
            let Some(mod_id) = target.get("id").and_then(|v| v.as_str()) else {
                continue;
            };
            let canonical = match rel_type.as_str() {
                "requires" => "requires",
                "optional" => "optional",
                "conflicts" => "conflicts",
                _ => continue,
            };
            out.push(MomiProjectedRelation {
                pack_id: source.clone(),
                momi_mod_id: mod_id.to_string(),
                relation: canonical.to_string(),
                source: "efdat".to_string(),
            });
        }
    }
    out
}

fn runtime_progress_index(engine: &EngineState) -> usize {
    match engine.phase {
        Phase::Boot => 0,
        Phase::Diagnostics => 1,
        Phase::Monitor => 5,
    }
}

fn show_boot_steps(ui: &mut egui::Ui, engine: &EngineState, default_open: bool) {
    if engine.boot_steps.is_empty() {
        return;
    }

    let ok_count = engine
        .boot_steps
        .iter()
        .filter(|s| s.status == BootStepStatus::Ok)
        .count();
    let boot_header = format!("BOOT  ({}/{} steps ok)", ok_count, engine.boot_steps.len());

    CollapsingHeader::new(
        RichText::new(boot_header)
            .color(theme::TEXT_PRIMARY)
            .size(12.0),
    )
    .default_open(default_open)
    .show(ui, |ui| {
        for step in &engine.boot_steps {
            ui.horizontal(|ui| {
                let (color, symbol) = match step.status {
                    BootStepStatus::Ok => (theme::GREEN, "✓"),
                    BootStepStatus::Warning => (theme::AMBER, "⚠"),
                    BootStepStatus::Error => (theme::SEV_ERROR, "✗"),
                    BootStepStatus::InProgress => (theme::TEXT_MUTED, "…"),
                };
                ui.label(
                    RichText::new(format!("  {symbol}  "))
                        .color(color)
                        .size(11.0),
                );
                ui.label(
                    RichText::new(&step.label)
                        .color(theme::TEXT_SECONDARY)
                        .size(11.0),
                );
            });
        }
    });

    ui.add_space(2.0);
}

fn kind_color(kind: &str) -> Color32 {
    match kind {
        "yyc_script" => theme::SEV_HAZARD,
        "frame" => theme::AMBER,
        "detour" => theme::SEV_ERROR,
        _ => theme::TEXT_SECONDARY,
    }
}

fn event_type_color(event_type: &str) -> Color32 {
    match event_type {
        "HOOK" => theme::GREEN,
        "EVENT" => theme::SEV_HAZARD,
        "STORY" => Color32::from_rgb(0xab, 0x47, 0xbc), // purple
        "QUEST" => theme::AMBER,
        "DIALOGUE" => Color32::from_rgb(0x29, 0xb6, 0xf6), // light blue
        _ => theme::TEXT_SECONDARY,
    }
}

fn badge(ui: &mut egui::Ui, label: &str, color: Color32) {
    ui.label(RichText::new(label).color(color).size(12.0).strong());
}

fn id_color(id: &str) -> Color32 {
    let mut hash: u32 = 2166136261;
    for b in id.as_bytes() {
        hash ^= *b as u32;
        hash = hash.wrapping_mul(16777619);
    }
    let r = ((hash >> 16) & 0x7F) as u8 + 64;
    let g = ((hash >> 8) & 0x7F) as u8 + 64;
    let b = (hash & 0x7F) as u8 + 64;
    Color32::from_rgb(r, g, b)
}
