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

const expectSnapshot: Expectation[] = [
    {'programCounter': {kind: 'description', value: Description.defined} as Expected<number>},
    {
        'breakpoints': {
            kind: 'comparison', value: (_: Object, value: Array<any>) => {
                return value.length === 0;
            }, message: 'list of breakpoints should be empty'
        } as Expected<Array<any>>
    },
    ];

const SNAPSHOT: Step = {
    title: 'Send SNAPSHOT command',
    instruction: {kind: Kind.Request, value: Message.snapshot},
    expected: expectSnapshot
};

// Test full snapshot command

const snapshotTest: TestScenario = {
    title: 'Test snapshot',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Send snapshot command',
        instruction: {kind: Kind.Request, value: Message.snapshot},
        expected: expectSnapshot
    }]
};

integration.test(snapshotTest);

const expectSnapshotLocals: Expectation[] = [
    ...expectSnapshot,
    {'locals': {kind: 'description', value: Description.defined} as Expected<Object>},
    {'locals.values': {kind: 'description', value: Description.defined} as Expected<Array<unknown>>}
];

integration.test({
    title: 'Test SNAPSHOT button',
    program: EXAMPLES + 'button.wast',
    steps: [SNAPSHOT]
});

integration.test({
    title: 'Test SNAPSHOT call',
    program: EXAMPLES + 'call.wast',
    steps: [SNAPSHOT]
});

integration.test({
    title: 'Test SNAPSHOT factorial',
    program: EXAMPLES + 'factorial.wast',
    steps: [SNAPSHOT]
});

for (const [name, program] of [['blink', 'blink.wast'], ['button', 'button.wast'], ['call', 'call.wast'], ['factorial', 'factorial.wast']] as const) {
    integration.test({
        title: 'Test SNAPSHOT locals ' + name,
        program: EXAMPLES + program,
        steps: [{
            title: 'Send SNAPSHOT command',
            instruction: {kind: Kind.Request, value: Message.snapshot},
            expected: expectSnapshotLocals
        }]
    });
}

// Test *run* command

