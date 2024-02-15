import {
    ArduinoSpecification,
    EmulatorSpecification,
    Framework,
    HybridScheduler,
    invoke,
    OutputStyle,
    returns,
    Step,
    WASM
} from "latch";

const framework = Framework.getImplementation();
framework.style(OutputStyle.github);

const suite = framework.suite('Specification test suite for WebAssembly');
suite.testee('emulator [:8500]', new EmulatorSpecification(8500));
// suite.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'), new HybridScheduler());

// create large tests

const steps: Step[] = [];

steps.push({
    title: `fac(2)`,
    instruction: invoke("fac", [WASM.i32(2)]),
    expected: returns(WASM.i32(2))
});

steps.push({
    title: `fac(6)`,
    instruction: invoke("fac", [WASM.i32(6)]),
    expected: returns(WASM.i32(720))
});

steps.push({
    title: `fac(7)`,
    instruction: invoke("fac", [WASM.i32(7)]),
    expected: returns(WASM.i32(5040))
});

steps.push({
    title: `fac(8)`,
    instruction: invoke("fac", [WASM.i32(8)]),
    expected: returns(WASM.i32(40320))
});

steps.push({
    title: `fac(9)`,
    instruction: invoke("fac", [WASM.i32(9)]),
    expected: returns(WASM.i32(362880))
});

steps.push({
    title: `fac(10)`,
    instruction: invoke("fac", [WASM.i32(10)]),
    expected: returns(WASM.i32(3628800))
});

steps.push({
    title: `fib(0)`,
    instruction: invoke("fib", [WASM.i32(0)]),
    expected: returns(WASM.i32(0))
});

steps.push({
    title: `fib(1)`,
    instruction: invoke("fib", [WASM.i32(1)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `fib(2)`,
    instruction: invoke("fac", [WASM.i32(2)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `fib(3)`,
    instruction: invoke("fac", [WASM.i32(3)]),
    expected: returns(WASM.i32(2))
});

steps.push({
    title: `fib(4)`,
    instruction: invoke("fac", [WASM.i32(4)]),
    expected: returns(WASM.i32(3))
});

steps.push({
    title: `fib(5)`,
    instruction: invoke("fac", [WASM.i32(5)]),
    expected: returns(WASM.i32(5))
});

steps.push({
    title: `fib(6)`,
    instruction: invoke("fac", [WASM.i32(6)]),
    expected: returns(WASM.i32(8))
});

steps.push({
    title: `fib(8)`,
    instruction: invoke("fac", [WASM.i32(8)]),
    expected: returns(WASM.i32(21))
});

steps.push({
    title: `fib(10)`,
    instruction: invoke("fac", [WASM.i32(10)]),
    expected: returns(WASM.i32(55))
});

steps.push({
    title: `fib(12)`,
    instruction: invoke("fac", [WASM.i32(12)]),
    expected: returns(WASM.i32(144))
});

steps.push({
    title: `fib(16)`,
    instruction: invoke("fac", [WASM.i32(16)]),
    expected: returns(WASM.i32(987))
});

steps.push({
    title: `is_prime(3)`,
    instruction: invoke("prim", [WASM.i32(3)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `is_prime(4)`,
    instruction: invoke("prim", [WASM.i32(4)]),
    expected: returns(WASM.i32(0))
});

steps.push({
    title: `is_prime(5)`,
    instruction: invoke("prim", [WASM.i32(5)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `is_prime(6)`,
    instruction: invoke("prim", [WASM.i32(6)]),
    expected: returns(WASM.i32(0))
});

steps.push({
    title: `is_prime(7)`,
    instruction: invoke("prim", [WASM.i32(7)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `is_prime(97)`,
    instruction: invoke("prim", [WASM.i32(97)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `is_prime(98)`,
    instruction: invoke("prim", [WASM.i32(98)]),
    expected: returns(WASM.i32(0))
});

steps.push({
    title: `is_prime(97)`,
    instruction: invoke("prim", [WASM.i32(509)]),
    expected: returns(WASM.i32(1))
});

steps.push({
    title: `is_prime(97)`,
    instruction: invoke("prim", [WASM.i32(513)]),
    expected: returns(WASM.i32(0))
});

suite.test({
    title: `Test computational functions`,
    program: `src/maths.wast`,
    steps: steps
})

// run tests

framework.run([suite]);
