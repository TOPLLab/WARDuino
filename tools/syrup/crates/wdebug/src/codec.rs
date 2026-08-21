use prost::Message;

use crate::{
    CodeLocation, CommandKind, DebugCommand, DebugError, DebugEvent, ModuleIndex, OperationResult,
    ProgramCounter, Result, Snapshot, StopReason, Stopped, VmState, wire,
};

const CONTINUED: u8 = 0;
const HALTED: u8 = 1;
const PAUSED: u8 = 2;
const STEPPED: u8 = 3;
const HIT_BREAKPOINT: u8 = 4;
const SNAPSHOT: u8 = 8;
const MALFORMED: u8 = 12;
const UNKNOWN_COMMAND: u8 = 13;
const OPERATION_RESULT: u8 = 14;

pub(super) struct EncodedMessage {
    pub message_type: u8,
    pub payload: Vec<u8>,
}

pub(super) fn encode_command(command: DebugCommand) -> Result<EncodedMessage> {
    match command {
        DebugCommand::Continue => empty(0),
        DebugCommand::Halt => empty(1),
        DebugCommand::Pause => empty(2),
        DebugCommand::Step => empty(3),
        DebugCommand::StepOver => empty(4),
        DebugCommand::ContinueFor(count) => {
            if count == 0 {
                return Err(DebugError::InvalidPayload {
                    message_type: 22,
                    reason: "count must not be zero",
                });
            }
            encode(22, wire::ContinueFor { count })
        }
        DebugCommand::AddBreakpoint(location) => encode_breakpoint(5, location),
        DebugCommand::RemoveBreakpoint(location) => encode_breakpoint(6, location),
        DebugCommand::RequestSnapshot => empty(9),
        DebugCommand::Inspect(state) => encode(23, wire::Inspect { state }),
        DebugCommand::Reset => empty(24),
    }
}

pub(super) fn decode_event(message_type: u8, payload: &[u8]) -> Result<DebugEvent> {
    match message_type {
        CONTINUED => {
            require_empty(message_type, payload)?;
            Ok(DebugEvent::Continued)
        }
        HALTED => {
            require_empty(message_type, payload)?;
            Ok(DebugEvent::Halted)
        }
        PAUSED => stopped(message_type, payload, StopReason::Pause, None),
        STEPPED => stopped(message_type, payload, StopReason::Step, None),
        HIT_BREAKPOINT => {
            let hit = decode::<wire::HitBreakpoint>(message_type, payload)?;
            let location = hit.location.ok_or(DebugError::InvalidPayload {
                message_type,
                reason: "breakpoint location is required",
            })?;
            stopped(
                message_type,
                payload,
                StopReason::Breakpoint,
                Some(location),
            )
        }
        SNAPSHOT => {
            let snapshot = decode::<wire::Snapshot>(message_type, payload)?;
            Ok(DebugEvent::Snapshot(Snapshot {
                program_counter: ProgramCounter(snapshot.program_counter),
                state: vm_state(snapshot.state),
                breakpoints: snapshot
                    .breakpoints
                    .into_iter()
                    .map(ProgramCounter)
                    .collect(),
            }))
        }
        MALFORMED => {
            require_empty(message_type, payload)?;
            Ok(DebugEvent::TargetMalformedCommand)
        }
        UNKNOWN_COMMAND => {
            require_empty(message_type, payload)?;
            Ok(DebugEvent::TargetUnknownCommand)
        }
        OPERATION_RESULT => {
            let result = decode::<wire::OperationResult>(message_type, payload)?;
            Ok(DebugEvent::OperationResult(OperationResult {
                command: command_kind(result.command),
                success: result.success,
            }))
        }
        other => Err(DebugError::UnknownMessageType(other)),
    }
}

fn empty(message_type: u8) -> Result<EncodedMessage> {
    Ok(EncodedMessage {
        message_type,
        payload: Vec::new(),
    })
}

fn encode_breakpoint(message_type: u8, location: CodeLocation) -> Result<EncodedMessage> {
    encode(
        message_type,
        wire::Breakpoint {
            location: Some(to_wire_location(location)),
        },
    )
}

