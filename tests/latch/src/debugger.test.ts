/* eslint-disable @typescript-eslint/naming-convention */

import {
    Behaviour,
    DebugProtocol,
    Description,
    EmulatorSpecification,
    Expectation,
    Expected,
    Framework,
    Kind,
    Message,
    Request, Step, Suite,
    TestScenario,
    Breakpoint,
    WASM,
    WARDuino,
    Verbosity
} from 'latch';

export const EMULATOR: string = process.env.EMULATOR ?? `${require('os').homedir()}/Arduino/libraries/WARDuino/build-emu/wdcli`;


const EXAMPLES: string = `${__dirname}/../static/examples/`;

/**
 * Tests of the Remote Debugger API
 */

const framework = Framework.getImplementation();

const integration: Suite = framework.suite('Integration tests: Debugger'); // must be called first

integration.testee('emulator [:8500]', new EmulatorSpecification(8500));

const expectDUMP: Expectation[] = [
    {'programCounter': {kind: 'description', value: Description.defined} as Expected<number>},
    {
        'breakpoints': {
            kind: 'comparison', value: (_: Object, value: Array<any>) => {
                return value.length === 0;
            }, message: 'list of breakpoints should be empty'
        } as Expected<Array<any>>
    },
    ];

const expectDUMPLocals: Expectation[] = [
    {'values': {kind: 'description', value: Description.defined} as Expected<Array<unknown>>}];

const DUMP: Step = {
    title: 'Send DUMP command',
    instruction: {kind: Kind.Request, value: Message.snapshot},
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

const snapshotTest: TestScenario = {
    title: 'Test snapshot',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send snapshot command',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: expectDUMP
    }]
};

integration.test(snapshotTest);

// Test *run* command

const running: Step[] = [DUMP, {
    title: 'Send RUN command',
    instruction: {kind: Kind.Request, value: Message.run},
}, {
    title: 'CHECK: execution continues',
    instruction: {kind: Kind.Request, value: Message.snapshot},
    expected: [{
        'programCounter': {kind: 'description', value: Description.defined} as Expected<number>
    }, {
        'programCounter': {kind: 'behaviour', value: Behaviour.changed} as Expected<number>
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
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{
            'programCounter': {kind: 'description', value: Description.defined} as Expected<number>
        }]
    }, {
        title: 'CHECK: execution is stopped',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{
            'programCounter': {kind: 'description', value: Description.defined} as Expected<number>
        }, {
            'programCounter': {kind: 'behaviour', value: Behaviour.unchanged} as Expected<number>
        }]
    }]
};

integration.test(pauseTest);

// Test *step into* command

function stepping(start: number, end: number): Step[] {
    return  [{
        title: 'Send DUMP command',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: start} as Expected<number>}]
    }, {
        title: 'Send STEP command',
        instruction: {kind: Kind.Request, value: Message.step},
    }, {
        title: 'CHECK: execution took one step',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: end} as Expected<number>}]
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

const stepOverCall: Request<DebugProtocol.HitBreakpoint> = {
    type: DebugProtocol.Command.COMMAND_STEP_OVER,
    notification: DebugProtocol.NotificationType.NOTIFICATION_HIT_BREAKPOINT,
    parser: DebugProtocol.HitBreakpoint.decode
};

const stepOverTest: TestScenario = {
    title: 'Test STEP OVER',
    program: `${EXAMPLES}call.wast`,
    steps: [{
        title: 'Send DUMP command',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: 167} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: stepOverCall},
    }, {
        title: 'CHECK: execution stepped over direct call',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: 169} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: Message.stepOver}
    }, {
        title: 'CHECK: execution took one step',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: 171} as Expected<number>}]
    }, {
        title: 'Send STEP OVER command',
        instruction: {kind: Kind.Request, value: stepOverCall}
    }, {
        title: 'CHECK: execution stepped over indirect call',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: [{'programCounter': {kind: 'primitive', value: 174} as Expected<number>}]
    }]
}

integration.test(stepOverTest);

// EDWARD tests with mock proxy

