# WARDuino VS Code Debugger Plugin

This directory contains the source code for the VS Code plugin for the WARDuino debugger.

## Features


## Requirements


## Extension Settings

The debugger contributes the following settings:

## Development Notes

To run the extension in developer mode, perform the following steps:

- Create a directory `/tmp/warduino`
- Compile the WARDuino CLI (Emulator version) in this directory
- Build [the custom WABT toolkit](https://github.com/TOPLLab/wabt) and add `wat2wasm` to `$PATH`

When you have performed each step above. You should be able to open this directory in VS Code and run the extension.
When you run `Run Extension` a new VS Code instance should start.
In order to launch the debugger in this new VS Code window, perform the following steps:

- open a directory with a WAT file
- add a `.vscode/launch.json` file with the same content as the example file: `src/test/UnitTests/TestSource/.vscode/launch.json`
- start the debugger with the `Debug WARDuino` button

## Known Issues