fn encode(message_type: u8, message: impl Message) -> Result<EncodedMessage> {
    let mut payload = Vec::with_capacity(message.encoded_len());
    message.encode(&mut payload).map_err(DebugError::Encode)?;
    Ok(EncodedMessage {
        message_type,
        payload,
    })
}

fn stopped(
    message_type: u8,
    payload: &[u8],
    reason: StopReason,
    location: Option<wire::CodeLocation>,
) -> Result<DebugEvent> {
    if location.is_none() {
        require_empty(message_type, payload)?;
    }
    Ok(DebugEvent::Stopped(Stopped {
        reason,
        location: location.map(from_wire_location),
    }))
}

fn decode<T: Message + Default>(message_type: u8, payload: &[u8]) -> Result<T> {
    T::decode(payload).map_err(|source| DebugError::Decode {
        message_type,
        source,
    })
}

fn require_empty(message_type: u8, payload: &[u8]) -> Result<()> {
    if payload.is_empty() {
        Ok(())
    } else {
        Err(DebugError::InvalidPayload {
            message_type,
            reason: "payload must be empty",
        })
    }
}

fn to_wire_location(location: CodeLocation) -> wire::CodeLocation {
    wire::CodeLocation {
        module_index: location.module.0,
        program_counter: location.program_counter.0,
    }
}

fn from_wire_location(location: wire::CodeLocation) -> CodeLocation {
    CodeLocation {
        module: ModuleIndex(location.module_index),
        program_counter: ProgramCounter(location.program_counter),
    }
}

fn vm_state(value: i32) -> VmState {
    match value {
        0 => VmState::Running,
        1 => VmState::Paused,
        2 => VmState::Stepping,
        3 => VmState::ProxyRunning,
        4 => VmState::ProxyHalted,
        other => VmState::Unknown(other),
    }
}

fn command_kind(value: i32) -> CommandKind {
    match value {
        0 => CommandKind::Continue,
        1 => CommandKind::Halt,
        2 => CommandKind::Pause,
        3 => CommandKind::Step,
        4 => CommandKind::StepOver,
        5 => CommandKind::AddBreakpoint,
        6 => CommandKind::RemoveBreakpoint,
        9 => CommandKind::Snapshot,
        22 => CommandKind::ContinueFor,
        24 => CommandKind::Reset,
        other => CommandKind::Other(other),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encodes_breakpoint_command() {
        let message = encode_command(DebugCommand::AddBreakpoint(CodeLocation {
            module: ModuleIndex(0),
            program_counter: ProgramCounter(23),
        }))
        .unwrap();
        assert_eq!(message.message_type, 5);
        assert_eq!(message.payload, [10, 2, 16, 23]);
    }

    #[test]
    fn decodes_breakpoint_stop() {
        let event = decode_event(HIT_BREAKPOINT, &[10, 2, 16, 23]).unwrap();
        assert_eq!(
            event,
            DebugEvent::Stopped(Stopped {
                reason: StopReason::Breakpoint,
                location: Some(CodeLocation {
                    module: ModuleIndex(0),
                    program_counter: ProgramCounter(23)
                }),
            })
        );
    }

    #[test]
    fn encodes_each_command_type() {
        let location = CodeLocation {
            module: ModuleIndex(1),
            program_counter: ProgramCounter(2),
        };
        let cases = [
            (DebugCommand::Continue, 0),
            (DebugCommand::Halt, 1),
            (DebugCommand::Pause, 2),
            (DebugCommand::Step, 3),
            (DebugCommand::StepOver, 4),
            (DebugCommand::AddBreakpoint(location), 5),
            (DebugCommand::RemoveBreakpoint(location), 6),
            (DebugCommand::RequestSnapshot, 9),
            (DebugCommand::Inspect(Vec::new()), 23),
            (DebugCommand::Reset, 24),
            (DebugCommand::ContinueFor(1), 22),
        ];
        for (command, message_type) in cases {
            assert_eq!(encode_command(command).unwrap().message_type, message_type);
        }
    }
}
