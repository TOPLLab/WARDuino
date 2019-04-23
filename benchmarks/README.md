# Benchmakrs

Run with:

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