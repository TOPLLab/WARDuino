use std::{error::Error, fmt, io};

pub type Result<T> = std::result::Result<T, DebugError>;

#[derive(Debug)]
pub enum DebugError {
    NotConnected,
    UnsupportedCommand(&'static str),
    Transport(io::Error),
    FrameTooLarge {
        size: usize,
        max: usize,
    },
    InvalidFrame(&'static str),
    UnknownMessageType(u8),
    Decode {
        message_type: u8,
        source: prost::DecodeError,
    },
    InvalidPayload {
        message_type: u8,
        reason: &'static str,
    },
    Encode(prost::EncodeError),
}

impl fmt::Display for DebugError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NotConnected => formatter.write_str("debug session is not connected"),
            Self::UnsupportedCommand(name) => write!(formatter, "unsupported command: {name}"),
            Self::Transport(error) => write!(formatter, "debug transport error: {error}"),
            Self::FrameTooLarge { size, max } => {
                write!(formatter, "frame payload {size} exceeds {max}")
            }
            Self::InvalidFrame(reason) => write!(formatter, "invalid debug frame: {reason}"),
            Self::UnknownMessageType(kind) => {
                write!(formatter, "unknown debug message type: {kind}")
            }
            Self::Decode {
                message_type,
                source,
            } => write!(formatter, "cannot decode message {message_type}: {source}"),
            Self::InvalidPayload {
                message_type,
                reason,
            } => write!(
                formatter,
                "invalid payload for message {message_type}: {reason}"
            ),
            Self::Encode(error) => write!(formatter, "cannot encode debug message: {error}"),
        }
    }
}

impl Error for DebugError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Transport(error) => Some(error),
            Self::Decode { source, .. } => Some(source),
            Self::Encode(error) => Some(error),
            _ => None,
        }
    }
}

impl From<io::Error> for DebugError {
    fn from(error: io::Error) -> Self {
        Self::Transport(error)
    }
}
