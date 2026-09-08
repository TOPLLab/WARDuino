use prost::Message;

use crate::{
    DebugCommand, DebugError, DebugEvent, Result,
    schema::{Command, NotificationType},
};

pub(super) struct EncodedMessage {
    pub message_type: u8,
    pub payload: Vec<u8>,
}

pub(super) fn encode_command(command: DebugCommand) -> Result<EncodedMessage> {
    match command {
        DebugCommand::Run => empty(Command::Run),
        DebugCommand::Halt => empty(Command::Halt),
        DebugCommand::Pause => empty(Command::Pause),
        DebugCommand::Step => empty(Command::Step),
        DebugCommand::StepOver => empty(Command::StepOver),
        DebugCommand::AddBreakpoint(message) => encode(Command::AddBreakpoint, message),
        DebugCommand::RemoveBreakpoint(message) => encode(Command::RemoveBreakpoint, message),
        DebugCommand::ClearBreakpoints => empty(Command::ClearBreakpoints),
        DebugCommand::HeapUsage => empty(Command::HeapUsage),
        DebugCommand::Snapshot(message) => encode(Command::Snapshot, message),
        DebugCommand::UpdateFunction(message) => encode(Command::UpdateFunction, message),
        DebugCommand::UpdateLocal(message) => encode(Command::UpdateLocal, message),
        DebugCommand::UpdateCallbacks(message) => encode(Command::UpdateCallbacks, message),
        DebugCommand::UpdateModule(message) => encode(Command::UpdateModule, message),
        DebugCommand::UpdateGlobal(message) => encode(Command::UpdateGlobal, message),
        DebugCommand::UpdateStack(message) => encode(Command::UpdateStack, message),
        DebugCommand::LoadSnapshot(message) => encode(Command::LoadSnapshot, message),
        DebugCommand::Proxify => empty(Command::Proxify),
        DebugCommand::AddProxy(message) => encode(Command::AddProxy, message),
        DebugCommand::RemoveProxy(message) => encode(Command::RemoveProxy, message),
        DebugCommand::ProxyCall(message) => encode(Command::ProxyCall, message),
        DebugCommand::PopEvent => empty(Command::PopEvent),
        DebugCommand::PushEvent(message) => encode(Command::PushEvent, message),
        DebugCommand::ContinueFor(message) => encode(Command::ContinueFor, message),
        DebugCommand::Reset => empty(Command::Reset),
        DebugCommand::Invoke(message) => encode(Command::Invoke, message),
        DebugCommand::SetSnapshotPolicy(message) => encode(Command::SetSnapshotPolicy, message),
        DebugCommand::SetOverride(message) => encode(Command::SetOverride, message),
        DebugCommand::RemoveOverride(message) => encode(Command::RemoveOverride, message),
    }
}

