//! The deliberately small Debug Adapter Protocol bridge used to review the
//! WARDuino debug-session API.

pub mod adapter;
pub mod protocol;
pub mod source;

use debug::WarduinoSession;

pub use adapter::{Adapter, AdapterOutput, SessionConnector};
pub use protocol::{Request, read_message, write_message};

pub type WarduinoAdapter = Adapter<WarduinoSession, fn(&str) -> Result<WarduinoSession, String>>;

pub fn warduino_adapter() -> WarduinoAdapter {
    Adapter::new(|device: &str| WarduinoSession::connect(device).map_err(|error| error.to_string()))
}
