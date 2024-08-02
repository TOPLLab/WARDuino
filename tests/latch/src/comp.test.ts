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
//suite.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'), new HybridScheduler(), {timeout: 16000});

// create large tests

const steps: Step[] = [];

function fac(i: number): number {
    return i === 0 ? 1 : i * fac(i - 1)
}

for (let i = 1; i < 13; i++) {
    steps.push({
        title: `fac(${i})`,
        instruction: invoke("fac", [WASM.i32(i)]),
        expected: returns(WASM.i32(fac(i)))
    });
}

function fib(i: number): number {
    return i <= 2 ? 1 : fib(i - 1) + fib(i - 2)
}

for (let i = 1; i < 23; i++) {
    steps.push({
        title: `fib(${i})`,
        instruction: invoke("fib", [WASM.i32(i)]),
        expected: returns(WASM.i32(fib(i)))
    });
}

function gcd(a: number, b: number): number {
    return b === 0 ? a : gcd(b, a % b);
}

for (let i = 101; i < 512; i++) {
    steps.push({
        title: `gcd(${i})`,
        instruction: invoke("gcd", [WASM.i32(i), WASM.i32(12345)]),
        expected: returns(WASM.i32(gcd(i, 12345)))
    });
}

for (let i = 1024; i < 2048; i++) {
    steps.push({
        title: `gcd(${i})`,
        instruction: invoke("gcd", [WASM.i32(i), WASM.i32(32768)]),
        expected: returns(WASM.i32(gcd(i, 32768)))
    });
}

function is_prime(n: number): number {
    if (n <= 1) return 0;
    for (let i = 2; i <= Math.sqrt(n); i++) {
        if (n % i === 0) return 0;
    }
    return 1;
}

for (let i = 1000; i < 3000; i++) {
    steps.push({
        title: `is_prime(${i})`,
        instruction: invoke("prim", [WASM.i32(i)]),
        expected: returns(WASM.i32(is_prime(i)))
    });
}

suite.test({
    title: `Test computational functions`,
    program: `src/maths.wast`,
    steps: steps
})

// run tests

framework.run([suite]);
