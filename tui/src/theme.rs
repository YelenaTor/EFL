use ratatui::style::Color;

// Marathon (2025) inspired color palette
// Dark chrome background with neon accent colors

/// Background
pub const BG_PRIMARY: Color = Color::Rgb(26, 26, 26);       // #1a1a1a
pub const BG_PANEL: Color = Color::Rgb(32, 32, 32);         // #202020
pub const BG_HEADER: Color = Color::Rgb(40, 40, 40);        // #282828

/// Borders
pub const BORDER_DEFAULT: Color = Color::Rgb(64, 64, 64);   // #404040
#[allow(dead_code)]
pub const BORDER_ACTIVE: Color = Color::Rgb(96, 96, 96);    // #606060

/// Status accents
pub const GREEN: Color = Color::Rgb(0, 230, 118);           // Active / pass / ok
pub const AMBER: Color = Color::Rgb(255, 193, 7);           // Warning / neutral
pub const MAGENTA: Color = Color::Rgb(233, 30, 99);         // Error / hostile / fatal
pub const CYAN: Color = Color::Rgb(0, 188, 212);            // Info / data

/// Text
pub const TEXT_PRIMARY: Color = Color::Rgb(224, 224, 224);   // #e0e0e0
pub const TEXT_SECONDARY: Color = Color::Rgb(158, 158, 158); // #9e9e9e
pub const TEXT_MUTED: Color = Color::Rgb(97, 97, 97);       // #616161

/// Severity badge backgrounds
pub const SEVERITY_ERROR_BG: Color = Color::Rgb(62, 8, 20);
pub const SEVERITY_WARNING_BG: Color = Color::Rgb(51, 39, 0);
pub const SEVERITY_HAZARD_BG: Color = Color::Rgb(0, 38, 43);
