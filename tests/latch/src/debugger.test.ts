/* eslint-disable @typescript-eslint/naming-convention */

import {
    ArduinoSpecification,
    Behaviour,
    Description,
    EmulatorSpecification,
    Expectation,
    Expected,
    Framework,
    getValue, HybridScheduler,
    Kind,
    Message, OutputStyle,
    Step, Suite,
    TestScenario,
    Breakpoint
} from 'latch';

export const EMULATOR: string = process.env.EMULATOR ?? `${require('os').homedir()}/Arduino/libraries/WARDuino/build-emu/wdcli`;


const EXAMPLES: string = `${__dirname}/../examples/`;

/**
 * Tests of the Remote Debugger API
 */

const framework = Framework.getImplementation();
framework.style(OutputStyle.github);

const integration: Suite = framework.suite('Integration tests: Debugger'); // must be called first

integration.testee('emulator [:8500]', new EmulatorSpecification(8500));
//integration.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'), new HybridScheduler(), {timeout: 0});

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
    instruction: {kind: Kind.Request, value: Message.dump},
    expected: expectDUMP
};

// Test *dump* command

integration.test({
    title: 'Test DUMP blink',
    program: `${EXAMPLES}blink.wast`,
    steps: [DUMP]
});

integration.test({
    title: 'Test DUMP button',
    program: `${EXAMPLES}button.wast`,
    steps: [DUMP]
});

integration.test({
    title: 'Test DUMP call',
    program: `${EXAMPLES}call.wast`,
    steps: [DUMP]
});

integration.test({
    title: 'Test DUMP factorial',
    program: `${EXAMPLES}factorial.wast`,
    steps: [DUMP]
});

// Test *dump local* command

integration.test({
    title: 'Test DUMPLocals blink',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals button',
    program: `${EXAMPLES}button.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals call',
    program: `${EXAMPLES}call.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals factorial',
    program: `${EXAMPLES}factorial.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals button',
    program: `${EXAMPLES}button.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals call',
    program: `${EXAMPLES}call.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

integration.test({
    title: 'Test DUMPLocals factorial',
    program: `${EXAMPLES}factorial.wast`,
    steps: [{
        title: 'Send DUMPLocals command',
        instruction: {kind: Kind.Request, value: Message.dumpLocals},
        expected: expectDUMPLocals
    }]
});

// Test *dump full* command

const dumpFullTest: TestScenario = {
    title: 'Test DUMPFull',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send DUMPFull command',
        instruction: {kind: Kind.Request, value: Message.dumpAll},
        expected: expectDUMP.concat([{
            'locals.count': {
                kind: 'comparison', value: (state: Object, value: number) => {
                    return value === getValue(state, 'locals.locals').length;
                }, message: 'locals.count should equal length of locals array'
            } as Expected<number>
        }])
    }]
};

integration.test(dumpFullTest);

// Test *run* command

const running: Step[] = [DUMP, {
    title: 'Send RUN command',
    instruction: {kind: Kind.Request, value: Message.run},
}, {
    title: 'CHECK: execution continues',
    instruction: {kind: Kind.Request, value: Message.dump},
    expected: [{
        'pc': {kind: 'description', value: Description.defined} as Expected<string>
    }, {
        'pc': {kind: 'behaviour', value: Behaviour.changed} as Expected<string>
    }]
}];

integration.test({
    title: 'Test RUN blink',
    program: `${EXAMPLES}blink.wast`,
    steps: running
});

integration.test({
    title: 'Test RUN button',
    program: `${EXAMPLES}button.wast`,
    steps: running
});

// Test *pause* command

const pauseTest: TestScenario = {
    title: 'Test PAUSE',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send RUN command',
        instruction: {kind: Kind.Request, value: Message.run},
    }, {
        title: 'Send PAUSE command',
        instruction: {kind: Kind.Request, value: Message.pause},
    }, {
        title: 'Send DUMP command',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }]
    }, {
        title: 'CHECK: execution is stopped',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{
            'pc': {kind: 'description', value: Description.defined} as Expected<string>
        }, {
            'pc': {kind: 'behaviour', value: Behaviour.unchanged} as Expected<string>
        }]
    }]
};

integration.test(pauseTest);

// Test *step into* command

function stepping(start: number, end: number): Step[] {
    return  [{
        title: 'Send DUMP command',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: start} as Expected<number>}]
    }, {
        title: 'Send STEP command',
        instruction: {kind: Kind.Request, value: Message.step},
    }, {
        title: 'CHECK: execution took one step',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{'pc': {kind: 'primitive', value: end} as Expected<number>}]
    }];
}

integration.test({
    title: 'Test STEP blink',
    program: `${EXAMPLES}blink.wast`,
    steps: stepping(169, 172)
});

integration.test({
    title: 'Test STEP button',
    program: `${EXAMPLES}button.wast`,
    steps: stepping(296, 298)
});

integration.test({
    title: 'Test STEP call',
    program: `${EXAMPLES}call.wast`,
    steps: stepping(167, 143)
});

integration.test({
    title: 'Test STEP factorial',
    program: `${EXAMPLES}factorial.wast`,
    steps: stepping(155, 157)
});

// Test *step over* command

const stepOverTest: TestScenario = {
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
}

integration.test(stepOverTest);

// EDWARD tests with mock proxy

const dumpEventsTest: TestScenario = {
    title: 'Test DUMPEvents',
    program: `${EXAMPLES}button.wast`,
    steps: [{
        title: 'CHECK: event queue',
        instruction: {kind: Kind.Request, value: Message.dumpEvents},
        expected: [{
            'events': {
                kind: 'comparison',
                value: (state: string, value: Array<any>) => value.length === 0,
                message: 'events queue is should be empty'
            } as Expected<Array<any>>
        }]
    }]
};

integration.test(dumpEventsTest);

framework.run([integration]);
