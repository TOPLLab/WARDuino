//! DAP adapter translating debugger requests into WARDuino VM commands.

use std::{
    path::Path,
    time::{Duration, Instant},
};

use debug::{DebugCommand, DebugEvent, DebugSession, ReceivedFrame, StopReason, Stopped};
use serde_json::{Value, json};

use crate::{
    Request,
    source::{MappedFrame, ProgramImage, SourceLocation},
};

const THREAD_ID: i64 = 1;
const SOURCE_REFERENCE: i64 = 1;
const INSPECT_TIMEOUT: Duration = Duration::from_secs(1);
const OPERATION_TIMEOUT: Duration = Duration::from_secs(5);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum AdapterState {
    AwaitingInitialize,
    Ready,
    AwaitingModuleUpdate,
    Configuring,
    Attached,
    Disconnected,
}

#[derive(Clone, Debug)]
struct Step {
    kind: StepKind,
    location: SourceLocation,  // current location before step
    deadline: Instant,
}

#[derive(Clone, Copy, Debug)]
enum StepKind {
    Step,
    StepOver,
}

impl StepKind {
    fn command(self) -> DebugCommand {
        match self {
            Self::Step => DebugCommand::Step,
            Self::StepOver => DebugCommand::StepOver,
        }
    }

    fn accepts_stop(self, reason: StopReason) -> bool {
        match self {
            Self::Step => reason == StopReason::Step,
            Self::StepOver => matches!(reason, StopReason::Step | StopReason::Breakpoint),
        }
    }
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
    upload_deadline: Option<Instant>,
    source_step: Option<Step>,
    image: Option<ProgramImage>,
    source_path: Option<String>,
    vm_frame_trace: bool,
}