pub(super) fn decode_event(message_type: u8, payload: &[u8]) -> Result<DebugEvent> {
    match message_type {
        value if value == notification_type(NotificationType::NotificationContinued) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::Continued)
        }
        value if value == notification_type(NotificationType::NotificationHalted) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::Halted)
        }
        value if value == notification_type(NotificationType::NotificationPaused) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::Paused)
        }
        value if value == notification_type(NotificationType::NotificationStepped) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::Stepped)
        }
        value if value == notification_type(NotificationType::NotificationHitBreakpoint) => {
            Ok(DebugEvent::HitBreakpoint(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationNewEvent) => {
            Ok(DebugEvent::NewEvent(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationSnapshot) => {
            Ok(DebugEvent::Snapshot(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationMalformed) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::TargetMalformedCommand)
        }
        value if value == notification_type(NotificationType::NotificationUnknownCommand) => {
            require_empty(value, payload)?;
            Ok(DebugEvent::TargetUnknownCommand)
        }
        value if value == notification_type(NotificationType::NotificationOperationResult) => {
            Ok(DebugEvent::OperationResult(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationRemoteFunctionResult) => {
            Ok(DebugEvent::RemoteFunctionResult(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationCheckpoint) => {
            Ok(DebugEvent::Checkpoint(decode(value, payload)?))
        }
        value if value == notification_type(NotificationType::NotificationHeapUsage) => {
            Ok(DebugEvent::HeapUsage(decode(value, payload)?))
        }
        value => Err(DebugError::UnknownMessageType(value)),
    }
}

fn command_type(command: Command) -> u8 {
    u8::try_from(command as i32).expect("debug.proto command discriminators must fit in a byte")
}

fn notification_type(notification: NotificationType) -> u8 {
    u8::try_from(notification as i32)
        .expect("debug.proto notification discriminators must fit in a byte")
}

fn empty(command: Command) -> Result<EncodedMessage> {
    Ok(EncodedMessage {
        message_type: command_type(command),
        payload: Vec::new(),
    })
}

fn encode(command: Command, message: impl Message) -> Result<EncodedMessage> {
    let mut payload = Vec::with_capacity(message.encoded_len());
    message.encode(&mut payload).map_err(DebugError::Encode)?;
    Ok(EncodedMessage {
        message_type: command_type(command),
        payload,
    })
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::schema;

    #[test]
    fn encodes_every_declared_command() {
        let cases = [
            (DebugCommand::Run, Command::Run),
            (DebugCommand::Halt, Command::Halt),
            (DebugCommand::Pause, Command::Pause),
            (DebugCommand::Step, Command::Step),
            (DebugCommand::StepOver, Command::StepOver),
            (
                DebugCommand::AddBreakpoint(schema::CodeLocation::default()),
                Command::AddBreakpoint,
            ),
            (
                DebugCommand::RemoveBreakpoint(schema::CodeLocation::default()),
                Command::RemoveBreakpoint,
            ),
            (DebugCommand::ClearBreakpoints, Command::ClearBreakpoints),
            (DebugCommand::HeapUsage, Command::HeapUsage),
            (
                DebugCommand::Snapshot(schema::Include::default()),
                Command::Snapshot,
            ),
            (
                DebugCommand::UpdateFunction(schema::Function::default()),
                Command::UpdateFunction,
            ),
            (
                DebugCommand::UpdateLocal(schema::ValueUpdate::default()),
                Command::UpdateLocal,
            ),
            (
                DebugCommand::UpdateCallbacks(schema::CallbackMapping::default()),
                Command::UpdateCallbacks,
            ),
            (
                DebugCommand::UpdateModule(schema::ModuleUpdate::default()),
                Command::UpdateModule,
            ),
            (
                DebugCommand::UpdateGlobal(schema::ValueUpdate::default()),
                Command::UpdateGlobal,
            ),
            (
                DebugCommand::UpdateStack(schema::ValueUpdate::default()),
                Command::UpdateStack,
            ),
            (
                DebugCommand::LoadSnapshot(schema::Snapshot::default()),
                Command::LoadSnapshot,
            ),
            (DebugCommand::Proxify, Command::Proxify),
            (
                DebugCommand::AddProxy(schema::FunctionRef::default()),
                Command::AddProxy,
            ),
            (
                DebugCommand::RemoveProxy(schema::FunctionRef::default()),
                Command::RemoveProxy,
            ),
            (
                DebugCommand::ProxyCall(schema::RemoteFunctionCall::default()),
                Command::ProxyCall,
            ),
            (DebugCommand::PopEvent, Command::PopEvent),
            (
                DebugCommand::PushEvent(schema::Event::default()),
                Command::PushEvent,
            ),
            (
                DebugCommand::ContinueFor(schema::ContinueFor::default()),
                Command::ContinueFor,
            ),
            (DebugCommand::Reset, Command::Reset),
            (
                DebugCommand::Invoke(schema::RemoteFunctionCall::default()),
                Command::Invoke,
            ),
            (
                DebugCommand::SetSnapshotPolicy(schema::SnapshotPolicyConfig::default()),
                Command::SetSnapshotPolicy,
            ),
            (
                DebugCommand::SetOverride(schema::Override::default()),
                Command::SetOverride,
            ),
            (
                DebugCommand::RemoveOverride(schema::Override::default()),
                Command::RemoveOverride,
            ),
        ];

        for (command, discriminator) in cases {
            assert_eq!(
                encode_command(command).unwrap().message_type,
                command_type(discriminator)
            );
        }
    }

    #[test]
    fn snapshot_include_preserves_empty_and_selected_forms() {
        assert!(
            encode_command(DebugCommand::Snapshot(schema::Include::default()))
                .unwrap()
                .payload
                .is_empty()
        );
        assert_eq!(
            encode_command(DebugCommand::Snapshot(schema::Include { fields: vec![1] }))
                .unwrap()
                .payload,
            [0x0a, 0x01, 0x01]
        );
    }

    #[test]
    fn decodes_every_declared_notification() {
        for notification in [
            NotificationType::NotificationContinued,
            NotificationType::NotificationHalted,
            NotificationType::NotificationPaused,
            NotificationType::NotificationStepped,
            NotificationType::NotificationMalformed,
            NotificationType::NotificationUnknownCommand,
        ] {
            decode_event(notification_type(notification), &[]).unwrap();
        }
        for notification in [
            NotificationType::NotificationHitBreakpoint,
            NotificationType::NotificationNewEvent,
            NotificationType::NotificationSnapshot,
            NotificationType::NotificationOperationResult,
            NotificationType::NotificationRemoteFunctionResult,
            NotificationType::NotificationCheckpoint,
            NotificationType::NotificationHeapUsage,
        ] {
            decode_event(notification_type(notification), &[]).unwrap();
        }
    }
}
