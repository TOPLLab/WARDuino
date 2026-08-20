use std::io::{self, BufRead, Write};

use serde::{Deserialize, Serialize};
use serde_json::Value;

const MAX_MESSAGE_SIZE: usize = 1024 * 1024;
const MAX_HEADER_LINE_LENGTH: usize = 8 * 1024;

#[derive(Clone, Debug, Deserialize)]
pub struct Request {
    pub seq: u64,
    #[serde(rename = "type")]
    pub message_type: String,
    pub command: String,
    #[serde(default)]
    pub arguments: Value,
}

pub fn read_message(reader: &mut impl BufRead) -> io::Result<Option<Request>> {
    let mut content_length = None;
    let mut saw_header = false;
    let mut line = Vec::new();
    loop {
        if !read_header_line(reader, &mut line)? {
            return if saw_header {
                Err(invalid_data("unexpected EOF in DAP headers"))
            } else {
                Ok(None)
            };
        }
        saw_header = true;

        let line = line
            .strip_suffix(b"\n")
            .expect("header lines end in newline");
        let line = line.strip_suffix(b"\r").unwrap_or(line);
        if line.is_empty() {
            break;
        }
        let line =
            std::str::from_utf8(line).map_err(|_| invalid_data("DAP header is not valid UTF-8"))?;
        let (name, value) = line
            .split_once(':')
            .ok_or_else(|| invalid_data("malformed DAP header"))?;
        if name.eq_ignore_ascii_case("Content-Length") {
            if content_length.is_some() {
                return Err(invalid_data("duplicate Content-Length header"));
            }
            let length = value
                .trim()
                .parse::<usize>()
                .map_err(|_| invalid_data("invalid Content-Length header"))?;
            if length > MAX_MESSAGE_SIZE {
                return Err(invalid_data("DAP message exceeds maximum size"));
            }
            content_length = Some(length);
        }
    }
    let length = content_length.ok_or_else(|| invalid_data("missing Content-Length header"))?;
    let mut body = vec![0; length];
    reader.read_exact(&mut body)?;
    serde_json::from_slice(&body)
        .map(Some)
        .map_err(|error| invalid_data(format!("invalid DAP JSON: {error}")))
}

fn read_header_line(reader: &mut impl BufRead, line: &mut Vec<u8>) -> io::Result<bool> {
    line.clear();
    loop {
        let buffer = reader.fill_buf()?;
        if buffer.is_empty() {
            return if line.is_empty() {
                Ok(false)
            } else {
                Err(invalid_data("unexpected EOF in DAP headers"))
            };
        }
        let count = buffer
            .iter()
            .position(|byte| *byte == b'\n')
            .map_or(buffer.len(), |position| position + 1);
        if line.len() + count > MAX_HEADER_LINE_LENGTH {
            return Err(invalid_data("DAP header line exceeds maximum size"));
        }
        line.extend_from_slice(&buffer[..count]);
        reader.consume(count);
        if line.last() == Some(&b'\n') {
            return Ok(true);
        }
    }
}

pub fn write_message(writer: &mut impl Write, value: &impl Serialize) -> io::Result<()> {
    let body = serde_json::to_vec(value)
        .map_err(|error| invalid_data(format!("cannot serialize DAP JSON: {error}")))?;
    writer.write_all(format!("Content-Length: {}\r\n\r\n", body.len()).as_bytes())?;
    writer.write_all(&body)?;
    writer.flush()
}

fn invalid_data(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

#[cfg(test)]
mod tests {
    use std::io::{BufReader, Cursor};

    use serde_json::json;

    use super::*;

    #[test]
    fn framing_uses_byte_length_and_accepts_extra_headers() {
        let value = json!({"message": "é"});
        let mut framed = Vec::new();
        write_message(&mut framed, &value).unwrap();
        let expected = serde_json::to_vec(&value).unwrap().len();
        assert!(framed.starts_with(format!("Content-Length: {expected}\r\n\r\n").as_bytes()));

        let body = br#"{"seq":1,"type":"request","command":"initialize"}"#;
        let input = format!("X-Trace: smoke\r\nContent-Length: {}\r\n\r\n", body.len());
        let mut bytes = input.into_bytes();
        bytes.extend_from_slice(body);
        let request = read_message(&mut BufReader::new(Cursor::new(bytes)))
            .unwrap()
            .unwrap();
        assert_eq!(request.command, "initialize");
    }

    #[test]
    fn framing_rejects_invalid_lengths_and_accepts_clean_eof() {
        assert!(
            read_message(&mut BufReader::new(Cursor::new(b"")))
                .unwrap()
                .is_none()
        );
        assert!(
            read_message(&mut BufReader::new(Cursor::new(
                b"Content-Length: 1\r\nContent-Length: 1\r\n\r\n{}",
            )))
            .is_err()
        );
        assert!(
            read_message(&mut BufReader::new(Cursor::new(
                b"Content-Length: 1048577\r\n\r\n",
            )))
            .is_err()
        );
    }

    #[test]
    fn framing_rejects_overlong_header_lines() {
        let mut input = vec![b'x'; MAX_HEADER_LINE_LENGTH + 1];
        input.push(b'\n');
        assert!(read_message(&mut BufReader::new(Cursor::new(input))).is_err());
    }
}
