use std::{
    cell::RefCell,
    collections::VecDeque,
    io::{BufRead, BufReader, Cursor, Read},
    rc::Rc,
};

use debug::{
    CommandKind, DebugCommand, DebugEvent, DebugSession, OperationResult, ProgramCounter,
    ReceivedFrame, Result, SentFrame, Snapshot, StopReason, Stopped, VmState,
};
use serde_json::{Value, json};
use warduino_dap::{Adapter, AdapterOutput, read_message, source::ProgramImage, write_message};

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
        Ok(SentFrame::from_complete_frame(vec![0, 0]))
    }

    fn try_recv(&mut self) -> Result<Option<ReceivedFrame>> {
        self.0
            .borrow_mut()
            .events
            .pop_front()
            .unwrap_or(Ok(None))
            .map(|event| event.map(|event| ReceivedFrame::from_complete_frame(event, vec![0, 0])))
    }
}

type FakeAdapter = Adapter<FakeSession, FakeConnector>;

fn request(seq: u64, command: &str, arguments: Value) -> Value {
    json!({"seq": seq, "type": "request", "command": command, "arguments": arguments})
}

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
        header.clear();
        reader.read_line(&mut header).unwrap();
        assert_eq!(header, "\r\n");
        let mut body = vec![0; length];
        reader.read_exact(&mut body).unwrap();
        values.push(serde_json::from_slice(&body).unwrap());
    }
}

fn program_path() -> &'static str {
    concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/source-step.wat"
    )
}

fn acknowledged_adapter() -> (FakeAdapter, Rc<RefCell<FakeState>>, Vec<u32>) {
    let fake = FakeSession::default();
    let state = fake.0.clone();
    let mut adapter = Adapter::new(FakeConnector(fake));
    assert_eq!(
        dispatch(&mut adapter, request(1, "initialize", json!({})))[0]["success"],
        true
    );
    assert!(
        dispatch(
            &mut adapter,
            request(
                2,
                "attach",
                json!({"device": "test", "program": program_path()})
            )
        )
        .is_empty()
    );
    assert!(
        matches!(state.borrow().commands.as_slice(), [DebugCommand::UpdateModule(bytes)] if !bytes.is_empty())
    );
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::OperationResult(OperationResult {
            command: CommandKind::UpdateModule,
            success: true,
        }))));
    let initialized = framed_values(adapter.pump_events());
    assert_eq!(initialized[0]["event"], "initialized");
    let configured = dispatch(&mut adapter, request(3, "configurationDone", json!({})));
    assert_eq!(
        configured
            .iter()
            .map(|message| &message["command"])
            .collect::<Vec<_>>(),
        vec!["configurationDone", "attach"]
    );
    let pcs = ProgramImage::from_path(std::path::Path::new(program_path()))
        .unwrap()
        .instruction_pcs()
        .collect();
    (adapter, state, pcs)
}

fn stop_at(adapter: &mut FakeAdapter, state: &Rc<RefCell<FakeState>>, pc: u32) -> Vec<Value> {
    state.borrow_mut().events.extend([
        Ok(Some(DebugEvent::Stopped(Stopped {
            reason: StopReason::Step,
            location: None,
        }))),
        Ok(Some(DebugEvent::Snapshot(Snapshot {
            program_counter: ProgramCounter(pc),
            state: VmState::Paused,
            breakpoints: Vec::new(),
        }))),
    ]);
    framed_values(adapter.pump_events())
}

#[test]
fn acknowledged_attach_exposes_one_mapped_frame_and_immutable_source() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    assert_eq!(stop_at(&mut adapter, &state, pcs[0])[0]["event"], "stopped");

    let stack = dispatch(
        &mut adapter,
        request(4, "stackTrace", json!({"threadId": 1})),
    );
    let frame = &stack[0]["body"]["stackFrames"][0];
    assert_eq!(stack[0]["body"]["totalFrames"], 1);
    assert_eq!(frame["name"], "main");
    assert_eq!(frame["source"]["sourceReference"], 1);
    assert_eq!(frame["line"], 3);
    assert!(frame["column"].as_u64().unwrap() > 0);

    let source = dispatch(
        &mut adapter,
        request(5, "source", json!({"sourceReference": 1})),
    );
    assert_eq!(
        source[0]["body"]["content"],
        include_str!("fixtures/source-step.wat")
    );
    let stale = dispatch(&mut adapter, request(6, "scopes", json!({"frameId": 0})));
    assert_eq!(stale[0]["success"], false);

    stop_at(&mut adapter, &state, 0);
    let unmapped = dispatch(
        &mut adapter,
        request(7, "stackTrace", json!({"threadId": 1})),
    );
    assert_eq!(unmapped[0]["success"], false);
}

#[test]
fn source_steps_hide_intermediate_vm_stops_and_instruction_steps_send_once() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    stop_at(&mut adapter, &state, pcs[0]);

    let next = dispatch(
        &mut adapter,
        request(4, "next", json!({"threadId": 1, "granularity": "line"})),
    );
    assert_eq!(next[0]["success"], true);
    assert!(stop_at(&mut adapter, &state, pcs[0]).is_empty());
    let final_stop = stop_at(&mut adapter, &state, pcs[1]);
    assert_eq!(final_stop.len(), 1);
    assert_eq!(final_stop[0]["event"], "stopped");
    assert_eq!(
        state
            .borrow()
            .commands
            .iter()
            .filter(|command| **command == DebugCommand::StepOver)
            .count(),
        2
    );

    let instruction = dispatch(
        &mut adapter,
        request(
            5,
            "stepIn",
            json!({"threadId": 1, "granularity": "instruction"}),
        ),
    );
    assert_eq!(instruction[0]["success"], true);
    assert_eq!(
        state
            .borrow()
            .commands
            .iter()
            .filter(|command| **command == DebugCommand::Step)
            .count(),
        1
    );
}
