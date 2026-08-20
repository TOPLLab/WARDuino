# WARDuino Adapter Protocol

The adapter supports `initialize`, `attach`, `configurationDone`, `threads`,
`continue`, `pause`, and `disconnect`. Start it without an endpoint, then send
`attach` with `{"device":"host:port"}`. A successful attach emits
`initialized`; after `configurationDone` it replies to the retained attach.

Both run-control requests require `threadId: 1`. Disconnect detaches without
terminating the VM; it best-effort resumes it before closing the TCP session.

Start the standalone stdio adapter from the workspace with:

```text
cargo run -p warduino-dap
```

It speaks DAP framing on standard input/output. For example, send these three
frames (with the blank line after each header) to initialize and attach to a
local debug socket:

```text
Content-Length: 83

{"seq":1,"type":"request","command":"initialize","arguments":{"adapterID":"smoke"}}
Content-Length: 85

{"seq":2,"type":"request","command":"attach","arguments":{"device":"127.0.0.1:8100"}}
Content-Length: 71

{"seq":3,"type":"request","command":"configurationDone","arguments":{}}
```

The adapter responds to `initialize`, emits `initialized` after the successful
`attach`, then responds to `configurationDone` followed by the deferred
`attach` response.
