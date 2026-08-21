use crate::{DebugError, Result};

pub(super) const MAX_PAYLOAD: usize = 64 * 1024;

pub(super) struct Frame {
    pub message_type: u8,
    pub payload: Vec<u8>,
}

pub(super) fn encode_frame(message_type: u8, payload: &[u8]) -> Result<Vec<u8>> {
    if payload.len() > MAX_PAYLOAD {
        return Err(DebugError::FrameTooLarge {
            size: payload.len(),
            max: MAX_PAYLOAD,
        });
    }

    let mut frame = Vec::with_capacity(6 + payload.len());
    frame.push(message_type);
    push_varint(&mut frame, payload.len());
    frame.extend_from_slice(payload);
    Ok(frame)
}

pub(super) struct FrameDecoder {
    bytes: Vec<u8>,
}

impl FrameDecoder {
    pub(super) fn new() -> Self {
        Self { bytes: Vec::new() }
    }

    pub(super) fn push(&mut self, bytes: &[u8]) {
        self.bytes.extend_from_slice(bytes);
    }

    pub(super) fn next_frame(&mut self) -> Result<Option<Frame>> {
        if self.bytes.len() < 2 {
            return Ok(None);
        }
        let Some((header_len, payload_len)) = decode_length(&self.bytes)? else {
            return Ok(None);
        };
        if payload_len > MAX_PAYLOAD {
            return Err(DebugError::FrameTooLarge {
                size: payload_len,
                max: MAX_PAYLOAD,
            });
        }
        let end = header_len + payload_len;
        if self.bytes.len() < end {
            return Ok(None);
        }
        let payload = self.bytes[header_len..end].to_vec();
        let message_type = self.bytes[0];
        self.bytes.drain(..end);
        Ok(Some(Frame {
            message_type,
            payload,
        }))
    }
}

fn push_varint(output: &mut Vec<u8>, mut value: usize) {
    loop {
        let mut byte = (value & 0x7f) as u8;
        value >>= 7;
        if value != 0 {
            byte |= 0x80;
        }
        output.push(byte);
        if value == 0 {
            return;
        }
    }
}

fn decode_length(bytes: &[u8]) -> Result<Option<(usize, usize)>> {
    let mut value = 0usize;
    for index in 0..5 {
        let position = index + 1;
        let Some(&byte) = bytes.get(position) else {
            return Ok(None);
        };
        value |= ((byte & 0x7f) as usize) << (index * 7);
        if byte & 0x80 == 0 {
            if index > 0 && value < (1usize << (index * 7)) {
                return Err(DebugError::InvalidFrame("non-canonical payload length"));
            }
            return Ok(Some((position + 1, value)));
        }
    }
    Err(DebugError::InvalidFrame(
        "payload length varint exceeds five bytes",
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn decodes_fragmented_and_concatenated_frames() {
        let mut decoder = FrameDecoder::new();
        decoder.push(&[2, 2, 8]);
        assert!(decoder.next_frame().unwrap().is_none());
        decoder.push(&[1, 0, 0]);
        let first = decoder.next_frame().unwrap().unwrap();
        assert_eq!(first.message_type, 2);
        assert_eq!(first.payload, [8, 1]);
        assert_eq!(decoder.next_frame().unwrap().unwrap().message_type, 0);
    }

    #[test]
    fn encodes_complete_outgoing_frames() {
        assert_eq!(encode_frame(0, &[]).unwrap(), [0, 0]);
        assert_eq!(
            encode_frame(5, &[0x0a, 0x02, 0x10, 0x17]).unwrap(),
            [5, 4, 0x0a, 0x02, 0x10, 0x17]
        );
        let payload = vec![0xa5; 128];
        let frame = encode_frame(7, &payload).unwrap();
        assert_eq!(&frame[..3], &[7, 0x80, 0x01]);
        assert_eq!(&frame[3..], payload);
    }
}
