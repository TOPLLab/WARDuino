mod app;
mod session;
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
use session::Session;

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

fn run(device: String) -> io::Result<()> {
    let mut app = App::live(device.clone());
    let mut session = Session::connect(&device, &mut app).map_err(io::Error::other)?;
    let _guard = TerminalGuard::enter()?;
    let backend = CrosstermBackend::new(stdout());
    let mut terminal = Terminal::new(backend)?;

    loop {
        terminal.draw(|frame| ui::draw(frame, &app))?;
        if !event::poll(Duration::from_millis(250))? {
            session.poll(&mut app);
            continue;
        }
        match event::read()? {
            Event::Key(key) if key.kind == KeyEventKind::Press => {
                if key.code == KeyCode::Char('c') && key.modifiers.contains(KeyModifiers::CONTROL) {
                    session.disconnect(&mut app);
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
                if let Some(intent) = app.handle_key(key, viewport) {
                    session.dispatch(intent, &mut app);
                }
                session.poll(&mut app);
            }
            Event::Resize(_, _) => terminal.autoresize()?,
            _ => {}
        }
    }
}

fn main() -> io::Result<()> {
    let mut arguments = std::env::args().skip(1);
    match (
        arguments.next().as_deref(),
        arguments.next(),
        arguments.next(),
    ) {
        (Some("--device"), Some(device), None) => run(device),
        _ => Err(io::Error::other("usage: syrup --device <host:port>")),
    }
}
