# Low-level WARDuino debug interface

`wdebug` owns framing and protobuf conversion for the WARDuino debugger channel. The V1 TCP session is synchronous: outgoing frames may block while being written, and receiving is always nonblocking.

The firmware does not provide request IDs, so `send` only confirms local frame acceptance. On success it returns `SentFrame`; `bytes()` is the exact accepted frame: command discriminator, canonical payload-length varint, then protobuf payload. State changes and operation results arrive as `DebugEvent` values.

`DebugCommand::UpdateModule(wasm)` encodes the existing protobuf `ModuleUpdate` command (kind 26). Empty modules are rejected locally, and the normal 64 KiB framed-payload limit still applies; callers must wait for `OperationResult { command: CommandKind::UpdateModule, .. }` before treating an upload as accepted by the VM.

```rust
use wdebug::{DebugCommand, DebugSession};

session.send(DebugCommand::Continue)?;
while let Some(event) = session.try_recv()? {
    // Handle the asynchronous target event.
}
```
