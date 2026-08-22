use std::time::{Duration, Instant};

use debug::{DebugCommand, DebugEvent, DebugSession, ReceivedFrame, StopReason, Stopped};
use serde_json::{Value, json};

use crate::Request;

const THREAD_ID: i64 = 1;
const INSPECT_TIMEOUT: Duration = Duration::from_secs(1);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum AdapterState {
    AwaitingInitialize,
    Ready,
    Configuring,
    Attached,
    Disconnected,
}

#[derive(Debug, Default)]
pub struct AdapterOutput {
    pub messages: Vec<Value>,
    pub terminate: bool,
}

impl AdapterOutput {
    fn one(message: Value) -> Self {
        Self {
            messages: vec![message],
            terminate: false,
        }
    }
}

pub trait SessionConnector<S> {
    fn connect(&mut self, device: &str) -> Result<S, String>;
}

impl<S, F, E> SessionConnector<S> for F
where
    F: FnMut(&str) -> Result<S, E>,
    E: std::fmt::Display,
{
    fn connect(&mut self, device: &str) -> Result<S, String> {
        self(device).map_err(|error| error.to_string())
    }
}

pub struct Adapter<S, C> {
    session: Option<S>,
    connector: C,
    state: AdapterState,
    paused: bool,
    next_seq: u64,
    pending_attach: Option<Request>,
    snapshot: Option<debug::Snapshot>,
    pending_stop: Option<Stopped>,
    generation: i64,
    inspect_deadline: Option<Instant>,
    vm_frame_trace: bool,
}

impl<S: DebugSession, C: SessionConnector<S>> Adapter<S, C> {
    pub fn new(connector: C) -> Self {
        Self {
            session: None,
            connector,
            state: AdapterState::AwaitingInitialize,
            paused: false,
            next_seq: 1,
            pending_attach: None,
            snapshot: None,
            pending_stop: None,
            generation: 0,
            inspect_deadline: None,
            vm_frame_trace: false,
        }
    }

    pub fn handle_request(&mut self, request: Request) -> AdapterOutput {
        if request.message_type != "request" {
            return AdapterOutput::one(self.failure(&request, "expected a DAP request"));
        }
        match request.command.as_str() {
            "initialize" => self.initialize(request),
            "attach" => self.attach(request),
            "configurationDone" => self.configuration_done(request),
            "threads" => self.threads(request),
            "stackTrace" => self.stack_trace(request),
            "scopes" => self.scopes(request),
            "variables" => self.variables(request),
            "continue" => self.command(request, DebugCommand::Continue, true),
            "pause" => self.command(request, DebugCommand::Pause, false),
            "stepIn" => self.command(request, DebugCommand::Step, false),
            "next" => self.command(request, DebugCommand::StepOver, false),
            "restart" => self.restart(request),
            "terminate" => self.terminate_request(request),
            "disconnect" => self.disconnect(request),
            _ => AdapterOutput::one(self.failure(&request, "unsupported DAP request")),
        }
    }

    pub fn pump_events(&mut self) -> AdapterOutput {
        let mut output = AdapterOutput::default();
        if self
            .inspect_deadline
            .is_some_and(|deadline| Instant::now() >= deadline)
        {
            self.inspect_deadline = None;
            if let Some(stopped) = self.pending_stop.take() {
                output.messages.push(
                    self.output_event("WARDuino inspect timed out; paused state is unavailable"),
                );
                self.emit_stopped(&mut output, stopped);
            }
            return output;
        }
        loop {
            let event = match self.session.as_mut() {
                Some(session) => match session.try_recv() {
                    Ok(event) => event,
                    Err(error) => return self.debug_error(&mut output, error.to_string()),
                },
                None => return output,
            };
            match event {
                Some(frame) => {
                    self.emit_received_vm_frame(&mut output, &frame);
                    self.translate_event(frame.event, &mut output)
                }
                None => return output,
            }
            if output.terminate {
                return output;
            }
        }
    }

