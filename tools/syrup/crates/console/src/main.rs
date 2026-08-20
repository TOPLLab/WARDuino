mod app;
mod ui;

use std::{
    io::{self, stdout},
    time::Duration,
};

use crossterm::{
    cursor::{Hide, Show},
    event::{self, Event, KeyCode, KeyEventKind, KeyModifiers},
    execute,
    terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
};
use ratatui::{Terminal, backend::CrosstermBackend};

use app::App;

struct TerminalGuard;

impl TerminalGuard {
    fn enter() -> io::Result<Self> {
        enable_raw_mode()?;
        if let Err(error) = execute!(stdout(), EnterAlternateScreen, Hide) {
            let _ = disable_raw_mode();
            return Err(error);
        }
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = disable_raw_mode();
        let _ = execute!(stdout(), Show, LeaveAlternateScreen);
    }
}

fn run() -> io::Result<()> {
    let _guard = TerminalGuard::enter()?;
    let backend = CrosstermBackend::new(stdout());
    let mut terminal = Terminal::new(backend)?;
    let mut app = App::sample();

    loop {
        terminal.draw(|frame| ui::draw(frame, &app))?;
        if !event::poll(Duration::from_millis(250))? {
            continue;
        }
        match event::read()? {
            Event::Key(key) if key.kind == KeyEventKind::Press => {
                if key.code == KeyCode::Char('c') && key.modifiers.contains(KeyModifiers::CONTROL) {
                    return Ok(());
                }
                let viewport = ui::calculate(
                    terminal.size()?.into(),
                    app.completions.len(),
                    app.feedback().is_some(),
                )
                .timeline
                .height
                .saturating_sub(1);
                app.handle_key(key, viewport);
            }
            Event::Resize(_, _) => terminal.autoresize()?,
            _ => {}
        }
    }
}

fn main() -> io::Result<()> {
    run()
}
