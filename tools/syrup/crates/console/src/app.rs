use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};

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
    pub wire: Option<Vec<u8>>,
    pub effect: Vec<String>,
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
        command: "next",
        usage: "next",
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
    pub selected: usize,
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
            selected: 0,
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
            entry(
                1042,
                Direction::Outgoing,
                "continue",
                EntryType::DBGCommand,
                Vec::<String>::new(),
                Some(vec![0, 0]),
            ),
            entry(
                1043,
                Direction::Incoming,
                "continued",
                EntryType::VmEvent,
                vec!["VM running"],
                None,
            ),
            entry(
                1044,
                Direction::Incoming,
                "stopped",
                EntryType::DapEvent,
                vec!["stopped at breakpoint 3", "function 12 · 0x003ad8"],
                None,
            ),
            entry(
                1045,
                Direction::Outgoing,
                "pause",
                EntryType::DapRequest,
                vec!["pause requested"],
                None,
            ),
            entry(
                1046,
                Direction::Incoming,
                "stopped",
                EntryType::DapEvent,
                vec!["VM paused by request"],
                None,
            ),
        ];
        Self {
            connection: "localhost:8100".into(),
            vm_name: "WARDuino".into(),
            vm_state: VmState::Paused,
            selected: timeline.len() - 1,
            timeline,
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
        self.timeline.get(self.selected)
    }

    pub fn newer_count(&self) -> usize {
        if self.follow_latest {
            0
        } else {
            self.timeline
                .len()
                .saturating_sub(self.selected.saturating_add(1))
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
            self.selected = self.timeline.len() - 1;
        }
    }

    pub fn select_delta(&mut self, delta: isize) {
        if self.timeline.is_empty() {
            return;
        }
        let last = self.timeline.len() - 1;
        self.selected = self.selected.saturating_add_signed(delta).min(last);
        self.follow_latest = self.selected == last;
    }

    pub fn page(&mut self, direction: isize, viewport_rows: u16) {
        self.select_delta(direction * isize::try_from(viewport_rows.max(1)).unwrap_or(1));
    }

    pub fn select_latest(&mut self) {
        if let Some(last) = self.timeline.len().checked_sub(1) {
            self.selected = last;
            self.follow_latest = true;
        }
    }

    pub fn handle_key(&mut self, key: KeyEvent, viewport_rows: u16) -> Option<CommandIntent> {
        if self.help_visible {
            if matches!(key.code, KeyCode::Esc | KeyCode::Char('?')) {
                self.help_visible = false;
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
            KeyCode::Char('?') => self.help_visible = true,
            KeyCode::PageUp => self.page(-1, viewport_rows),
            KeyCode::PageDown => self.page(1, viewport_rows),
            KeyCode::End if self.prompt.is_empty() => self.select_latest(),
            KeyCode::Up => self.previous_choice(),
            KeyCode::Down => self.next_choice(),
            KeyCode::Tab => self.accept_completion(),
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
            self.completion_index = (self.completion_index + 1).min(self.completions.len() - 1);
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
                .take(3)
                .collect();
        }
        self.completion_index = self
            .completion_index
            .min(self.completions.len().saturating_sub(1));
    }

    fn accept_completion(&mut self) {
        let Some(completion) = self.active_completion() else {
            return;
        };
        let suffix: String = self
            .prompt
            .chars()
            .skip_while(|character| !character.is_whitespace())
            .collect();
        self.prompt = format!("{}{}", completion.command, suffix);
        self.cursor = completion.command.chars().count();
        self.completions.clear();
        self.completions_dismissed = true;
        self.notice = None;
    }

    fn submit(&mut self) -> Option<CommandIntent> {
        let command = self.prompt.trim();
        let Some(completion) = COMMANDS
            .iter()
            .find(|candidate| candidate.command == command)
            .copied()
        else {
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

#[allow(dead_code)]
fn entry(
    sequence: u64,
    direction: Direction,
    kind: &str,
    entry_type: EntryType,
    effect: Vec<impl Into<String>>,
    wire: Option<Vec<u8>>,
) -> SessionEntry {
    SessionEntry {
        sequence,
        direction,
        event: kind.into(),
        entry_type,
        effect: effect.into_iter().map(Into::into).collect(),
        wire,
    }
}
