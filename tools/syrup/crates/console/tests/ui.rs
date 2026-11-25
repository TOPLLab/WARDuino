#[path = "../src/app.rs"]
mod app;
#[path = "../src/messages.rs"]
mod messages;
#[path = "../src/ui.rs"]
mod ui;

use app::{
    App, EntryPayload, EntryType, Focus, NamedValue, SessionEntry, StackFrame, StopContext,
    details_for,
};
use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};
use messages::{CommandIntent, Direction};
use ratatui::{
    Terminal,
    backend::TestBackend,
    buffer::Buffer,
    style::{Color, Modifier},
};
use serde_json::json;

fn key(code: KeyCode) -> KeyEvent {
    KeyEvent::new(code, KeyModifiers::NONE)
}
fn render_buffer(app: &App, width: u16, height: u16) -> Buffer {
    let backend = TestBackend::new(width, height);
    let mut terminal = Terminal::new(backend).unwrap();
    terminal.draw(|frame| ui::draw(frame, app)).unwrap();
    terminal.backend().buffer().clone()
}

fn render(app: &App, width: u16, height: u16) -> String {
    let b = render_buffer(app, width, height);
    (0..height)
        .map(|y| {
            (0..width)
                .map(|x| b.cell((x, y)).unwrap().symbol())
                .collect::<String>()
        })
        .collect::<Vec<_>>()
        .join("\n")
}
fn entry(sequence: u64, event: &str, payload: EntryPayload) -> SessionEntry {
    SessionEntry {
        sequence,
        direction: Direction::Incoming,
        event: event.into(),
        entry_type: EntryType::DapEvent,
        payload,
        stop_context: None,
    }
}

#[test]
fn selection_is_table_state_and_follow_latest_is_deterministic() {
    let mut app = App::sample();
    assert_eq!(app.session_table.selected(), Some(2));
    app.select_delta(-1);
    assert_eq!(app.selected_entry().unwrap().sequence, 1043);
    assert!(!app.follow_latest);
    app.append(entry(
        4,
        "output",
        EntryPayload::DapEvent {
            body: json!({"output":"later"}),
        },
    ));
    assert_eq!(app.selected_entry().unwrap().sequence, 1043);
    app.select_latest();
    assert!(app.follow_latest);
    assert_eq!(app.selected_entry().unwrap().sequence, 4);
}
#[test]
fn focus_and_keys_preserve_command_buffer() {
    let mut app = App::sample();
    app.insert('c');
    assert!(!app.completions.is_empty());
    app.handle_key(key(KeyCode::Esc), 4);
    assert_eq!(app.focus, Focus::Command);
    app.handle_key(key(KeyCode::Esc), 4);
    assert!(app.prompt.is_empty());
    app.handle_key(key(KeyCode::Esc), 4);
    assert_eq!(app.focus, Focus::Session);
    app.handle_key(key(KeyCode::Down), 4);
    let prompt = app.prompt.clone();
    app.handle_key(key(KeyCode::Char('p')), 4);
    assert_eq!(app.focus, Focus::Command);
    assert_eq!(app.prompt, format!("{prompt}p"));
    app.handle_key(KeyEvent::new(KeyCode::BackTab, KeyModifiers::SHIFT), 4);
    assert_eq!(app.focus, Focus::Session);
    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.focus, Focus::Command);
}

#[test]
fn tab_accepts_the_selected_completion_before_enter_submits() {
    let mut app = App::sample();
    for character in "ste".chars() {
        app.insert(character);
    }
    assert_eq!(
        app.completions
            .iter()
            .map(|completion| completion.command)
            .collect::<Vec<_>>(),
        vec!["step", "stepover"]
    );
    assert_eq!(app.completion_index, 0);

    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.prompt, "step");
    assert_eq!(app.cursor, 4);
    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.prompt, "stepover");
    assert_eq!(app.cursor, 8);
    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.prompt, "step");
    assert_eq!(app.cursor, 4);

    app.handle_key(key(KeyCode::Down), 4);
    assert_eq!(app.completion_index, 1);
    app.handle_key(key(KeyCode::Tab), 4);
    assert_eq!(app.prompt, "stepover");
    assert_eq!(app.cursor, 8);
    assert_eq!(
        app.handle_key(key(KeyCode::Enter), 4),
        Some(CommandIntent::Next)
    );
    assert!(app.prompt.is_empty());
}

