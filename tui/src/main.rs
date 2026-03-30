#![windows_subsystem = "windows"]

mod app;
mod demo;
mod diagnostics;
mod engine_state;
mod pack;
mod pipe;
mod settings;
mod state;
mod tabs;
mod theme;

fn main() -> eframe::Result<()> {
    eframe::run_native(
        "EFL DevKit",
        eframe::NativeOptions {
            viewport: egui::ViewportBuilder::default()
                .with_title("EFL DevKit")
                .with_inner_size([1200.0, 800.0])
                .with_min_inner_size([800.0, 600.0]),
            ..Default::default()
        },
        Box::new(|cc| Ok(Box::new(app::DevKitApp::new(cc)))),
    )
}