impl<S: DebugSession, C: SessionConnector<S>> Adapter<S, C> {
    /// Creates an adapter for a single debug session (and connection).
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
            upload_deadline: None,
            source_step: None,
            image: None,
            source_path: None,
            vm_frame_trace: false,
        }
    }

    /// Handles one incoming DAP request.
    pub fn handle_request(&mut self, request: Request) -> AdapterOutput {
        match request.command.as_str() {
            "initialize" => self.initialize(request),
            "attach" => self.attach(request),
            "configurationDone" => self.configuration_done(request),
            "threads" => self.threads(request),
            "stackTrace" => self.stack_trace(request),
            "source" => self.source(request),
            "scopes" => self.scopes(request),
            "variables" => self.variables(request),
            "continue" => self.command(request, DebugCommand::Continue, true),
            "pause" => self.command(request, DebugCommand::Pause, false),
            "stepIn" => self.source_step(request, StepKind::Step),
            "next" => self.source_step(request, StepKind::StepOver),
            "terminate" => self.terminate_request(request),
            "disconnect" => self.disconnect(request),
            _ => AdapterOutput::one(self.failure(&request, "unsupported DAP request")),
        }
    }

    /// Polls the debug session and translates pending events.
    pub fn pump_events(&mut self) -> AdapterOutput {
        let mut output = AdapterOutput::default();
        let now = Instant::now();
        if self.upload_deadline.is_some_and(|deadline| now >= deadline) {
            self.fail_attach(&mut output, "module update acknowledgement timed out");
            return output;
        }
        if self
            .inspect_deadline
            .is_some_and(|deadline| now >= deadline)
        {
            self.inspect_deadline = None;
            if let Some(stopped) = self.pending_stop.take() {
                output.messages.push(
                    self.output_event("WARDuino inspect timed out; paused state is unavailable"),
                );
                self.finish_stop(&mut output, stopped);
            }
            return output;
        }
        if self
            .source_step
            .as_ref()
            .is_some_and(|step| now >= step.deadline)
        {
            self.source_step = None;
            output.messages.push(self.output_event(
                "WARDuino source step timed out before reaching a new source location",
            ));
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
                    self.translate_event(frame.event, &mut output);
                }
                None => return output,
            }
            if output.terminate {
                return output;
            }
        }
    }

    /// Negotiates capabilities with the DAP client.
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
                "supportsTerminateRequest": true
            }),
        ))
    }

    /// Connects to the target and uploads the program image.
    fn attach(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Ready {
            return AdapterOutput::one(self.failure(&request, "attach requires initialize"));
        }
        let Some(device) = request
            .arguments
            .get("device")
            .and_then(Value::as_str)
            .filter(|value| !value.is_empty())
        else {
            return AdapterOutput::one(
                self.failure(&request, "attach requires a non-empty device"),
            );
        };
        let Some(program) = request
            .arguments
            .get("program")
            .and_then(Value::as_str)
            .filter(|value| !value.is_empty())
        else {
            return AdapterOutput::one(
                self.failure(&request, "attach requires a non-empty WAT program path"),
            );
        };
        let image = match ProgramImage::from_path(Path::new(program)) {
            Ok(image) => image,
            Err(error) => return AdapterOutput::one(self.failure(&request, &error)),
        };
        let mut session = match self.connector.connect(device) {
            Ok(session) => session,
            Err(error) => return AdapterOutput::one(self.failure(&request, &error)),
        };
        let upload = DebugCommand::UpdateModule(image.wasm().to_vec());
        let receipt = match session.send(upload.clone()) {
            Ok(receipt) => receipt,
            Err(error) => return AdapterOutput::one(self.failure(&request, &error.to_string())),
        };
        self.session = Some(session);
        self.image = Some(image);
        self.source_path = Some(program.to_owned());
        self.pending_attach = Some(request);
        self.state = AdapterState::AwaitingModuleUpdate;
        self.upload_deadline = Some(Instant::now() + OPERATION_TIMEOUT);
        let mut output = AdapterOutput::default();
        self.emit_vm_frame(
            &mut output,
            receipt,
            &DebugCommand::UpdateModule(Vec::new()),
        );
        output
    }

    /// Completes attachment after client configuration.
    fn configuration_done(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Configuring {
            return AdapterOutput::one(self.failure(
                &request,
                "configurationDone requires an acknowledged attach",
            ));
        }
        let Some(attach) = self.pending_attach.take() else {
            return AdapterOutput::one(self.failure(&request, "attach request is missing"));
        };
        self.state = AdapterState::Attached;
        AdapterOutput {
            messages: vec![
                self.success(&request, json!({})),
                self.success(&attach, json!({})),
            ],
            terminate: false,
        }
    }

    /// Returns the synthetic debug thread.
    fn threads(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "threads requires an attached session"),
            );
        }
        AdapterOutput::one(self.success(
            &request,
            json!({"threads": [{"id": THREAD_ID, "name": "WARDuino VM"}]}),
        ))
    }

    /// Returns the current mapped stack frame.
    fn stack_trace(&mut self, request: Request) -> AdapterOutput {
        if !has_synthetic_thread(&request.arguments) {
            return AdapterOutput::one(self.failure(&request, "unknown threadId"));
        }
        let Some(snapshot) = self.paused_snapshot() else {
            return AdapterOutput::one(
                self.failure(&request, "stackTrace requires a ready paused session"),
            );
        };
        let Some(image) = self.image.as_ref() else {
            return AdapterOutput::one(
                self.failure(&request, "no uploaded program image is available"),
            );
        };
        let Some(frame) = image.frame_at(snapshot.program_counter.0) else {
            return AdapterOutput::one(self.failure(
                &request,
                "current program counter is not mapped to the uploaded WAT",
            ));
        };
        let path = self.source_path.as_deref().unwrap_or(image.source_name());
        AdapterOutput::one(self.success(&request, json!({
            "stackFrames": [{
                "id": self.generation,
                "name": frame.function,
                "source": {"name": image.source_name(), "path": path, "sourceReference": SOURCE_REFERENCE},
                "line": frame.location.line,
                "column": frame.location.column,
                "instructionPointerReference": format!("0x{:08x}", snapshot.program_counter.0)
            }],
            "totalFrames": 1
        })))
    }

    /// Returns the uploaded source text.
    fn source(&mut self, request: Request) -> AdapterOutput {
        if request
            .arguments
            .get("sourceReference")
            .and_then(Value::as_i64)
            != Some(SOURCE_REFERENCE)
        {
            return AdapterOutput::one(self.failure(&request, "unknown sourceReference"));
        }
        let Some(image) = self.image.as_ref() else {
            return AdapterOutput::one(
                self.failure(&request, "no uploaded program source is available"),
            );
        };
        AdapterOutput::one(self.success(
            &request,
            json!({"content": image.source(), "mimeType": "text/plain"}),
        ))
    }

    /// Returns scopes for the current frame.
    fn scopes(&mut self, request: Request) -> AdapterOutput {
        if self.paused_snapshot().is_none()
            || request.arguments.get("frameId").and_then(Value::as_i64) != Some(self.generation)
        {
            return AdapterOutput::one(self.failure(&request, "unknown or stale frameId"));
        }
        AdapterOutput::one(self.success(&request, json!({"scopes": [{"name": "VM", "variablesReference": self.generation, "expensive": false}]})))
    }

    /// Returns variables for the current frame.
    fn variables(&mut self, request: Request) -> AdapterOutput {
        let Some(snapshot) = self.paused_snapshot() else {
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
        AdapterOutput::one(self.success(&request, json!({"variables": [
            {"name": "pc", "value": format!("0x{:08x}", snapshot.program_counter.0), "type": "u32", "variablesReference": 0},
            {"name": "state", "value": format!("{:?}", snapshot.state), "type": "WARDuino state", "variablesReference": 0}
        ]})))
    }

    /// Starts a source-level stepping operation.
    fn source_step(&mut self, request: Request, kind: StepKind) -> AdapterOutput {
        if request.arguments.get("granularity").and_then(Value::as_str) == Some("instruction") {
            return self.command(request, kind.command(), false);
        }
        if self.state != AdapterState::Attached || !has_synthetic_thread(&request.arguments) {
            return AdapterOutput::one(self.failure(
                &request,
                "source step requires an attached synthetic thread",
            ));
        }
        let Some(snapshot) = self.paused_snapshot() else {
            return AdapterOutput::one(
                self.failure(&request, "source step requires a ready paused session"),
            );
        };
        let Some(location) = self
            .image
            .as_ref()
            .and_then(|image| image.frame_at(snapshot.program_counter.0))
            .map(|frame| frame.location)
        else {
            return AdapterOutput::one(self.failure(
                &request,
                "cannot source-step from an unmapped program counter",
            ));
        };
        let command = kind.command();
        let send = self
            .session
            .as_mut()
            .map(|session| session.send(command.clone()));
        match send {
            Some(Ok(receipt)) => {
                self.paused = false;
                self.snapshot = None;
                self.source_step = Some(Step {
                    kind,
                    location,
                    deadline: Instant::now() + OPERATION_TIMEOUT,
                });
                let mut output = AdapterOutput::one(self.success(&request, json!({})));
                self.emit_vm_frame(&mut output, receipt, &command);
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

    /// Returns the snapshot when the adapter is paused and attached.
    fn paused_snapshot(&self) -> Option<&debug::Snapshot> {
        (self.state == AdapterState::Attached && self.paused).then_some(())?;
        self.snapshot.as_ref()
    }

    /// Sends a command to the debug session.
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
                if continued {
                    self.paused = false;
                    self.snapshot = None;
                }
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

    /// Halts the target in response to a terminate request.
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

    /// Disconnects from the target and terminates the adapter.
    fn disconnect(&mut self, request: Request) -> AdapterOutput {
        if self.state == AdapterState::Disconnected {
            return AdapterOutput::one(self.failure(&request, "session is already disconnected"));
        }
        let pending_attach = self.pending_attach.take();
        let mut output = AdapterOutput::default();
        if self.state == AdapterState::Attached
            && self.paused
            && let Some(Ok(receipt)) = self
                .session
                .as_mut()
                .map(|session| session.send(DebugCommand::Continue))
        {
            self.emit_vm_frame(&mut output, receipt, &DebugCommand::Continue);
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

    /// Emits optional tracing for an incoming VM frame.
    fn emit_received_vm_frame(&mut self, output: &mut AdapterOutput, frame: &ReceivedFrame) {
        if self.vm_frame_trace {
            output.messages.push(self.event("warduino/vmFrame", json!({"direction":"incoming", "bytes":frame.bytes(), "command":debug_event_name(&frame.event), "fields":debug_event_fields(&frame.event)})));
        }
    }

    /// Applies a debug event and emits its DAP messages.
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
                    self.finish_stop(output, stopped);
                }
            }
            DebugEvent::OperationResult(result)
                if result.command == debug::CommandKind::UpdateModule
                    && self.state == AdapterState::AwaitingModuleUpdate =>
            {
                self.upload_deadline = None;
                if result.success {
                    self.state = AdapterState::Configuring;
                    output.messages.push(self.event("initialized", json!({})));
                } else {
                    self.fail_attach(output, "WARDuino target rejected module update");
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

    /// Determines whether a source step should continue.
    fn should_continue(&mut self, step: &Step, stopped: Stopped) -> bool {
        step.kind.accepts_stop(stopped.reason)
            && self
                .mapped()
                .as_ref()
                .is_some_and(|frame| frame.location == step.location)
            && Instant::now() < step.deadline
    }

    /// Maps the current program counter to source.
    fn mapped(&mut self) -> Option<MappedFrame> {
        self.snapshot.as_ref().and_then(|snapshot| {
            self.image
                .as_ref()
                .and_then(|image| image.frame_at(snapshot.program_counter.0))
        })
    }

    /// Completes processing of a stopped target.
    fn finish_stop(&mut self, output: &mut AdapterOutput, mut stopped: Stopped) {
        let Some(step) = self.source_step.clone() else {
            self.emit_stopped(output, stopped);
            return;
        };

        if self.should_continue(&step, stopped) {
            let command = step.kind.command();
            let send = self
                .session
                .as_mut()
                .map(|session| session.send(command.clone()));
            match send {
                Some(Ok(receipt)) => {
                    self.paused = false;
                    self.snapshot = None;
                    self.emit_vm_frame(output, receipt, &command);
                    return;
                }
                Some(Err(error)) => {
                    self.source_step = None;
                    self.disconnect_after_error(output, error.to_string());
                    return;
                }
                None => {}
            }
        }
        if self.mapped().is_none() {
            output.messages.push(
                self.output_event("WARDuino source step stopped at an unmapped program counter"),
            );
        } else if self.should_continue(&step, stopped) {
            output.messages.push(
                self.output_event("WARDuino source step was interrupted by an unrelated stop"),
            );
        } else if Instant::now() >= step.deadline {
            output.messages.push(self.output_event(
                "WARDuino source step timed out before reaching a new source location",
            ));
        }
        self.source_step = None;
        if !self.should_continue(&step, stopped) && stopped.reason == StopReason::Breakpoint {
            stopped.reason = StopReason::Step;
        }
        self.emit_stopped(output, stopped);
    }

    /// Emits a DAP stopped event.
    fn emit_stopped(&mut self, output: &mut AdapterOutput, stopped: Stopped) {
        self.generation += 1;
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

    /// Reports an attachment failure and terminates.
    fn fail_attach(&mut self, output: &mut AdapterOutput, message: &str) {
        if let Some(attach) = self.pending_attach.take() {
            output.messages.push(self.failure(&attach, message));
        }
        output.messages.push(self.output_event(message));
        self.terminate(output);
    }

    /// Reports an asynchronous target failure.
    fn remote_failure(&mut self, output: &mut AdapterOutput, message: String) {
        output.messages.push(self.output_event(&message));
    }

    /// Emits termination and closes the session.
    fn terminate(&mut self, output: &mut AdapterOutput) {
        if self.state == AdapterState::Disconnected {
            return;
        }
        output.messages.push(self.event("terminated", json!({})));
        self.session.take();
        self.state = AdapterState::Disconnected;
        output.terminate = true;
    }

    /// Emits optional tracing for an outgoing VM frame.
    fn emit_vm_frame(
        &mut self,
        output: &mut AdapterOutput,
        frame: debug::SentFrame,
        command: &DebugCommand,
    ) {
        if self.vm_frame_trace {
            output.messages.push(self.event("warduino/vmFrame", json!({"direction":"outgoing", "command":debug_command_name(command), "bytes":frame.bytes(), "fields":debug_command_fields(command)})));
        }
    }

    /// Converts a debug-session error into adapter output.
    fn debug_error(&mut self, output: &mut AdapterOutput, message: String) -> AdapterOutput {
        self.disconnect_after_error(output, message);
        std::mem::take(output)
    }

    /// Disconnects after a fatal debug-session error.
    fn disconnect_after_error(&mut self, output: &mut AdapterOutput, message: String) {
        output.messages.push(self.output_event(&message));
        self.terminate(output);
    }

    /// Builds a successful DAP response.
    fn success(&mut self, request: &Request, body: Value) -> Value {
        self.response(request, true, body, None)
    }
    /// Builds a failed DAP response.
    fn failure(&mut self, request: &Request, message: &str) -> Value {
        self.response(request, false, json!({}), Some(message))
    }

    /// Builds a DAP response envelope.
    fn response(
        &mut self,
        request: &Request,
        success: bool,
        body: Value,
        message: Option<&str>,
    ) -> Value {
        let mut response = json!({"seq": self.sequence(), "type": "response", "request_seq": request.seq, "success": success, "command": request.command, "body": body});
        if let Some(message) = message {
            response["message"] = json!(message);
        }
        response
    }

    /// Builds a DAP event envelope.
    fn event(&mut self, name: &str, body: Value) -> Value {
        json!({"seq": self.sequence(), "type": "event", "event": name, "body": body})
    }
    /// Builds an adapter output event.
    fn output_event(&mut self, message: &str) -> Value {
        self.event(
            "output",
            json!({"category": "stderr", "output": format!("{message}\n")}),
        )
    }
    /// Allocates the next DAP message sequence number.
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
        DebugCommand::UpdateModule(_) => "updateModule",
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
        DebugCommand::UpdateModule(wasm) => json!({"bytes": wasm.len()}),
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
    arguments.get("threadId").and_then(Value::as_i64) == Some(THREAD_ID)
}
