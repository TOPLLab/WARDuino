use std::{
    cell::RefCell,
    collections::VecDeque,
    io::{BufRead, BufReader, Cursor, Read},
    rc::Rc,
    thread,
    time::Duration,
};

use debug::{DebugCommand, DebugEvent, DebugSession, ReceivedFrame, Result, SentFrame, schema};
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

fn callstack_path() -> &'static str {
    concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/callstack.wat")
}

fn acknowledged_adapter() -> (FakeAdapter, Rc<RefCell<FakeState>>, Vec<u32>) {
    acknowledged_adapter_for(program_path())
}

fn acknowledged_adapter_for(
    program: &'static str,
) -> (FakeAdapter, Rc<RefCell<FakeState>>, Vec<u32>) {
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
            request(2, "attach", json!({"device": "test", "program": program}))
        )
        .is_empty()
    );
    assert!(
        matches!(state.borrow().commands.as_slice(), [DebugCommand::UpdateModule(bytes)] if !bytes.wasm.is_empty())
    );
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::OperationResult(
            schema::OperationResult {
                command: schema::Command::UpdateModule as i32,
                success: true,
            },
        ))));
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
    let pcs = ProgramImage::from_path(std::path::Path::new(program))
        .unwrap()
        .instruction_pcs()
        .collect();
    (adapter, state, pcs)
}

fn stop_at(adapter: &mut FakeAdapter, state: &Rc<RefCell<FakeState>>, pc: u32) -> Vec<Value> {
    stop_at_event(adapter, state, pc, DebugEvent::Stepped)
}

fn stop_at_event(
    adapter: &mut FakeAdapter,
    state: &Rc<RefCell<FakeState>>,
    pc: u32,
    event: DebugEvent,
) -> Vec<Value> {
    state.borrow_mut().events.extend([
        Ok(Some(event)),
        Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            program_counter: pc,
            state: schema::State::WarduinoPause as i32,
            ..Default::default()
        }))),
    ]);
    framed_values(adapter.pump_events())
}

#[test]
fn requests_a_pc_snapshot_after_every_stop_notification() {
    for event in [
        DebugEvent::Paused,
        DebugEvent::Stepped,
        DebugEvent::HitBreakpoint(schema::CodeLocation::default()),
    ] {
        let (mut adapter, state, pcs) = acknowledged_adapter();
        let output = stop_at_event(&mut adapter, &state, pcs[0], event);
        assert_eq!(output[0]["event"], "stopped");
        assert!(matches!(
            state.borrow().commands.last(),
            Some(DebugCommand::Snapshot(include)) if include.fields == [schema::SnapshotSection::Pc as u8]
        ));
    }
}

#[test]
fn coalesces_paused_notification_emitted_by_its_snapshot_request() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    state.borrow_mut().events.extend([
        Ok(Some(DebugEvent::Paused)),
        Ok(Some(DebugEvent::Paused)),
        Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            program_counter: pcs[0],
            state: schema::State::WarduinoPause as i32,
            ..Default::default()
        }))),
    ]);

    let output = framed_values(adapter.pump_events());
    assert_eq!(
        output
            .iter()
            .filter(|message| message["event"] == "stopped")
            .count(),
        1
    );
    assert_eq!(
        state
            .borrow()
            .commands
            .iter()
            .filter(|command| matches!(command, DebugCommand::Snapshot(_)))
            .count(),
        1
    );
}

#[test]
fn deferred_stack_and_locals_use_request_time_snapshots() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    assert_eq!(stop_at(&mut adapter, &state, pcs[0])[0]["event"], "stopped");

    assert!(
        dispatch(
            &mut adapter,
            request(4, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    assert!(matches!(
        state.borrow().commands.last(),
        Some(DebugCommand::Snapshot(include)) if include.fields == [0x04]
    ));
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            callstack: vec![schema::CallstackEntry {
                function_index: 0,
                ..Default::default()
            }],
            ..Default::default()
        }))));
    let stack = framed_values(adapter.pump_events());
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
    let frame_id = frame["id"].as_i64().unwrap();
    let scopes = dispatch(
        &mut adapter,
        request(6, "scopes", json!({"frameId": frame_id})),
    );
    let reference = scopes[0]["body"]["scopes"][0]["variablesReference"]
        .as_i64()
        .unwrap();
    assert_eq!(reference, frame_id);
    assert!(
        dispatch(
            &mut adapter,
            request(7, "variables", json!({"variablesReference": reference}))
        )
        .is_empty()
    );
    assert!(matches!(
        state.borrow().commands.last(),
        Some(DebugCommand::Snapshot(include)) if include.fields == [0x00, 0x20]
    ));
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            locals: Some(schema::Locals {
                values: vec![schema::Value {
                    index: 3,
                    data: Some(schema::value::Data::I32Bits((-7_i32) as u32)),
                }],
            }),
            ..Default::default()
        }))));
    let variables = framed_values(adapter.pump_events());
    assert_eq!(variables[0]["body"]["variables"][1]["value"], "paused");
    assert_eq!(variables[0]["body"]["variables"][2]["name"], "local[3]");
    assert_eq!(variables[0]["body"]["variables"][2]["value"], "-7");
    assert_eq!(variables[0]["body"]["variables"][2]["type"], "i32");

    let stale = dispatch(&mut adapter, request(8, "scopes", json!({"frameId": 0})));
    assert_eq!(stale[0]["success"], false);
}

