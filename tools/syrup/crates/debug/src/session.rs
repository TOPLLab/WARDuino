use std::{
    collections::VecDeque,
    io,
    net::{TcpStream, ToSocketAddrs},
};

use crate::{
    DebugCommand, DebugError, DebugEvent, DebugSession, DisconnectReason, Result, codec,
    framing::{self, FrameDecoder},
    transport::{TcpTransport, Transport},
};

const READ_BUFFER_SIZE: usize = 1024;
const MAX_EVENTS_PER_RECEIVE: usize = 16;

/// A TCP connection to a WARDuino debugger channel.
pub struct WarduinoSession {
    inner: Session<TcpTransport>,
}

impl WarduinoSession {
    pub fn connect(address: impl ToSocketAddrs) -> Result<Self> {
        let stream = TcpStream::connect(address).map_err(DebugError::Transport)?;
        Self::from_tcp_stream(stream)
    }

    pub fn from_tcp_stream(stream: TcpStream) -> Result<Self> {
        let transport = TcpTransport::new(stream).map_err(DebugError::Transport)?;
        Ok(Self {
            inner: Session::new(transport),
        })
    }
}

impl DebugSession for WarduinoSession {
    fn send(&mut self, command: DebugCommand) -> Result<()> {
        self.inner.send(command)
    }

    fn try_recv(&mut self) -> Result<Option<DebugEvent>> {
        self.inner.try_recv()
    }
}

struct Session<T> {
    transport: T,
    decoder: FrameDecoder,
    events: VecDeque<DebugEvent>,
    state: ConnectionState,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum ConnectionState {
    Connected,
    Closed,
    Failed,
}

impl<T: Transport> Session<T> {
    fn new(transport: T) -> Self {
        Self {
            transport,
            decoder: FrameDecoder::new(),
            events: VecDeque::new(),
            state: ConnectionState::Connected,
        }
    }

    fn send(&mut self, command: DebugCommand) -> Result<()> {
        if self.state != ConnectionState::Connected {
            return Err(DebugError::NotConnected);
        }
        let message = codec::encode_command(command)?;
        let frame = framing::encode_frame(message.message_type, &message.payload)?;
        self.transport
            .write_all(&frame)
            .map_err(|error| self.fail_transport(error))
    }

    fn try_recv(&mut self) -> Result<Option<DebugEvent>> {
        if let Some(event) = self.events.pop_front() {
            return Ok(Some(event));
        }
        if self.state != ConnectionState::Connected {
            return Ok(None);
        }

        self.decode_buffered()?;
        if let Some(event) = self.events.pop_front() {
            return Ok(Some(event));
        }

        let mut bytes = [0; READ_BUFFER_SIZE];
        match self.transport.read_available(&mut bytes) {
            Ok(0) => {
                self.state = ConnectionState::Closed;
                Ok(Some(DebugEvent::Disconnected(
                    DisconnectReason::TransportClosed,
                )))
            }
            Ok(count) => {
                self.decoder.push(&bytes[..count]);
                self.decode_buffered()?;
                Ok(self.events.pop_front())
            }
            Err(error)
                if matches!(
                    error.kind(),
                    io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut
                ) =>
            {
                Ok(None)
            }
            Err(error) => Err(self.fail_transport(error)),
        }
    }

    fn decode_buffered(&mut self) -> Result<()> {
        for _ in 0..MAX_EVENTS_PER_RECEIVE {
            let frame = match self.decoder.next_frame() {
                Ok(Some(frame)) => frame,
                Ok(None) => return Ok(()),
                Err(error) => return Err(self.fail(error)),
            };
            let event = codec::decode_event(frame.message_type, &frame.payload)
                .map_err(|error| self.fail(error))?;
            self.events.push_back(event);
        }
        Ok(())
    }

    fn fail_transport(&mut self, error: io::Error) -> DebugError {
        self.fail(DebugError::Transport(error))
    }

    fn fail(&mut self, error: DebugError) -> DebugError {
        self.state = ConnectionState::Failed;
        error
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct MemoryTransport {
        input: VecDeque<io::Result<Vec<u8>>>,
        written: Vec<u8>,
    }

    impl Transport for MemoryTransport {
        fn write_all(&mut self, bytes: &[u8]) -> io::Result<()> {
            self.written.extend_from_slice(bytes);
            Ok(())
        }

        fn read_available(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
            match self
                .input
                .pop_front()
                .unwrap_or_else(|| Err(io::Error::from(io::ErrorKind::WouldBlock)))
            {
                Ok(input) => {
                    bytes[..input.len()].copy_from_slice(&input);
                    Ok(input.len())
                }
                Err(error) => Err(error),
            }
        }
    }

    #[test]
    fn sends_complete_frame() {
        let transport = MemoryTransport {
            input: VecDeque::new(),
            written: Vec::new(),
        };
        let mut session = Session::new(transport);
        session.send(DebugCommand::Continue).unwrap();
        assert_eq!(session.transport.written, [0, 0]);
    }

    #[test]
    fn returns_fragmented_events_without_waiting() {
        let transport = MemoryTransport {
            input: VecDeque::from([Ok(vec![4, 4, 10]), Ok(vec![2, 16, 23])]),
            written: Vec::new(),
        };
        let mut session = Session::new(transport);
        assert_eq!(session.try_recv().unwrap(), None);
        assert!(matches!(
            session.try_recv().unwrap(),
            Some(DebugEvent::Stopped(_))
        ));
        assert_eq!(session.try_recv().unwrap(), None);
    }

    #[test]
    fn reports_disconnect_once() {
        let transport = MemoryTransport {
            input: VecDeque::from([Ok(Vec::new())]),
            written: Vec::new(),
        };
        let mut session = Session::new(transport);
        assert!(matches!(
            session.try_recv().unwrap(),
            Some(DebugEvent::Disconnected(_))
        ));
        assert_eq!(session.try_recv().unwrap(), None);
    }
}
