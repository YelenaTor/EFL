use egui::RichText;

use crate::theme;

pub fn show(ui: &mut egui::Ui) {
    ui.add_space(36.0);
    ui.vertical_centered(|ui| {
        ui.label(
            RichText::new("Creation Kit (Preview)")
                .size(28.0)
                .color(theme::TEXT_PRIMARY),
        );
        ui.add_space(10.0);
        ui.label(
            RichText::new(
                "Creation Kit is an early visual-authoring surface for future EFL workflows.",
            )
            .size(14.0)
            .color(theme::TEXT_MUTED),
        );
        ui.add_space(4.0);
        ui.label(
            RichText::new(
                "For shipping today, use DevKit for pack build, validation, diagnostics, and release artifacts.",
            )
            .size(14.0)
            .color(theme::TEXT_MUTED),
        );
        ui.add_space(4.0);
        ui.label(
            RichText::new("Use this area as a preview workspace while core authoring remains in DevKit.")
                .size(14.0)
                .color(theme::TEXT_MUTED),
        );
    });
}
