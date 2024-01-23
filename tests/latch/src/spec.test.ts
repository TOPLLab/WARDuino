import {EmulatorSpecification, Framework, Invoker, Message, Step, TestScenario, WASM} from 'latch';
import {readdirSync} from 'fs';
import {basename} from 'path';
import {find, parseArguments, parseAsserts, parseResult} from "./util/spec.util";

// Parse tests from official suite

export const CORESUITE: string = process.env.CORESUITE ?? 'core/';

const TESTFILE: string = process.env.TESTFILE ?? '';

const tests: TestScenario[] = [];

if (TESTFILE.length > 0) {
    const module: string = TESTFILE.replace('.asserts.wast', '.wast');
    const asserts: string[] = parseAsserts(CORESUITE + TESTFILE);
    tests.push(createTest(CORESUITE + module, asserts));
} else {
    process.stdout.write(`> Scanning suite: ${CORESUITE}\n\n`);

    const files: string[] = readdirSync(CORESUITE).filter((file) => file.endsWith('.asserts.wast'));

    let count = 0;
    let tally: string = ` [${count++}/${files.length}]`;
    process.stdout.write(`> Building scenarios${tally}`);
    for (const file of files) {
        const module: string = file.replace('.asserts.wast', '.wast');
        const asserts: string[] = parseAsserts(CORESUITE + file);
        tests.push(createTest(CORESUITE + module, asserts));

        if (process.stdout.moveCursor !== undefined) {
            process.stdout.moveCursor(-tally.length, 0);
        }
        tally = ` [${count++}/${files.length}]`;
        process.stdout.write(tally);
    }

    process.stdout.write('\n\n> Starting framework (this may take a while)\n\n');


}

// run tests

const framework = Framework.getImplementation();

framework.suite('Specification test suite for WebAssembly');
framework.testee('emulator [:8500]', new EmulatorSpecification(8500));
// framework.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'));
framework.tests(tests);

framework.run();

// Helper function

function createTest(module: string, asserts: string[]): TestScenario {
    const steps: Step[] = [];

    for (const assert of asserts) {
        const cursor = {value: 0};
        const fidx: string = find(/invoke "([^"]+)"/, assert);
        const args: WASM.Value[] = parseArguments(assert.replace(`(invoke "${fidx} "`, ''), cursor);
        const result: WASM.Value | undefined = parseResult(assert.slice(cursor.value));

        if (result !== undefined) {
            steps.push(new Invoker(fidx, args, result.value));
        }
    }

    return {
        title: `Test: ${basename(module)}`,
        program: module,
        dependencies: [],
        steps: steps
    };
}


