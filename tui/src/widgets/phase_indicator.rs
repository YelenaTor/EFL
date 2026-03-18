use ratatui::buffer::Buffer;
use ratatui::layout::Rect;
use ratatui::style::{Modifier, Style};
use ratatui::widgets::Widget;

use crate::app::Phase;
use crate::theme;

/// Header bar showing current phase: BOOT / DIAGNOSTICS / MONITOR.
/// Active phase highlighted with accent color, others muted.
pub struct PhaseIndicator<'a> {
    current: &'a Phase,
}

impl<'a> PhaseIndicator<'a> {
    pub fn new(current: &'a Phase) -> Self {
        Self { current }
    }
}

impl Widget for PhaseIndicator<'_> {
    fn render(self, area: Rect, buf: &mut Buffer) {
        if area.height == 0 {
            return;
        }

        let phases = [
            (Phase::Boot, "BOOT", theme::GREEN),
            (Phase::Diagnostics, "DIAGNOSTICS", theme::AMBER),
            (Phase::Monitor, "MONITOR", theme::CYAN),
        ];

        // Fill background
        for x in area.x..area.x + area.width {
            buf.set_string(x, area.y, " ", Style::default().bg(theme::BG_HEADER));
        }

        // "EFL" branding on the left
        buf.set_string(
            area.x + 1,
            area.y,
            "EFL",
            Style::default()
                .fg(theme::GREEN)
                .bg(theme::BG_HEADER)
                .add_modifier(Modifier::BOLD),
        );

        // Phase tabs centered
        let total_width: u16 = phases.iter().map(|(_, name, _)| name.len() as u16 + 4).sum();
        let start_x = area.x + (area.width.saturating_sub(total_width)) / 2;
        let mut x = start_x;

        for (phase, name, accent) in &phases {
            let is_active = phase == self.current;
            let style = if is_active {
                Style::default()
                    .fg(*accent)
                    .bg(theme::BG_HEADER)
                    .add_modifier(Modifier::BOLD)
            } else {
                Style::default()
                    .fg(theme::TEXT_MUTED)
                    .bg(theme::BG_HEADER)
            };

            let label = if is_active {
                format!("[ {} ]", name)
            } else {
                format!("  {}  ", name)
            };

            buf.set_string(x, area.y, &label, style);
            x += label.len() as u16;
        }
    }
}
