# Latch test suites for WARDuino

Run test with the following commands:

```bash
npm run spectest
npm run primtest
npm run debugtest
npm run comptest
```

Make sure the EMULATOR environment variable is set when you run the tests on the emulator.

For the spectests be sure the WABT is also set, so the `wat2wasm` command can be found.

