use egui::{Key, Modifiers, RichText, ViewportCommand};
use std::collections::BTreeSet;

use crate::pipe::{discovery::connect_to_latest_efl_pipe, PipeReader};
use crate::settings::AppSettings;
use crate::state::{AppState, PipeStatus, Tab};
use crate::tabs::{creation_kit, diagnostics, packs};
use crate::theme;

pub struct DevKitApp {
    state: AppState,
}

impl DevKitApp {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        theme::apply_theme(&cc.egui_ctx);

        let settings = AppSettings::load();
        let mut state = AppState::new(settings);

        // Demo mode: efl-devkit --demo
        if std::env::args().any(|a| a == "--demo") {
            let rx = crate::demo::start_demo();
            state.diag.pipe_reader = Some(PipeReader::from_channel(rx));
            state.diag.pipe_status = PipeStatus::Connected("demo".into());
        }

        // If projects dir is already set, scan immediately
        if state.settings.projects_dir.is_some() {
            packs::scan_project_folders(&mut state);
        }

        Self { state }
    }

    /// Handle keyboard shortcuts.
    fn handle_input(&mut self, ctx: &egui::Context) {
        let shortcut_fired = ctx.input(|i| {
            i.key_pressed(Key::K)
                && i.modifiers
                    .matches_logically(Modifiers::CTRL | Modifiers::SHIFT)
        });

        if shortcut_fired {
            self.state.ck_visible = !self.state.ck_visible;
            if !self.state.ck_visible && self.state.active_tab == Tab::CreationKit {
                self.state.active_tab = Tab::Packs;
            }
        }
    }

    /// Discover / drain the named pipe; handle reconnect.
    fn tick_pipe(&mut self, ctx: &egui::Context) {
        // Skip if demo mode
        if matches!(self.state.diag.pipe_status, PipeStatus::Connected(ref n) if n == "demo") {
            if let Some(ref reader) = self.state.diag.pipe_reader {
                let mut count = 0;
                while let Some(msg) = reader.try_recv() {
                    self.state.engine.handle_message(msg);
                    count += 1;
                    if count >= 100 {
                        break;
                    }
                }
                if count > 0 {
                    ctx.request_repaint();
                }
            }
            return;
        }

        // If no reader, try to discover a pipe
        if self.state.diag.pipe_reader.is_none() {
            if let PipeStatus::Searching | PipeStatus::Disconnected = &self.state.diag.pipe_status {
                match connect_to_latest_efl_pipe() {
                    Some(name) => {
                        self.state.diag.pipe_reader = Some(PipeReader::connect(&name));
                        self.state.diag.pipe_status = PipeStatus::Connected(name);
                    }
                    None => {
                        self.state.diag.pipe_status = PipeStatus::NoEngine;
                    }
                }
            }
            return;
        }

        // Drain messages
        let reader = self.state.diag.pipe_reader.as_ref().unwrap();
        let mut count = 0;
        loop {
            match reader.try_recv() {
                Some(msg) => {
                    self.state.engine.handle_message(msg);
                    count += 1;
                    if count >= 100 {
                        break;
                    }
                }
                None => {
                    // Check for disconnect
                    if reader.is_disconnected() {
                        self.state.diag.pipe_reader = None;
                        self.state.diag.pipe_status = PipeStatus::Disconnected;
                    }
                    break;
                }
            }
        }

        if count > 0 {
            ctx.request_repaint();
        }
    }

    /// Poll the file watcher and auto-repack on changes.
    fn tick_watcher(&mut self) {
        if !self.state.packs.watch_active {
            return;
        }
        let Some(ref watcher) = self.state.packs.watcher else {
            return;
        };
        let Some(ref pack_path) = self.state.packs.selected_pack.clone() else {
            return;
        };

        // Drain watcher events; repack on modify/create/remove and show clear diff summary.
        let mut should_repack = false;
        let mut changed_paths: BTreeSet<String> = BTreeSet::new();
        let mut reasons: BTreeSet<&'static str> = BTreeSet::new();
        while let Ok(event) = watcher.rx.try_recv() {
            if let Ok(ev) = event {
                use notify::EventKind;
                let reason = match ev.kind {
                    EventKind::Modify(_) => Some("modified"),
                    EventKind::Create(_) => Some("created"),
                    EventKind::Remove(_) => Some("removed"),
                    _ => None,
                };
                if let Some(reason) = reason {
                    should_repack = true;
                    reasons.insert(reason);
                    for path in ev.paths {
                        let label = path
                            .strip_prefix(pack_path)
                            .ok()
                            .map(|p| p.to_string_lossy().into_owned())
                            .unwrap_or_else(|| path.to_string_lossy().into_owned())
                            .replace('\\', "/");
                        changed_paths.insert(label);
                    }
                }
            }
        }

        if should_repack {
            let mut preview: Vec<String> = changed_paths.into_iter().collect();
            let total = preview.len();
            preview.truncate(4);
            let changed = if preview.is_empty() {
                "unknown files".to_string()
            } else if total > preview.len() {
                format!("{} (+{} more)", preview.join(", "), total - preview.len())
            } else {
                preview.join(", ")
            };
            let why = reasons.into_iter().collect::<Vec<_>>().join("/");
            self.state.packs.last_watch_summary =
                Some(format!("Repack triggered by {why}: {changed}"));
            let result = if let Some(ref out_dir) = self.state.settings.output_dir {
                crate::pack::pack_with_request(&crate::pack::BuildRequest::one_command(
                    pack_path.clone(),
                    out_dir.clone(),
                ))
                .map_err(|e| e.to_string())
            } else {
                Err("No output directory configured.".into())
            };
            let succeeded = result.is_ok();
            self.state.packs.last_pack_result = Some(result);
            if succeeded && self.state.packs.auto_reload_on_build {
                self.state.packs.last_reload_status = Some(
                    crate::tabs::packs::try_send_reload(&self.state, "devkit-watch"),
                );
            }
        }
    }

    fn render_menu_bar(&mut self, ctx: &egui::Context) {
        egui::TopBottomPanel::top("menu_bar").show(ctx, |ui| {
            egui::menu::bar(ui, |ui| {
                ui.menu_button("File", |ui| {
                    if ui.button("Open Pack...").clicked() {
                        if let Some(file) = rfd::FileDialog::new()
                            .add_filter("EFL Artifacts", &["efpack", "efdat"])
                            .pick_file()
                        {
                            self.state.packs.last_inspect_result = match file
                                .extension()
                                .and_then(|e| e.to_str())
                                .unwrap_or_default()
                                .to_ascii_lowercase()
                                .as_str()
                            {
                                "efdat" => crate::pack::inspect_efdat(&file).ok(),
                                _ => crate::pack::inspect_efpack(&file).ok(),
                            };
                            self.state.active_tab = Tab::Packs;
                        }
                        ui.close_menu();
                    }
                    ui.separator();
                    if ui.button("Quit").clicked() {
                        ctx.send_viewport_cmd(ViewportCommand::Close);
                    }
                });

                ui.menu_button("Settings", |ui| {
                    if ui.button("Configure Paths...").clicked() {
                        self.state.packs.first_run_modal_open = true;
                        ui.close_menu();
                    }
                });

                ui.menu_button("Import", |ui| {
                    if ui.button("Import .efpack...").clicked() {
                        if let Some(file) = rfd::FileDialog::new()
                            .add_filter("EFL Artifacts", &["efpack", "efdat"])
                            .pick_file()
                        {
                            self.state.packs.last_inspect_result = match file
                                .extension()
                                .and_then(|e| e.to_str())
                                .unwrap_or_default()
                                .to_ascii_lowercase()
                                .as_str()
                            {
                                "efdat" => crate::pack::inspect_efdat(&file).ok(),
                                _ => crate::pack::inspect_efpack(&file).ok(),
                            };
                            self.state.active_tab = Tab::Packs;
                        }
                        ui.close_menu();
                    }
                });

                ui.menu_button("View", |ui| {
                    let ck_label = if self.state.ck_visible {
                        "✓ Creation Kit"
                    } else {
                        "  Creation Kit"
                    };
                    if ui.button(ck_label).clicked() {
                        self.state.ck_visible = !self.state.ck_visible;
                        if !self.state.ck_visible && self.state.active_tab == Tab::CreationKit {
                            self.state.active_tab = Tab::Packs;
                        }
                        ui.close_menu();
                    }
                    if ui.button("Refresh Packs").clicked() {
                        packs::scan_project_folders(&mut self.state);
                        ui.close_menu();
                    }
                });
            });
        });
    }

    fn render_sidebar_nav(&mut self, ctx: &egui::Context) {
        egui::SidePanel::left("main_nav_sidebar")
            .resizable(false)
            .default_width(190.0)
            .show(ctx, |ui| {
                ui.add_space(8.0);
                ui.label(
                    RichText::new("DevKit")
                        .size(14.0)
                        .strong()
                        .color(theme::TEXT_PRIMARY),
                );
                ui.add_space(6.0);
                self.nav_button(ui, "Packs", Tab::Packs);
                self.nav_button(ui, "Diagnostics", Tab::Diagnostics);

                if self.state.ck_visible {
                    ui.add_space(14.0);
                    ui.label(
                        RichText::new("Creation Kit")
                            .size(14.0)
                            .strong()
                            .color(theme::TEXT_PRIMARY),
                    );
                    ui.add_space(6.0);
                    self.nav_button(ui, "Preview", Tab::CreationKit);
                }
            });
    }

    fn nav_button(&mut self, ui: &mut egui::Ui, label: &str, tab: Tab) {
        let is_active = self.state.active_tab == tab;
        let text = if is_active {
            RichText::new(label).color(theme::ACCENT).strong()
        } else {
            RichText::new(label).color(theme::TEXT_SECONDARY)
        };

        let btn = egui::Button::new(text)
            .fill(egui::Color32::TRANSPARENT)
            .stroke(if is_active {
                egui::Stroke::new(2.0, theme::ACCENT)
            } else {
                egui::Stroke::NONE
            });

        if ui.add(btn).clicked() {
            self.state.active_tab = tab;
        }
        ui.add_space(2.0);
    }

    fn render_active_tab(&mut self, ctx: &egui::Context) {
        egui::CentralPanel::default().show(ctx, |ui| match self.state.active_tab {
            Tab::Packs => packs::show(ui, &mut self.state, ctx),
            Tab::Diagnostics => diagnostics::show(
                ctx,
                ui,
                &mut self.state.diag,
                &mut self.state.engine,
                self.state.packs.selected_pack.as_deref(),
                self.state.settings.projects_dir.as_deref(),
            ),
            Tab::CreationKit => creation_kit::show(ui),
        });
    }
}

impl eframe::App for DevKitApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.handle_input(ctx);
        if !self.state.ck_visible && self.state.active_tab == Tab::CreationKit {
            self.state.active_tab = Tab::Packs;
        }
        self.tick_pipe(ctx);
        self.tick_watcher();
        self.render_menu_bar(ctx);
        self.render_sidebar_nav(ctx);
        self.render_active_tab(ctx);
    }

    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        let _ = self.state.settings.save();
    }
}
