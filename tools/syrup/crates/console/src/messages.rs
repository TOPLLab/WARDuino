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

    pub const fn label(self) -> &'static str {
        match self {
            Self::Outgoing => "→ Outgoing",
            Self::Incoming => "← Incoming",
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
