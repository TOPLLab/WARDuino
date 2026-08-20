//! A synchronous, nonblocking interface to the WARDuino debugger protocol.
//!
//! Commands are accepted by the local transport only. The current firmware
//! does not attach request identifiers to frames, so notifications cannot be
//! correlated to a particular command.

mod codec;
mod error;
mod framing;
mod session;
mod transport;
mod types;
mod wire;

pub use error::{DebugError, Result};
pub use session::WarduinoSession;
pub use types::{
    CodeLocation, CommandKind, DebugCommand, DebugEvent, DebugSession, DisconnectReason,
    ModuleIndex, OperationResult, ProgramCounter, Snapshot, StopReason, Stopped, VmState,
};
