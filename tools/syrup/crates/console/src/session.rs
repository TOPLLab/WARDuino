use serde_json::{Value, json};

use dap::{AdapterOutput, Request, WarduinoAdapter, warduino_adapter};

use crate::app::{
    App, CommandIntent, Direction, EntryPayload, EntryType, NamedValue, SessionEntry, StackFrame,
    StopContext, VmState,
};

pub struct Session {
    adapter: WarduinoAdapter,
    seq: u64,
    thread_id: i64,
    closed: bool,
}

impl Session {
    pub fn connect(device: &str, app: &mut App) -> Result<Self, String> {
        let mut session = Self {
            adapter: warduino_adapter(),
            seq: 1,
            thread_id: 1,
            closed: false,
        };
        session.request(
            "initialize",
            json!({"adapterID": "syrup", "warduinoVmFrame": true}),
            app,
        )?;
        let (attach_seq, output) = session.send("attach", json!({"device": device}), app);
        if let Some(response) = output
            .messages
            .iter()
            .find(|message| message["request_seq"] == attach_seq)
        {
            let error = response["message"]
                .as_str()
                .unwrap_or("attach failed")
                .to_owned();
            session.apply(output, app);
            return Err(error);
        }
        session.apply(output, app);
        session.request("configurationDone", json!({}), app)?;
        let threads = session.request("threads", json!({}), app)?;
        session.thread_id = threads
            .pointer("/threads/0/id")
            .and_then(Value::as_i64)
            .ok_or("adapter returned no thread")?;
        Ok(session)
    }

    pub fn dispatch(&mut self, intent: CommandIntent, app: &mut App) {
        let (command, arguments) = match intent {
            CommandIntent::Continue => ("continue", json!({"threadId": self.thread_id})),
            CommandIntent::Pause => ("pause", json!({"threadId": self.thread_id})),
            CommandIntent::Step => ("stepIn", json!({"threadId": self.thread_id})),
            CommandIntent::Next => ("next", json!({"threadId": self.thread_id})),
            CommandIntent::Restart => ("restart", json!({})),
            CommandIntent::Terminate => ("terminate", json!({})),
        };
        if let Err(error) = self.request(command, arguments, app) {
            app.notice = Some(error);
        }
    }

    pub fn poll(&mut self, app: &mut App) {
        let output = self.adapter.pump_events();
        self.apply(output, app);
    }

    pub fn disconnect(&mut self, app: &mut App) {
        if !self.closed {
            let _ = self.request("disconnect", json!({}), app);
            self.closed = true;
        }
    }

    fn request(&mut self, command: &str, arguments: Value, app: &mut App) -> Result<Value, String> {
        let (seq, output) = self.send(command, arguments, app);
        let response = output
            .messages
            .iter()
            .find(|message| message["type"] == "response" && message["request_seq"] == seq)
            .cloned();
        self.apply(output, app);
        let response = response.ok_or_else(|| format!("{command}: no response"))?;
        if response["success"] != true {
            return Err(response["message"]
                .as_str()
                .unwrap_or("request failed")
                .into());
        }
        Ok(response["body"].clone())
    }

    /// Reads paused-state data without adding three bookkeeping rows to the
    /// timeline. The resulting text belongs to the `stopped` event instead.
    fn inspection_request(&mut self, command: &str, arguments: Value) -> Result<Value, String> {
        let seq = self.seq;
        self.seq += 1;
        let output = self.adapter.handle_request(Request {
            seq,
            message_type: "request".into(),
            command: command.into(),
            arguments,
        });
        let response = output
            .messages
            .iter()
            .find(|message| message["type"] == "response" && message["request_seq"] == seq)
            .ok_or_else(|| format!("{command}: no response"))?;
        if response["success"] != true {
            return Err(format!(
                "{command}: {}",
                response["message"].as_str().unwrap_or("request failed")
            ));
        }
        Ok(response["body"].clone())
    }

    fn send(&mut self, command: &str, arguments: Value, app: &mut App) -> (u64, AdapterOutput) {
        let seq = self.seq;
        self.seq += 1;
        app.append(entry(
            seq,
            Direction::Outgoing,
            command,
            EntryType::DapRequest,
            EntryPayload::DapRequest {
                arguments: arguments.clone(),
            },
        ));
        let request = Request {
            seq,
            message_type: "request".into(),
            command: command.into(),
            arguments,
        };
        let output = self.adapter.handle_request(request);
        (seq, output)
    }

