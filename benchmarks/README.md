# Benchmaks

This folder includes benchmaks of WARDuino.

The various benchmark tests are listed in the [tasks](tasks) folder. In there, each task has a folder and two subfolders: 

- `espruino` containing a `impl.js` that can be sent over to Espruino
- `wast` containing a `impl.c` implementation of the task that will be compuiled to web assembly by the makefile

## Local run

To run the benchmarks localy on your computer, without board, execute one of the following in this folder.

- No tracing no debugging
  ```bash
  make clean all run
  ```
- No tracing only
  ```bash
  make clean all run TRACE=1
  ```
- No debug output only
  ```bash
  make clean all run DEBUG=1
  ```
- No trace and debug output
  ```bash
  make clean all run TRACE=1 DEBUG=1
  ```
- All the output
  ```bash
  make clean all run TRACE=1 DEBUG=1 WARN=1 INFO=1
  ```

The `clean` can be removed if the debugginf flags are not changed

## Runnig on a board

Place a list of benchmarks you wich to execute in `bench.list` then run `make all` followd by:

- For WARDuino: `./warduino_bench.sh {OUTPUFILE}` (this will flash an ino file on the board that runs the webassembly bytecode generated from the `C` implementaion with `emcc`)
- For Espruino: `./espruino_bench.sh {OUTPUFILE}` (this will flash the board with Espruino and execute all the tasks)
