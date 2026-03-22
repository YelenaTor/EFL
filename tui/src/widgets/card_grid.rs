use ratatui::buffer::Buffer;
use ratatui::layout::Rect;
use ratatui::style::{Modifier, Style};
use ratatui::widgets::Widget;

use crate::theme;

/// A single subsystem status card.
pub struct StatusCard {
    pub name: String,
    pub status: CardStatus,
    #[allow(dead_code)]
    pub error_count: usize,
    #[allow(dead_code)]
    pub warning_count: usize,
    #[allow(dead_code)]
    pub hazard_count: usize,
}

#[derive(Debug, Clone, PartialEq)]
pub enum CardStatus {
    Ok,
    Warning,
    Error,
    Hazard,
}

impl CardStatus {
    pub fn color(&self) -> ratatui::style::Color {
        match self {
            CardStatus::Ok => theme::GREEN,
            CardStatus::Warning => theme::AMBER,
            CardStatus::Error => theme::MAGENTA,
            CardStatus::Hazard => theme::CYAN,
        }
    }

    pub fn label(&self) -> &'static str {
        match self {
            CardStatus::Ok => "OK",
            CardStatus::Warning => "WARN",
            CardStatus::Error => "ERR",
            CardStatus::Hazard => "HAZ",
        }
    }
}

/// Grid layout of status cards (Marathon faction-screen style).
pub struct CardGrid<'a> {
    cards: &'a [StatusCard],
    columns: u16,
}

impl<'a> CardGrid<'a> {
    pub fn new(cards: &'a [StatusCard], columns: u16) -> Self {
        Self { cards, columns: columns.max(1) }
    }
}

impl Widget for CardGrid<'_> {
    fn render(self, area: Rect, buf: &mut Buffer) {
        if area.height < 3 || area.width < 4 || self.cards.is_empty() {
            return;
        }

        let card_width = area.width / self.columns;
        let card_height: u16 = 3;

        for (i, card) in self.cards.iter().enumerate() {
            let col = (i as u16) % self.columns;
            let row = (i as u16) / self.columns;
            let x = area.x + col * card_width;
            let y = area.y + row * card_height;

            if y + card_height > area.y + area.height {
                break;
            }

            let card_area = Rect::new(x, y, card_width.min(area.x + area.width - x), card_height);
            render_card(buf, card_area, card);
        }
    }
}

fn render_card(buf: &mut Buffer, area: Rect, card: &StatusCard) {
    let color = card.status.color();

    // Top border
    let border_style = Style::default().fg(color);
    buf.set_string(area.x, area.y, "┌", border_style);
    for x in (area.x + 1)..(area.x + area.width - 1) {
        buf.set_string(x, area.y, "─", border_style);
    }
    buf.set_string(area.x + area.width - 1, area.y, "┐", border_style);

    // Middle: name + badge
    buf.set_string(area.x, area.y + 1, "│", border_style);
    let name = &card.name.to_uppercase();
    buf.set_string(
        area.x + 1,
        area.y + 1,
        name,
        Style::default().fg(theme::TEXT_PRIMARY).add_modifier(Modifier::BOLD),
    );
    // Status badge
    let badge = format!("[{}]", card.status.label());
    let badge_x = area.x + area.width - 1 - badge.len() as u16;
    buf.set_string(badge_x, area.y + 1, &badge, Style::default().fg(color));
    buf.set_string(area.x + area.width - 1, area.y + 1, "│", border_style);

    // Bottom border
    buf.set_string(area.x, area.y + 2, "└", border_style);
    for x in (area.x + 1)..(area.x + area.width - 1) {
        buf.set_string(x, area.y + 2, "─", border_style);
    }
    buf.set_string(area.x + area.width - 1, area.y + 2, "┘", border_style);
}
