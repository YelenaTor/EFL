use ratatui::Frame;
use ratatui::widgets::{Block, Borders, Paragraph};
use ratatui::style::Style;

use crate::app::App;
use crate::theme;

/// Render the diagnostics phase: structured validation report.
/// Marathon-style card grid showing per-subsystem results,
/// with a scrollable coded diagnostic list below.
pub fn render(frame: &mut Frame, _app: &App) {
    let area = frame.area();

    let block = Block::default()
        .title(" EFL DIAGNOSTICS ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(theme::BORDER_DEFAULT))
        .style(Style::default().bg(theme::BG_PRIMARY));

    let content = Paragraph::new("Running validation...")
        .style(Style::default().fg(theme::TEXT_PRIMARY))
        .block(block);

    frame.render_widget(content, area);
}
