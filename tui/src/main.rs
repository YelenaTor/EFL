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
    let icon = load_icon();

    eframe::run_native(
        "EFL DevKit",
        eframe::NativeOptions {
            viewport: egui::ViewportBuilder::default()
                .with_title("EFL DevKit")
                .with_inner_size([1200.0, 800.0])
                .with_min_inner_size([800.0, 600.0])
                .with_icon(icon),
            ..Default::default()
        },
        Box::new(|cc| Ok(Box::new(app::DevKitApp::new(cc)))),
    )
}

fn load_icon() -> egui::viewport::IconData {
    let bytes = include_bytes!("../assets/logo.png");
    let img = image::load_from_memory(bytes)
        .expect("failed to decode logo.png")
        .to_rgba8();
    let (width, height) = img.dimensions();
    egui::viewport::IconData {
        rgba: img.into_raw(),
        width,
        height,
    }
}
