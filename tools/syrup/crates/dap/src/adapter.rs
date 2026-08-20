use debug::{DebugCommand, DebugEvent, DebugSession, StopReason};
use serde_json::{Value, json};

use crate::Request;

const THREAD_ID: i64 = 1;

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

pub struct Adapter<S> {
    session: Option<S>,
    state: AdapterState,
    next_seq: u64,
    pending_attach: Option<Request>,
}

impl<S: DebugSession> Adapter<S> {
    pub fn new(session: S) -> Self {
        Self {
            session: Some(session),
            state: AdapterState::AwaitingInitialize,
            next_seq: 1,
            pending_attach: None,
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
            "continue" => self.command(request, DebugCommand::Continue, true, false),
            "pause" => self.command(request, DebugCommand::Pause, false, true),
            "disconnect" => self.disconnect(request),
            _ => AdapterOutput::one(self.failure(&request, "unsupported DAP request")),
        }
    }

    pub fn pump_events(&mut self) -> AdapterOutput {
        let mut output = AdapterOutput::default();
        loop {
            let event = match self.session.as_mut() {
                Some(session) => match session.try_recv() {
                    Ok(event) => event,
                    Err(error) => return self.debug_error(&mut output, error.to_string()),
                },
                None => return output,
            };
            match event {
                Some(event) => self.translate_event(event, &mut output),
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
        self.state = AdapterState::Ready;
        AdapterOutput::one(
            self.success(&request, json!({"supportsConfigurationDoneRequest": true})),
        )
    }

    fn attach(&mut self, request: Request) -> AdapterOutput {
        if self.state != AdapterState::Ready {
            return AdapterOutput::one(self.failure(&request, "attach requires initialize"));
        }
        if self.session.is_none() {
            return AdapterOutput::one(self.failure(&request, "debug session is disconnected"));
        }
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

    fn command(
        &mut self,
        request: Request,
        command: DebugCommand,
        continued: bool,
        require_thread_id: bool,
    ) -> AdapterOutput {
        if self.state != AdapterState::Attached {
            return AdapterOutput::one(
                self.failure(&request, "request requires an attached session"),
            );
        }
        if !has_synthetic_thread(&request.arguments, require_thread_id) {
            return AdapterOutput::one(self.failure(&request, "unknown threadId"));
        }
        let send = match self.session.as_mut() {
            Some(session) => session.send(command),
            None => {
                return AdapterOutput::one(self.failure(&request, "debug session is disconnected"));
            }
        };
        match send {
            Ok(()) => {
                let body = if continued {
                    json!({"allThreadsContinued": true})
                } else {
                    json!({})
                };
                AdapterOutput::one(self.success(&request, body))
            }
            Err(error) => {
                let mut output = AdapterOutput::one(self.failure(&request, &error.to_string()));
                self.disconnect_after_error(&mut output, error.to_string());
                output
            }
        }
    }

    fn disconnect(&mut self, request: Request) -> AdapterOutput {
        if self.state == AdapterState::Disconnected {
            return AdapterOutput::one(self.failure(&request, "session is already disconnected"));
        }
        let pending_attach = self.pending_attach.take();
        self.session.take();
        self.state = AdapterState::Disconnected;
        let mut messages = Vec::new();
        if let Some(attach) = pending_attach {
            messages.push(self.failure(&attach, "attach cancelled by disconnect"));
        }
        messages.push(self.success(&request, json!({})));
        AdapterOutput {
            messages,
            terminate: true,
        }
    }

    fn translate_event(&mut self, event: DebugEvent, output: &mut AdapterOutput) {
        match event {
            DebugEvent::Continued => output.messages.push(self.event(
                "continued",
                json!({"threadId": THREAD_ID, "allThreadsContinued": true}),
            )),
            DebugEvent::Stopped(stopped) => {
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

    fn remote_failure(&mut self, output: &mut AdapterOutput, message: String) {
        output.messages.push(self.output_event(&message));
    }

    fn terminate(&mut self, output: &mut AdapterOutput) {
        output.messages.push(self.event("terminated", json!({})));
        self.session.take();
        self.state = AdapterState::Disconnected;
        output.terminate = true;
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

fn has_synthetic_thread(arguments: &Value, required: bool) -> bool {
    match arguments.get("threadId") {
        Some(thread_id) => thread_id.as_i64() == Some(THREAD_ID),
        None => !required,
    }
}