#[test]
fn callstack_frames_are_current_first_and_only_current_has_locals() {
    let (mut adapter, state, pcs) = acknowledged_adapter_for(callstack_path());
    let callee_pc = pcs[0];
    let caller_pc = *pcs.last().unwrap();
    stop_at(&mut adapter, &state, callee_pc);
    assert!(
        dispatch(
            &mut adapter,
            request(4, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            // Firmware order is oldest-to-current and includes control blocks.
            callstack: vec![
                schema::CallstackEntry {
                    function_index: 1,
                    ..Default::default()
                },
                schema::CallstackEntry {
                    r#type: 3, // loop
                    ..Default::default()
                },
                schema::CallstackEntry {
                    function_index: 0,
                    return_address: caller_pc,
                    ..Default::default()
                },
                schema::CallstackEntry {
                    r#type: 4, // if
                    ..Default::default()
                },
            ],
            ..Default::default()
        }))));
    let stack = framed_values(adapter.pump_events());
    let frames = stack[0]["body"]["stackFrames"].as_array().unwrap();
    assert_eq!(stack[0]["body"]["totalFrames"], 2);
    assert_eq!(frames[0]["name"], "callee");
    assert_eq!(frames[1]["name"], "caller");
    assert_ne!(frames[0]["id"], frames[1]["id"]);
    assert!(frames[0]["source"].is_object());
    assert!(frames[1]["source"].is_object());
    let caller_scope = dispatch(
        &mut adapter,
        request(5, "scopes", json!({"frameId": frames[1]["id"]})),
    );
    assert_eq!(caller_scope[0]["success"], false);
}

#[test]
fn concurrent_stack_traces_share_one_inspection() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    stop_at(&mut adapter, &state, pcs[0]);
    let snapshots_before = state
        .borrow()
        .commands
        .iter()
        .filter(|command| matches!(command, DebugCommand::Snapshot(_)))
        .count();

    assert!(
        dispatch(
            &mut adapter,
            request(4, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    assert!(
        dispatch(
            &mut adapter,
            request(5, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    assert_eq!(
        state
            .borrow()
            .commands
            .iter()
            .filter(|command| matches!(command, DebugCommand::Snapshot(_)))
            .count(),
        snapshots_before + 1
    );

    state
        .borrow_mut()
        .events
        .push_back(Ok(Some(DebugEvent::Snapshot(schema::Snapshot {
            callstack: vec![schema::CallstackEntry {
                function_index: 0,
                ..Default::default()
            }],
            ..Default::default()
        }))));
    let responses = framed_values(adapter.pump_events());
    assert_eq!(responses.len(), 2);
    assert!(responses.iter().all(|response| response["success"] == true));
    assert_eq!(responses[0]["request_seq"], 4);
    assert_eq!(responses[1]["request_seq"], 5);
}

#[test]
fn restart_and_run_control_cancel_a_retained_inspection() {
    let fake = FakeSession::default();
    let mut adapter = Adapter::new(FakeConnector(fake));
    let initialize = dispatch(&mut adapter, request(1, "initialize", json!({})));
    assert_eq!(initialize[0]["body"]["supportsRestartRequest"], true);
    // Exercise restart once attached and paused.
    let (mut adapter, state, pcs) = acknowledged_adapter();
    stop_at(&mut adapter, &state, pcs[0]);
    assert!(
        dispatch(
            &mut adapter,
            request(4, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    let concurrent = dispatch(
        &mut adapter,
        request(5, "stackTrace", json!({"threadId": 1})),
    );
    assert!(concurrent.is_empty());
    let restart = dispatch(&mut adapter, request(6, "restart", json!({})));
    assert!(
        restart
            .iter()
            .any(|message| message["request_seq"] == 4 && message["success"] == false)
    );
    assert!(
        restart
            .iter()
            .any(|message| message["request_seq"] == 5 && message["success"] == false)
    );
    assert!(
        restart
            .iter()
            .any(|message| message["request_seq"] == 6 && message["success"] == true)
    );
    assert!(matches!(
        state.borrow().commands.last(),
        Some(DebugCommand::Reset)
    ));
    drop(state);
}

#[test]
fn unanswered_inspection_times_out_once_and_can_be_retried() {
    let (mut adapter, state, pcs) = acknowledged_adapter();
    stop_at(&mut adapter, &state, pcs[0]);
    assert!(
        dispatch(
            &mut adapter,
            request(4, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
    thread::sleep(Duration::from_millis(1_050));
    let timeout = framed_values(adapter.pump_events());
    assert_eq!(timeout.len(), 1);
    assert_eq!(timeout[0]["request_seq"], 4);
    assert_eq!(timeout[0]["success"], false);
    // The stop cache survives the inspection timeout, so a later request is valid.
    assert!(
        dispatch(
            &mut adapter,
            request(5, "stackTrace", json!({"threadId": 1}))
        )
        .is_empty()
    );
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
    assert!(
        stop_at_event(
            &mut adapter,
            &state,
            pcs[0],
            DebugEvent::HitBreakpoint(schema::CodeLocation::default())
        )
        .is_empty()
    );
    let final_stop = stop_at_event(
        &mut adapter,
        &state,
        pcs[1],
        DebugEvent::HitBreakpoint(schema::CodeLocation::default()),
    );
    assert_eq!(final_stop.len(), 1);
    assert_eq!(final_stop[0]["event"], "stopped");
    assert_eq!(final_stop[0]["body"]["reason"], "step");
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
