import {
    Action,
    ArduinoSpecification,
    EmulatorSpecification,
    Expected,
    Framework,
    Kind,
    Message,
    TestScenario,
    WASM,
    awaitBreakpoint, PureAction
} from 'latch';
import * as mqtt from 'mqtt';
import Type = WASM.Type;
import {Breakpoint} from "latch/dist/types/debug/Breakpoint";

const framework = Framework.getImplementation();

// TODO disclaimer: file is currently disabled until latch supports AS compilation

framework.suite('Integration tests: basic primitives');

framework.testee('emulator [:8520]', new EmulatorSpecification(8520));
framework.testee('esp wrover', new ArduinoSpecification('/dev/ttyUSB0', 'esp32:esp32:esp32wrover'));


const io: TestScenario = {
    title: 'Test digital I/O primitives',
    program: 'io.ts',
    dependencies: [],
    steps: [{
        title: 'Check: read LOW sensor value',
        instruction: {kind: Kind.Request, value: Message.invoke('digital.read', [WASM.i32(12)])},
        expected: [{'value': {kind: 'comparison', value: (state, value: string) => parseInt(value) === 0}}]
    }, {
        title: 'Drop stack value',
        instruction: {kind: Kind.Request, value: Message.invoke('digital.read', [])},
        expected: [{
            'stack': {
                kind: 'comparison', value: (state: Object, value: Array<any>) => {
                    return value.length === 0;
                }, message: 'stack should be empty'
            }
        }]
    }, {
        title: 'Check: write HIGH to pin',
        instruction: {kind: Kind.Request, value: Message.invoke('digital.write', [WASM.i32(36)])},
        expected: [{
            'stack': {
                kind: 'comparison', value: (state: Object, value: Array<any>) => {
                    return value.length === 0;
                }, message: 'stack should be empty'
            }
        }]
    }, {
        title: 'Check: read HIGH from pin',
        instruction: {kind: Kind.Request, value: Message.invoke('digital.read', [WASM.i32(36)])},
        expected: [{'value': {kind: 'comparison', value: (state, value: string) => parseInt(value) === 1}}]
    }]
};

framework.test(io);

const interrupts: TestScenario = {
    title: 'Test hardware interrupt primitives',
    program: 'interrupts.ts',
    steps: [{
        title: 'Subscribe to falling interrupt on pin 36',
        instruction: {
            kind: Kind.Request, value: Message.invoke('interrupts.subscribe', [
                {type: Type.i32, value: 36},
                {type: Type.i32, value: 0},
                {type: Type.i32, value: 2}])
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
        instruction: {kind: Kind.Request, value: Message.dumpCallbackmapping},
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

function sendMessage(): PureAction<void> {
    return {
        act: () => new Promise<void>((resolve) => {
            // send mqtt message
            let client: mqtt.MqttClient = mqtt.connect('mqtt://test.mosquitto.org');
            client.publish('parrot', 'This is an ex-parrot!');
            resolve();
        })
    };
}

const scenario: TestScenario = { // MQTT test scenario
    title: 'Test MQTT primitives',
    program: 'program.ts',
    dependencies: [],
    // initialBreakpoints: [{line: 8, column: 1}, {line: 11, column: 55}], TODO
    steps: [{
        title: 'Continue',
        instruction: {kind: Kind.Request, value: Message.run},
    }, {
        title: 'CHECK: callback function registered',
        instruction: {kind: Kind.Request, value: Message.dumpCallbackmapping},
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
        title: 'Send MQTT message',
        instruction: {kind: Kind.Action, value: sendMessage()}
    }, {
        title: 'Await breakpoint hit',
        instruction: {kind: Kind.Action, value: awaitBreakpoint()}
    }, {
        title: 'CHECK: entered callback function',
        instruction: {kind: Kind.Request, value: Message.dump},
        expected: [{
            'state': {kind: 'primitive', value: 'paused'},
            'line': {kind: 'primitive', value: 11},
            'column': {kind: 'primitive', value: 55}
        }]
    }]
};

framework.test(scenario);

// framework.run();
