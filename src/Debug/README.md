#Debugger code structure

The debugger implementation is split by responsibility:

- `debugger.h`: public `Debugger` class and debugger state declarations.
- `debugger.cpp`: debugger lifecycle, message transport, and shared utilities.
- `debugger-command.cpp`: decoding and handling incoming debugger commands.
- `debugger-snapshot.cpp`: collecting and sending snapshots and checkpoints.
- `debugger-proxy.cpp`: debugger operations involving the proxy.
- `debugger-overrides.cpp`: function override management.
- `debugger-decode.h`: protocol framing and protobuf decoding helpers.
- `debugger-encode.h`: protobuf encoding callbacks for debugger data.
- `debugger-private.h`: shared private includes and implementation dependencies.
- `nanopb_encoder.*`: reusable nanopb callback implementations.

protobuf code:

- `debug.proto`: protobuf schema for debugger commands, notifications, and state.
- `nanopb/`: [vendored] generated protobuf bindings and the nanopb runtime.
