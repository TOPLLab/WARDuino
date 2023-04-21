/**
 * This file contains test suites of the WARDuino VM and debugger.
 *
 * These tests are independent of the plugin and uses the emulator version of the VM (wdcli).
 */

/* eslint-disable @typescript-eslint/naming-convention */

import 'mocha';
import {Behaviour, Description, Expectation, Expected, getValue, Step, TestScenario} from '../../src/framework/Describer';
import {Framework} from '../../src/framework/Framework';
import {DependenceScheduler} from '../../src/framework/Scheduler';
import {ARDUINO, EMULATOR, EmulatorBridge, HardwareBridge} from './util/warduino.bridge';
import {Instruction} from '../../src/framework/Actions';

const EXAMPLES: string = 'src/test/suite/examples/';
let INITIAL_PORT: number = 7900;

/**
 * Test Suite of the WARDuino CLI
 */

// describe('WARDuino CLI: test exit codes', () => {
//     let process: ChildProcess;
//
//     /**
//      * Tests to see if VM and debugger start properly
//      */
//
//     it('Test: exit code (0)', function (done) {
//         this.timeout(3500);
//         process = spawn(EMULATOR, [`${EXAMPLES}hello.wasm`, '--no-debug']).on('exit', function (code) {
//             expect(code).to.equal(0);
//             done();
//         });
//     });
//
//     it('Test: exit code (1)', function (done) {
//         process = spawn(EMULATOR, [`${EXAMPLES}nonexistent.wasm`, '--socket', (INITIAL_PORT++).toString()]).on('exit', function (code) {
//             expect(code).to.equal(1);
//             done();
//         });
//     });
//
//     afterEach('Shutdown CLI', function () {
//         process.removeAllListeners('exit');
//         process.kill('SIGKILL');
//     });
// });
//
// describe('WARDuino CLI: test debugging socket', () => {
//
//     it('Test: start websocket', function (done) {
//         let succeeded = false;
//
//         const process: ChildProcess = startWARDuino(EMULATOR, `${EXAMPLES}blink.wasm`, INITIAL_PORT++);
//         process.on('exit', function (code) {
//             assert.isTrue(succeeded, `Interpreter should not exit (${code}).`);
//             done();
//         });
//
//         while (process.stdout === undefined) {
//         }
//
//         if (isReadable(process.stdout)) {
//             const reader = new ReadlineParser();
//             process.stdout.pipe(reader);
//
//             reader.on('data', (data) => {
//                 if (data.includes('Listening')) {
//                     succeeded = true;
//                     process.kill('SIGKILL');
//                 }
//             });
//         }
//     });
//
//     it('Test: connect to websocket', async function () {
//         const instance: Emulator = await connectSocket(EMULATOR, `${EXAMPLES}blink.wasm`, INITIAL_PORT++);
//         instance.interface.destroy();
//         instance.process.kill('SIGKILL');
//     });
// });
//
// describe.skip('WARDuino CLI: test proxy connection', () => {
//     it('Test: --proxy flag', function (done) {
//         const address = {port: INITIAL_PORT, host: '127.0.0.1'};
//         const proxy: net.Server = new net.Server();
//         proxy.listen(INITIAL_PORT++);
//         proxy.on('connection', () => {
//             done();
//         });
//
//         connectSocket(EMULATOR, `${EXAMPLES}blink.wasm`, INITIAL_PORT++, ['--proxy', address.port.toString()]).then((instance: Emulator) => {
//             instance.process.on('exit', function (code) {
//                 assert.fail(`Interpreter should not exit. (code: ${code})`);
//                 done();
//             });
//         }).catch(function (message) {
//             assert.fail(message);
//             done();
//         });
//     });
// });

/**
 * Tests of the Remote Debugger API
 */

const framework = Framework.getImplementation();

framework.platform(new EmulatorBridge(EMULATOR));
framework.platform(new HardwareBridge(ARDUINO), new DependenceScheduler(), true);

framework.suite('Integration tests: Debugger');

const expectDUMP: Expectation[] = [
    {'pc': {kind: 'description', value: Description.defined} as Expected<string>},
    {
        'breakpoints': {
            kind: 'comparison', value: (state: Object, value: Array<any>) => {
                return value.length === 0;
            }, message: 'list of breakpoints should be empty'
        } as Expected<Array<any>>
    },
    {'callstack[0].sp': {kind: 'primitive', value: -1} as Expected<number>},
    {'callstack[0].fp': {kind: 'primitive', value: -1} as Expected<number>}];

const expectDUMPLocals: Expectation[] = [
    {'locals': {kind: 'description', value: Description.defined} as Expected<string>},
    {
        'count': {
            kind: 'comparison', value: (state: Object, value: number) => {
                return value === getValue(state, 'locals').length;
            }, message: 'count should equal length of locals array'
        } as Expected<number>
    }];

const DUMP: Step = {
    title: 'Send DUMP command',
    instruction: Instruction.dump,
    expected: expectDUMP
};

const dumpTest: TestScenario = {
    title: 'Test DUMP',
    program: `${EXAMPLES}blink.wast`,
    steps: [DUMP]
};

