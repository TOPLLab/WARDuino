import {ChildProcess, spawn} from 'child_process';
import * as net from 'net';
import {DebugBridge} from './DebugBridge';
import {DebugBridgeListener} from './DebugBridgeListener';
import {InterruptTypes} from './InterruptTypes';
import {DebugInfoParser} from "../Parsers/DebugInfoParser";
import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {Frame} from "../Parsers/Frame";
import { start } from 'repl';

export class WARDuinoDebugBridgeEmulator implements DebugBridge {

    private client?: net.Socket;
    private wasmPath: string;
    private cp?: ChildProcess;
    private listener: DebugBridgeListener;
    private parser: DebugInfoParser;
    private pc: number = 0;
    private locals: VariableInfo[] = [];
    private callstack: Frame[] = [];
    private startAddress : number = 0;

    constructor(wasmPath: string, listener: DebugBridgeListener) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.parser = new DebugInfoParser();
        this.connect();
    }
    pause(): void {
        throw new Error('Method not implemented.');
    }
    setStartAddress(startAddress: number) {
        this.startAddress = startAddress;
    }

    run(): void {
        this.sendInterrupt(InterruptTypes.interruptRUN);
    }
    
    setBreakPoint(x: number): void {
        console.log(this.startAddress);
        throw new Error('Method not implemented.');
    }

    public connect(): Promise<string> {
        return new Promise((resolve, reject) => {
            this.startEmulator();
            resolve("127.0.0.1:8192");
        });
    }

    public getProgramCounter(): number {
        return this.pc;
    }

    setProgramCounter(pc: number) {
        this.pc = pc;
    }

    getLocals(): VariableInfo[] {
        return this.locals;
    }

    setLocals(locals: VariableInfo[]) {
        this.locals = locals;
    }

    getCallstack(): Frame[] {
        return this.callstack;
    }

    setCallstack(callstack: Frame[]): void {
        this.callstack = callstack;
        this.listener.notifyStateUpdate();
    }

    private initClient() {
        let that = this;
        if (this.client === undefined) {
            this.client = new net.Socket();
            this.client.connect({port: 8192, host: '127.0.0.1'});  // TODO config
            this.listener.notifyProgress('Connected socket');

            this.client.on('error', err => {
                    this.listener.notifyError('Lost connection to the board');
                    console.log(err);
                }
            );

            this.client.on('data', data => {
                    console.log(`data: ${data}`);
                    that.parser.parse(that, data.toString());
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
    }

    public refresh() {
        this.sendInterrupt(InterruptTypes.interruptDUMPFull);
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

        this.initClient();

        this.cp.stdout?.on('data', (data) => {  // Print debug and trace information
            console.log(`stdout: ${data}`);
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

    public disconnect(): void {
        this.cp?.kill();
        this.client?.destroy();
    }

    private static spawnEmulatorProcess(): ChildProcess {
        // TODO no absolute path. package extension with upload.wasm and compile warduino during installation.
        return spawn('/home/tolauwae/Arduino/libraries/WARDuino/vs-debugger/wdcli', ['--file', '/tmp/warduino/upload.wasm']);
    }

}