# Virtual Machine Internals Tests

The tests in this folder are designed to test the internals of the virtual machine.
The current test suite contains tests for the following components:

1. Physical to virtual address translation
2. WebAssembly module loading
3. WebAssembly module unloading

## Running the tests

To run the tests, execute the following command from the root of the repository:

```bash
$ mkdir tests/build ; cd tests/build ; cmake ../../ -D BUILD_UNITTEST=ON ; cmake --build . ; ctest -VV
```