framework.test(dumpTest);

const dumpLocalsTest: TestScenario = {
    title: 'Test DUMPLocals',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: Instruction.dumpLocals,
        expected: expectDUMPLocals
    }]
};

framework.test(dumpLocalsTest);

const dumpFullTest: TestScenario = {
    title: 'Test DUMPFull',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send DUMPFull command',
        instruction: Instruction.dumpAll,
        expected: expectDUMP.concat([{
            'locals.count': {
                kind: 'comparison', value: (state: Object, value: number) => {
                    return value === getValue(state, 'locals.locals').length;
                }, message: 'locals.count should equal length of locals array'
            } as Expected<number>
        }])
    }]
};

framework.test(dumpFullTest);

const pauseTest: TestScenario = {
    title: 'Test PAUSE',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'Send PAUSE command',
        instruction: Instruction.pause,
        expectResponse: false
    }, {
        title: 'Send DUMP command',
        instruction: Instruction.dump,
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }]
    }, {
        title: 'CHECK: execution is stopped',
        instruction: Instruction.dump,
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }, {
            'pc': {kind: 'behaviour', value: Behaviour.unchanged} as Expected<string>
        }]
    }]
};

framework.test(pauseTest);

const stepTest: TestScenario = {
    title: 'Test STEP',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'Send PAUSE command',
        instruction: Instruction.pause,
        expectResponse: false
    }, DUMP, {
        title: 'Send STEP command',
        instruction: Instruction.step,
        expectResponse: false
    }, {
        title: 'CHECK: execution took one step',
        instruction: Instruction.dump,
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }, {
            'pc': {kind: 'behaviour', value: Behaviour.changed} as Expected<string>
        }]
    }]
};

framework.test(stepTest);

const runTest: TestScenario = {
    title: 'Test RUN',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'Send PAUSE command',
        instruction: Instruction.pause,
        expectResponse: false
    }, DUMP, {
        title: 'CHECK: execution is stopped',
        instruction: Instruction.dump,
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }, {
            'pc': {kind: 'behaviour', value: Behaviour.unchanged} as Expected<string>
        }]
    }, {
        title: 'Send RUN command',
        instruction: Instruction.run,
        delay: 100,
        expectResponse: false
    }, {
        title: 'CHECK: execution continues',
        instruction: Instruction.dump,
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }, {
            'pc': {kind: 'behaviour', value: Behaviour.changed} as Expected<string>
        }]
    }]
};

framework.test(runTest);

// EDWARD tests with mock proxy

function encodeEvent(topic: string, payload: string): Promise<string> {
    return Promise.resolve(`{topic: '${topic}', payload: '${payload}'}`);
}

function ackParser(text: string): Object {
    return {'ack': text};
}

const eventNotificationTest: TestScenario = {
    title: 'Test "pushed event" Notification',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    skip: true,
    steps: [{
        title: 'Push mock event',
        instruction: Instruction.pushEvent,
        payload: encodeEvent('interrupt', ''),
        parser: ackParser,
        expected: [{
            'ack': {
                kind: 'comparison',
                value: (state: string, value: string) => value.includes('Interrupt: 73'),
                message: 'no acknowledge received from runtime'
            } as Expected<string>
        }]
    }]
};

framework.test(eventNotificationTest);

const dumpEventsTest: TestScenario = {
    title: 'Test DUMPEvents',
    program: `${EXAMPLES}button.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'CHECK: event queue',
        instruction: Instruction.dumpEvents,
        expected: [{
            'events': {
                kind: 'comparison',
                value: (state: string, value: Array<any>) => value.length === 0,
                message: 'events queue is should be empty'
            } as Expected<Array<any>>
        }]
    }]
};

framework.test(dumpEventsTest);

const receiveEventTest: TestScenario = {
    title: 'Test Event Transfer (supervisor side)',
    program: `${EXAMPLES}button.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'Send PAUSE command',
        instruction: Instruction.pause,
        expectResponse: false
    }, {
        title: 'Push mock event',
        instruction: Instruction.pushEvent,
        payload: encodeEvent('interrupt', ''),
        expectResponse: false
    }, {
        title: 'CHECK: event queue',
        instruction: Instruction.dumpEvents,
        expected: [{
            'events': {
                kind: 'comparison',
                value: (state: string, value: Array<any>) => value.length === 1,
                message: 'events queue should include 1 event'
            } as Expected<Array<any>>
        }]
    }],
    skip: true
};

framework.test(receiveEventTest);

const dumpCallbackMappingTest: TestScenario = {
    title: 'Test DUMPCallbackmapping',
    program: `${EXAMPLES}button.wast`,
    dependencies: [dumpTest],
    skip: true,
    steps: [{
        title: 'CHECK: callbackmapping',
        instruction: Instruction.dumpCallbackmapping,
        expected: [{
            'callbacks': {
                kind: 'comparison',
                value: (state: string, value: Array<any>) => value.length === 1,
                message: 'callbackmapping should contain one callback'
            } as Expected<Array<any>>
        }]
    }]
};

framework.test(dumpCallbackMappingTest);

framework.run();
