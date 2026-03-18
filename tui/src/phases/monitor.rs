use ratatui::Frame;
use ratatui::widgets::{Block, Borders, Paragraph};
use ratatui::style::Style;

use crate::app::App;
use crate::theme;

/// Render the monitor phase: live dashboard.
/// Panels for hooks, events, saves, and mod status.
/// Auto-updates via named pipe feed from engine DLL.
pub fn render(frame: &mut Frame, _app: &App) {
    let area = frame.area();

    let block = Block::default()
        .title(" EFL MONITOR ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(theme::BORDER_DEFAULT))
        .style(Style::default().bg(theme::BG_PRIMARY));

    let content = Paragraph::new("Monitoring...")
        .style(Style::default().fg(theme::GREEN))
        .block(block);

    frame.render_widget(content, area);
}
