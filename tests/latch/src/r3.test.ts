import {EmulatorSpecification, Framework, Kind, Message, StyleType, Suite, TestScenario, Verbosity, WASM} from 'latch';
import * as fs from 'fs';
import * as path from 'path';

const framework = Framework.getImplementation();
framework.style(StyleType.github);
framework.reporter.verbosity(Verbosity.short);

// TODO disclaimer: file is currently disabled until latch supports AS compilation

const suite: Suite = framework.suite('End-to-end tests: R3 benchmark suite');

suite.testee('emulator [:8530]', new EmulatorSpecification(8530));

const scenarios: TestScenario[] = [];
const r3Directory = path.resolve(__dirname, '../static/r3');
console.error(r3Directory)

if (fs.existsSync(r3Directory)) {
    fs.readdirSync(r3Directory)
        .filter((file: string) => file.endsWith('.wat'))
        .forEach((wasmFile: string) => {
            scenarios.push({
                title: `R3: ${wasmFile}`,
                program: path.join('static', 'r3', wasmFile),
                dependencies: [],
                steps: [{
                    title: `Run ${wasmFile}`,
                    instruction: {kind: Kind.Request, value: Message.run}
                }]
            });
        });
}

suite.tests(scenarios);

framework.run([suite]);
