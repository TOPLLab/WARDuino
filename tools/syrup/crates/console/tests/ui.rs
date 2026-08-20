#[path = "../src/app.rs"]
mod app;
#[path = "../src/ui.rs"]
mod ui;

use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};
use ratatui::{Terminal, backend::TestBackend};

use app::{App, Direction, TimelineEntry};

fn key(code: KeyCode) -> KeyEvent {
    KeyEvent::new(code, KeyModifiers::NONE)
}

fn render(app: &App, width: u16, height: u16) -> String {
    let backend = TestBackend::new(width, height);
    let mut terminal = Terminal::new(backend).unwrap();
    terminal.draw(|frame| ui::draw(frame, app)).unwrap();
    let buffer = terminal.backend().buffer();
    (0..height)
        .map(|y| {
            (0..width)
                .map(|x| buffer.cell((x, y)).unwrap().symbol())
                .collect::<String>()
        })
        .collect::<Vec<_>>()
        .join("\n")
}

#[test]
fn selection_and_following_keep_results_deterministic() {
    let mut app = App::sample();
    assert_eq!(
        app.selected_entry().unwrap().effect,
        vec!["VM paused by request"]
    );
    app.select_delta(-2);
    assert_eq!(app.selected_entry().unwrap().kind, "stopped");
    assert!(!app.follow_latest);
    app.append(TimelineEntry {
        sequence: 1047,
        direction: Direction::Incoming,
        kind: "note".into(),
        summary: "new".into(),
        effect: vec!["new effect".into()],
    });
    assert_eq!(app.selected_entry().unwrap().sequence, 1044);
    assert_eq!(app.newer_count(), 3);
    app.select_latest();
    assert!(app.follow_latest);
    app.append(TimelineEntry {
        sequence: 1048,
        direction: Direction::Incoming,
        kind: "note".into(),
        summary: "latest".into(),
        effect: vec![],
    });
    assert_eq!(app.selected_entry().unwrap().sequence, 1048);
}

#[test]
fn prompt_edits_completes_submits_and_rejects_unsupported_commands() {
    let mut app = App::sample();
    app.insert('c');
    assert_eq!(app.completions.len(), 1);
    assert_eq!(app.active_completion().unwrap().command, "continue");
    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.prompt, "continue");
    let intent = app.handle_key(key(KeyCode::Enter), 4);
    assert!(app.prompt.is_empty());
    assert_eq!(intent, Some(app::CommandIntent::Continue));
    assert_eq!(app.timeline.last().unwrap().kind, "stopped");
    assert_eq!(app.vm_state.label(), "PAUSED");

    app.insert('x');
    app.insert('🙂');
    app.handle_key(key(KeyCode::Left), 4);
    app.handle_key(key(KeyCode::Backspace), 4);
    assert_eq!(app.prompt, "🙂");
    let entries_before = app.timeline.len();
    app.handle_key(key(KeyCode::Enter), 4);
    assert_eq!(app.prompt, "🙂");
    assert!(app.notice.as_deref().unwrap().contains("Unknown command"));
    assert_eq!(app.timeline.len(), entries_before);
    assert!(app.completions.len() <= 3);
}

#[test]
fn responsive_buffers_preserve_prompt_and_selected_result() {
    let app = App::sample();
    let wide = render(&app, 140, 40);
    assert!(!wide.contains("SYRUP"));
    assert!(wide.contains("Session"));
    assert!(wide.contains("Result"));
    assert!(wide.contains("VM paused by request"));
    assert!(wide.contains("›"));

    let medium = render(&app, 100, 30);
    assert!(medium.contains("Session"));
    assert!(medium.contains("Result"));
    assert!(medium.find("Session").unwrap() < medium.find("Result").unwrap());

    let narrow = render(&app, 72, 24);
    assert!(narrow.contains("Session"));
    assert!(narrow.contains("history"));
    assert!(narrow.contains("›"));

    let minimum = render(&app, 50, 14);
    assert!(minimum.contains("Session"));
    assert!(minimum.contains("Result"));
    assert!(minimum.contains("›"));

    let too_small = render(&app, 49, 13);
    assert!(too_small.contains("Terminal too small — minimum 50×14"));
    assert!(!too_small.contains("Session"));
}