#[test]
fn enter_does_not_submit_an_unaccepted_completion() {
    let mut app = App::sample();
    for character in "ste".chars() {
        app.insert(character);
    }

    assert_eq!(app.handle_key(key(KeyCode::Enter), 4), None);
    assert_eq!(app.prompt, "ste");
    assert!(
        app.notice
            .as_deref()
            .is_some_and(|notice| notice.contains("Unknown command"))
    );
}

#[test]
fn completion_candidates_render_horizontally() {
    let mut app = App::sample();
    for character in "ste".chars() {
        app.insert(character);
    }
    let output = render(&app, 72, 24);
    assert!(output.contains("step   stepover"));
}
#[test]
fn details_are_owned_by_each_entry() {
    let vm_frame = entry(
        8,
        "continue",
        EntryPayload::VmFrame {
            direction: Direction::Outgoing,
            bytes: vec![0, 0],
            fields: json!({}),
        },
    );
    let details = details_for(&vm_frame);
    let wire = details
        .sections
        .iter()
        .find(|section| section.title == "Wire")
        .unwrap();
    assert_eq!(wire.rows[0].value, "→ outgoing");
    assert_eq!(wire.rows[1].label, "size");
    assert_eq!(wire.rows[1].value, "2 bytes");
    let request = entry(
        9,
        "pause",
        EntryPayload::DapRequest {
            arguments: json!({"threadId":1}),
        },
    );
    assert_eq!(details_for(&request).sections[0].title, "Arguments");
    let mut stopped = entry(
        10,
        "stopped",
        EntryPayload::DapEvent {
            body: json!({"reason":"pause","threadId":1}),
        },
    );
    stopped.stop_context = Some(StopContext {
        state: Some("Paused".into()),
        pc: Some("0x17".into()),
        frames: vec![StackFrame {
            name: "WARDuino".into(),
            address: "0x17".into(),
        }],
        locals: vec![NamedValue {
            name: "x".into(),
            value: "1".into(),
        }],
        detail: None,
    });
    let d = details_for(&stopped);
    assert!(d.sections.iter().any(|s| s.title == "Execution"));
    assert!(d.sections.iter().any(|s| s.title == "Stack"));
    assert!(d.sections.iter().any(|s| s.title == "Locals"));
    let empty = entry(11, "note", EntryPayload::None);
    assert_eq!(
        details_for(&empty).sections[0].rows[0].value,
        "No additional details"
    );
}
#[test]
fn details_render_responsively() {
    let app = App::sample();
    for (w, h) in [(140, 40), (100, 30), (72, 24), (50, 14)] {
        let output = render(&app, w, h);
        assert!(output.contains("Session"));
        assert!(output.contains("Details"));
        assert!(output.contains("stopped"));
    }
    assert!(render(&app, 49, 13).contains("Terminal too small"));
}

#[test]
fn session_arrows_follow_message_direction() {
    let mut app = App::live("localhost:8100".into());
    let mut outgoing = entry(
        1,
        "continue",
        EntryPayload::VmFrame {
            direction: Direction::Outgoing,
            bytes: vec![0, 0],
            fields: json!({}),
        },
    );
    outgoing.direction = Direction::Outgoing;
    outgoing.entry_type = EntryType::VmCommand;
    app.append(outgoing);
    app.append(entry(
        2,
        "stopped",
        EntryPayload::DapEvent {
            body: json!({"reason": "pause"}),
        },
    ));

    let output = render(&app, 72, 24);
    assert!(
        output
            .lines()
            .any(|line| line.contains("continue") && line.contains("→"))
    );
    assert!(
        output
            .lines()
            .any(|line| line.contains("stopped") && line.contains("←"))
    );
}

