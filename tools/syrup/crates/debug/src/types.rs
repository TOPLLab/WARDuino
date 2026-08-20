use crate::Result;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ModuleIndex(pub u32);

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ProgramCounter(pub u32);

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct CodeLocation {
    pub module: ModuleIndex,
    pub program_counter: ProgramCounter,
}

#[non_exhaustive]
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DebugCommand {
    Continue,
    Pause,
    Step,
    StepOver,
    ContinueFor(u32),
    AddBreakpoint(CodeLocation),
    RemoveBreakpoint(CodeLocation),
    RequestSnapshot,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CommandKind {
    Continue,
    Pause,
    Step,
    StepOver,
    AddBreakpoint,
    RemoveBreakpoint,
    ContinueFor,
    Snapshot,
    Other(i32),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StopReason {
    Pause,
    Step,
    Breakpoint,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Stopped {
    pub reason: StopReason,
    pub location: Option<CodeLocation>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum VmState {
    Running,
    Paused,
    Stepping,
    ProxyRunning,
    ProxyHalted,
    Unknown(i32),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Snapshot {
    pub program_counter: ProgramCounter,
    pub state: VmState,
    pub breakpoints: Vec<ProgramCounter>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OperationResult {
    pub command: CommandKind,
    pub success: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DisconnectReason {
    TransportClosed,
}

#[non_exhaustive]
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DebugEvent {
    Stopped(Stopped),
    Continued,
    Halted,
    Snapshot(Snapshot),
    OperationResult(OperationResult),
    TargetMalformedCommand,
    TargetUnknownCommand,
    Disconnected(DisconnectReason),
}

/// A WARDuino debug connection.
///
/// `send` completes when the complete frame has been accepted by the local
/// transport. It does not confirm execution by the VM. `try_recv` never waits
/// for bytes and returns one queued event at a time.
pub trait DebugSession {
    fn send(&mut self, command: DebugCommand) -> Result<()>;
    fn try_recv(&mut self) -> Result<Option<DebugEvent>>;
}
