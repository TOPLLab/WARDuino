//! Rust bindings generated from the WARDuino debugger protobuf descriptor.
//!
//! Regenerate `debug.pb` with `just schema` in `tools/syrup`; Cargo then builds
//! these bindings from that descriptor without invoking `protoc` itself.

include!(concat!(env!("OUT_DIR"), "/debug.rs"));