fn cells(buffer: &Buffer) -> impl Iterator<Item = &ratatui::buffer::Cell> {
    let area = buffer.area;
    (0..area.height).flat_map(move |y| (0..area.width).map(move |x| buffer.cell((x, y)).unwrap()))
}

#[test]
fn terminal_theme_styles_use_resets_and_ansi_roles() {
    let app = App::sample();
    let buffer = render_buffer(&app, 72, 24);

    assert!(cells(&buffer).all(|cell| {
        cell.bg == Color::Reset
            && !matches!(cell.fg, Color::Rgb(_, _, _))
            && !matches!(cell.bg, Color::Rgb(_, _, _))
    }));
    assert!(cells(&buffer).any(|cell| cell.fg == Color::Reset));
    assert!(cells(&buffer).any(|cell| cell.fg == Color::Cyan));
    assert!(cells(&buffer).any(|cell| cell.fg == Color::DarkGray));
    assert!(cells(&buffer).any(|cell| cell.fg == Color::Yellow));

    let mut connected = App::sample();
    connected.vm_state = app::VmState::Connected;
    assert!(cells(&render_buffer(&connected, 72, 24)).any(|cell| cell.fg == Color::Green));

    let mut disconnected = App::sample();
    disconnected.vm_state = app::VmState::Disconnected;
    assert!(cells(&render_buffer(&disconnected, 72, 24)).any(|cell| cell.fg == Color::Red));
}

#[test]
fn focus_and_inactive_selection_use_modifiers_without_backgrounds() {
    let mut session = App::sample();
    session.focus = Focus::Session;
    let focused = render_buffer(&session, 72, 24);
    assert!(cells(&focused).any(|cell| {
        cell.symbol() == "s"
            && cell.fg == Color::Reset
            && cell.modifier.contains(Modifier::REVERSED | Modifier::BOLD)
    }));
    assert!(cells(&focused).all(|cell| cell.bg == Color::Reset));

    let command = render_buffer(&App::sample(), 72, 24);
    let placeholder = command.cell((4, 22)).unwrap();
    assert_eq!(placeholder.fg, Color::DarkGray);
    assert_eq!(placeholder.bg, Color::Reset);
    assert!(!placeholder.modifier.contains(Modifier::REVERSED));
    assert!(placeholder.modifier.contains(Modifier::BOLD));

    let inactive = render_buffer(&App::sample(), 72, 24);
    assert!(cells(&inactive).any(|cell| {
        cell.symbol() == "s" && cell.fg == Color::Reset && cell.modifier.contains(Modifier::DIM)
    }));
    assert!(cells(&inactive).all(|cell| cell.bg == Color::Reset));
}

#[test]
fn focused_typed_command_uses_terminal_background_without_reverse_video() {
    let mut app = App::sample();
    app.insert('x');
    assert!(app.completions.is_empty());

    let command = render_buffer(&app, 72, 24);
    let prompt = command.cell((2, 22)).unwrap();
    let typed = command.cell((4, 22)).unwrap();

    assert_eq!(prompt.symbol(), "›");
    assert_eq!(prompt.fg, Color::Cyan);
    assert_eq!(prompt.bg, Color::Reset);
    assert!(!prompt.modifier.contains(Modifier::REVERSED));
    assert!(prompt.modifier.contains(Modifier::BOLD));

    assert_eq!(typed.symbol(), "x");
    assert_eq!(typed.fg, Color::Reset);
    assert_eq!(typed.bg, Color::Reset);
    assert!(!typed.modifier.contains(Modifier::REVERSED));
    assert!(typed.modifier.contains(Modifier::BOLD));

    assert!((21..23).all(|y| {
        (0..72).all(|x| {
            let cell = command.cell((x, y)).unwrap();
            cell.bg == Color::Reset && !cell.modifier.contains(Modifier::REVERSED)
        })
    }));
}
