use ratatui::buffer::Buffer;
use ratatui::layout::Rect;
use ratatui::style::{Modifier, Style};
use ratatui::widgets::Widget;

use crate::diagnostics::collector::Diagnostic;
use crate::diagnostics::severity::Severity;
use crate::theme;

/// Scrollable list of diagnostic entries with severity badges.
pub struct DiagnosticList<'a> {
    entries: &'a [Diagnostic],
    scroll_offset: usize,
}

impl<'a> DiagnosticList<'a> {
    pub fn new(entries: &'a [Diagnostic], scroll_offset: usize) -> Self {
        Self { entries, scroll_offset }
    }
}

impl Widget for DiagnosticList<'_> {
    fn render(self, area: Rect, buf: &mut Buffer) {
        if area.height == 0 || self.entries.is_empty() {
            let empty_msg = if self.entries.is_empty() { "No diagnostics" } else { "" };
            buf.set_string(
                area.x + 1,
                area.y,
                empty_msg,
                Style::default().fg(theme::TEXT_MUTED),
            );
            return;
        }

        let visible = area.height as usize;
        let start = self.scroll_offset.min(self.entries.len().saturating_sub(visible));

        for (i, entry) in self.entries.iter().skip(start).take(visible).enumerate() {
            let y = area.y + i as u16;
            let severity = Severity::from_wire(&entry.severity);
            let color = severity.map(|s| s.color()).unwrap_or(theme::TEXT_MUTED);
            let badge_bg = severity.map(|s| s.badge_bg()).unwrap_or(theme::BG_PANEL);

            // Severity badge: [ERR] / [WRN] / [HAZ]
            let badge = match entry.severity.as_str() {
                "error" => "ERR",
                "warning" => "WRN",
                "hazard" => "HAZ",
                _ => "???",
            };
            let badge_str = format!(" {} ", badge);
            buf.set_string(
                area.x,
                y,
                &badge_str,
                Style::default().fg(color).bg(badge_bg).add_modifier(Modifier::BOLD),
            );

            // Code
            let code_x = area.x + badge_str.len() as u16 + 1;
            buf.set_string(
                code_x,
                y,
                &entry.code,
                Style::default().fg(color),
            );

            // Message
            let msg_x = code_x + entry.code.len() as u16 + 1;
            let available = area.width.saturating_sub(msg_x - area.x) as usize;
            let message: String = entry.message.chars().take(available).collect();
            buf.set_string(
                msg_x,
                y,
                &message,
                Style::default().fg(theme::TEXT_SECONDARY),
            );
        }
    }
}
