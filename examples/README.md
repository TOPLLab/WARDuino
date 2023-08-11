# Examples

This folder contains the code for all examples from the [tutorials](https://topllab.github.io/WARDuino/guide/examples/) from the documentation website.

Additionally, the `wat` folder contains a simple example in textual WebAssembly, which relies on no other higher-level languages.

## Running the examples

All WebAssembly binaries (.wasm) from the examples can be run without additional hardware, by using the emulated version of WARDuino.
This version is packaged as a command line interface.

```bash
./wdcli --file example.wasm
```

