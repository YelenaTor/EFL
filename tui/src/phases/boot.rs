use ratatui::Frame;
use ratatui::layout::{Constraint, Layout, Direction};
use ratatui::style::{Modifier, Style};
use ratatui::widgets::Paragraph;

use crate::app::{App, BootStepStatus};
use crate::theme;
use crate::widgets::panel::Panel;
use crate::widgets::phase_indicator::PhaseIndicator;

/// Render the boot phase: animated step list as boot.status messages arrive.
pub fn render(frame: &mut Frame, app: &App) {
    let area = frame.area();

    // Background fill
    let bg = ratatui::widgets::Block::default()
        .style(Style::default().bg(theme::BG_PRIMARY));
    frame.render_widget(bg, area);

    let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),  // phase indicator
            Constraint::Length(1),  // spacer
            Constraint::Min(5),    // boot steps
        ])
        .split(area);

    // Phase indicator header
    frame.render_widget(PhaseIndicator::new(&app.phase), layout[0]);

    // Boot steps panel
    let panel = Panel::new("BOOT SEQUENCE").title_color(theme::GREEN);
    let inner = panel.block().inner(layout[2]);
    frame.render_widget(panel, layout[2]);

    // Version bar
    if inner.height > 0 {
        let version_line = format!("EFL v{}", app.efl_version);
        frame.render_widget(
            Paragraph::new(version_line)
                .style(Style::default().fg(theme::CYAN)),
            ratatui::layout::Rect::new(inner.x, inner.y, inner.width, 1),
        );
    }

    // Boot step list
    let steps_y = inner.y + 2;
    let steps_height = inner.height.saturating_sub(2);

    for (i, step) in app.boot_steps.iter().enumerate() {
        let y = steps_y + i as u16;
        if y >= steps_y + steps_height {
            break;
        }

        let (icon, color) = match step.status {
            BootStepStatus::Ok => ("✓", theme::GREEN),
            BootStepStatus::Warning => ("!", theme::AMBER),
            BootStepStatus::Error => ("✗", theme::MAGENTA),
            BootStepStatus::InProgress => ("…", theme::CYAN),
        };

        let line = format!("  {}  {}", icon, step.label);
        let step_area = ratatui::layout::Rect::new(inner.x, y, inner.width, 1);
        frame.render_widget(
            Paragraph::new(line)
                .style(Style::default().fg(color).add_modifier(
                    if step.status == BootStepStatus::InProgress {
                        Modifier::empty()
                    } else {
                        Modifier::empty()
                    },
                )),
            step_area,
        );
    }

    // Show waiting message if no steps yet
    if app.boot_steps.is_empty() {
        let msg_area = ratatui::layout::Rect::new(inner.x, steps_y, inner.width, 1);
        frame.render_widget(
            Paragraph::new("  Awaiting engine connection...")
                .style(Style::default().fg(theme::TEXT_MUTED)),
            msg_area,
        );
    }
}
