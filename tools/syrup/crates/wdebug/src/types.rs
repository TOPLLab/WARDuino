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
    Halt,
    Pause,
    Step,
    StepOver,
    ContinueFor(u32),
    AddBreakpoint(CodeLocation),
    RemoveBreakpoint(CodeLocation),
    RequestSnapshot,
    Inspect(Vec<u8>),
    Reset,
    UpdateModule(Vec<u8>),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CommandKind {
    Continue,
    Halt,
    Pause,
    Step,
    StepOver,
    AddBreakpoint,
    RemoveBreakpoint,
    ContinueFor,
    Snapshot,
    Reset,
    UpdateModule,
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

/// A complete WARDuino VM frame accepted by the local transport.
///
/// The bytes include the command discriminator, canonical payload-length
/// varint, and protobuf payload. This only confirms local transport
/// acceptance; it does not confirm execution by the VM.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SentFrame(Vec<u8>);

impl SentFrame {
    /// Constructs a receipt for a complete frame accepted by an implementation
    /// of `DebugSession`. This is chiefly useful for custom session implementations.
    pub fn from_complete_frame(bytes: Vec<u8>) -> Self {
        Self(bytes)
    }

    pub fn bytes(&self) -> &[u8] {
        &self.0
    }
}

/// A decoded inbound VM event and the exact complete frame that carried it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReceivedFrame {
    pub event: DebugEvent,
    bytes: Vec<u8>,
}

impl ReceivedFrame {
    pub fn from_complete_frame(event: DebugEvent, bytes: Vec<u8>) -> Self {
        Self { event, bytes }
    }
    pub fn bytes(&self) -> &[u8] {
        &self.bytes
    }
}

/// A WARDuino debug connection.
///
/// `send` completes when the complete frame has been accepted by the local
/// transport. It does not confirm execution by the VM. `try_recv` never waits
/// for bytes and returns one queued event at a time.
pub trait DebugSession {
    fn send(&mut self, command: DebugCommand) -> Result<SentFrame>;
    fn try_recv(&mut self) -> Result<Option<ReceivedFrame>>;
}
