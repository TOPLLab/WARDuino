import {
    Action,
    DependenceScheduler,
    EmulatorBridge,
    Expected,
    Framework,
    HardwareBridge,
    HybridScheduler,
    Instance,
    Instruction,
    ProcessBridge,
    TestScenario,
    Type
} from 'latch';
import * as mqtt from 'mqtt';

export const EMULATOR: string = process.env.EMULATOR ?? `${require('os').homedir()}/Arduino/libraries/WARDuino/build-emu/wdcli`;
export const ARDUINO: string = `${require('os').homedir()}/Arduino/libraries/WARDuino/platforms/Arduino/`;

const framework = Framework.getImplementation();

// TODO disclaimer: file is currently disabled until latch supports AS compilation

framework.platform(new EmulatorBridge(EMULATOR), new HybridScheduler(), true);
framework.platform(new HardwareBridge(ARDUINO), new DependenceScheduler(), true);

framework.suite('Integration tests: basic primitives');

function identity(text: string): Object {
    return {output: text};
}

const serial: TestScenario = {
    title: 'Test serial bus primitives',
    program: 'serial.ts',
    dependencies: [],
    steps: [{
        title: 'Check: print_int primitive',
        instruction: Instruction.invoke,
        parser: identity,
        expected: [{
            'output': {kind: 'primitive', value: '42\n'},
        }]
    }, {
        title: 'Check: print_string primitive with constant string',
        instruction: Instruction.invoke,
        parser: identity,
        expected: [{
            'output': {kind: 'primitive', value: 'What is the answer to life, the universe, and everything?\n'},
        }]
    }, {
        title: 'Check: print_string primitive with formatted string',
        instruction: Instruction.invoke,
        parser: identity,
        expected: [{
            'output': {kind: 'primitive', value: 'What do you get if you multiply six by nine? 42\n'},
        }]
    }]
};

framework.test(serial);

const io: TestScenario = {
    title: 'Test digital I/O primitives',
    program: 'io.ts',
    dependencies: [],
    steps: [{
        title: 'Check: read LOW sensor value',
        instruction: Instruction.invoke,
        payload: {name: 'digital.read', args: [{type: Type.i32, value: 12}]},
        expected: [{'value': {kind: 'comparison', value: (state, value: string) => parseInt(value) === 0}}]
    }, {
        title: 'Drop stack value',
        instruction: Instruction.invoke,
        payload: {name: 'drop', args: []},
        expected: [{
            'stack': {
                kind: 'comparison', value: (state: Object, value: Array<any>) => {
                    return value.length === 0;
                }, message: 'stack should be empty'
            }
        }]
    }, {
        title: 'Check: write HIGH to pin',
        instruction: Instruction.invoke,
        payload: {name: 'digital.write', args: [{type: Type.i32, value: 36}]},
        expected: [{
            'stack': {
                kind: 'comparison', value: (state: Object, value: Array<any>) => {
                    return value.length === 0;
                }, message: 'stack should be empty'
            }
        }]
    }, {
        title: 'Check: read HIGH from pin',
        instruction: Instruction.invoke,
        payload: {name: 'digital.read', args: [{type: Type.i32, value: 36}]},
        expected: [{'value': {kind: 'comparison', value: (state, value: string) => parseInt(value) === 1}}]
    }]
};

framework.test(io);

const interrupts: TestScenario = {
    title: 'Test hardware interrupt primitives',
    program: 'interrupts.ts',
    steps: [{
        title: 'Subscribe to falling interrupt on pin 36',
        instruction: Instruction.invoke,
        payload: {
            name: 'interrupts.subscribe', args: [
                {type: Type.i32, value: 36},
                {type: Type.i32, value: 0},
                {type: Type.i32, value: 2}]
        },
        expected: [{
            'stack': {
                kind: 'comparison', value: (state: Object, value: Array<any>) => {
                    return value.length === 0;
                }, message: 'stack should be empty'
            }
        }]
    }, {
        title: 'CHECK: callback function registered for pin 36',
        instruction: Instruction.dumpCallbackmapping,
        expected: [{
            'callbacks': {
                kind: 'comparison',
                value: (state: Object, mapping: Array<any>) => mapping.some((map: any) => {
                    return map.hasOwnProperty('interrupt_36') && map['interrupt_36'].length > 0 && map['interrupt_36'].includes(0);
                }),
                message: 'callback should be registered for parrot topic'
            } as Expected<Array<any>>
        }]
    }]
};

framework.test(interrupts);

framework.suite('Integration tests: Wi-Fi and MQTT primitives');

function awaitBreakpoint(bridge: ProcessBridge, instance: Instance): Promise<string> {
    return new Promise<string>((resolve) => {
        // await breakpoint hit
        bridge.addListener(instance, (data: string) => {
            bridge.clearListeners(instance);
            resolve(data);
        });

        // send mqtt message
        let client: mqtt.MqttClient = mqtt.connect('mqtt://test.mosquitto.org');
        client.publish('parrot', 'This is an ex-parrot!');
    });
}

const scenario: TestScenario = { // MQTT test scenario
    title: 'Test MQTT primitives',
    program: 'program.ts',
    dependencies: [],
    initialBreakpoints: [{line: 8, column: 1}, {line: 11, column: 55}],
    steps: [{
        title: 'Continue',
        instruction: Instruction.run,
        expectResponse: false
    }, {
        title: 'CHECK: callback function registered',
        instruction: Instruction.dumpCallbackmapping,
        expected: [{
            'callbacks': {
                kind: 'comparison',
                value: (state: Object, mapping: Array<any>) => mapping.some((map: any) => {
                    return map.hasOwnProperty('parrot') && map['parrot'].length > 0;
                }),
                message: 'callback should be registered for parrot topic'
            } as Expected<Array<any>>
        }]
    }, {
        title: 'Send MQTT message and await breakpoint hit',
        instruction: new Action(awaitBreakpoint)
    }, {
        title: 'CHECK: entered callback function',
        instruction: Instruction.dump,
        expected: [{
            'state': {kind: 'primitive', value: 'paused'},
            'line': {kind: 'primitive', value: 11},
            'column': {kind: 'primitive', value: 55}
        }]
    }]
};

framework.test(scenario);

framework.run();
