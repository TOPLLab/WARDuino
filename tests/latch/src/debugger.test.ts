/* eslint-disable @typescript-eslint/naming-convention */

import {
    Behaviour,
    Description,
    EmulatorBridge,
    Expectation,
    Expected,
    Framework,
    getValue,
    Instruction,
    Step,
    TestScenario
} from 'latch';

export const EMULATOR: string = process.env.EMULATOR ?? `${require('os').homedir()}/Arduino/libraries/WARDuino/build-emu/wdcli`;


const EXAMPLES: string = 'examples/';

/**
 * Tests of the Remote Debugger API
 */

const framework = Framework.getImplementation();

framework.platform(new EmulatorBridge(EMULATOR));
//framework.platform(new HardwareBridge(ARDUINO), new DependenceScheduler(), true);

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

const runTest: TestScenario = {
    title: 'Test RUN',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    steps: [DUMP, {
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

const pauseTest: TestScenario = {
    title: 'Test PAUSE',
    program: `${EXAMPLES}blink.wast`,
    dependencies: [dumpTest],
    steps: [{
        title: 'Send RUN command',
        instruction: Instruction.run,
        expectResponse: false
    }, {
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
        title: 'Send DUMP command',
        instruction: Instruction.dump,
        expected: [{'pc': {kind: 'primitive', value: 169} as Expected<number>}]
    }, {
        title: 'Send STEP command',
        instruction: Instruction.step,
        expectResponse: false
    }, {
        title: 'CHECK: execution took one step',
        instruction: Instruction.dump,
        expected: [{'pc': {kind: 'primitive', value: 172} as Expected<number>}]
    }]
};

framework.test(stepTest);

const stepOverTest: TestScenario = {
    title: 'Test STEP OVER',
    program: `${EXAMPLES}call.wast`,
    dependencies: [dumpTest],
    steps: [{
            title: 'Send DUMP command',
            instruction: Instruction.dump,
            expected: [{'pc': {kind: 'primitive', value: 167} as Expected<number>}]
        }, {
            title: 'Send STEP OVER command',
            instruction: Instruction.stepOver,
            delay: 500,
            expectResponse: false
        }, {
            title: 'CHECK: execution stepped over direct call',
            instruction: Instruction.dump,
            expected: [{'pc': {kind: 'primitive', value: 169} as Expected<number>}]
        }, {
            title: 'Send STEP OVER command',
            instruction: Instruction.stepOver,
            expectResponse: false
        }, {
            title: 'CHECK: execution took one step',
            instruction: Instruction.dump,
            expected: [{'pc': {kind: 'primitive', value: 171} as Expected<number>}]
        }, {
            title: 'Send STEP OVER command',
            instruction: Instruction.stepOver,
            delay: 500,
            expectResponse: false
        }, {
            title: 'CHECK: execution stepped over indirect call',
            instruction: Instruction.dump,
            expected: [{'pc': {kind: 'primitive', value: 174} as Expected<number>}]
        }]
}

framework.test(stepOverTest);

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
