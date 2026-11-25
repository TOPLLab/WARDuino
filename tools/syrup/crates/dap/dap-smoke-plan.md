# Minimal WARDuino DAP Smoke-Test Plan

## Goal

Build the smallest protocol-valid DAP executable that demonstrates the preliminary `../debug` library in action.

The demo proves only this path:

```text
DAP continue request
  → DebugCommand::Continue
  → WARDuino/Waffyr
  → DebugEvent::Continued
  → DAP continued event

DAP pause request
  → DebugCommand::Pause
  → WARDuino/Waffyr
  → DebugEvent::Stopped
  → DAP stopped event
```

It is a review harness, not the production DAP adapter.

## Scope

Implement only these DAP messages:

| Direction | Message | Purpose |
|---|---|---|
| Client → adapter | `initialize` | Establish the DAP session |
| Client → adapter | `attach` | Mark the preconfigured Waffyr connection active |
| Adapter → client | `initialized` | Signal readiness for configuration |
| Client → adapter | `configurationDone` | Finish the empty configuration phase |
| Client → adapter | `threads` | Return one synthetic Waffyr thread |
| Client → adapter | `continue` | Send the WARDuino continue/run command |
| Adapter → client | `continued` | Report that execution resumed |
| Client → adapter | `pause` | Send the WARDuino pause command |
| Adapter → client | `stopped` | Report the pause/stop reason |
| Client → adapter | `disconnect` | Close the smoke-test session |

Every unsupported request receives a normal DAP failure response. It must not panic or silently disappear.

Explicitly exclude breakpoints, stack traces, scopes, variables, source files, stepping, launch, restart, terminate, configuration screens, and editor-extension packaging.

## Package layout

Add one small binary crate or binary target:

```text
crates/dap/
├── Cargo.toml
├── src/
│   ├── main.rs       process setup and real DebugSession construction
│   ├── protocol.rs   Content-Length framing and minimal JSON helpers
│   └── adapter.rs    request dispatch and debug-event translation
└── tests/
    └── smoke.rs      deterministic transcript using a fake DebugSession
```

If `crates/dap` already exists, add these pieces there instead of introducing another crate.

## Dependencies

Use only:

- `debug` through a workspace path dependency;
- `serde` with derive;
- `serde_json`;
- the Rust standard library.

Do not add an async runtime, DAP framework, CLI framework, logging framework, or channel crate. Write diagnostics to stderr because stdout is reserved exclusively for DAP frames.

## Process interface

Run the adapter over stdio:

```bash
cargo run -p warduino-dap -- --device localhost:8100
```

The exact connection argument should match the current `debug` constructor. Parse only the one required argument manually. Connect to Waffyr once during startup and retain the session for the process lifetime.

For this smoke test, `attach` does not establish a second connection. It begins the empty DAP configuration phase for the session that was connected at startup.

## Minimal DAP framing

DAP uses a header followed by a JSON body:

```text
Content-Length: <byte count>\r\n
\r\n
<UTF-8 JSON>
```

Implement:

```rust
fn read_message(reader: &mut impl BufRead) -> io::Result<Option<Request>>;
fn write_message(writer: &mut impl Write, value: &impl Serialize) -> io::Result<()>;
```

Requirements:

- measure JSON length in bytes, not characters;
- accept additional headers but require one valid `Content-Length`;
- use `read_exact` for the body;
- treat clean EOF before a header as normal shutdown;
- reject malformed or oversized messages;
- flush stdout after every response or event;
- never print ordinary logs to stdout.

Use a small maximum request size, such as 1 MiB, to avoid unbounded allocation.

## Minimal message representation

Only deserialize the common request envelope:

```rust
#[derive(Deserialize)]
struct Request {
    seq: u64,
    #[serde(rename = "type")]
    message_type: String,
    command: String,
    #[serde(default)]
    arguments: serde_json::Value,
}
```

Build responses and events through two helpers rather than defining the entire DAP schema:

```rust
fn response(request: &Request, success: bool, body: Value) -> Value;
fn event(name: &str, body: Value) -> Value;
```

The adapter owns one monotonically increasing outgoing DAP `seq` used by both responses and events.

## Adapter state

Keep only the state required to reject obviously invalid requests:

```rust
enum AdapterState {
    AwaitingInitialize,
    Ready,
    Configuring,
    Attached,
    Disconnected,
}

struct Adapter<S> {
    session: S,
    state: AdapterState,
    next_seq: u64,
}
```

Use one fixed DAP thread:

```rust
const THREAD_ID: i64 = 1;
```

The `threads` response returns:

```json
{"threads":[{"id":1,"name":"Waffyr VM"}]}
```

Do not construct a second model of VM state. State transitions come from `warduino-debug` events.

## Request behavior

### `initialize`

1. Require `AwaitingInitialize`.
2. Return a successful response with an empty/minimal capabilities body.
3. Advertise only `supportsConfigurationDoneRequest: true`.
4. Enter `Ready`.

Do not send `initialized` yet. DAP initialization completes before the client sends `launch` or `attach`; the adapter sends `initialized` once it is ready to receive configuration.

### `attach`

1. Require `Ready`.
2. Confirm that the already-created debug session is connected.
3. Retain the attach request instead of responding immediately.
4. Emit `initialized`.
5. Enter `Configuring`.

The configuration phase is empty, but retaining the request preserves the normal DAP launch sequence.

### `configurationDone`

1. Require `Configuring`.
2. Return a successful `configurationDone` response.
3. Return the retained successful `attach` response.
4. Enter `Attached`.

This one extra handler makes the smoke adapter usable by conforming clients without implementing breakpoints or exception configuration.

### `threads`