const running: Step[] = [SNAPSHOT, {
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
        title: 'Send SNAPSHOT command',
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
        title: 'Send SNAPSHOT command',
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
        title: 'Send SNAPSHOT command',
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

integration.test({
    title: 'Test INSPECT',
    program: `${EXAMPLES}blink.wast`,
    steps: [{
        title: 'Inspect complete state',
        instruction: {kind: Kind.Request, value: Message.inspect([])},
        expected: expectSnapshot
    }]
});

const inspectExpectations: Record<WARDuino.Inspect, Expectation> = {
    [WARDuino.Inspect.counter]: {programCounter: {kind: 'description', value: Description.defined} as Expected<number>},
    [WARDuino.Inspect.breakpoints]: {breakpoints: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.callstack]: {callstack: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.globals]: {globals: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.table]: {table: {kind: 'description', value: Description.notDefined} as Expected<Object>},
    [WARDuino.Inspect.memory]: {memory: {kind: 'description', value: Description.notDefined} as Expected<Object>},
    [WARDuino.Inspect.branching]: {branchTable: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.stack]: {stack: {kind: 'description', value: Description.defined} as Expected<Array<any>>},
    [WARDuino.Inspect.callbacks]: {callbacks: {kind: 'description', value: Description.notDefined} as Expected<Object>},
    [WARDuino.Inspect.events]: {queue: {kind: 'description', value: Description.notDefined} as Expected<Object>},
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
        {title: 'Check reset state', instruction: {kind: Kind.Request, value: Message.snapshot}, expected: expectSnapshot}
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

const operationSucceeded: Expectation[] = [
    {success: {kind: "primitive", value: true} as Expected<boolean>}
];

const proxyCallWithoutProxy: Request<DebugProtocol.OperationResult> = {
    ...Message.proxyCall({functionIndex: 3, arguments: []}),
    notification: DebugProtocol.NotificationType.NOTIFICATION_OPERATION_RESULT,
    parser: DebugProtocol.OperationResult.decode
};

const popEventWithoutCallback: Request<DebugProtocol.OperationResult> = {
    ...Message.popEvent,
    parser: DebugProtocol.OperationResult.decode
};

const addProxyWithoutSupervisor: Request<DebugProtocol.OperationResult> = {
    ...Message.addProxy({functionIndex: 3}),
    parser: DebugProtocol.OperationResult.decode
};

const removeProxyWithoutSupervisor: Request<DebugProtocol.OperationResult> = {
    ...Message.removeProxy({functionIndex: 3}),
    parser: DebugProtocol.OperationResult.decode
};

integration.test({
    title: "Test remaining debugger requests",
    program: `${EXAMPLES}blink.wast`,
    steps: [
        {title: "Update function body", instruction: {kind: Kind.Request, value: Message.updateFunction({functionIndex: 3, instructions: Buffer.from([0x0b])})}, expected: operationSucceeded},
        {title: "Update local value", instruction: {kind: Kind.Request, value: Message.updateLocal(0, {i32Bits: 7, index: 0})}, expected: operationSucceeded},
        {title: "Update stack value", instruction: {kind: Kind.Request, value: Message.updateStack(0, {i32Bits: 8, index: 0})}, expected: operationSucceeded},
        {title: "Update global value", instruction: {kind: Kind.Request, value: Message.updateGlobal(0, {i32Bits: 9, index: 0})}, expected: operationSucceeded},
        {title: "Update callback mapping", instruction: {kind: Kind.Request, value: Message.updateCallbacks({entries: []})}, expected: operationSucceeded},
        {title: "Push an event", instruction: {kind: Kind.Request, value: Message.pushEvent("test", "payload")}},
        {title: "Pop event", instruction: {kind: Kind.Request, value: popEventWithoutCallback}, expected: [{success: {kind: "primitive", value: false} as Expected<boolean>}]},
        {title: "Load snapshot state", instruction: {kind: Kind.Request, value: Message.loadSnapshot({programCounter: 0, state: DebugProtocol.State.STATE_WARDUINO_PAUSE, breakpoints: [], functions: [], callstack: [], globals: [], stack: [], branchTable: [], io: [], overrides: [], heapUsed: 0})}, expected: operationSucceeded},
        {title: "Set snapshot policy", instruction: {kind: Kind.Request, value: Message.setSnapshotPolicy({policy: DebugProtocol.SnapshotPolicy.SNAPSHOT_POLICY_NONE, interval: 1, minimumReturnCount: 0, selectedState: Buffer.alloc(0)})}, expected: operationSucceeded},
        {title: "Set primitive override", instruction: {kind: Kind.Request, value: Message.setOverride({primitiveName: "chip_delay", argumentWords: [1000], result: 0})}, expected: operationSucceeded},
        {title: "Remove primitive override", instruction: {kind: Kind.Request, value: Message.removeOverride({primitiveName: "chip_delay", argumentWords: [1000], result: 0})}, expected: operationSucceeded},
        {title: "Proxy call without proxy", instruction: {kind: Kind.Request, value: proxyCallWithoutProxy}, expected: [{success: {kind: "primitive", value: false} as Expected<boolean>}]},
        {title: "Create proxy", instruction: {kind: Kind.Request, value: Message.proxifyRequest}, expected: operationSucceeded},
        {title: "Add proxied function", instruction: {kind: Kind.Request, value: addProxyWithoutSupervisor}, expected: [{success: {kind: "primitive", value: false} as Expected<boolean>}]},
        {title: "Remove proxied function", instruction: {kind: Kind.Request, value: removeProxyWithoutSupervisor}, expected: [{success: {kind: "primitive", value: false} as Expected<boolean>}]}
    ]
});

integration.test({
    title: "Test CONTINUE_FOR and checkpointing",
    program: `${EXAMPLES}blink.wast`,
    steps: [
        {title: "Enable checkpointing", instruction: {kind: Kind.Request, value: Message.setSnapshotPolicy({policy: DebugProtocol.SnapshotPolicy.SNAPSHOT_POLICY_CHECKPOINTING, interval: 1, minimumReturnCount: 0, selectedState: Buffer.alloc(0)})}, expected: operationSucceeded},
        {title: "Continue for one instruction", instruction: {kind: Kind.Request, value: Message.continueFor({count: 1})}, expected: [{instructionCount: {kind: "description", value: Description.defined} as Expected<number>}]}
    ]
});

integration.test({
    title: "Test HALT",
    program: `${EXAMPLES}blink.wast`,
    steps: [{title: "Halt debugger connection", instruction: {kind: Kind.Request, value: Message.halt}}]
});

framework.reporter.verbosity(Verbosity.more);
framework.run([integration]);
