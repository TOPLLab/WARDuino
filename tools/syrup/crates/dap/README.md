# WARDuino DAP

`warduino-dap` is a deliberately limited, source-aware Debug Adapter Protocol bridge. It uses only the existing VM ABI: module update acknowledgement, compact PC stop snapshots, request-time call-stack/local snapshots, and `Step`/`StepOver`.

Attach requires both `device` and a `program` path to a WAT file. The adapter compiles that file once, uploads those exact WASM bytes, and waits up to five seconds for the existing `UpdateModule` operation result before emitting `initialized`. It retains the immutable WAT text and uses it for DAP `source` requests.

The adapter exposes one synthetic thread. A stop keeps a compact PC/state snapshot; each `stackTrace` request then sends its own call-stack snapshot and replies when that asynchronous notification arrives. Frames are current-first, use retained WAT source locations where addresses map, and otherwise fall back to `func[index]`.

Supported run control is `continue`, `pause`, `stepIn`, `next`, `restart`, `terminate`, and `disconnect`. `restart` sends the firmware empty `COMMAND_RESET` command. Line/statement `stepIn` repeats VM `Step`; line/statement `next` repeats VM `StepOver`, stopping only when the locally mapped source location changes. Instruction granularity sends one VM operation. A five-second safety deadline prevents same-location loops from running indefinitely.

Only the current frame has a VM scope. Its variables request sends a request-time locals snapshot and returns cached PC/state plus firmware locals. Caller-frame locals, `stepOut`, breakpoints, evaluate, memory access, disassembly, and source breakpoints are not implemented and are not advertised.

Run the standalone adapter from `tools/syrup`:

```text
cargo run -p warduino-dap -- --dap-server 127.0.0.1:4711
```