    fn initialize(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::AwaitingInitialize {
            return AdapterOutput::one(self.failure(&request, "initialize is only valid once"));
        }
        self.vm_frame_trace = request
            .arguments
            .get("warduinoVmFrame")
            .and_then(Value::as_bool)
            == Some(true);
        self.state = AdapterState::Ready;
        AdapterOutput::one(self.success(
            &request,
            json!({
                "supportsConfigurationDoneRequest": true,
                "supportsTerminateRequest": true,
                "supportsRestartRequest": true
            }),
        ))
    }

    fn attach(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Ready {
            return AdapterOutput::one(self.failure(&request, "attach requires initialize"));
        }
        let Some(device) = request
            .arguments
            .get("device")
            .and_then(Value::as_str)
            .filter(|device| !device.is_empty())
        else {
            return AdapterOutput::one(
                self.failure(&request, "attach requires a non-empty device"),
            );
        };
        let session = match self.connector.connect(device) {
            Ok(session) => session,
            Err(error) => return AdapterOutput::one(self.failure(&request, &error)),
        };
        self.session = Some(session);
        self.pending_attach = Some(request);
        self.state = AdapterState::Configuring;
        AdapterOutput::one(self.event("initialized", json!({})))
    }

    fn configuration_done(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Configuring {
            return AdapterOutput::one(self.failure(&request, "configurationDone requires attach"));
        }
        let Some(attach) = self.pending_attach.take() else {
            return AdapterOutput::one(self.failure(&request, "attach request is missing"));
        };
        self.state = AdapterState::Attached;
        let configuration_done = self.success(&request, json!({}));
        let attached = self.success(&attach, json!({}));
        AdapterOutput {
            messages: vec![configuration_done, attached],
            terminate: false,
        }
    }

