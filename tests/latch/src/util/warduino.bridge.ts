/**
 * Functions and classes to bridge communication with WARDuino vm and debugger.
 */
import {Duplex, Readable} from 'stream';
import {ChildProcess, spawn} from 'child_process';
import {
    ArduinoUploader,
    CompilerFactory,
    Emulator,
    Instance,
    Instruction,
    ProcessBridge,
    SerialInstance,
    timeout,
    WatCompiler
} from 'latch';
import {ReadlineParser} from 'serialport';
import * as net from 'net';
import * as fs from 'fs';

export const WABT: string = process.env.WABT ?? '';

export const EMULATOR: string = process.env.EMULATOR ?? `${require('os').homedir()}/Arduino/libraries/WARDuino/build-emu/wdcli`;
export const ARDUINO: string = `${require('os').homedir()}/Arduino/libraries/WARDuino/platforms/Arduino/`;

export function isReadable(x: Readable | null): x is Readable {
    return x !== null;
}

export function startWARDuino(interpreter: string, program: string, port: number, args: string[] = []): ChildProcess {
    const _args: string[] = [program, '--socket', (port).toString(), '--paused'].concat(args);
    return spawn(interpreter, _args);
}

export function connectSocket(interpreter: string, program: string, port: number, args: string[] = []): Promise<Emulator> {
    const address = {port: port, host: '127.0.0.1'};
    const process = startWARDuino(interpreter, program, port, args);

    return new Promise(function (resolve, reject) {
        if (process === undefined) {
            reject('Failed to start process.');
        }

        while (process.stdout === undefined) {
        }

        if (isReadable(process.stdout)) {
            let error: string = '';

            const reader = new ReadlineParser();
            process.stdout.pipe(reader);

            reader.on('data', (data) => {
                if (data.includes('Listening')) {
                    const client = new net.Socket();
                    client.connect(address, () => {
                        resolve({process: process, interface: client});
                    });
                } else {
                    error = data.toString();
                }
            });

            reader.on('error', (err: Error) => {
                error = err.message;
            });

            reader.on('close', () => {
                reject(`Could not connect. Error:  ${error}`);
            });
        } else {
            process.kill();
            reject();
        }
    });
}

function convertToLEB128(a: number): string { // TODO can only handle 32 bit
    a |= 0;
    const result = [];
    while (true) {
        const byte_ = a & 0x7f;
        a >>= 7;
        if (
            (a === 0 && (byte_ & 0x40) === 0) ||
            (a === -1 && (byte_ & 0x40) !== 0)
        ) {
            result.push(byte_.toString(16).padStart(2, '0'));
            return result.join('').toUpperCase();
        }
        result.push((byte_ | 0x80).toString(16).padStart(2, '0'));
    }
}

export abstract class WARDuinoBridge extends ProcessBridge {
    public readonly instructionTimeout: number = 2000;

    protected connections: Instance[] = [];

    sendInstruction(socket: Duplex, chunk: any, expectResponse: boolean, parser: (text: string) => Object): Promise<Object | void> {
        const stack: MessageStack = new MessageStack('\n');

        return new Promise(function (resolve) {
            socket.on('data', (data: Buffer) => {
                stack.push(data.toString());
                stack.tryParser(parser, resolve);
            });

            if (chunk) {
                socket.write(`${chunk} \n`);
            }

            if (!expectResponse) {
                resolve(undefined);
            }
        });
    }

    protected check(instance: Instance): void {
        // todo implement
    }


    addListener(instance: Instance, listener: (data: string) => void): void {
        instance.interface.on('data', listener);
    }

    clearListeners(instance: Instance): void {
        instance.interface.removeAllListeners();
    }

    setProgram(socket: Duplex, program: string): Promise<boolean> {
        const binary: Buffer = fs.readFileSync(program);
        const size: string = convertToLEB128(binary.length);
        return timeout<Object | void>(`uploading ${program}`, this.instructionTimeout,
            this.sendInstruction(
                socket,
                `${Instruction.updateModule}${size}${binary.toString('hex')}`,
                true,
                (text: string) => {
                    if (!text.includes('CHANGE Module')) {
                        throw Error('No acknowledgment found.');
                    }
                    return true;
                })
        ).then((value: Object | void) => {
            return Promise.resolve(true);
        }).catch((reason: any) => {
            return Promise.resolve(false);
        });
        // todo all instructions time out after successful module change!
    }

