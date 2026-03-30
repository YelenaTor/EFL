use egui::{Color32, Rounding, Style, Visuals};

// EFL DevKit color palette — dark carbon + coral pink + gunmetal
pub const BG_PRIMARY: Color32     = Color32::from_rgb(0x1a, 0x1a, 0x1a); // carbon black
pub const BG_PANEL: Color32       = Color32::from_rgb(0x20, 0x20, 0x20);
pub const BG_HEADER: Color32      = Color32::from_rgb(0x28, 0x28, 0x28);
pub const BG_HOVER: Color32       = Color32::from_rgb(0x3a, 0x3a, 0x3a); // gunmetal
pub const ACCENT: Color32         = Color32::from_rgb(0xe8, 0x62, 0x5a); // coral pink
pub const BORDER: Color32         = Color32::from_rgb(0x2e, 0x2e, 0x2e);
pub const TEXT_PRIMARY: Color32   = Color32::from_rgb(0xe0, 0xe0, 0xe0);
pub const TEXT_SECONDARY: Color32 = Color32::from_rgb(0x9e, 0x9e, 0x9e);
pub const TEXT_MUTED: Color32     = Color32::from_rgb(0x61, 0x61, 0x61);
pub const GREEN: Color32          = Color32::from_rgb(0x00, 0xe6, 0x76);
pub const AMBER: Color32          = Color32::from_rgb(0xff, 0xc1, 0x07);

// Severity colors (matching diagnostics/severity.rs RGB arrays)
pub const SEV_ERROR: Color32      = Color32::from_rgb(0xe9, 0x1e, 0x63);
pub const SEV_WARNING: Color32    = Color32::from_rgb(0xff, 0xc1, 0x07);
pub const SEV_HAZARD: Color32     = Color32::from_rgb(0x00, 0xbc, 0xd4);

/// Convert a severity [R, G, B] array to a Color32.
pub fn severity_color(rgb: [u8; 3]) -> Color32 {
    Color32::from_rgb(rgb[0], rgb[1], rgb[2])
}

/// Apply the EFL DevKit theme to an egui context.
pub fn apply_theme(ctx: &egui::Context) {
    let mut style = Style::default();
    let mut visuals = Visuals::dark();

    // Backgrounds
    visuals.panel_fill = BG_PRIMARY;
    visuals.window_fill = BG_PANEL;
    visuals.faint_bg_color = BG_HEADER;
    visuals.extreme_bg_color = BG_PRIMARY;

    // Borders
    visuals.widgets.noninteractive.bg_stroke.color = BORDER;
    visuals.widgets.inactive.bg_stroke.color = BORDER;

    // Inactive / default widget state
    visuals.widgets.inactive.weak_bg_fill = BG_PANEL;
    visuals.widgets.inactive.bg_fill = BG_PANEL;
    visuals.widgets.inactive.fg_stroke.color = TEXT_SECONDARY;

    // Hovered — gunmetal bg, coral stroke
    visuals.widgets.hovered.weak_bg_fill = BG_HOVER;
    visuals.widgets.hovered.bg_fill = BG_HOVER;
    visuals.widgets.hovered.bg_stroke.color = ACCENT;
    visuals.widgets.hovered.fg_stroke.color = TEXT_PRIMARY;

    // Active / pressed — coral fill
    visuals.widgets.active.bg_fill = ACCENT;
    visuals.widgets.active.weak_bg_fill = ACCENT;
    visuals.widgets.active.fg_stroke.color = Color32::WHITE;

    // Selection
    visuals.selection.bg_fill = Color32::from_rgba_unmultiplied(0xe8, 0x62, 0x5a, 0x45);
    visuals.selection.stroke.color = ACCENT;

    // Hyperlinks
    visuals.hyperlink_color = ACCENT;

    // Rounding
    visuals.window_rounding = Rounding::same(4.0);
    visuals.widgets.noninteractive.rounding = Rounding::same(3.0);
    visuals.widgets.inactive.rounding = Rounding::same(3.0);
    visuals.widgets.hovered.rounding = Rounding::same(3.0);
    visuals.widgets.active.rounding = Rounding::same(3.0);

    style.visuals = visuals;
    ctx.set_style(style);
}