    fn apply(&mut self, output: AdapterOutput, app: &mut App) {
        for message in output.messages {
            let kind = message["event"].as_str().unwrap_or("response");
            if message["type"] == "response" {
                app.append(entry(
                    message["seq"].as_u64().unwrap_or(0),
                    Direction::Incoming,
                    message["command"].as_str().unwrap_or(kind),
                    EntryType::DapResponse,
                    EntryPayload::DapResponse {
                        success: message["success"].as_bool().unwrap_or(false),
                        body: message["body"].clone(),
                        message: message["message"].as_str().map(str::to_owned),
                    },
                ));
                continue;
            }
            if message["type"] != "event" {
                continue;
            }
            if kind == "warduino/vmFrame" {
                match parse_vm_frame(&message["body"]["bytes"]) {
                    Ok(bytes) => {
                        let direction = if message["body"]["direction"] == "incoming" {
                            Direction::Incoming
                        } else {
                            Direction::Outgoing
                        };
                        app.append(entry(
                            message["seq"].as_u64().unwrap_or(0),
                            direction,
                            message["body"]["command"].as_str().unwrap_or("unknown"),
                            if direction == Direction::Incoming {
                                EntryType::VmEvent
                            } else {
                                EntryType::DBGCommand
                            },
                            EntryPayload::VmFrame {
                                direction,
                                bytes,
                                fields: message["body"]["fields"].clone(),
                            },
                        ));
                    }
                    Err(error) => app.notice = Some(error),
                }
                continue;
            }
            app.append(entry(
                message["seq"].as_u64().unwrap_or(0),
                Direction::Incoming,
                kind,
                EntryType::DapEvent,
                EntryPayload::DapEvent {
                    body: message["body"].clone(),
                },
            ));
            let stopped_entry = app.timeline.len() - 1;
            match kind {
                "continued" => app.vm_state = VmState::Running,
                "stopped" => {
                    app.vm_state = VmState::Paused;
                    let thread_id = message["body"]["threadId"]
                        .as_i64()
                        .unwrap_or(self.thread_id);
                    app.timeline[stopped_entry].stop_context =
                        Some(self.inspect_paused_state(thread_id));
                }
                "terminated" => {
                    app.vm_state = VmState::Disconnected;
                    self.closed = true;
                }
                "output" => {
                    app.notice = message["body"]["output"]
                        .as_str()
                        .map(str::trim)
                        .map(str::to_owned)
                }
                _ => {}
            }
        }
    }

    fn inspect_paused_state(&mut self, thread_id: i64) -> StopContext {
        let mut context = StopContext::default();
        let stack = match self.inspection_request("stackTrace", json!({"threadId": thread_id})) {
            Ok(body) => body,
            Err(error) => {
                context.detail = Some(error);
                return context;
            }
        };
        let Some(frames) = stack.get("stackFrames").and_then(Value::as_array) else {
            context.detail = Some("stackTrace returned no stackFrames".into());
            return context;
        };
        for frame in frames {
            context.frames.push(StackFrame {
                name: frame
                    .get("name")
                    .and_then(Value::as_str)
                    .unwrap_or("<unnamed>")
                    .into(),
                address: frame
                    .get("instructionPointerReference")
                    .and_then(Value::as_str)
                    .unwrap_or("<unavailable>")
                    .into(),
            });
        }
        let Some(frame_id) = frames
            .first()
            .and_then(|frame| frame.get("id"))
            .and_then(Value::as_i64)
        else {
            context.detail = Some("stack frame has no id".into());
            return context;
        };
        if let Some(frame) = frames.first() {
            context.pc = frame
                .get("instructionPointerReference")
                .and_then(Value::as_str)
                .map(str::to_owned);
        }
        context.state = Some("Paused".into());
        let scopes = match self.inspection_request("scopes", json!({"frameId": frame_id})) {
            Ok(body) => body,
            Err(error) => {
                context.detail = Some(format!("Locals unavailable: {error}"));
                return context;
            }
        };
        let Some(reference) = scopes
            .get("scopes")
            .and_then(Value::as_array)
            .and_then(|scopes| {
                scopes.iter().find(|scope| {
                    scope.get("name").and_then(Value::as_str) == Some("VM")
                        && scope.get("expensive").and_then(Value::as_bool) != Some(true)
                })
            })
            .and_then(|scope| scope.get("variablesReference"))
            .and_then(Value::as_i64)
        else {
            return context;
        };
        let variables =
            match self.inspection_request("variables", json!({"variablesReference": reference})) {
                Ok(body) => body,
                Err(error) => {
                    context.detail = Some(format!("Locals unavailable: {error}"));
                    return context;
                }
            };
        if let Some(variables) = variables.get("variables").and_then(Value::as_array) {
            for variable in variables {
                if let (Some(name), Some(value)) = (
                    variable.get("name").and_then(Value::as_str),
                    scalar_text(variable.get("value")),
                ) {
                    match name {
                        "pc" => context.pc = Some(value),
                        "state" => context.state = Some(value),
                        _ => context.locals.push(NamedValue {
                            name: name.into(),
                            value,
                        }),
                    }
                }
            }
        }
        context
    }
}

fn scalar_text(value: Option<&Value>) -> Option<String> {
    match value? {
        Value::String(value) => Some(value.clone()),
        Value::Number(value) => Some(value.to_string()),
        Value::Bool(value) => Some(value.to_string()),
        _ => None,
    }
}

fn entry(
    sequence: u64,
    direction: Direction,
    event: &str,
    entry_type: EntryType,
    payload: EntryPayload,
) -> SessionEntry {
    SessionEntry {
        sequence,
        direction,
        event: event.into(),
        entry_type,
        payload,
        stop_context: None,
    }
}

fn parse_vm_frame(value: &Value) -> Result<Vec<u8>, String> {
    value
        .as_array()
        .ok_or_else(|| "invalid VM frame bytes".to_owned())?
        .iter()
        .map(|value| {
            value
                .as_u64()
                .and_then(|byte| u8::try_from(byte).ok())
                .ok_or_else(|| "invalid VM frame byte".to_owned())
        })
        .collect()
}
