use ratatui::buffer::Buffer;
use ratatui::layout::Rect;
use ratatui::style::Style;
use ratatui::widgets::Widget;

use crate::theme;

/// Horizontal colored stat bar with label and numeric value.
/// Marathon weapon-stat style: thin filled bar proportional to value/max.
pub struct StatusBar<'a> {
    label: &'a str,
    value: u32,
    max: u32,
    color: ratatui::style::Color,
}

impl<'a> StatusBar<'a> {
    #[allow(dead_code)]
    pub fn new(label: &'a str, value: u32, max: u32, color: ratatui::style::Color) -> Self {
        Self { label, value, max, color }
    }
}

impl Widget for StatusBar<'_> {
    fn render(self, area: Rect, buf: &mut Buffer) {
        if area.height == 0 || area.width < 4 {
            return;
        }

        // Layout: [LABEL  ████████░░░░  VALUE]
        let label_width = self.label.len() as u16 + 1;
        let value_str = format!("{}", self.value);
        let value_width = value_str.len() as u16 + 1;
        let bar_width = area.width.saturating_sub(label_width + value_width);

        // Render label
        buf.set_string(
            area.x,
            area.y,
            self.label,
            Style::default().fg(theme::TEXT_SECONDARY),
        );

        // Render bar
        if bar_width > 0 && self.max > 0 {
            let filled = ((self.value as f64 / self.max as f64) * bar_width as f64) as u16;
            let bar_x = area.x + label_width;
            for i in 0..bar_width {
                let ch = if i < filled { '█' } else { '░' };
                let color = if i < filled { self.color } else { theme::BG_HEADER };
                buf.set_string(
                    bar_x + i,
                    area.y,
                    &ch.to_string(),
                    Style::default().fg(color),
                );
            }
        }

        // Render value
        let val_x = area.x + label_width + bar_width;
        buf.set_string(
            val_x,
            area.y,
            &value_str,
            Style::default().fg(self.color),
        );
    }
}
