use std::{env, fs, path::PathBuf};

use prost::Message;
use prost_types::FileDescriptorSet;

fn main() {
    let manifest_dir =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").expect("missing manifest directory"));
    let descriptor = manifest_dir.join("../..").join("debug.pb");
    println!("cargo:rerun-if-changed={}", descriptor.display());

    let bytes = fs::read(&descriptor).unwrap_or_else(|error| {
        panic!(
            "cannot read {} ({error}); run `just schema` from tools/syrup first",
            descriptor.display()
        )
    });
    let descriptor_set = FileDescriptorSet::decode(bytes.as_slice()).unwrap_or_else(|error| {
        panic!(
            "cannot decode {} as a protobuf descriptor set: {error}",
            descriptor.display()
        )
    });

    prost_build::Config::new()
        .out_dir(PathBuf::from(
            env::var_os("OUT_DIR").expect("missing OUT_DIR"),
        ))
        .compile_fds(descriptor_set)
        .expect("failed to generate Rust bindings from debug.pb");
}
