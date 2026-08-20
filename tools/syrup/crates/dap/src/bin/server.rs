use std::{
    env,
    io::{self, BufReader, BufWriter},
    sync::mpsc,
    thread,
    time::Duration,
};

use debug::WarduinoSession;
use warduino_dap::{Adapter, Request, read_message, write_message};

enum Input {
    Request(Request),
    End,
    Error(io::Error),
}

fn main() {
    let device = match device_argument() {
        Ok(device) => device,
        Err(message) => {
            eprintln!("{message}");
            std::process::exit(2);
        }
    };
    let session = match WarduinoSession::connect(&device) {
        Ok(session) => session,
        Err(error) => {
            eprintln!("cannot connect to {device}: {error}");
            std::process::exit(1);
        }
    };

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
    let mut adapter = Adapter::new(session);
    loop {
        match receiver.recv_timeout(Duration::from_millis(20)) {
            Ok(Input::Request(request)) => {
                let output = adapter.handle_request(request);
                if !write_all(&mut writer, output.messages) || output.terminate {
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
            break;
        }
    }
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

fn device_argument() -> Result<String, &'static str> {
    let mut arguments = env::args().skip(1);
    match (
        arguments.next().as_deref(),
        arguments.next(),
        arguments.next(),
    ) {
        (Some("--device"), Some(device), None) => Ok(device),
        _ => Err("usage: warduino-dap --device <host:port>"),
    }
}
