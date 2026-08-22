use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};
use ratatui::widgets::TableState;
use serde_json::Value;

#[allow(dead_code)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum VmState {
    Connected,
    Paused,
    Running,
    Disconnected,
}

impl VmState {
    pub const fn label(self) -> &'static str {
        match self {
            Self::Connected => "CONNECTED",
            Self::Paused => "PAUSED",
            Self::Running => "RUNNING",
            Self::Disconnected => "DISCONNECTED",
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Focus {
    Command,
    Session,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CommandIntent {
    Continue,
    Pause,
    Step,
    Next,
    Restart,
    Terminate,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Direction {
    Outgoing,
    Incoming,
}

impl Direction {
    pub const fn symbol(self) -> &'static str {
        match self {
            Self::Outgoing => "→",
            Self::Incoming => "←",
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum EntryType {
    DapRequest,
    DapResponse,
    DapEvent,
    DBGCommand,
    VmEvent,
}

impl EntryType {
    pub const fn label(self) -> &'static str {
        match self {
            Self::DapRequest => "DAP request",
            Self::DapResponse => "DAP response",
            Self::DapEvent => "DAP event",
            Self::DBGCommand => "DBG command",
            Self::VmEvent => "VM event",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SessionEntry {
    pub sequence: u64,
    pub direction: Direction,
    pub event: String,
    pub entry_type: EntryType,
    pub payload: EntryPayload,
    pub stop_context: Option<StopContext>,
}

#[derive(Clone, Debug, Eq, PartialEq, Default)]
pub enum EntryPayload {
    #[default]
    None,
    DapRequest {
        arguments: Value,
    },
    DapResponse {
        success: bool,
        body: Value,
        message: Option<String>,
    },
    DapEvent {
        body: Value,
    },
    VmFrame {
        direction: Direction,
        bytes: Vec<u8>,
        fields: Value,
    },
}

#[derive(Clone, Debug, Eq, PartialEq, Default)]
pub struct StopContext {
    pub state: Option<String>,
    pub pc: Option<String>,
    pub frames: Vec<StackFrame>,
    pub locals: Vec<NamedValue>,
    pub detail: Option<String>,
}
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StackFrame {
    pub name: String,
    pub address: String,
}
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NamedValue {
    pub name: String,
    pub value: String,
}
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EntryDetails {
    pub heading: String,
    pub metadata: String,
    pub sections: Vec<DetailSection>,
}
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DetailSection {
    pub title: String,
    pub rows: Vec<DetailRow>,
}
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DetailRow {
    pub label: String,
    pub value: String,
    pub style: DetailStyle,
}
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DetailStyle {
    Normal,
    Muted,
    Success,
    Warning,
    Error,
    Address,
}

pub fn details_for(entry: &SessionEntry) -> EntryDetails {
    let mut sections = Vec::new();
    let rows = |value: &Value| -> Vec<DetailRow> {
        value
            .as_object()
            .map(|object| {
                object
                    .iter()
                    .map(|(key, value)| DetailRow {
                        label: key.clone(),
                        value: value_text(value),
                        style: DetailStyle::Normal,
                    })
                    .collect()
            })
            .unwrap_or_default()
    };
    match &entry.payload {
        EntryPayload::DapRequest { arguments } => {
            let rows = rows(arguments);
            if !rows.is_empty() {
                sections.push(DetailSection {
                    title: "Arguments".into(),
                    rows,
                });
            }
        }
        EntryPayload::DapResponse {
            success,
            body,
            message,
        } => {
            let mut response = vec![DetailRow {
                label: "success".into(),
                value: success.to_string(),
                style: if *success {
                    DetailStyle::Success
                } else {
                    DetailStyle::Error
                },
            }];
            response.extend(rows(body));
            sections.push(DetailSection {
                title: "Response".into(),
                rows: response,
            });
            if !*success {
                if let Some(message) = message {
                    sections.push(DetailSection {
                        title: "Error".into(),
                        rows: vec![DetailRow {
                            label: String::new(),
                            value: message.clone(),
                            style: DetailStyle::Error,
                        }],
                    });
                }
            }
        }
        EntryPayload::DapEvent { body } => {
            let rows = if entry.event == "stopped" {
                Vec::new()
            } else {
                rows(body)
            };
            if !rows.is_empty() {
                sections.push(DetailSection {
                    title: "Event".into(),
                    rows,
                });
            }
        }
        EntryPayload::VmFrame {
            direction,
            bytes,
            fields,
        } => {
            let rows = rows(fields);
            if !rows.is_empty() {
                sections.push(DetailSection {
                    title: "Fields".into(),
                    rows,
                });
            }
            sections.push(DetailSection {
                title: "Wire".into(),
                rows: vec![
                    DetailRow {
                        label: "direction".into(),
                        value: direction.symbol().into(),
                        style: DetailStyle::Muted,
                    },
                    DetailRow {
                        label: "bytes".into(),
                        value: bytes
                            .iter()
                            .map(|b| format!("{b:02X}"))
                            .collect::<Vec<_>>()
                            .join(" "),
                        style: DetailStyle::Address,
                    },
                ],
            });
        }
        EntryPayload::None => {}
    }
    if let Some(context) = &entry.stop_context {
        let mut execution = Vec::new();
        if let Some(reason) = event_field(&entry.payload, "reason") {
            execution.push(DetailRow {
                label: "reason".into(),
                value: reason,
                style: DetailStyle::Normal,
            });
        }
        if let Some(state) = &context.state {
            execution.push(DetailRow {
                label: "state".into(),
                value: state.clone(),
                style: DetailStyle::Warning,
            });
        }
        if let Some(pc) = &context.pc {
            execution.push(DetailRow {
                label: "pc".into(),
                value: pc.clone(),
                style: DetailStyle::Address,
            });
        }
        if let Some(thread) = event_field(&entry.payload, "threadId") {
            execution.push(DetailRow {
                label: "thread".into(),
                value: thread,
                style: DetailStyle::Normal,
            });
        }
        if !execution.is_empty() {
            sections.push(DetailSection {
                title: "Execution".into(),
                rows: execution,
            });
        }
        if !context.frames.is_empty() {
            sections.push(DetailSection {
                title: "Stack".into(),
                rows: context
                    .frames
                    .iter()
                    .map(|frame| DetailRow {
                        label: frame.name.clone(),
                        value: frame.address.clone(),
                        style: DetailStyle::Address,
                    })
                    .collect(),
            });
        }
        if !context.locals.is_empty() {
            sections.push(DetailSection {
                title: "Locals".into(),
                rows: context
                    .locals
                    .iter()
                    .map(|local| DetailRow {
                        label: local.name.clone(),
                        value: local.value.clone(),
                        style: DetailStyle::Normal,
                    })
                    .collect(),
            });
        }
        if let Some(detail) = &context.detail {
            sections.push(DetailSection {
                title: "Inspection".into(),
                rows: vec![DetailRow {
                    label: String::new(),
                    value: detail.clone(),
                    style: DetailStyle::Error,
                }],
            });
        }
    }
    if sections.is_empty() {
        sections.push(DetailSection {
            title: String::new(),
            rows: vec![DetailRow {
                label: String::new(),
                value: "No additional details".into(),
                style: DetailStyle::Muted,
            }],
        });
    }
    EntryDetails {
        heading: entry.event.clone(),
        metadata: format!("{} · #{}", entry.entry_type.label(), entry.sequence),
        sections,
    }
}
fn value_text(value: &Value) -> String {
    match value {
        Value::String(value) => value.clone(),
        Value::Null => "null".into(),
        _ => value.to_string(),
    }
}
fn event_field(payload: &EntryPayload, name: &str) -> Option<String> {
    match payload {
        EntryPayload::DapEvent { body } => body.get(name).map(value_text),
        _ => None,
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Completion {
    pub command: &'static str,
    pub usage: &'static str,
    pub intent: CommandIntent,
}

pub const COMMANDS: [Completion; 6] = [
    Completion {
        command: "continue",
        usage: "continue",
        intent: CommandIntent::Continue,
    },
    Completion {
        command: "pause",
        usage: "pause",
        intent: CommandIntent::Pause,
    },
    Completion {
        command: "step",
        usage: "step",
        intent: CommandIntent::Step,
    },
    Completion {
        command: "stepover",
        usage: "stepover",
        intent: CommandIntent::Next,
    },
    Completion {
        command: "restart",
        usage: "restart",
        intent: CommandIntent::Restart,
    },
    Completion {
        command: "terminate",
        usage: "terminate",
        intent: CommandIntent::Terminate,
    },
];

#[derive(Debug)]
pub struct App {
    pub connection: String,
    pub vm_name: String,
    pub vm_state: VmState,
    pub timeline: Vec<SessionEntry>,
    pub focus: Focus,
    pub session_table: TableState,
    pub details_scroll: u16,
    pub follow_latest: bool,
    pub prompt: String,
    /// A character offset, never a byte offset.
    pub cursor: usize,
    pub completions: Vec<Completion>,
    pub completion_index: usize,
    pub notice: Option<String>,
    pub help_visible: bool,
    history: Vec<String>,
    history_index: Option<usize>,
    completions_dismissed: bool,
}

impl App {
    #[allow(dead_code)]
    pub fn live(connection: String) -> Self {
        Self {
            connection,
            vm_name: "WARDuino".into(),
            vm_state: VmState::Connected,
            timeline: Vec::new(),
            focus: Focus::Command,
            session_table: TableState::default(),
            details_scroll: 0,
            follow_latest: true,
            prompt: String::new(),
            cursor: 0,
            completions: Vec::new(),
            completion_index: 0,
            notice: None,
            help_visible: false,
            history: Vec::new(),
            history_index: None,
            completions_dismissed: false,
        }
    }
    #[allow(dead_code)]
    pub fn sample() -> Self {
        let timeline = vec![
            sample_entry(
                1042,
                Direction::Outgoing,
                "continue",
                EntryType::DBGCommand,
                EntryPayload::VmFrame {
                    direction: Direction::Outgoing,
                    bytes: vec![0, 0],
                    fields: serde_json::json!({}),
                },
            ),
            sample_entry(
                1043,
                Direction::Incoming,
                "continued",
                EntryType::VmEvent,
                EntryPayload::DapEvent {
                    body: serde_json::json!({"state":"running"}),
                },
            ),
            sample_entry(
                1044,
                Direction::Incoming,
                "stopped",
                EntryType::DapEvent,
                EntryPayload::DapEvent {
                    body: serde_json::json!({"reason":"pause", "threadId":1}),
                },
            ),
        ];
        let mut session_table = TableState::default();
        session_table.select(Some(timeline.len() - 1));
        Self {
            connection: "localhost:8100".into(),
            vm_name: "WARDuino".into(),
            vm_state: VmState::Paused,
            timeline,
            focus: Focus::Command,
            session_table,
            details_scroll: 0,
            follow_latest: true,
            prompt: String::new(),
            cursor: 0,
            completions: Vec::new(),
            completion_index: 0,
            notice: None,
            help_visible: false,
            history: Vec::new(),
            history_index: None,
            completions_dismissed: false,
        }
    }

    pub fn selected_entry(&self) -> Option<&SessionEntry> {
        self.session_table
            .selected()
            .and_then(|index| self.timeline.get(index))
    }

    #[allow(dead_code)]
    pub fn newer_count(&self) -> usize {
        if self.follow_latest {
            0
        } else {
            self.timeline
                .len()
                .saturating_sub(self.session_table.selected().unwrap_or(0).saturating_add(1))
        }
    }

    pub fn active_completion(&self) -> Option<Completion> {
        self.completions.get(self.completion_index).copied()
    }

    pub fn feedback(&self) -> Option<&str> {
        self.notice.as_deref().or_else(|| self.usage_hint())
    }

    pub fn usage_hint(&self) -> Option<&str> {
        if let Some(completion) = self.active_completion() {
            return Some(completion.usage);
        }
        let command = self.prompt.split_whitespace().next()?;
        COMMANDS
            .iter()
            .find(|candidate| candidate.command.eq_ignore_ascii_case(command))
            .map(|candidate| candidate.usage)
    }

    pub fn append(&mut self, entry: SessionEntry) {
        self.timeline.push(entry);
        if self.follow_latest {
            self.session_table.select(Some(self.timeline.len() - 1));
        }
    }

    pub fn select_delta(&mut self, delta: isize) {
        if self.timeline.is_empty() {
            return;
        }
        let last = self.timeline.len() - 1;
        let selected = self
            .session_table
            .selected()
            .unwrap_or(last)
            .saturating_add_signed(delta)
            .min(last);
        self.session_table.select(Some(selected));
        self.details_scroll = 0;
        self.follow_latest = selected == last;
    }

    pub fn page(&mut self, direction: isize, viewport_rows: u16) {
        self.select_delta(direction * isize::try_from(viewport_rows.max(1)).unwrap_or(1));
    }

    pub fn select_latest(&mut self) {
        if let Some(last) = self.timeline.len().checked_sub(1) {
            self.session_table.select(Some(last));
            self.details_scroll = 0;
            self.follow_latest = true;
        }
    }

    pub fn handle_key(&mut self, key: KeyEvent, viewport_rows: u16) -> Option<CommandIntent> {
        if !self.help_visible && key.code == KeyCode::Char('?') {
            self.help_visible = true;
            return None;
        }
        if self.help_visible {
            if matches!(key.code, KeyCode::Esc | KeyCode::Char('?')) {
                self.help_visible = false;
            }
            return None;
        }
        if self.focus == Focus::Command
            && (key.code == KeyCode::BackTab
                || (key.code == KeyCode::Tab && key.modifiers.contains(KeyModifiers::SHIFT)))
        {
            self.focus = Focus::Session;
            return None;
        }
        if self.focus == Focus::Session {
            match key.code {
                KeyCode::Tab | KeyCode::BackTab => self.focus = Focus::Command,
                KeyCode::Up => self.select_delta(-1),
                KeyCode::Down => self.select_delta(1),
                KeyCode::PageUp => self.page(-1, viewport_rows),
                KeyCode::PageDown => self.page(1, viewport_rows),
                KeyCode::Home => {
                    if !self.timeline.is_empty() {
                        self.session_table.select(Some(0));
                        self.details_scroll = 0;
                        self.follow_latest = false;
                    }
                }
                KeyCode::End => self.select_latest(),
                KeyCode::Char(character) if !key.modifiers.contains(KeyModifiers::CONTROL) => {
                    self.focus = Focus::Command;
                    self.insert(character);
                }
                _ => {}
            }
            return None;
        }
        if key.modifiers.contains(KeyModifiers::SHIFT) {
            match key.code {
                KeyCode::Up => {
                    self.select_delta(-1);
                    return None;
                }
                KeyCode::Down => {
                    self.select_delta(1);
                    return None;
                }
                _ => {}
            }
        }
        match key.code {
            KeyCode::Up => self.previous_choice(),
            KeyCode::Down => self.next_choice(),
            KeyCode::Tab => self.next_choice(),
            KeyCode::Enter => return self.submit(),
            KeyCode::Esc => self.escape(),
            KeyCode::Left => self.cursor = self.cursor.saturating_sub(1),
            KeyCode::Right => self.cursor = (self.cursor + 1).min(self.char_len()),
            KeyCode::Home => self.cursor = 0,
            KeyCode::End => self.cursor = self.char_len(),
            KeyCode::Backspace => self.backspace(),
            KeyCode::Delete => self.delete(),
            KeyCode::Char(character) if !key.modifiers.contains(KeyModifiers::CONTROL) => {
                self.insert(character)
            }
            _ => {}
        }
        None
    }

    fn previous_choice(&mut self) {
        if !self.completions.is_empty() {
            self.completion_index = self.completion_index.saturating_sub(1);
        } else {
            self.history_previous();
        }
    }

    fn next_choice(&mut self) {
        if !self.completions.is_empty() {
            self.completion_index = (self.completion_index + 1) % self.completions.len();
        } else {
            self.history_next();
        }
    }

    pub fn insert(&mut self, character: char) {
        let byte = self.byte_index(self.cursor);
        self.prompt.insert(byte, character);
        self.cursor += 1;
        self.after_edit();
    }

    fn backspace(&mut self) {
        if self.cursor == 0 {
            return;
        }
        let end = self.byte_index(self.cursor);
        let start = self.byte_index(self.cursor - 1);
        self.prompt.replace_range(start..end, "");
        self.cursor -= 1;
        self.after_edit();
    }

    fn delete(&mut self) {
        if self.cursor == self.char_len() {
            return;
        }
        let start = self.byte_index(self.cursor);
        let end = self.byte_index(self.cursor + 1);
        self.prompt.replace_range(start..end, "");
        self.after_edit();
    }

    fn after_edit(&mut self) {
        self.notice = None;
        self.history_index = None;
        self.completions_dismissed = false;
        self.refresh_completions();
    }

    fn refresh_completions(&mut self) {
        if self.completions_dismissed {
            self.completions.clear();
            return;
        }
        let token = self.prompt.split_whitespace().next().unwrap_or("");
        if token.is_empty() || self.prompt.chars().any(char::is_whitespace) {
            self.completions.clear();
        } else {
            self.completions = COMMANDS
                .iter()
                .copied()
                .filter(|candidate| candidate.command.starts_with(&token.to_ascii_lowercase()))
                .collect();
        }
        self.completion_index = self
            .completion_index
            .min(self.completions.len().saturating_sub(1));
    }

    fn submit(&mut self) -> Option<CommandIntent> {
        let typed = self.prompt.trim();
        let completion = COMMANDS
            .iter()
            .find(|candidate| candidate.command == typed)
            .copied()
            .or_else(|| {
                (!typed.chars().any(char::is_whitespace))
                    .then(|| self.active_completion())
                    .flatten()
            });
        let Some(completion) = completion else {
            self.notice = Some(format!(
                "Unknown command — use {}",
                COMMANDS
                    .iter()
                    .map(|candidate| candidate.command)
                    .collect::<Vec<_>>()
                    .join(", ")
            ));
            self.completions.clear();
            return None;
        };
        let command = completion.command;
        if self
            .history
            .last()
            .is_none_or(|previous| previous != command)
        {
            self.history.push(command.into());
        }
        self.history_index = None;
        self.prompt.clear();
        self.cursor = 0;
        self.completions.clear();
        self.completions_dismissed = false;
        self.notice = None;
        Some(completion.intent)
    }

    fn escape(&mut self) {
        if !self.completions.is_empty() {
            self.completions.clear();
            self.completions_dismissed = true;
        } else if !self.prompt.is_empty() || self.notice.is_some() {
            self.prompt.clear();
            self.cursor = 0;
            self.notice = None;
            self.history_index = None;
        } else {
            self.focus = Focus::Session;
        }
    }

    fn history_previous(&mut self) {
        let Some(last) = self.history.len().checked_sub(1) else {
            return;
        };
        let next = self
            .history_index
            .map_or(last, |index| index.saturating_sub(1));
        self.load_history(next);
    }

    fn history_next(&mut self) {
        let Some(index) = self.history_index else {
            return;
        };
        if index + 1 >= self.history.len() {
            self.history_index = None;
            self.prompt.clear();
            self.cursor = 0;
        } else {
            self.load_history(index + 1);
        }
    }

    fn load_history(&mut self, index: usize) {
        self.prompt = self.history[index].clone();
        self.cursor = self.char_len();
        self.history_index = Some(index);
        self.completions.clear();
    }

    fn char_len(&self) -> usize {
        self.prompt.chars().count()
    }

    fn byte_index(&self, character_index: usize) -> usize {
        self.prompt
            .char_indices()
            .nth(character_index)
            .map_or(self.prompt.len(), |(index, _)| index)
    }
}

fn sample_entry(
    sequence: u64,
    direction: Direction,
    event: &str,
    entry_type: EntryType,
    payload: EntryPayload,
) -> SessionEntry {
    SessionEntry {
        sequence,
        direction,
        event: event.into(),
        entry_type,
        payload,
        stop_context: None,
    }
}
