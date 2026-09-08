use crate::{Result, schema};

#[non_exhaustive]
#[derive(Clone, Debug, PartialEq)]
pub enum DebugCommand {
    Run,
    Halt,
    Pause,
    Step,
    StepOver,
    AddBreakpoint(schema::CodeLocation),
    RemoveBreakpoint(schema::CodeLocation),
    ClearBreakpoints,
    HeapUsage,
    Snapshot(schema::Include),
    UpdateFunction(schema::Function),
    UpdateLocal(schema::ValueUpdate),
    UpdateCallbacks(schema::CallbackMapping),
    UpdateModule(schema::ModuleUpdate),
    UpdateGlobal(schema::ValueUpdate),
    UpdateStack(schema::ValueUpdate),
    LoadSnapshot(schema::Snapshot),
    Proxify,
    AddProxy(schema::FunctionRef),
    RemoveProxy(schema::FunctionRef),
    ProxyCall(schema::RemoteFunctionCall),
    PopEvent,
    PushEvent(schema::Event),
    ContinueFor(schema::ContinueFor),
    Reset,
    Invoke(schema::RemoteFunctionCall),
    SetSnapshotPolicy(schema::SnapshotPolicyConfig),
    SetOverride(schema::Override),
    RemoveOverride(schema::Override),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum DisconnectReason {
    TransportClosed,
}

#[non_exhaustive]
#[derive(Clone, Debug, PartialEq)]
pub enum DebugEvent {
    Continued,
    Halted,
    Paused,
    Stepped,
    HitBreakpoint(schema::CodeLocation),
    NewEvent(schema::Event),
    Snapshot(schema::Snapshot),
    ChangeAffected,
    TargetMalformedCommand,
    TargetUnknownCommand,
    OperationResult(schema::OperationResult),
    RemoteFunctionResult(schema::RemoteFunctionResult),
    Checkpoint(schema::Checkpoint),
    HeapUsage(schema::HeapUsage),
    Disconnected(DisconnectReason),
}

/// A complete WARDuino VM frame accepted by the local transport.
///
/// The bytes include the command discriminator, canonical payload-length
/// varint, and protobuf payload. This only confirms local transport
/// acceptance; it does not confirm execution by the VM.
#[derive(Clone, Debug, PartialEq)]
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
#[derive(Clone, Debug, PartialEq)]
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
