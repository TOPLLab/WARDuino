use std::{
    io::{self, BufReader, BufWriter},
    sync::mpsc,
    thread,
    time::Duration,
};

use warduino_dap::{Request, read_message, warduino_adapter, write_message};

enum Input {
    Request(Request),
    End,
    Error(io::Error),
}

fn main() {
    let (sender, receiver) = mpsc::channel();
    thread::spawn(move || {
        let stdin = io::stdin();
        let mut reader = BufReader::new(stdin.lock());
        loop {
            match read_message(&mut reader) {
                Ok(Some(request)) => {
                    if sender.send(Input::Request(request)).is_err() {
                        return;
                    }
                }
                Ok(None) => {
                    let _ = sender.send(Input::End);
                    return;
                }
                Err(error) => {
                    let _ = sender.send(Input::Error(error));
                    return;
                }
            }
        }
    });

    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());
    let mut adapter = warduino_adapter();
    let mut disconnected = false;
    loop {
        match receiver.recv_timeout(Duration::from_millis(20)) {
            Ok(Input::Request(request)) => {
                let output = adapter.handle_request(request);
                if !write_all(&mut writer, output.messages) {
                    break;
                }
                if output.terminate {
                    disconnected = true;
                    break;
                }
            }
            Ok(Input::End) => break,
            Ok(Input::Error(error)) => {
                eprintln!("malformed DAP input: {error}");
                break;
            }
            Err(mpsc::RecvTimeoutError::Timeout) => {}
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
        }

        let output = adapter.pump_events();
        if !write_all(&mut writer, output.messages) || output.terminate {
            disconnected = output.terminate;
            break;
        }
    }
    if !disconnected {
        detach(&mut adapter);
    }
}

fn detach(adapter: &mut warduino_dap::WarduinoAdapter) {
    let _ = adapter.handle_request(Request {
        seq: 0,
        message_type: "request".into(),
        command: "disconnect".into(),
        arguments: serde_json::json!({}),
    });
}

fn write_all(writer: &mut impl io::Write, messages: Vec<serde_json::Value>) -> bool {
    for message in messages {
        if let Err(error) = write_message(writer, &message) {
            eprintln!("cannot write DAP response: {error}");
            return false;
        }
    }
    true
}
