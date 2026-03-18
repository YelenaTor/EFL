use ratatui::Frame;
use ratatui::layout::Constraint;
use ratatui::widgets::{Block, Borders, Paragraph};
use ratatui::style::Style;

use crate::app::App;
use crate::theme;

/// Render the boot phase: version checks, manifest discovery, subsystem init.
/// Items appear with status indicators as they complete.
pub fn render(frame: &mut Frame, _app: &App) {
    let area = frame.area();

    let block = Block::default()
        .title(" EFL BOOT SEQUENCE ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(theme::BORDER_DEFAULT))
        .style(Style::default().bg(theme::BG_PRIMARY));

    let content = Paragraph::new("Initializing EFL...")
        .style(Style::default().fg(theme::TEXT_PRIMARY))
        .block(block);

    frame.render_widget(content, area);
}
