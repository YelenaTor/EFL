use egui::{Key, Modifiers, RichText, ViewportCommand};

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
                && i.modifiers.matches_logically(Modifiers::CTRL | Modifiers::SHIFT)
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
                    if count >= 100 { break; }
                }
                if count > 0 { ctx.request_repaint(); }
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
                    if count >= 100 { break; }
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

        // Drain watcher events; repack on any write/create event
        let mut should_repack = false;
        while let Ok(event) = watcher.rx.try_recv() {
            if let Ok(ev) = event {
                use notify::EventKind;
                if matches!(ev.kind, EventKind::Modify(_) | EventKind::Create(_)) {
                    should_repack = true;
                }
            }
        }

        if should_repack {
            let out_dir = self.state.settings.output_dir.clone();
            let out_path = out_dir.map(|d| {
                let name = pack_path
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_else(|| "pack".into());
                d.join(format!("{name}.efpack"))
            });
            self.state.packs.last_pack_result = Some(
                crate::pack::pack_folder(pack_path, out_path.as_deref())
                    .map_err(|e| e.to_string()),
            );
        }
    }

    fn render_menu_bar(&mut self, ctx: &egui::Context) {
        egui::TopBottomPanel::top("menu_bar").show(ctx, |ui| {
            egui::menu::bar(ui, |ui| {
                ui.menu_button("File", |ui| {
                    if ui.button("Open Pack...").clicked() {
                        if let Some(file) = rfd::FileDialog::new()
                            .add_filter("EFL Pack", &["efpack"])
                            .pick_file()
                        {
                            self.state.packs.last_inspect_result =
                                crate::pack::inspect_efpack(&file).ok();
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
                            .add_filter("EFL Pack", &["efpack"])
                            .pick_file()
                        {
                            self.state.packs.last_inspect_result =
                                crate::pack::inspect_efpack(&file).ok();
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

    fn render_tab_strip(&mut self, ctx: &egui::Context) {
        egui::TopBottomPanel::top("tab_strip").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.add_space(4.0);
                self.tab_button(ui, "Packs", Tab::Packs);
                self.tab_button(ui, "Diagnostics", Tab::Diagnostics);
                if self.state.ck_visible {
                    self.tab_button(ui, "Creation Kit*", Tab::CreationKit);
                }
            });
        });
    }

    fn tab_button(&mut self, ui: &mut egui::Ui, label: &str, tab: Tab) {
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
        ui.add_space(4.0);
    }

    fn render_active_tab(&mut self, ctx: &egui::Context) {
        egui::CentralPanel::default().show(ctx, |ui| {
            match self.state.active_tab {
                Tab::Packs => packs::show(ui, &mut self.state, ctx),
                Tab::Diagnostics => {
                    diagnostics::show(ui, &mut self.state.diag, &mut self.state.engine)
                }
                Tab::CreationKit => creation_kit::show(ui),
            }
        });
    }
}

impl eframe::App for DevKitApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.handle_input(ctx);
        self.tick_pipe(ctx);
        self.tick_watcher();
        self.render_menu_bar(ctx);
        self.render_tab_strip(ctx);
        self.render_active_tab(ctx);
    }

    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        let _ = self.state.settings.save();
    }
}
