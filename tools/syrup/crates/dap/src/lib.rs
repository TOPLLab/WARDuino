//! The deliberately small Debug Adapter Protocol bridge used to review the
//! WARDuino debug-session API.

pub mod adapter;
pub mod protocol;

pub use adapter::{Adapter, AdapterOutput};
pub use protocol::{Request, read_message, write_message};