const dumpEventsTest: TestScenario = {
    title: 'Test DUMPEvents',
    program: `${EXAMPLES}button.wast`,
    steps: [{
        title: 'CHECK: event queue',
        instruction: {kind: Kind.Request, value: Message.dumpAllEvents},
        expected: [{
            'events': {
                kind: 'comparison',
                value: (_: string, value: Array<any>) => value.length === 0,
                message: 'events queue is should be empty'
            } as Expected<Array<any>>
        }]
    }]
};

integration.test(dumpEventsTest);

integration.test({
    title: 'Test INSPECT',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Inspect complete state',
        instruction: {kind: Kind.Request, value: Message.inspect([])},
        expected: expectDUMP
    }]
});

integration.test({
    title: 'Test DUMPEvents range',
    program: `${EXAMPLES}button.wast`,
    steps: [{
        title: 'Dump event range',
        instruction: {kind: Kind.Request, value: Message.dumpEvents({start: 0, end: 0})},
        expected: [{totalCount: {kind: 'primitive', value: 0} as Expected<number>}]
    }]
});

const inspectExpectations: Record<WARDuino.Inspect, Expectation> = {
    [WARDuino.Inspect.counter]: {programCounter: {kind: 'description', value: Description.defined} as Expected<number>},
    [WARDuino.Inspect.breakpoints]: {breakpoints: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.callstack]: {callstack: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.globals]: {globals: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.table]: {table: {kind: 'description', value: Description.defined} as Expected<Object>},
    [WARDuino.Inspect.memory]: {memory: {kind: 'description', value: Description.defined} as Expected<Object>},
    [WARDuino.Inspect.branching]: {branchTable: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.stack]: {stack: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.callbacks]: {callbacks: {kind: 'description', value: Description.defined} as Expected<Object>},
    [WARDuino.Inspect.events]: {queue: {kind: 'description', value: Description.defined} as Expected<Object>},
    [WARDuino.Inspect.io]: {io: {kind: 'description', value: Description.defined} as Expected<Array<any>>}
};

integration.test({
    title: 'Test INSPECT selectors',
    program: `${EXAMPLES}blink.wast`,
    steps: [
        ...(Object.keys(inspectExpectations) as WARDuino.Inspect[]).map((selector): Step => ({
            title: `Inspect ${selector}`,
            instruction: {kind: Kind.Request, value: Message.inspect([selector])},
            expected: [inspectExpectations[selector]]
        })),
        {
            title: 'Inspect all selectors',
            instruction: {kind: Kind.Request, value: Message.inspect(Object.keys(inspectExpectations) as WARDuino.Inspect[])},
            expected: (Object.keys(inspectExpectations) as WARDuino.Inspect[]).map((selector) => inspectExpectations[selector])
        } as Step
    ]
});

integration.test({
    title: 'Test breakpoints',
    program: `${EXAMPLES}blink.wast`,
    steps: [
        {title: 'Add breakpoint', instruction: {kind: Kind.Request, value: Message.addBreakpoint(new Breakpoint(169, 0))}},
        {title: 'Remove breakpoint', instruction: {kind: Kind.Request, value: Message.removeBreakpoint(new Breakpoint(169, 0))}}
    ]
});

integration.test({
    title: 'Test RESET',
    program: `${EXAMPLES}blink.wast`,
    steps: [
        {title: 'Reset runtime', instruction: {kind: Kind.Request, value: Message.reset}},
        {title: 'Check reset state', instruction: {kind: Kind.Request, value: Message.snapshot}, expected: expectDUMP}
    ]
});

integration.test({
    title: 'Test INVOKE',
    program: `${EXAMPLES}factorial.wast`,
    steps: [{
        title: 'Invoke fac(5)',
        instruction: {kind: Kind.Request, value: Message.invoke('fac', [WASM.i32(5n)])},
        expected: [{value: {kind: 'comparison', value: (_: Object, value: string) => parseInt(value) === 120} as Expected<string>}]
    }]
});

framework.reporter.verbosity(Verbosity.more);
framework.run([integration]);
