# Low-level WARDuino debug interface

`wdebug` owns framing and schema-driven protobuf conversion for the WARDuino debugger channel. The V1 TCP session is synchronous: outgoing frames may block while being written, and receiving is always nonblocking.

The firmware does not provide request IDs, so `send` only confirms local frame acceptance. On success it returns `SentFrame`; `bytes()` is the exact accepted frame: command discriminator, canonical payload-length varint, then protobuf payload. State changes and operation results arrive as `DebugEvent` values.

The public `schema` module contains every generated protobuf message, enum, and oneof from `src/Debug/debug.proto`. For example, program uploads use `DebugCommand::UpdateModule(schema::ModuleUpdate { wasm })`, and callers should wait for an `OperationResult` whose generated `command` field is `schema::Command::UpdateModule as i32` before treating the upload as accepted by the VM.

```rust
use wdebug::{DebugCommand, DebugSession};

session.send(DebugCommand::Run)?;
while let Some(event) = session.try_recv()? {
    // Handle the asynchronous target event.
}
```
