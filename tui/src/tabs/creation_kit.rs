use egui::RichText;

use crate::theme;

pub fn show(ui: &mut egui::Ui) {
    ui.add_space(60.0);
    ui.vertical_centered(|ui| {
        ui.label(
            RichText::new("Creation Kit")
                .size(28.0)
                .color(theme::TEXT_PRIMARY),
        );
        ui.add_space(10.0);
        ui.label(
            RichText::new("Coming in a future release")
                .size(14.0)
                .color(theme::TEXT_MUTED),
        );
    });
}
