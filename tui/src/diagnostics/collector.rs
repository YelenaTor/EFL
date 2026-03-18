use serde::Deserialize;

/// A single diagnostic entry received from the engine.
#[derive(Debug, Clone, Deserialize)]
pub struct Diagnostic {
    pub code: String,
    pub severity: String,
    pub category: String,
    pub message: String,
    #[serde(default)]
    pub suggestion: Option<String>,
    #[serde(default)]
    pub source: Option<DiagnosticSource>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct DiagnosticSource {
    pub file: Option<String>,
    pub field: Option<String>,
}

/// Collects diagnostics during boot and validation phases.
pub struct DiagnosticCollector {
    entries: Vec<Diagnostic>,
}

impl DiagnosticCollector {
    pub fn new() -> Self {
        Self {
            entries: Vec::new(),
        }
    }

    pub fn add(&mut self, diagnostic: Diagnostic) {
        self.entries.push(diagnostic);
    }

    pub fn all(&self) -> &[Diagnostic] {
        &self.entries
    }

    /// Count diagnostics by severity.
    pub fn count_by_severity(&self, severity: &str) -> usize {
        self.entries.iter().filter(|d| d.severity == severity).count()
    }

    /// Count diagnostics by category prefix.
    pub fn count_by_category(&self, category: &str) -> usize {
        self.entries.iter().filter(|d| d.category == category).count()
    }

    pub fn error_count(&self) -> usize {
        self.count_by_severity("error")
    }

    pub fn warning_count(&self) -> usize {
        self.count_by_severity("warning")
    }

    pub fn hazard_count(&self) -> usize {
        self.count_by_severity("hazard")
    }

    pub fn clear(&mut self) {
        self.entries.clear();
    }
}
