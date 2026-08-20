use std::{
    io::{self, Read, Write},
    net::TcpStream,
};

pub(super) trait Transport {
    fn write_all(&mut self, bytes: &[u8]) -> io::Result<()>;
    fn read_available(&mut self, bytes: &mut [u8]) -> io::Result<usize>;
}

pub(super) struct TcpTransport {
    stream: TcpStream,
}

impl TcpTransport {
    pub(super) fn new(stream: TcpStream) -> io::Result<Self> {
        stream.set_nonblocking(true)?;
        Ok(Self { stream })
    }
}

impl Transport for TcpTransport {
    fn write_all(&mut self, bytes: &[u8]) -> io::Result<()> {
        self.stream.write_all(bytes)
    }

    fn read_available(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        self.stream.read(bytes)
    }
}