    fn threads(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "threads requires an attached session"),
            );
        }
        AdapterOutput::one(self.success(
            &request,
            json!({"threads": [{"id": THREAD_ID, "name": "Waffyr VM"}]}),
        ))
    }

    fn stack_trace(&mut self, request: Request) -> AdapterOutput {
        if !has_synthetic_thread(&request.arguments) {
            return AdapterOutput::one(self.failure(&request, "unknown threadId"));
        }
        let Some(snapshot) = self.paused_snapshot(&request, "stackTrace") else {
            return AdapterOutput::one(
                self.failure(&request, "stackTrace requires a ready paused session"),
            );
        };
        AdapterOutput::one(self.success(
            &request,
            json!({
                "stackFrames": [{
                    "id": self.generation,
                    "name": "WARDuino",
                    "instructionPointerReference": format!("0x{:08x}", snapshot.program_counter.0)
                }],
                "totalFrames": 1
            }),
        ))
    }

    fn scopes(&mut self, request: Request) -> AdapterOutput {
        if self.paused_snapshot(&request, "scopes").is_none()
            || request.arguments.get("frameId").and_then(Value::as_i64) != Some(self.generation)
        {
            return AdapterOutput::one(self.failure(&request, "unknown or stale frameId"));
        }
        AdapterOutput::one(self.success(&request, json!({
            "scopes": [{"name": "VM", "variablesReference": self.generation, "expensive": false}]
        })))
    }

    fn variables(&mut self, request: Request) -> AdapterOutput {
        let Some(snapshot) = self.paused_snapshot(&request, "variables") else {
            return AdapterOutput::one(
                self.failure(&request, "variables requires a ready paused session"),
            );
        };
        if request
            .arguments
            .get("variablesReference")
            .and_then(Value::as_i64)
            != Some(self.generation)
        {
            return AdapterOutput::one(
                self.failure(&request, "unknown or stale variablesReference"),
            );
        }
        AdapterOutput::one(self.success(&request, json!({
            "variables": [
                {"name": "pc", "value": format!("0x{:08x}", snapshot.program_counter.0), "type": "u32", "variablesReference": 0},
                {"name": "state", "value": format!("{:?}", snapshot.state), "type": "WARDuino state", "variablesReference": 0}
            ]
        })))
    }

    fn paused_snapshot(&self, _request: &Request, _operation: &str) -> Option<&debug::Snapshot> {
        (self.state == AdapterState::Attached && self.paused).then_some(())?;
        self.snapshot.as_ref()
    }

    fn command(
        &mut self,
        request: Request,
        command: DebugCommand,
        continued: bool,
    ) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "request requires an attached session"),
            );
        }
        if !has_synthetic_thread(&request.arguments) {
            return AdapterOutput::one(self.failure(&request, "unknown threadId"));
        }
        let trace_command = command.clone();
        let send = self.session.as_mut().map(|session| session.send(command));
        match send {
            Some(Ok(receipt)) => {
                let body = if continued {
                    json!({"allThreadsContinued": true})
                } else {
                    json!({})
                };
                let mut output = AdapterOutput::one(self.success(&request, body));
                self.emit_vm_frame(&mut output, receipt, &trace_command);
                output
            }
            Some(Err(error)) => {
                let mut output = AdapterOutput::one(self.failure(&request, &error.to_string()));
                self.disconnect_after_error(&mut output, error.to_string());
                output
            }
            None => AdapterOutput::one(self.failure(&request, "debug session is disconnected")),
        }
    }

    fn restart(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "restart requires an attached session"),
            );
        }
        let send = self
            .session
            .as_mut()
            .map(|session| session.send(DebugCommand::Reset));
        match send {
            Some(Ok(receipt)) => {
                self.paused = true;
                let mut output = AdapterOutput {
                    messages: vec![
                        self.success(&request, json!({})),
                        self.event(
                            "stopped",
                            json!({"reason":"pause","threadId":THREAD_ID,"allThreadsStopped":true}),
                        ),
                    ],
                    terminate: false,
                };
                self.emit_vm_frame(&mut output, receipt, &DebugCommand::Reset);
                output
            }
            Some(Err(error)) => {
                let mut output = AdapterOutput::one(self.failure(&request, &error.to_string()));
                self.disconnect_after_error(&mut output, error.to_string());
                output
            }
            None => AdapterOutput::one(self.failure(&request, "debug session is disconnected")),
        }
    }

    fn terminate_request(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "terminate requires an attached session"),
            );
        }
        let send = self
            .session
            .as_mut()
            .map(|session| session.send(DebugCommand::Halt));
        match send {
            Some(Ok(receipt)) => {
                let mut output = AdapterOutput::one(self.success(&request, json!({})));
                self.emit_vm_frame(&mut output, receipt, &DebugCommand::Halt);
                output
            }
            Some(Err(error)) => {
                let mut output = AdapterOutput::one(self.failure(&request, &error.to_string()));
                self.disconnect_after_error(&mut output, error.to_string());
                output
            }
            None => AdapterOutput::one(self.failure(&request, "debug session is disconnected")),
        }
    }

    fn disconnect(&mut self, request: Request) -> AdapterOutput {
        if self.state == AdapterState::Disconnected {
            return AdapterOutput::one(self.failure(&request, "session is already disconnected"));
        }
        let pending_attach = self.pending_attach.take();
        let attached = self.state == AdapterState::Attached;
        let mut output = AdapterOutput::default();
        if attached && self.paused {
            match self
                .session
                .as_mut()
                .map(|session| session.send(DebugCommand::Continue))
            {
                Some(Ok(receipt)) => {
                    self.emit_vm_frame(&mut output, receipt, &DebugCommand::Continue)
                }
                Some(Err(error)) => output.messages.push(self.output_event(&error.to_string())),
                None => {}
            }
        }
        self.session.take();
        self.state = AdapterState::Disconnected;
        if let Some(attach) = pending_attach {
            output
                .messages
                .push(self.failure(&attach, "attach cancelled by disconnect"));
        }
        output.messages.push(self.success(&request, json!({})));
        output.messages.push(self.event("terminated", json!({})));
        output.terminate = true;
        output
    }

    fn emit_received_vm_frame(&mut self, output: &mut AdapterOutput, frame: &ReceivedFrame) {
        if self.vm_frame_trace {
            output.messages.push(self.event("warduino/vmFrame", json!({"direction":"incoming", "bytes":frame.bytes(), "command": debug_event_name(&frame.event), "fields": debug_event_fields(&frame.event)})));
        }
    }

    fn translate_event(&mut self, event: DebugEvent, output: &mut AdapterOutput) {
        match event {
            DebugEvent::Continued => {
                self.paused = false;
                self.snapshot = None;
                self.pending_stop = None;
                self.inspect_deadline = None;
                output.messages.push(self.event(
                    "continued",
                    json!({"threadId": THREAD_ID, "allThreadsContinued": true}),
                ));
            }
            DebugEvent::Stopped(stopped) => {
                self.paused = true;
                self.snapshot = None;
                self.pending_stop = Some(stopped);
                self.inspect_deadline = Some(Instant::now() + INSPECT_TIMEOUT);
                self.generation += 1;
                match self
                    .session
                    .as_mut()
                    .map(|session| session.send(DebugCommand::Inspect(Vec::new())))
                {
                    Some(Ok(receipt)) => {
                        self.emit_vm_frame(output, receipt, &DebugCommand::Inspect(Vec::new()))
                    }
                    Some(Err(error)) => self.disconnect_after_error(output, error.to_string()),
                    None => {}
                }
            }
            DebugEvent::Snapshot(snapshot) if self.pending_stop.is_some() => {
                self.inspect_deadline = None;
                self.snapshot = Some(snapshot);
                if let Some(stopped) = self.pending_stop.take() {
                    self.emit_stopped(output, stopped);
                }
            }
            DebugEvent::Halted | DebugEvent::Disconnected(_) => self.terminate(output),
            DebugEvent::OperationResult(result) if !result.success => self.remote_failure(
                output,
                format!(
                    "WARDuino target rejected {:?} command asynchronously",
                    result.command
                ),
            ),
            DebugEvent::TargetMalformedCommand => self.remote_failure(
                output,
                "WARDuino target rejected a malformed command asynchronously".into(),
            ),
            DebugEvent::TargetUnknownCommand => self.remote_failure(
                output,
                "WARDuino target rejected an unknown command asynchronously".into(),
            ),
            _ => {}
        }
    }

    fn emit_stopped(&mut self, output: &mut AdapterOutput, stopped: Stopped) {
        let reason = match stopped.reason {
            StopReason::Pause => "pause",
            StopReason::Step => "step",
            StopReason::Breakpoint => "breakpoint",
        };
        output.messages.push(self.event(
            "stopped",
            json!({"reason": reason, "threadId": THREAD_ID, "allThreadsStopped": true}),
        ));
    }

    fn remote_failure(&mut self, output: &mut AdapterOutput, message: String) {
        output.messages.push(self.output_event(&message));
    }

    fn terminate(&mut self, output: &mut AdapterOutput) {
        output.messages.push(self.event("terminated", json!({})));
        self.session.take();
        self.state = AdapterState::Disconnected;
        output.terminate = true;
    }

    fn emit_vm_frame(
        &mut self,
        output: &mut AdapterOutput,
        frame: debug::SentFrame,
        command: &DebugCommand,
    ) {
        if self.vm_frame_trace {
            output.messages.push(self.event(
                "warduino/vmFrame",
                json!({
                    "direction": "outgoing",
                    "command": debug_command_name(command),
                    "bytes": frame.bytes(),
                    "fields": debug_command_fields(command),
                }),
            ));
        }
    }

    fn debug_error(&mut self, output: &mut AdapterOutput, message: String) -> AdapterOutput {
        self.disconnect_after_error(output, message);
        std::mem::take(output)
    }

    fn disconnect_after_error(&mut self, output: &mut AdapterOutput, message: String) {
        output.messages.push(self.output_event(&message));
        output.messages.push(self.event("terminated", json!({})));
        self.session.take();
        self.state = AdapterState::Disconnected;
        output.terminate = true;
    }

    fn success(&mut self, request: &Request, body: Value) -> Value {
        self.response(request, true, body, None)
    }

    fn failure(&mut self, request: &Request, message: &str) -> Value {
        self.response(request, false, json!({}), Some(message))
    }

    fn response(
        &mut self,
        request: &Request,
        success: bool,
        body: Value,
        message: Option<&str>,
    ) -> Value {
        let mut response = json!({
            "seq": self.sequence(),
            "type": "response",
            "request_seq": request.seq,
            "success": success,
            "command": request.command,
            "body": body,
        });
        if let Some(message) = message {
            response["message"] = json!(message);
        }
        response
    }

    fn event(&mut self, name: &str, body: Value) -> Value {
        json!({
            "seq": self.sequence(),
            "type": "event",
            "event": name,
            "body": body,
        })
    }

    fn output_event(&mut self, message: &str) -> Value {
        self.event(
            "output",
            json!({"category": "stderr", "output": format!("{message}\n")}),
        )
    }

    fn sequence(&mut self) -> u64 {
        let sequence = self.next_seq;
        self.next_seq += 1;
        sequence
    }
}