Return the single synthetic thread. Supporting this tiny request makes the adapter understandable to ordinary DAP clients without adding real thread handling.

### `continue`

1. Require `Attached` and `threadId == 1` when supplied.
2. Call `session.send(DebugCommand::Continue)`; use `Run` instead only if that is the current public variant.
3. Return a successful DAP response with `{"allThreadsContinued":true}`.
4. When the library reports `DebugEvent::Continued`, emit:

```json
{
  "threadId": 1,
  "allThreadsContinued": true
}
```

If the current ABI never sends a continued notification, emit the DAP `continued` event immediately after a successful local send and document that it confirms transmission, not remote execution.

### `pause`

1. Require `Attached` and the synthetic thread ID.
2. Call `session.send(DebugCommand::Pause)`.
3. Return a successful empty DAP response.
4. Wait for the normal debug-event pump to observe the stop.
5. Translate the resulting stop to a DAP `stopped` event.

For the revised debug API:

```rust
DebugEvent::Stopped(stopped)
```

For the preliminary API, temporarily accept its equivalent `Paused` or `BreakpointHit` variant. Keep this compatibility match local to `adapter.rs` so the review shows whether the planned API change improves the translation.

Emit a body containing only:

```json
{
  "reason": "pause",
  "threadId": 1,
  "allThreadsStopped": true
}
```

Map other reasons to `step`, `breakpoint`, or `exception` if they arrive, even though the smoke client does not request those operations.

### `disconnect`

1. Return success.
2. Close/drop the debug session.
3. Enter `Disconnected` and terminate cleanly.

Do not implicitly resume or terminate Waffyr.

## Response timing

Use the strongest semantics provided by the preliminary library:

- If `warduino-debug` exposes correlated `CommandResponse` values, retain `CommandId → DAP request seq` and send DAP success/failure when the response arrives.
- If `send` is currently fire-and-forget, respond after successful encoding and transport write. A later VM transition remains a separate DAP event.

Do not delay a DAP response until a `Stopped` or `Continued` event. A command response and the state-change event are different protocol messages.

## Event loop

Avoid async dependencies with one standard-library input thread:

1. A reader thread owns stdin, decodes DAP requests, and sends them through `std::sync::mpsc`.
2. The main thread owns stdout and `DebugSession`.
3. The main loop receives DAP requests with a short timeout.
4. Between requests, it repeatedly calls `session.try_recv()` until it returns `None`.
5. It translates each debug response/event and writes the corresponding DAP message.

This allows spontaneous pause/disconnect events to reach the client even while no DAP request is arriving, without adding Tokio.

## Error handling

- Malformed DAP input terminates the adapter with a diagnostic on stderr.
- Unsupported or invalid-state DAP requests receive `success: false` with a concise message.
- A local `warduino-debug::DebugError` fails the current DAP request when applicable, then emits an output diagnostic and disconnects.
- A remote `CommandError`, if supported, becomes a failed response to the correlated DAP request.
- `DebugEvent::Disconnected` emits a DAP `terminated` event and ends the loop.

The adapter must always restore/flush its output path and exit without a panic.

## Automated smoke test

Make `Adapter` generic over `DebugSession` and use a deterministic fake session. Test this exact transcript:

1. `initialize` → success response advertising configuration completion support.
2. `attach` → adapter emits `initialized` and retains the request.
3. `configurationDone` → its response followed by the successful attach response.
4. `threads` → one thread named `Waffyr VM`.
5. `continue` → fake receives `DebugCommand::Continue`; DAP response succeeds.
6. Fake emits `Continued` → adapter emits `continued` for thread 1.
7. `pause` → fake receives `DebugCommand::Pause`; DAP response succeeds.
8. Fake emits `Stopped(Pause)` → adapter emits `stopped` with reason `pause`.
9. `disconnect` → success response and clean exit.

Also test one unsupported request and one debug-library error. Assert the actual Content-Length-framed JSON output rather than calling handlers directly.

## Real Waffyr demonstration

Add a tiny standard-library Python script only if manual frame construction becomes inconvenient. It should:

1. spawn `warduino-dap` with the Waffyr connection argument;
2. send `initialize`, `attach`, `configurationDone`, `threads`, and `continue`;
3. print the received `continued` event;
4. wait briefly, then send `pause`;
5. print the received `stopped` event;
6. send `disconnect` and verify clean exit.

This is preferable to creating a VS Code extension for the review. Editor packaging can happen after the API shape is accepted.

## Implementation order

1. Add Content-Length read/write helpers and framing tests.
2. Add the minimal request dispatcher and outgoing sequence counter.
3. Implement `initialize`, retained `attach`, `configurationDone`, `threads`, and `disconnect` with a fake session.
4. Map DAP `continue` and `pause` to `warduino-debug` commands.
5. Add the debug-event pump and translate continued/stopped/disconnected events.
6. Run the transcript test through real DAP framing.
7. Run the same sequence against Waffyr and record the transcript for code review.

## Completion criteria

- The adapter uses real DAP Content-Length framing over stdio.
- It implements only initialize, attach, configurationDone, threads, continue, pause, and disconnect.
- Continue and pause call the real typed `warduino-debug` API.
- WARDuino state events become correctly ordered DAP events.
- Stdout contains only DAP messages; diagnostics use stderr.
- Unsupported requests fail cleanly.
- The fake-session transcript test is deterministic.
- One real Waffyr run demonstrates continue followed by pause.
- No async runtime, DAP framework, editor extension, or unrelated debugger feature is added.

## References

- [DAP overview and launch sequencing](https://microsoft.github.io/debug-adapter-protocol/overview.html)
- [DAP protocol specification](https://microsoft.github.io/debug-adapter-protocol/specification.html)
