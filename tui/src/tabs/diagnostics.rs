use egui::{Color32, RichText, ScrollArea};

use crate::engine_state::EngineState;
use crate::state::{DiagnosticsState, PipeStatus};
use crate::theme;

pub fn show(ui: &mut egui::Ui, diag: &mut DiagnosticsState, engine: &mut EngineState) {
    // ── Status bar ──────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        let (status_color, status_text) = match &diag.pipe_status {
            PipeStatus::Connected(name) => {
                (theme::GREEN, format!("Connected: {name}"))
            }
            PipeStatus::Searching => (theme::AMBER, "Searching for engine...".into()),
            PipeStatus::Disconnected => (theme::SEV_ERROR, "Disconnected".into()),
            PipeStatus::NoEngine => (theme::SEV_ERROR, "No engine found".into()),
        };

        let pill = RichText::new(format!("  ●  {status_text}  "))
            .size(12.0)
            .color(status_color);
        ui.label(pill);

        if matches!(diag.pipe_status, PipeStatus::NoEngine | PipeStatus::Disconnected) {
            if ui.small_button("Retry").clicked() {
                diag.pipe_reader = None;
                diag.pipe_status = PipeStatus::Searching;
            }
        }
    });

    ui.separator();

    // ── Stats row ────────────────────────────────────────────────────────────
    ui.horizontal(|ui| {
        badge(ui, &format!("{} errors", engine.collector.error_count()), theme::SEV_ERROR);
        ui.add_space(6.0);
        badge(ui, &format!("{} warnings", engine.collector.warning_count()), theme::SEV_WARNING);
        ui.add_space(6.0);
        badge(ui, &format!("{} hazards", engine.collector.hazard_count()), theme::SEV_HAZARD);
    });

    ui.add_space(4.0);
    ui.separator();

    // ── Log ──────────────────────────────────────────────────────────────────
    let entries = engine.collector.all();

    if entries.is_empty() {
        ui.add_space(40.0);
        ui.vertical_centered(|ui| {
            ui.label(
                RichText::new("Waiting for engine...")
                    .color(theme::TEXT_MUTED)
                    .size(14.0),
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
                    // Severity badge
                    let (sev_color, sev_label) = match entry.severity.as_str() {
                        "error" => (theme::SEV_ERROR, "E"),
                        "warning" => (theme::SEV_WARNING, "W"),
                        "hazard" => (theme::SEV_HAZARD, "H"),
                        _ => (theme::TEXT_MUTED, "?"),
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

fn badge(ui: &mut egui::Ui, label: &str, color: Color32) {
    ui.label(RichText::new(label).color(color).size(12.0).strong());
}
