use serde_json::{Value, json};

use dap::{AdapterOutput, Request, WarduinoAdapter, warduino_adapter};

use crate::app::{App, CommandIntent, Direction, EntryType, SessionEntry, VmState};

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

    fn send(&mut self, command: &str, arguments: Value, app: &mut App) -> (u64, AdapterOutput) {
        let seq = self.seq;
        self.seq += 1;
        app.append(entry(
            seq,
            Direction::Outgoing,
            command,
            EntryType::DapRequest,
            None,
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
                    None,
                ));
            }
            if message["type"] == "event" {
                if kind == "warduino/vmFrame" {
                    match parse_vm_frame(&message["body"]["bytes"]) {
                        Ok(bytes) => {
                            app.append(entry(
                                message["seq"].as_u64().unwrap_or(0),
                                if message["body"]["direction"] == "incoming" {
                                    Direction::Incoming
                                } else {
                                    Direction::Outgoing
                                },
                                message["body"]["command"].as_str().unwrap_or("unknown"),
                                if message["body"]["direction"] == "incoming" {
                                    EntryType::VmEvent
                                } else {
                                    EntryType::DBGCommand
                                },
                                Some(bytes),
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
                    None,
                ));
                match kind {
                    "continued" => app.vm_state = VmState::Running,
                    "stopped" => app.vm_state = VmState::Paused,
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
    }
}

fn entry(
    sequence: u64,
    direction: Direction,
    event: &str,
    entry_type: EntryType,
    wire: Option<Vec<u8>>,
) -> SessionEntry {
    SessionEntry {
        sequence,
        direction,
        event: event.into(),
        entry_type,
        wire,
        effect: Vec::new(),
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
