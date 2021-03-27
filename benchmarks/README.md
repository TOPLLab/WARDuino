# Benchmaks

This folder includes benchmaks of WARDuino.

The various benchmark tests are listed in the [tasks](tasks) folder. In there, each task has a folder and three subfolders:

- `espruino` containing a `impl.js` that can be sent over to Espruino
- `wast` containing a `impl.c` implementation of the task that will be compiled to web assembly by the makefile
- `c` containing a `arduino.ino` implementation of the task **generated automaticly** from the `wast` dirctory

## Local run

To run the benchmarks localy on your computer, without board, execute one of the following in this folder.
The benchmark program will also check the return values of all the functions in this mode and keep track of the total.
Edit the `main` of benchmarks.cpp to add and/or remove tasks or modify their expected return values.

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

Currently, the ESPruinio tests only work for the `esp32`.

Place a list of benchmarks you wich to execute in `bench.list` then run:

- For WARDuino: `./warduino_bench.sh {OUTPUFILE}` (this will flash an ino file on the board that runs the webassembly bytecode generated from the `C` implementaion with `emcc`)
   - Add the env variable `BOARD=ESP8266` for that board
- For Espruino: `./espruino_bench.sh {OUTPUFILE}` (this will flash the board with Espruino and execute all the tasks)
- For Espruino: `./native_bench.sh {OUTPUFILE}` (this will flash the board with each of the C files and time execution)
- Or `./all_bench.sh {out_file}` to do then all

