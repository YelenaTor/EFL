use egui::{Color32, CollapsingHeader, RichText, ScrollArea};

use crate::engine_state::{BootStepStatus, EngineState, Phase};
use crate::state::{DiagnosticsState, PipeStatus};
use crate::theme;

pub fn show(ui: &mut egui::Ui, diag: &mut DiagnosticsState, engine: &mut EngineState) {
    // ── Status bar ──────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        let (status_color, status_text) = match &diag.pipe_status {
            PipeStatus::Connected(name) => (theme::GREEN, format!("Connected: {name}")),
            PipeStatus::Searching      => (theme::AMBER, "Searching for engine...".into()),
            PipeStatus::Disconnected   => (theme::SEV_ERROR, "Disconnected".into()),
            PipeStatus::NoEngine       => (theme::SEV_ERROR, "No engine found".into()),
        };

        ui.label(
            RichText::new(format!("  ●  {status_text}  "))
                .size(12.0)
                .color(status_color),
        );

        if matches!(diag.pipe_status, PipeStatus::NoEngine | PipeStatus::Disconnected) {
            if ui.small_button("Retry").clicked() {
                diag.pipe_reader = None;
                diag.pipe_status = PipeStatus::Searching;
            }
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
        badge(ui, &format!("{} errors",   engine.collector.error_count()),   theme::SEV_ERROR);
        ui.add_space(6.0);
        badge(ui, &format!("{} warnings", engine.collector.warning_count()), theme::SEV_WARNING);
        ui.add_space(6.0);
        badge(ui, &format!("{} hazards",  engine.collector.hazard_count()),  theme::SEV_HAZARD);
    });

    ui.add_space(4.0);
    ui.separator();

    // ── Monitor panels (visible once engine reaches Monitor phase) ───────────
    if engine.phase == Phase::Monitor {
        show_monitor_panels(ui, engine);
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

    ScrollArea::vertical()
        .stick_to_bottom(true)
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            for entry in entries {
                ui.horizontal(|ui| {
                    let (sev_color, sev_label) = match entry.severity.as_str() {
                        "error"   => (theme::SEV_ERROR,   "E"),
                        "warning" => (theme::SEV_WARNING, "W"),
                        "hazard"  => (theme::SEV_HAZARD,  "H"),
                        _         => (theme::TEXT_MUTED,  "?"),
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
            }
        });
}

fn show_monitor_panels(ui: &mut egui::Ui, engine: &EngineState) {
    // Boot Steps (collapsed by default in monitor phase — init succeeded)
    show_boot_steps(ui, engine, false);

    // Loaded Mods
    let mod_header = format!("MODS  ({})", engine.mods.len());
    CollapsingHeader::new(RichText::new(mod_header).color(theme::TEXT_PRIMARY).size(12.0))
        .default_open(true)
        .show(ui, |ui| {
            if engine.mods.is_empty() {
                ui.label(RichText::new("No mods loaded").color(theme::TEXT_MUTED).size(11.0));
            } else {
                for m in &engine.mods {
                    ui.horizontal(|ui| {
                        let status_color = match m.status.as_str() {
                            "loaded" => theme::GREEN,
                            "error"  => theme::SEV_ERROR,
                            _        => theme::AMBER,
                        };
                        ui.label(
                            RichText::new(format!("  ●  {}", m.id))
                                .color(status_color)
                                .size(12.0),
                        );
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

    // Active Hooks
    let hook_header = format!("HOOKS  ({})", engine.hooks.len());
    CollapsingHeader::new(RichText::new(hook_header).color(theme::TEXT_PRIMARY).size(12.0))
        .default_open(true)
        .show(ui, |ui| {
            if engine.hooks.is_empty() {
                ui.label(RichText::new("No hooks registered").color(theme::TEXT_MUTED).size(11.0));
            } else {
                // Sort by fire count descending so active hooks rise to the top
                let mut sorted: Vec<_> = engine.hooks.iter().collect();
                sorted.sort_by(|a, b| b.fire_count.cmp(&a.fire_count));

                for hook in sorted {
                    ui.horizontal(|ui| {
                        let kind_color = kind_color(&hook.kind);
                        ui.label(
                            RichText::new(format!("  {:>5}× ", hook.fire_count))
                                .color(if hook.fire_count > 0 { theme::GREEN } else { theme::TEXT_MUTED })
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
    CollapsingHeader::new(RichText::new(event_header).color(theme::TEXT_PRIMARY).size(12.0))
        .default_open(true)
        .show(ui, |ui| {
            if engine.event_log.is_empty() {
                ui.label(RichText::new("No events yet").color(theme::TEXT_MUTED).size(11.0));
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
    CollapsingHeader::new(RichText::new(save_header).color(theme::TEXT_PRIMARY).size(12.0))
        .default_open(false)
        .show(ui, |ui| {
            if engine.save_log.is_empty() {
                ui.label(RichText::new("No save operations").color(theme::TEXT_MUTED).size(11.0));
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

fn show_boot_steps(ui: &mut egui::Ui, engine: &EngineState, default_open: bool) {
    if engine.boot_steps.is_empty() {
        return;
    }

    let ok_count = engine.boot_steps.iter()
        .filter(|s| s.status == BootStepStatus::Ok)
        .count();
    let boot_header = format!("BOOT  ({}/{} steps ok)", ok_count, engine.boot_steps.len());

    CollapsingHeader::new(RichText::new(boot_header).color(theme::TEXT_PRIMARY).size(12.0))
        .default_open(default_open)
        .show(ui, |ui| {
            for step in &engine.boot_steps {
                ui.horizontal(|ui| {
                    let (color, symbol) = match step.status {
                        BootStepStatus::Ok         => (theme::GREEN,       "✓"),
                        BootStepStatus::Warning    => (theme::AMBER,       "⚠"),
                        BootStepStatus::Error      => (theme::SEV_ERROR,   "✗"),
                        BootStepStatus::InProgress => (theme::TEXT_MUTED,  "…"),
                    };
                    ui.label(RichText::new(format!("  {symbol}  ")).color(color).size(11.0));
                    ui.label(RichText::new(&step.label).color(theme::TEXT_SECONDARY).size(11.0));
                });
            }
        });

    ui.add_space(2.0);
}

fn kind_color(kind: &str) -> Color32 {
    match kind {
        "yyc_script" => theme::SEV_HAZARD,
        "frame"      => theme::AMBER,
        "detour"     => theme::SEV_ERROR,
        _            => theme::TEXT_SECONDARY,
    }
}

fn event_type_color(event_type: &str) -> Color32 {
    match event_type {
        "HOOK"    => theme::GREEN,
        "EVENT"   => theme::SEV_HAZARD,
        "STORY"   => Color32::from_rgb(0xab, 0x47, 0xbc),  // purple
        "QUEST"   => theme::AMBER,
        "DIALOGUE"=> Color32::from_rgb(0x29, 0xb6, 0xf6),  // light blue
        _         => theme::TEXT_SECONDARY,
    }
}

fn badge(ui: &mut egui::Ui, label: &str, color: Color32) {
    ui.label(RichText::new(label).color(color).size(12.0).strong());
}
