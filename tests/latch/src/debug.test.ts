/* eslint-disable @typescript-eslint/naming-convention */

import {
    Behaviour,
    Description,
    EmulatorSpecification,
    Expectation,
    Expected,
    Framework,
    getValue,
    Kind,
    Message, OutputStyle,
    Step, Suite,
    TestScenario
} from 'latch';

const EXAMPLES: string = 'examples/';

/**
 * Tests of the Remote Debugger API
 */

const tests: TestScenario[] = [];

tests.push({
    title: 'Test STEP OVER',
    program: `${EXAMPLES}call.wast`,
    steps: [{
        title: 'Send DUMP command',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: 167} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: Message.stepOver},
    }, {
        title: 'CHECK: execution stepped over direct call',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: 169} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: Message.stepOver}
    }, {
        title: 'CHECK: execution took one step',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: 171} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: Message.stepOver}
    }, {
        title: 'CHECK: execution stepped over indirect call',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: 174} as Expected<number>}]
    }]
});

// run tests

const framework = Framework.getImplementation();
framework.style(OutputStyle.github);

const integration = framework.suite('Specification test suite for WebAssembly');
integration.testee('emulator [:8500]', new EmulatorSpecification(8500));
// integration.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'), new HybridScheduler(), {timeout: 0});
integration.tests(tests);

framework.run([integration]);
