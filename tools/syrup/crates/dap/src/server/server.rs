use std::{
    env,
    io::{self, BufRead, BufReader, BufWriter, Write},
    net::{TcpListener, TcpStream},
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

enum Transport {
    Stdio,
    Tcp(String),
}

fn main() {
    match parse_transport() {
        Ok(Transport::Stdio) => run_stdio(),
        Ok(Transport::Tcp(address)) => run_tcp(&address),
        Err(()) => {
            eprintln!("usage: warduino-dap [--dap-server <host:port>]");
            std::process::exit(2);
        }
    }
}

fn parse_transport() -> Result<Transport, ()> {
    let arguments: Vec<_> = env::args().skip(1).collect();
    match arguments.as_slice() {
        [] => Ok(Transport::Stdio),
        [flag, address] if flag == "--dap-server" => Ok(Transport::Tcp(address.clone())),
        _ => Err(()),
    }
}

fn run_stdio() {
    let stdin = io::stdin();
    let stdout = io::stdout();
    run_connection(
        move |sender| read_requests(BufReader::new(stdin.lock()), sender),
        BufWriter::new(stdout.lock()),
        warduino_adapter(),
    );
}

fn run_tcp(address: &str) {
    let listener = match TcpListener::bind(address) {
        Ok(listener) => listener,
        Err(error) => {
            eprintln!("cannot bind DAP server at {address}: {error}");
            std::process::exit(1);
        }
    };
    eprintln!("DAP server listening on {address}");

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                eprintln!("DAP client connected");
                run_tcp_connection(stream);
            }
            Err(error) => eprintln!("cannot accept DAP client: {error}"),
        }
    }
}

fn run_tcp_connection(stream: TcpStream) {
    let reader_stream = match stream.try_clone() {
        Ok(stream) => stream,
        Err(error) => {
            eprintln!("cannot prepare DAP client connection: {error}");
            return;
        }
    };
    run_connection(
        move |sender| read_requests(BufReader::new(reader_stream), sender),
        BufWriter::new(stream),
        warduino_adapter(),
    );
}

fn read_requests(mut reader: impl BufRead, sender: mpsc::Sender<Input>) {
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
}

fn run_connection<W, F>(read: F, mut writer: W, mut adapter: warduino_dap::WarduinoAdapter)
where
    W: Write,
    F: FnOnce(mpsc::Sender<Input>) + Send + 'static,
{
    let (sender, receiver) = mpsc::channel();
    thread::spawn(move || read(sender));

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

fn write_all(writer: &mut impl Write, messages: Vec<serde_json::Value>) -> bool {
    for message in messages {
        if let Err(error) = write_message(writer, &message) {
            eprintln!("cannot write DAP response: {error}");
            return false;
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use std::net::TcpListener;

    use serde_json::json;

    use super::*;

    #[test]
    fn tcp_connection_handles_initialize_and_client_eof() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let server = thread::spawn(move || {
            let (stream, _) = listener.accept().unwrap();
            run_tcp_connection(stream);
        });
        let mut client = TcpStream::connect(address).unwrap();
        let initialize =
            json!({"seq": 1, "type": "request", "command": "initialize", "arguments": {}});
        write_message(&mut client, &initialize).unwrap();

        let mut reader = BufReader::new(client.try_clone().unwrap());
        let response = read_response(&mut reader);
        assert_eq!(response["success"], true);
        assert_eq!(response["body"]["supportsConfigurationDoneRequest"], true);

        drop(reader);
        drop(client);
        server.join().unwrap();
    }

    fn read_response(reader: &mut impl BufRead) -> serde_json::Value {
        let mut header = String::new();
        reader.read_line(&mut header).unwrap();
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
        serde_json::from_slice(&body).unwrap()
    }
}
