# Low-level WARDuino debug interface

`wdebug` owns framing and protobuf conversion for the WARDuino debugger channel.
The V1 TCP session is synchronous: outgoing frames may block while being written,
and receiving is always nonblocking.

The firmware does not provide request IDs, so `send` only confirms local frame
acceptance. State changes and operation results arrive as `DebugEvent` values.

```rust
use wdebug::{DebugCommand, DebugSession};

session.send(DebugCommand::Continue)?;
while let Some(event) = session.try_recv()? {
    // Handle the asynchronous target event.
}
```
