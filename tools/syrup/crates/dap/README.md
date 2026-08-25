# WARDuino DAP

`warduino-dap` is a deliberately limited, source-aware Debug Adapter Protocol bridge. It uses only the existing VM ABI: module update acknowledgement, snapshots containing the current PC, and `Step`/`StepOver`.

Attach requires both `device` and a `program` path to a WAT file. The adapter compiles that file once, uploads those exact WASM bytes, and waits up to five seconds for the existing `UpdateModule` operation result before emitting `initialized`. It retains the immutable WAT text and uses it for DAP `source` requests.

The adapter exposes one synthetic thread and, while paused at a mapped PC, exactly one current frame (`totalFrames: 1`). The frame name is the mapped WAT function name (or `func[index]`) and its source line/column come from the retained WAT. This is not a complete VM call stack.

Supported run control is `continue`, `pause`, `stepIn`, `next`, `terminate`, and `disconnect`. Line/statement `stepIn` repeats VM `Step`; line/statement `next` repeats VM `StepOver`, stopping only when the locally mapped source location changes. Instruction granularity sends one VM operation. A five-second safety deadline prevents same-location loops from running indefinitely.

The current VM scope exposes PC and VM state. Full caller stacks, `stepOut`, breakpoints, locals, evaluate, memory access, disassembly, and source breakpoints are not implemented and are not advertised.

Run the standalone adapter from `tools/syrup`:

```text
cargo run -p warduino-dap -- --dap-server 127.0.0.1:4711
```
