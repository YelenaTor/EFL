use std::fmt;

use ratatui::style::Color;

use crate::theme;

/// Diagnostic severity levels.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Severity {
    /// Fatal to subsystem, prevents operation.
    Error,
    /// Degraded but functional, should be fixed.
    Warning,
    /// Potential future problem, proactive alert.
    Hazard,
}

impl Severity {
    /// Wire format string (lowercase, as sent over named pipe).
    pub fn wire_name(&self) -> &'static str {
        match self {
            Severity::Error => "error",
            Severity::Warning => "warning",
            Severity::Hazard => "hazard",
        }
    }

    /// Single-character code used in diagnostic codes (E/W/H).
    pub fn code_letter(&self) -> char {
        match self {
            Severity::Error => 'E',
            Severity::Warning => 'W',
            Severity::Hazard => 'H',
        }
    }

    /// Accent color for this severity.
    pub fn color(&self) -> Color {
        match self {
            Severity::Error => theme::MAGENTA,
            Severity::Warning => theme::AMBER,
            Severity::Hazard => theme::CYAN,
        }
    }

    /// Badge background color for this severity.
    pub fn badge_bg(&self) -> Color {
        match self {
            Severity::Error => theme::SEVERITY_ERROR_BG,
            Severity::Warning => theme::SEVERITY_WARNING_BG,
            Severity::Hazard => theme::SEVERITY_HAZARD_BG,
        }
    }

    pub fn from_wire(s: &str) -> Option<Self> {
        match s {
            "error" => Some(Severity::Error),
            "warning" => Some(Severity::Warning),
            "hazard" => Some(Severity::Hazard),
            _ => None,
        }
    }
}

impl fmt::Display for Severity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Severity::Error => write!(f, "ERROR"),
            Severity::Warning => write!(f, "WARNING"),
            Severity::Hazard => write!(f, "HAZARD"),
        }
    }
}
