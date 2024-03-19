import {
    ArduinoSpecification,
    EmulatorSpecification,
    Framework, HybridScheduler,
    invoke,
    Invoker,
    OutputStyle,
    returns,
    Step,
    TestScenario,
    WASM
} from 'latch';
import {readdirSync} from 'fs';
import {basename} from 'path';
import {find, parseArguments, parseAsserts, parseResult} from "./util/spec.util";

// Parse tests from official suite

export const CORESUITE: string = process.env.CORESUITE ?? 'core/';
export const TARGET: string = process.env.TARGET ?? 'core-cleaned/';

const TESTFILE: string = process.env.TESTFILE ?? '';

const tests: TestScenario[] = [];

if (TESTFILE.length > 0) {
    const module: string = TESTFILE.replace('.asserts.wast', '.wast');
    const asserts: string[] = parseAsserts(CORESUITE + TESTFILE);
    tests.push(createTest(CORESUITE + module, asserts));
} else {
    console.log(`::group::converting ${CORESUITE}`);
    process.stdout.write(`> Scanning suite: ${CORESUITE}\n\n`);

    const files: string[] = readdirSync(CORESUITE).filter((file) => file.endsWith('.asserts.wast'));
    //const files: string[] = ['names_2.asserts.wast'];

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

    console.log('::endgroup::');
    console.log();
}

// run tests

const framework = Framework.getImplementation();
framework.style(OutputStyle.github);

const spec = framework.suite('Specification test suite for WebAssembly');
spec.testee('emulator [:8500]', new EmulatorSpecification(8500));
//spec.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'), new HybridScheduler());
spec.tests(tests);

framework.run([spec]);

// Helper function

function createTest(module: string, asserts: string[]): TestScenario {
    const steps: Step[] = [];

    for (const assert of asserts) {
        const cursor = {value: 0};
        const func: string = find(/invoke "([^"]+)"/, assert);
        const args: WASM.Value[] = parseArguments(assert.replace(`(invoke "${func} "`, ''), cursor);
        const result: WASM.Value | undefined = parseResult(assert.slice(cursor.value));

        if (result !== undefined) {
            steps.push({
                title: assert,
                instruction: invoke(func, args),
                expected: returns(result)
            });
        }
    }

    return {
        title: `Test: ${basename(module)}`,
        program: module,
        dependencies: [],
        steps: steps
    };
}


