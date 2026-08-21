use std::{
    cell::RefCell,
    collections::VecDeque,
    io::{BufRead, BufReader, Cursor, Read},
    rc::Rc,
};

use debug::{
    CommandKind, DebugCommand, DebugError, DebugEvent, DebugSession, OperationResult,
    ProgramCounter, Result, SentFrame, Snapshot, StopReason, Stopped, VmState,
};
use serde_json::{Value, json};
use warduino_dap::{Adapter, AdapterOutput, read_message, write_message};

#[derive(Default)]
struct FakeState {
    commands: Vec<DebugCommand>,
    events: VecDeque<Result<Option<DebugEvent>>>,
}

#[derive(Clone, Default)]
struct FakeSession(Rc<RefCell<FakeState>>);

#[derive(Clone)]
struct FakeConnector(FakeSession);

impl warduino_dap::SessionConnector<FakeSession> for FakeConnector {
    fn connect(&mut self, _device: &str) -> std::result::Result<FakeSession, String> {
        Ok(self.0.clone())
    }
}

impl DebugSession for FakeSession {
    fn send(&mut self, command: DebugCommand) -> Result<SentFrame> {
        self.0.borrow_mut().commands.push(command);
        Ok(SentFrame::from_complete_frame(vec![5, 4, 10, 2, 16, 23]))
    }

    fn try_recv(&mut self) -> Result<Option<DebugEvent>> {
        self.0.borrow_mut().events.pop_front().unwrap_or(Ok(None))
    }
}

fn request(seq: u64, command: &str, arguments: Value) -> Value {
    json!({"seq": seq, "type": "request", "command": command, "arguments": arguments})
}

type FakeAdapter = Adapter<FakeSession, FakeConnector>;

fn dispatch(adapter: &mut FakeAdapter, input: Value) -> Vec<Value> {
    let mut incoming = Vec::new();
    write_message(&mut incoming, &input).unwrap();
    let request = read_message(&mut BufReader::new(Cursor::new(incoming)))
        .unwrap()
        .unwrap();
    framed_values(adapter.handle_request(request))
}

fn framed_values(output: AdapterOutput) -> Vec<Value> {
    let mut bytes = Vec::new();
    for message in output.messages {
        write_message(&mut bytes, &message).unwrap();
    }
    let mut reader = BufReader::new(Cursor::new(bytes));
    let mut values = Vec::new();
    loop {
        let mut header = String::new();
        if reader.read_line(&mut header).unwrap() == 0 {
            return values;
        }
        let length = header
            .strip_prefix("Content-Length: ")
            .unwrap()
            .trim()
            .parse::<usize>()
            .unwrap();
        let mut separator = String::new();
        reader.read_line(&mut separator).unwrap();
        assert_eq!(separator, "\r\n");
        let mut body = vec![0; length];
        reader.read_exact(&mut body).unwrap();
        values.push(serde_json::from_slice(&body).unwrap());
    }
}

