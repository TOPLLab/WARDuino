//! A synchronous, nonblocking interface to the WARDuino debugger protocol.
//!
//! Commands are accepted by the local transport only. The current firmware
//! does not attach request identifiers to frames, so notifications cannot be
//! correlated to a particular command.

mod codec;
mod error;
mod framing;
pub mod schema;
mod session;
mod transport;
mod types;

pub use error::{DebugError, Result};
pub use session::WarduinoSession;
pub use types::{
    DebugCommand, DebugEvent, DebugSession, DisconnectReason, ReceivedFrame, SentFrame, state_label,
};
