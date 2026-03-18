use ratatui::Frame;
use ratatui::layout::{Constraint, Direction, Layout};
use ratatui::style::{Modifier, Style};
use ratatui::widgets::Paragraph;

use crate::app::App;
use crate::theme;
use crate::widgets::panel::Panel;
use crate::widgets::phase_indicator::PhaseIndicator;

/// Render the monitor phase: multi-panel live dashboard.
pub fn render(frame: &mut Frame, app: &App) {
    let area = frame.area();

    let bg = ratatui::widgets::Block::default()
        .style(Style::default().bg(theme::BG_PRIMARY));
    frame.render_widget(bg, area);

    let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),  // phase indicator
            Constraint::Min(5),    // dashboard panels
        ])
        .split(area);

    // Phase indicator
    frame.render_widget(PhaseIndicator::new(&app.phase), layout[0]);

    // Dashboard: 2x2 grid
    let rows = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(layout[1]);

    let top_cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(rows[0]);

    let bottom_cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(rows[1]);

    // Active Hooks panel
    render_hooks_panel(frame, app, top_cols[0]);

    // Recent Events panel
    render_events_panel(frame, app, top_cols[1]);

    // Save Operations panel
    render_save_panel(frame, app, bottom_cols[0]);

    // Loaded Mods panel
    render_mods_panel(frame, app, bottom_cols[1]);
}

fn render_hooks_panel(frame: &mut Frame, app: &App, area: ratatui::layout::Rect) {
    let panel = Panel::new("ACTIVE HOOKS")
        .title_color(theme::GREEN)
        .border_color(theme::BORDER_DEFAULT);
    let inner = panel.block().inner(area);
    frame.render_widget(panel, area);

    if app.hooks.is_empty() {
        frame.render_widget(
            Paragraph::new("  No hooks registered")
                .style(Style::default().fg(theme::TEXT_MUTED)),
            inner,
        );
        return;
    }

    for (i, hook) in app.hooks.iter().enumerate() {
        let y = inner.y + i as u16;
        if y >= inner.y + inner.height {
            break;
        }
        let line = format!("  {} (x{})", hook.name, hook.fire_count);
        let row = ratatui::layout::Rect::new(inner.x, y, inner.width, 1);
        frame.render_widget(
            Paragraph::new(line).style(Style::default().fg(theme::TEXT_PRIMARY)),
            row,
        );
    }
}

fn render_events_panel(frame: &mut Frame, app: &App, area: ratatui::layout::Rect) {
    let panel = Panel::new("RECENT EVENTS")
        .title_color(theme::CYAN)
        .border_color(theme::BORDER_DEFAULT);
    let inner = panel.block().inner(area);
    frame.render_widget(panel, area);

    render_log_entries(frame, &app.event_log, inner);
}

fn render_save_panel(frame: &mut Frame, app: &App, area: ratatui::layout::Rect) {
    let panel = Panel::new("SAVE OPERATIONS")
        .title_color(theme::AMBER)
        .border_color(theme::BORDER_DEFAULT);
    let inner = panel.block().inner(area);
    frame.render_widget(panel, area);

    render_log_entries(frame, &app.save_log, inner);
}

fn render_log_entries(
    frame: &mut Frame,
    entries: &[crate::app::EventLogEntry],
    inner: ratatui::layout::Rect,
) {
    if entries.is_empty() {
        frame.render_widget(
            Paragraph::new("  No activity")
                .style(Style::default().fg(theme::TEXT_MUTED)),
            inner,
        );
        return;
    }

    // Show most recent entries that fit
    let visible = inner.height as usize;
    let start = entries.len().saturating_sub(visible);

    for (i, entry) in entries.iter().skip(start).enumerate() {
        let y = inner.y + i as u16;
        if y >= inner.y + inner.height {
            break;
        }
        let type_color = match entry.event_type.as_str() {
            "HOOK" => theme::GREEN,
            "EVENT" => theme::CYAN,
            "SAVE" | "LOAD" => theme::AMBER,
            _ => theme::TEXT_SECONDARY,
        };
        let line = format!("  [{}] {}", entry.event_type, entry.detail);
        let row = ratatui::layout::Rect::new(inner.x, y, inner.width, 1);
        frame.render_widget(
            Paragraph::new(line).style(Style::default().fg(type_color)),
            row,
        );
    }
}

fn render_mods_panel(frame: &mut Frame, app: &App, area: ratatui::layout::Rect) {
    let panel = Panel::new("LOADED MODS")
        .title_color(theme::MAGENTA)
        .border_color(theme::BORDER_DEFAULT);
    let inner = panel.block().inner(area);
    frame.render_widget(panel, area);

    if app.mods.is_empty() {
        frame.render_widget(
            Paragraph::new("  No mods loaded")
                .style(Style::default().fg(theme::TEXT_MUTED)),
            inner,
        );
        return;
    }

    for (i, m) in app.mods.iter().enumerate() {
        let y = inner.y + (i as u16 * 2);
        if y + 1 >= inner.y + inner.height {
            break;
        }

        let status_color = match m.status.as_str() {
            "active" => theme::GREEN,
            "error" => theme::MAGENTA,
            "warning" => theme::AMBER,
            _ => theme::TEXT_MUTED,
        };

        let name_line = format!("  {} v{}", m.name, m.version);
        let status_badge = format!("  [{}]", m.status.to_uppercase());

        let name_row = ratatui::layout::Rect::new(inner.x, y, inner.width, 1);
        frame.render_widget(
            Paragraph::new(name_line)
                .style(Style::default().fg(theme::TEXT_PRIMARY).add_modifier(Modifier::BOLD)),
            name_row,
        );

        let status_row = ratatui::layout::Rect::new(inner.x, y + 1, inner.width, 1);
        frame.render_widget(
            Paragraph::new(status_badge).style(Style::default().fg(status_color)),
            status_row,
        );
    }
}
