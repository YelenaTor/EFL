use ratatui::Frame;
use ratatui::layout::{Constraint, Direction, Layout};
use ratatui::style::Style;
use ratatui::widgets::Paragraph;

use crate::app::App;
use crate::diagnostics::codes::Category;
use crate::theme;
use crate::widgets::card_grid::{CardGrid, CardStatus, StatusCard};
use crate::widgets::diagnostic_list::DiagnosticList;
use crate::widgets::panel::Panel;
use crate::widgets::phase_indicator::PhaseIndicator;

/// Render the diagnostics phase: card grid + diagnostic list.
pub fn render(frame: &mut Frame, app: &App) {
    let area = frame.area();

    let bg = ratatui::widgets::Block::default()
        .style(Style::default().bg(theme::BG_PRIMARY));
    frame.render_widget(bg, area);

    let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),  // phase indicator
            Constraint::Length(3),  // summary stats
            Constraint::Percentage(40), // card grid
            Constraint::Min(5),    // diagnostic list
        ])
        .split(area);

    // Phase indicator
    frame.render_widget(PhaseIndicator::new(&app.phase), layout[0]);

    // Summary stats bar
    let errors = app.collector.error_count();
    let warnings = app.collector.warning_count();
    let hazards = app.collector.hazard_count();
    let total = app.collector.all().len();

    let summary = format!(
        "  {} total   {} errors   {} warnings   {} hazards",
        total, errors, warnings, hazards
    );

    let stats_panel = Panel::new("SUMMARY").title_color(theme::AMBER);
    let stats_inner = stats_panel.block().inner(layout[1]);
    frame.render_widget(stats_panel, layout[1]);

    if stats_inner.height > 0 {
        frame.render_widget(
            Paragraph::new(summary).style(Style::default().fg(theme::TEXT_PRIMARY)),
            stats_inner,
        );
    }

    // Card grid — one card per subsystem category
    let cards = build_subsystem_cards(app);
    let grid_panel = Panel::new("SUBSYSTEMS").title_color(theme::CYAN);
    let grid_inner = grid_panel.block().inner(layout[2]);
    frame.render_widget(grid_panel, layout[2]);

    let columns = if grid_inner.width > 80 { 4 } else if grid_inner.width > 40 { 3 } else { 2 };
    frame.render_widget(CardGrid::new(&cards, columns), grid_inner);

    // Diagnostic list
    let list_panel = Panel::new("DIAGNOSTICS").title_color(theme::MAGENTA);
    let list_inner = list_panel.block().inner(layout[3]);
    frame.render_widget(list_panel, layout[3]);
    frame.render_widget(DiagnosticList::new(app.collector.all(), 0), list_inner);
}

fn build_subsystem_cards(app: &App) -> Vec<StatusCard> {
    let categories = [
        Category::Boot,
        Category::Manifest,
        Category::Hook,
        Category::Area,
        Category::Warp,
        Category::Resource,
        Category::Npc,
        Category::Quest,
        Category::Trigger,
        Category::Story,
        Category::Save,
        Category::Ipc,
    ];

    categories
        .iter()
        .map(|cat| {
            let prefix = cat.prefix();
            let diags: Vec<_> = app.collector.all().iter()
                .filter(|d| d.category == prefix)
                .collect();

            let error_count = diags.iter().filter(|d| d.severity == "error").count();
            let warning_count = diags.iter().filter(|d| d.severity == "warning").count();
            let hazard_count = diags.iter().filter(|d| d.severity == "hazard").count();

            let status = if error_count > 0 {
                CardStatus::Error
            } else if warning_count > 0 {
                CardStatus::Warning
            } else if hazard_count > 0 {
                CardStatus::Hazard
            } else {
                CardStatus::Ok
            };

            StatusCard {
                name: prefix.to_string(),
                status,
                error_count,
                warning_count,
                hazard_count,
            }
        })
        .collect()
}