#[test]
fn smoke_transcript_is_framed_and_ordered() {
    let fake = FakeSession::default();
    let state = fake.0.clone();
    let mut adapter = Adapter::new(FakeConnector(fake));

    let initialize = dispatch(&mut adapter, request(1, "initialize", json!({})));
    assert_eq!(initialize[0]["success"], true);
    assert_eq!(
        initialize[0]["body"]["supportsConfigurationDoneRequest"],
        true
    );

    let attach = dispatch(
        &mut adapter,
        request(2, "attach", json!({"device": "test"})),
    );
    assert_eq!(attach[0]["event"], "initialized");

    let configured = dispatch(&mut adapter, request(3, "configurationDone", json!({})));
    assert_eq!(configured[0]["command"], "configurationDone");
    assert_eq!(configured[1]["command"], "attach");

    let threads = dispatch(&mut adapter, request(4, "threads", json!({})));
    assert_eq!(
        threads[0]["body"]["threads"][0],
        json!({"id": 1, "name": "Waffyr VM"})
    );

    let continued = dispatch(&mut adapter, request(5, "continue", json!({"threadId": 1})));
    assert_eq!(continued[0]["body"], json!({"allThreadsContinued": true}));
    assert_eq!(state.borrow().commands, vec![DebugCommand::Continue]);

    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Continued)));
    let continued_event = framed_values(adapter.pump_events());
    assert_eq!(continued_event[0]["event"], "continued");
    assert_eq!(continued_event[0]["body"]["threadId"], 1);

    let paused = dispatch(&mut adapter, request(6, "pause", json!({"threadId": 1})));
    assert_eq!(paused[0]["success"], true);
    assert_eq!(
        state.borrow().commands,
        vec![DebugCommand::Continue, DebugCommand::Pause]
    );

    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Stopped(Stopped {
            reason: StopReason::Pause,
            location: None,
        }))));
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Snapshot(Snapshot {
            program_counter: ProgramCounter(23),
            state: VmState::Paused,
            breakpoints: Vec::new(),
        }))));
    let stopped_event = framed_values(adapter.pump_events());
    assert_eq!(stopped_event[0]["event"], "stopped");
    assert_eq!(stopped_event[0]["body"]["reason"], "pause");

    let stack_trace = dispatch(
        &mut adapter,
        request(7, "stackTrace", json!({"threadId": 1})),
    );
    assert_eq!(stack_trace[0]["success"], true);
    let wrong_thread = dispatch(
        &mut adapter,
        request(70, "stackTrace", json!({"threadId": 2})),
    );
    assert_eq!(wrong_thread[0]["success"], false);
    assert_eq!(wrong_thread[0]["message"], "unknown threadId");
    assert_eq!(
        stack_trace[0]["body"]["stackFrames"][0]["instructionPointerReference"],
        "0x00000017"
    );
    let scopes = dispatch(&mut adapter, request(8, "scopes", json!({"frameId": 1})));
    assert_eq!(scopes[0]["body"]["scopes"][0]["name"], "VM");
    let variables = dispatch(
        &mut adapter,
        request(9, "variables", json!({"variablesReference": 1})),
    );
    assert_eq!(variables[0]["body"]["variables"][0]["value"], "0x00000017");

    let disconnected = dispatch(&mut adapter, request(10, "disconnect", json!({})));
    assert_eq!(disconnected[0]["success"], true);
}

#[test]
fn debug_library_error_becomes_diagnostic_and_termination() {
    let fake = FakeSession::default();
    fake.0
        .borrow_mut()
        .events
        .push_back(Err(DebugError::NotConnected));
    let mut adapter = Adapter::new(FakeConnector(fake));

    dispatch(&mut adapter, request(1, "initialize", json!({})));
    dispatch(
        &mut adapter,
        request(2, "attach", json!({"device": "test"})),
    );
    dispatch(&mut adapter, request(3, "configurationDone", json!({})));

    let output = framed_values(adapter.pump_events());
    assert_eq!(output[0]["event"], "output");
    assert_eq!(output[0]["body"]["category"], "stderr");
    assert_eq!(output[1]["event"], "terminated");
}

fn attached_adapter() -> (FakeAdapter, Rc<RefCell<FakeState>>) {
    let fake = FakeSession::default();
    let state = fake.0.clone();
    let mut adapter = Adapter::new(FakeConnector(fake));
    dispatch(&mut adapter, request(1, "initialize", json!({})));
    dispatch(
        &mut adapter,
        request(2, "attach", json!({"device": "test"})),
    );
    dispatch(&mut adapter, request(3, "configurationDone", json!({})));
    (adapter, state)
}

#[test]
fn disconnect_fails_a_retained_attach_before_terminating() {
    let fake = FakeSession::default();
    let mut adapter = Adapter::new(FakeConnector(fake));
    dispatch(&mut adapter, request(1, "initialize", json!({})));
    dispatch(
        &mut adapter,
        request(2, "attach", json!({"device": "test"})),
    );

    let disconnected = dispatch(&mut adapter, request(3, "disconnect", json!({})));
    assert_eq!(disconnected.len(), 3);
    assert_eq!(disconnected[0]["command"], "attach");
    assert_eq!(disconnected[0]["request_seq"], 2);
    assert_eq!(disconnected[0]["success"], false);
    assert_eq!(disconnected[1]["command"], "disconnect");
    assert_eq!(disconnected[1]["success"], true);
    assert_eq!(disconnected[2]["event"], "terminated");
}

#[test]
fn halted_terminates_once_without_a_stopped_event() {
    let (mut adapter, state) = attached_adapter();
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Halted)));

    let halted = framed_values(adapter.pump_events());
    assert_eq!(halted.len(), 1);
    assert_eq!(halted[0]["event"], "terminated");
    assert!(framed_values(adapter.pump_events()).is_empty());
}