fn debug_command_name(command: &DebugCommand) -> &'static str {
    match command {
        DebugCommand::Continue => "continue",
        DebugCommand::Halt => "halt",
        DebugCommand::Pause => "pause",
        DebugCommand::Step => "step",
        DebugCommand::StepOver => "next",
        DebugCommand::ContinueFor(_) => "continue",
        DebugCommand::AddBreakpoint(_) => "setBreakpoint",
        DebugCommand::RemoveBreakpoint(_) => "removeBreakpoint",
        DebugCommand::RequestSnapshot => "snapshot",
        DebugCommand::Inspect(_) => "inspect",
        DebugCommand::Reset => "reset",
        _ => "unknown",
    }
}

fn debug_command_fields(command: &DebugCommand) -> Value {
    match command {
        DebugCommand::ContinueFor(count) => json!({"count": count}),
        DebugCommand::AddBreakpoint(location) | DebugCommand::RemoveBreakpoint(location) => {
            json!({"module": location.module.0, "pc": location.program_counter.0})
        }
        DebugCommand::Inspect(state) => json!({"state": state}),
        _ => json!({}),
    }
}

fn debug_event_name(event: &DebugEvent) -> &'static str {
    match event {
        DebugEvent::Stopped(_) => "stopped",
        DebugEvent::Continued => "continued",
        DebugEvent::Halted => "halted",
        DebugEvent::Snapshot(_) => "snapshot",
        DebugEvent::OperationResult(_) => "operationResult",
        DebugEvent::TargetMalformedCommand => "targetMalformedCommand",
        DebugEvent::TargetUnknownCommand => "targetUnknownCommand",
        DebugEvent::Disconnected(_) => "disconnected",
        _ => "unknown",
    }
}

fn debug_event_fields(event: &DebugEvent) -> Value {
    match event {
        DebugEvent::Stopped(stopped) => {
            json!({"reason": format!("{:?}", stopped.reason), "pc": stopped.location.map(|location| location.program_counter.0)})
        }
        DebugEvent::Snapshot(snapshot) => {
            json!({"pc": snapshot.program_counter.0, "state": format!("{:?}", snapshot.state)})
        }
        DebugEvent::OperationResult(result) => {
            json!({"success": result.success, "command": format!("{:?}", result.command)})
        }
        _ => json!({}),
    }
}

fn has_synthetic_thread(arguments: &Value) -> bool {
    match arguments.get("threadId") {
        Some(thread_id) => thread_id.as_i64() == Some(THREAD_ID),
        None => false,
    }
}
