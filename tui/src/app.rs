use ratatui::Frame;

use crate::diagnostics::collector::DiagnosticCollector;
use crate::ipc::PipeReader;
use crate::phases;

/// TUI phases: Boot → Diagnostics → Monitor
#[derive(Debug, Clone, PartialEq)]
pub enum Phase {
    Boot,
    Diagnostics,
    Monitor,
}

/// Root application state machine.
pub struct App {
    pub phase: Phase,
    pub collector: DiagnosticCollector,
    pub pipe_reader: Option<PipeReader>,
}

impl App {
    pub fn new() -> Self {
        // TODO: Accept pipe name from CLI args
        Self {
            phase: Phase::Boot,
            collector: DiagnosticCollector::new(),
            pipe_reader: None,
        }
    }

    /// Render the current phase.
    pub fn render(&self, frame: &mut Frame) {
        match self.phase {
            Phase::Boot => phases::boot::render(frame, self),
            Phase::Diagnostics => phases::diagnostics::render(frame, self),
            Phase::Monitor => phases::monitor::render(frame, self),
        }
    }

    /// Called each tick — process incoming pipe messages and update state.
    pub fn tick(&mut self) {
        // TODO: Read from pipe, update collector, handle phase transitions
    }

    /// Transition to a new phase (driven by engine via phase.transition message).
    pub fn transition_to(&mut self, phase: Phase) {
        self.phase = phase;
    }
}