#[test]
fn target_command_failures_are_reported_as_async_diagnostics() {
    let (mut adapter, state) = attached_adapter();
    state.borrow_mut().events.extend([
        Ok(Some(DebugEvent::OperationResult(OperationResult {
            command: CommandKind::Pause,
            success: false,
        }))),
        Ok(Some(DebugEvent::TargetMalformedCommand)),
        Ok(Some(DebugEvent::TargetUnknownCommand)),
    ]);

    let failures = framed_values(adapter.pump_events());
    assert_eq!(failures.len(), 3);
    for failure in failures {
        assert_eq!(failure["event"], "output");
        assert_eq!(failure["body"]["category"], "stderr");
        assert!(
            failure["body"]["output"]
                .as_str()
                .unwrap()
                .contains("asynchronously")
        );
    }
}

#[test]
fn pause_requires_the_synthetic_thread_id() {
    let (mut adapter, state) = attached_adapter();
    let paused = dispatch(&mut adapter, request(4, "pause", json!({})));
    assert_eq!(paused[0]["success"], false);
    assert!(state.borrow().commands.is_empty());
}

#[test]
fn continue_requires_the_synthetic_thread_id() {
    let (mut adapter, state) = attached_adapter();
    let continued = dispatch(&mut adapter, request(4, "continue", json!({})));
    assert_eq!(continued[0]["success"], false);
    assert!(state.borrow().commands.is_empty());
}

#[test]
fn standard_run_control_commands_map_to_the_expected_warduino_commands() {
    let (mut adapter, state) = attached_adapter();

    let step = dispatch(&mut adapter, request(4, "stepIn", json!({"threadId": 1})));
    assert_eq!(step[0]["success"], true);
    let next = dispatch(&mut adapter, request(5, "next", json!({"threadId": 1})));
    assert_eq!(next[0]["success"], true);
    let restart = dispatch(&mut adapter, request(6, "restart", json!({})));
    assert_eq!(restart[0]["success"], true);
    assert_eq!(restart[1]["event"], "stopped");
    assert_eq!(restart[1]["body"]["reason"], "pause");
    let terminate = dispatch(&mut adapter, request(7, "terminate", json!({})));
    assert_eq!(terminate[0]["success"], true);

    assert_eq!(
        state.borrow().commands,
        vec![
            DebugCommand::Step,
            DebugCommand::StepOver,
            DebugCommand::Reset,
            DebugCommand::Halt,
        ]
    );
}

#[test]
fn initialize_only_advertises_implemented_capabilities() {
    let fake = FakeSession::default();
    let mut adapter = Adapter::new(FakeConnector(fake));
    let initialize = dispatch(&mut adapter, request(1, "initialize", json!({})));
    let capabilities = &initialize[0]["body"];
    assert_eq!(capabilities["supportsTerminateRequest"], true);
    assert_eq!(capabilities["supportsRestartRequest"], true);
    assert!(capabilities.get("supportsInstructionBreakpoints").is_none());
    assert!(capabilities.get("supportsSetVariable").is_none());
}

#[test]
fn vm_frame_trace_is_opt_in_and_preserves_complete_receipts() {
    let fake = FakeSession::default();
    let state = fake.0.clone();
    let mut adapter = Adapter::new(FakeConnector(fake));
    dispatch(
        &mut adapter,
        request(1, "initialize", json!({"warduinoVmFrame": true})),
    );
    dispatch(
        &mut adapter,
        request(2, "attach", json!({"device": "test"})),
    );
    dispatch(&mut adapter, request(3, "configurationDone", json!({})));

    let continued = dispatch(&mut adapter, request(4, "continue", json!({"threadId": 1})));
    assert_eq!(continued[0]["type"], "response");
    assert_eq!(continued[1]["event"], "warduino/vmFrame");
    assert_eq!(
        continued[1]["body"],
        json!({"direction":"outgoing","command":"continue","bytes":[5,4,10,2,16,23]})
    );

    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Stopped(Stopped {
            reason: StopReason::Pause,
            location: None,
        }))));
    let inspect = framed_values(adapter.pump_events());
    assert_eq!(inspect[0]["event"], "warduino/vmFrame");
    assert_eq!(inspect[0]["body"]["command"], "inspect");

    let disconnect = dispatch(&mut adapter, request(5, "disconnect", json!({})));
    assert_eq!(disconnect[0]["event"], "warduino/vmFrame");
    assert_eq!(disconnect[0]["body"]["command"], "continue");
    assert_eq!(disconnect[0]["body"]["bytes"], json!([5, 4, 10, 2, 16, 23]));
}
