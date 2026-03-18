use ratatui::buffer::Buffer;
use ratatui::layout::Rect;
use ratatui::style::{Modifier, Style};
use ratatui::widgets::{Block, Borders, Widget};

use crate::theme;

/// Marathon-style bordered panel with all-caps neon-colored title.
pub struct Panel<'a> {
    title: &'a str,
    title_color: ratatui::style::Color,
    border_color: ratatui::style::Color,
}

impl<'a> Panel<'a> {
    pub fn new(title: &'a str) -> Self {
        Self {
            title,
            title_color: theme::GREEN,
            border_color: theme::BORDER_DEFAULT,
        }
    }

    pub fn title_color(mut self, color: ratatui::style::Color) -> Self {
        self.title_color = color;
        self
    }

    pub fn border_color(mut self, color: ratatui::style::Color) -> Self {
        self.border_color = color;
        self
    }

    /// Return a ratatui Block configured with this panel's styling.
    /// Use this when you need to wrap inner content.
    pub fn block(&self) -> Block<'a> {
        let title = format!(" {} ", self.title.to_uppercase());
        Block::default()
            .title(title)
            .title_style(Style::default()
                .fg(self.title_color)
                .add_modifier(Modifier::BOLD))
            .borders(Borders::ALL)
            .border_style(Style::default().fg(self.border_color))
            .style(Style::default().bg(theme::BG_PANEL))
    }
}

impl Widget for Panel<'_> {
    fn render(self, area: Rect, buf: &mut Buffer) {
        self.block().render(area, buf);
    }
}
