use std::fmt;

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
    #[allow(dead_code)]
    pub fn wire_name(&self) -> &'static str {
        match self {
            Severity::Error => "error",
            Severity::Warning => "warning",
            Severity::Hazard => "hazard",
        }
    }

    /// Single-character code used in diagnostic codes (E/W/H).
    #[allow(dead_code)]
    pub fn code_letter(&self) -> char {
        match self {
            Severity::Error => 'E',
            Severity::Warning => 'W',
            Severity::Hazard => 'H',
        }
    }

    /// Accent color as [R, G, B] for this severity.
    pub fn color(&self) -> [u8; 3] {
        match self {
            Severity::Error => [233, 30, 99],
            Severity::Warning => [255, 193, 7],
            Severity::Hazard => [0, 188, 212],
        }
    }

    /// Badge background color as [R, G, B] for this severity.
    pub fn badge_bg(&self) -> [u8; 3] {
        match self {
            Severity::Error => [62, 8, 20],
            Severity::Warning => [51, 39, 0],
            Severity::Hazard => [0, 38, 43],
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