    disconnect(instance: Instance): Promise<void> {
        instance.interface.destroy();
        return Promise.resolve();
    }
}

export class EmulatorBridge extends WARDuinoBridge {
    public readonly name: string = 'Emulator';
    public readonly connectionTimeout: number = 8000;

    protected readonly interpreter: string;
    protected port: number;

    private readonly compilerFactory: CompilerFactory;

    constructor(interpreter: string, port: number = 8200) {
        super();
        this.interpreter = interpreter;
        this.port = port;
        console.error(`wabt: ${WABT}`);
        this.compilerFactory = new CompilerFactory(WABT);
    }

    connect(program: string, args: string[] = []): Promise<Instance> {
        return this.compilerFactory.pickCompiler(program).compile(program).then((output) => {
            return connectSocket(this.interpreter, output.file, this.port++, args);
        });
    }

    disconnect(instance: Emulator): Promise<void> {
        instance.interface.destroy();
        instance.process.kill('SIGKILL');
        return Promise.resolve();
    }
}

export class HardwareBridge extends WARDuinoBridge {
    public readonly name: string = 'Hardware';
    public readonly instructionTimeout: number = 5000;
    public readonly connectionTimeout: number = 50000;

    protected readonly interpreter: string;
    protected readonly port: string;

    private compiler: WatCompiler;

    constructor(interpreter: string, port: string = '/dev/ttyUSB0') {
        super();
        this.interpreter = interpreter;
        this.port = port;
        this.compiler = new WatCompiler(WABT);
    }

    connect(program: string, args: string[] = []): Promise<Instance> {
        const bridge = this;

        // TODO wabt + sdkpath
        return this.compiler.compile(program).then((output) => {
            return new ArduinoUploader(output.file, this.interpreter, {path: bridge.port}).upload();
        }).then((connection) => Promise.resolve({interface: connection, program: program}));
    }

    disconnect(instance: SerialInstance): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            instance.interface.close((err) => {
                if (err) {
                    reject(err.message);
                    return;
                }
                instance.interface.destroy();
                resolve();
            });
        });
    }
}

class EDWARDBridge extends EmulatorBridge {
    public name: string = 'EDWARD bridge';

    private readonly proxy: string;

    constructor(interpreter: string, port: number = 8200, proxy: string = '/dev/ttyUSB0') {
        super(interpreter, port);
        this.proxy = proxy;
    }

    connect(program: string, args: string[] = []): Promise<Instance> {
        // TODO start proxy and supervisor. connect to both.
        // TODO which connection to return?
        args.concat(['--proxy', this.proxy]);
        return connectSocket(this.interpreter, program, this.port, args);
    }
}

class MessageStack {
    private readonly delimiter: string;
    private stack: string[];

    constructor(delimiter: string) {
        this.delimiter = delimiter;
        this.stack = [];
    }

    public push(data: string): void {
        const messages: string[] = this.split(data);
        if (this.lastMessageIncomplete()) {
            this.stack[this.stack.length - 1] += messages.shift();
        }
        this.stack = this.stack.concat(messages);
    }

    public pop(): string | undefined {
        if (this.hasCompleteMessage()) {
            return this.stack.shift();
        }
    }

    public tryParser(parser: (text: string) => Object, resolver: (value: Object) => void): void {
        let message = this.pop();
        while (message !== undefined) {
            try {
                const parsed = parser(message);
                resolver(parsed);
            } catch (e) {
                // do nothing
            } finally {
                message = this.pop();
            }
        }
    }

    private split(text: string): string[] {
        return text.split(new RegExp(`(.*?${this.delimiter})`, 'g')).filter(s => {
            return s.length > 0;
        });
    }

    private lastMessageIncomplete(): boolean {
        const last: string | undefined = this.stack[this.stack.length - 1];
        return last !== undefined && !last.includes(this.delimiter);
    }

    private hasCompleteMessage(): boolean {
        return !this.lastMessageIncomplete() || this.stack.length > 1;
    }
}
