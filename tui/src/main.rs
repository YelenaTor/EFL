mod app;
mod demo;
mod diagnostics;
mod ipc;
mod phases;
mod theme;
mod widgets;

use std::io;

use crossterm::{
    event::{self, Event, KeyCode, KeyModifiers},
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use ratatui::{backend::CrosstermBackend, Terminal};

use app::App;
use ipc::PipeReader;

fn main() -> io::Result<()> {
    let args: Vec<String> = std::env::args().collect();

    let pipe_reader = if args.iter().any(|a| a == "--demo") {
        // Demo mode: feed mock messages through the channel
        let rx = demo::start_demo();
        Some(PipeReader::from_channel(rx))
    } else if let Some(pos) = args.iter().position(|a| a == "--pipe") {
        // Connect to a named pipe
        let pipe_name = args.get(pos + 1)
            .expect("--pipe requires a pipe name argument");
        Some(PipeReader::connect(pipe_name))
    } else {
        None
    };

    // Setup terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    // Run app
    let mut app = App::new(pipe_reader);
    let result = run(&mut terminal, &mut app);

    // Restore terminal
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()?;

    result
}

fn run(
    terminal: &mut Terminal<CrosstermBackend<io::Stdout>>,
    app: &mut App,
) -> io::Result<()> {
    loop {
        terminal.draw(|frame| app.render(frame))?;

        // Only Ctrl+C to exit — display-only, no other input
        if event::poll(std::time::Duration::from_millis(100))? {
            if let Event::Key(key) = event::read()? {
                if key.code == KeyCode::Char('c') && key.modifiers.contains(KeyModifiers::CONTROL) {
                    return Ok(());
                }
            }
        }

        app.tick();
    }
}
