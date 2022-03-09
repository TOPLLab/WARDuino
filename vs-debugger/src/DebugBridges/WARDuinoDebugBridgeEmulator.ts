import {ChildProcess, spawn} from 'child_process';
import * as net from 'net';
import {jsonParse} from '../Parsers/ParseUtils';
import {DebugBridge} from './DebugBridge';
import {DebugBridgeListener} from './DebugBridgeListener';
import {InterruptTypes} from './InterruptTypes';

export class WARDuinoDebugBridgeEmulator implements DebugBridge {

    private client?: net.Socket;
    private wasmPath: string;
    private cp?: ChildProcess;
    private listener: DebugBridgeListener;
    private pc: number = 0;

    constructor(wasmPath: string, listener: DebugBridgeListener) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.connect();
    }

    public async connect() {
        this.startEmulator();
    }

    public getProgramCounter(): number {
        return this.pc;
    }

    private initClient() {
        if (this.client === undefined) {
            this.client = new net.Socket();
            this.client.connect({port: 8192, host: '127.0.0.1'});
            this.listener.notifyProgress('Connected socket');
            this.client.on('error', err => {
                    this.listener.notifyError('Lost connection to the board');
                    console.log(err);
                }
            );
        }
    }

    private sendInterrupt(i: InterruptTypes) {
        let command = `${i} \n`;
        this.client?.write(command);
    }

    public step() {
        this.sendInterrupt(InterruptTypes.interruptSTEP);
        this.sendInterrupt(InterruptTypes.interruptDUMP);
    }

    private executeCommand(command: InterruptTypes) {
        console.log(command.toString());
        this.client?.write(command.toString + '\n');
    }

    private startEmulator() {
        this.cp = WARDuinoDebugBridgeEmulator.spawnEmulatorProcess();

        this.listener.notifyProgress('Started Emulator');
        while (this.cp.stdout === undefined) {
        }

        this.cp.stdout?.on('data', (data) => {
            this.initClient();
            console.log(`stdout: ${data}`);
            this.parse(data.toString());
        });

        this.cp.stderr?.on('data', (data) => {
            console.error(`stderr: ${data}`);
        });

        this.cp.on('error', (err) => {
            console.error('Failed to start subprocess.');
        });

        this.cp.on('close', (code) => {
            console.log('Something went wrong with the emulator stream');
            this.listener.notifyProgress('Disconnected from emulator');
        });

    }

    private parse(data: string) {
        let lines = data.split('\n');
        lines.forEach((line) => {
            if (line.startsWith('{"pc"')) {
                let json = jsonParse(line);
                let start = parseInt(json.start);
                this.pc = parseInt(json.pc) - start;
            }
        });
    }

    public disconnect(): void {
        this.cp?.kill();
        this.client?.destroy();
    }

    private static spawnEmulatorProcess(): ChildProcess {
        // TODO no absolute path. package extension with upload.wasm and compile warduino during installation.
        return spawn('/home/tolauwae/Arduino/libraries/WARDuino/vs-debugger/wdcli', ['--file', '/tmp/warduino/upload.wasm']);
    }

}